#pragma once

// Offline reader for Black Ops III fastfiles (zone/*.ff), no game process involved.
// Pure standard C++ (no MFC / WraithX / zlib dependency) so it is unit tested on its own,
// see ../Tests/OfflineTests.cpp, and shared by the GUI exe (--offline) and the standalone CLI (Tests/OfflineCli.cpp).
//
// What is implemented (all verified against the retail core_common.ff, see docs/OFFLINE.md):
//   * container: "TAff0000" header, zone name, chain of zlib blocks -> the decompressed zone stream
//   * zone stream head: XAssetList = script strings + the asset manifest (type of every asset, in load order)
// What is NOT implemented: the asset bodies (XAnimParts, XModel, ...). The zone stream is a sequential, unaligned,
// pointer-fixup-free serialisation; reaching one asset needs the loader of every asset before it (docs/OFFLINE.md).

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <algorithm>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h> // FindFirstFileA: the GUI project builds as C++14 (no <filesystem>)
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace BO3FF
{
    // ---------------------------------------------------------------- inflate (RFC 1950/1951)
    // Small table-less decoder (canonical Huffman by counts, as in zlib's puff.c). Returns false on corrupt input.
    namespace detail
    {
        struct Bits
        {
            const uint8_t* In; size_t Size; size_t Pos = 0; uint32_t Buf = 0; int Cnt = 0;
            bool Need(int n) { while (Cnt < n) { if (Pos >= Size) return false; Buf |= (uint32_t)In[Pos++] << Cnt; Cnt += 8; } return true; }
            bool Get(int n, uint32_t& V) { if (n == 0) { V = 0; return true; } if (!Need(n)) return false; V = Buf & ((1u << n) - 1); Buf >>= n; Cnt -= n; return true; }
        };

        struct Huff { uint16_t Count[16]; uint16_t Symbol[320]; };

        inline bool Build(Huff& H, const uint16_t* Len, int N)
        {
            std::memset(H.Count, 0, sizeof(H.Count));
            for (int i = 0; i < N; i++) H.Count[Len[i]]++;
            int Left = 1;
            for (int l = 1; l < 16; l++) { Left <<= 1; Left -= H.Count[l]; if (Left < 0) return false; }
            uint16_t Offs[16]; Offs[1] = 0;
            for (int l = 1; l < 15; l++) Offs[l + 1] = Offs[l] + H.Count[l];
            for (int i = 0; i < N; i++) if (Len[i]) H.Symbol[Offs[Len[i]]++] = (uint16_t)i;
            return true;
        }

        inline bool Decode(Bits& B, const Huff& H, int& Sym)
        {
            int Code = 0, First = 0, Index = 0;
            for (int l = 1; l < 16; l++)
            {
                uint32_t Bit; if (!B.Get(1, Bit)) return false;
                Code |= (int)Bit;
                int C = H.Count[l];
                if (Code - C < First) { Sym = H.Symbol[Index + (Code - First)]; return true; }
                Index += C; First += C; First <<= 1; Code <<= 1;
            }
            return false;
        }
    }

    // Raw deflate stream -> Out (appended). Consumed receives the number of input bytes used.
    inline bool InflateRaw(const uint8_t* In, size_t Size, std::vector<uint8_t>& Out, size_t* Consumed = nullptr)
    {
        using namespace detail;
        static const uint16_t LBase[29] = { 3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258 };
        static const uint16_t LExt[29] = { 0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0 };
        static const uint16_t DBase[30] = { 1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577 };
        static const uint16_t DExt[30] = { 0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13 };
        Bits B{ In, Size };
        uint32_t Final = 0;
        do
        {
            uint32_t Type;
            if (!B.Get(1, Final) || !B.Get(2, Type)) return false;
            if (Type == 0)
            {
                B.Buf = 0; B.Cnt = 0; // drop the partial byte (Need() never reads ahead more than needed)
                if (B.Pos + 4 > Size) return false;
                uint16_t Len = In[B.Pos] | (In[B.Pos + 1] << 8), NLen = In[B.Pos + 2] | (In[B.Pos + 3] << 8);
                if ((uint16_t)~NLen != Len) return false;
                B.Pos += 4;
                if (B.Pos + Len > Size) return false;
                Out.insert(Out.end(), In + B.Pos, In + B.Pos + Len);
                B.Pos += Len;
            }
            else if (Type == 1 || Type == 2)
            {
                static Huff LH, DH; // rebuilt per block; not thread safe by design (single threaded tool)
                uint16_t Len[320];
                if (Type == 1)
                {
                    int i = 0;
                    for (; i < 144; i++) Len[i] = 8;
                    for (; i < 256; i++) Len[i] = 9;
                    for (; i < 280; i++) Len[i] = 7;
                    for (; i < 288; i++) Len[i] = 8;
                    Build(LH, Len, 288);
                    for (i = 0; i < 30; i++) Len[i] = 5;
                    Build(DH, Len, 30);
                }
                else
                {
                    uint32_t HLit, HDist, HCLen;
                    if (!B.Get(5, HLit) || !B.Get(5, HDist) || !B.Get(4, HCLen)) return false;
                    HLit += 257; HDist += 1; HCLen += 4;
                    if (HLit > 286 || HDist > 30) return false;
                    static const uint8_t Order[19] = { 16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15 };
                    uint16_t CL[19] = {};
                    for (uint32_t i = 0; i < HCLen; i++) { uint32_t V; if (!B.Get(3, V)) return false; CL[Order[i]] = (uint16_t)V; }
                    Huff CH; if (!Build(CH, CL, 19)) return false;
                    uint32_t Idx = 0;
                    while (Idx < HLit + HDist)
                    {
                        int Sym; if (!Decode(B, CH, Sym)) return false;
                        if (Sym < 16) Len[Idx++] = (uint16_t)Sym;
                        else
                        {
                            uint16_t Prev = 0; uint32_t Rep, V;
                            if (Sym == 16) { if (Idx == 0) return false; Prev = Len[Idx - 1]; if (!B.Get(2, V)) return false; Rep = 3 + V; }
                            else if (Sym == 17) { if (!B.Get(3, V)) return false; Rep = 3 + V; }
                            else { if (!B.Get(7, V)) return false; Rep = 11 + V; }
                            if (Idx + Rep > HLit + HDist) return false;
                            while (Rep--) Len[Idx++] = Prev;
                        }
                    }
                    if (Len[256] == 0) return false;
                    if (!Build(LH, Len, (int)HLit)) return false;
                    if (!Build(DH, Len + HLit, (int)HDist)) return false;
                }
                for (;;)
                {
                    int Sym; if (!Decode(B, LH, Sym)) return false;
                    if (Sym < 256) Out.push_back((uint8_t)Sym);
                    else if (Sym == 256) break;
                    else
                    {
                        Sym -= 257; if (Sym >= 29) return false;
                        uint32_t E, Length, Dist;
                        if (!B.Get(LExt[Sym], E)) return false; Length = LBase[Sym] + E;
                        int DS; if (!Decode(B, DH, DS) || DS >= 30) return false;
                        if (!B.Get(DExt[DS], E)) return false; Dist = DBase[DS] + E;
                        if (Dist > Out.size()) return false;
                        size_t From = Out.size() - Dist;
                        for (uint32_t i = 0; i < Length; i++) Out.push_back(Out[From + i]);
                    }
                }
            }
            else return false;
        } while (!Final);
        if (Consumed) *Consumed = B.Pos - (size_t)(B.Cnt / 8);
        return true;
    }

    // zlib wrapped (2 byte header, adler32 trailer which is not verified: the block sizes are checked instead)
    inline bool InflateZlib(const uint8_t* In, size_t Size, std::vector<uint8_t>& Out)
    {
        if (Size < 2 || (In[0] & 0x0F) != 8 || ((In[0] << 8) | In[1]) % 31 != 0 || (In[1] & 0x20)) return false;
        return InflateRaw(In + 2, Size - 2, Out);
    }

    // ---------------------------------------------------------------- container
    constexpr size_t HeaderSize = 0x248;     // magic, version, RSA signature, zone name; first block header follows
    constexpr size_t ZoneNameOffset = 0xF8;  // NUL terminated
    constexpr size_t ZoneNameMax = 0x40;

    struct FastFile
    {
        uint32_t Version = 0;       // 0x251 on the retail PC build
        std::string ZoneName;
        uint32_t BlockCount = 0;
        std::vector<uint8_t> Zone;  // decompressed zone stream
    };

    inline uint32_t U32(const uint8_t* P) { uint32_t V; std::memcpy(&V, P, 4); return V; }
    inline uint64_t U64(const uint8_t* P) { uint64_t V; std::memcpy(&V, P, 8); return V; }

    // Parses an in-memory .ff. Error is filled on failure.
    inline bool ParseFastFile(const std::vector<uint8_t>& File, FastFile& Out, std::string& Error)
    {
        Out = FastFile();
        if (File.size() < HeaderSize) { Error = "file too small for a BO3 fastfile header"; return false; }
        if (std::memcmp(File.data(), "TAff0000", 8) != 0) { Error = "not a BO3 fastfile (magic is not TAff0000)"; return false; }
        Out.Version = U32(File.data() + 8);
        if (Out.Version != 0x251) { Error = "unsupported fastfile version " + std::to_string(Out.Version) + " (BO3 retail PC is 593)"; return false; }
        const char* N = (const char*)File.data() + ZoneNameOffset;
        Out.ZoneName.assign(N, strnlen(N, ZoneNameMax));

        size_t Pos = HeaderSize;
        while (Pos + 16 <= File.size())
        {
            uint32_t Comp = U32(&File[Pos]), Uncomp = U32(&File[Pos + 4]), Stride = U32(&File[Pos + 8]);
            if (Comp == 0 || Uncomp == 0) break; // trailer block (signature data), not part of the zone
            if (Pos + 16 + Comp > File.size()) { Error = "truncated block " + std::to_string(Out.BlockCount); return false; }
            size_t Before = Out.Zone.size();
            if (!InflateZlib(&File[Pos + 16], Comp, Out.Zone)) { Error = "corrupt zlib data in block " + std::to_string(Out.BlockCount); return false; }
            if (Out.Zone.size() - Before != Uncomp) { Error = "block " + std::to_string(Out.BlockCount) + " inflated to the wrong size"; return false; }
            Out.BlockCount++;
            if (Stride < Comp) { Error = "bad block stride"; return false; }
            Pos += 16 + Stride;
        }
        if (Out.BlockCount == 0) { Error = "no blocks"; return false; }
        return true;
    }

    inline bool ReadFile(const std::string& Path, std::vector<uint8_t>& Data, std::string& Error)
    {
        std::ifstream F(Path, std::ios::binary | std::ios::ate);
        if (!F) { Error = "cannot open " + Path; return false; }
        Data.resize((size_t)F.tellg());
        F.seekg(0);
        F.read((char*)Data.data(), (std::streamsize)Data.size());
        return (bool)F;
    }

    // ---------------------------------------------------------------- zone head (XAssetList)
    // Layout (verified): {u32 stringCount, pad, u64 ptr} {u32 dependCount, pad, u64 ptr} {u32 assetCount, pad, u64 ptr}
    // at 0x00/0x10/0x20, then stringCount u64 pointers at 0x30 (-1 = string follows inline, 0 = none), the inline
    // NUL-terminated strings, then assetCount 16 byte entries {u32 type, u32 pad, u64 ptr} (ptr -1 = body in the stream).
    struct AssetEntry { uint32_t Type; uint64_t Ptr; };

    struct ZoneHead
    {
        std::vector<std::string> Strings;      // script strings (inline ones; empty for ptr 0)
        std::vector<AssetEntry> Assets;        // in load order
        size_t AssetListOffset = 0;            // offset of the first entry in the zone stream
        size_t AssetListEnd = 0;               // offset of the first asset body
        std::map<uint32_t, uint32_t> TypeCounts;
    };

    constexpr uint64_t InlinePtr = 0xFFFFFFFFFFFFFFFFull;

    inline bool ParseZoneHead(const std::vector<uint8_t>& Zone, ZoneHead& H, std::string& Error)
    {
        H = ZoneHead();
        if (Zone.size() < 0x30) { Error = "zone too small"; return false; }
        const uint32_t StringCount = U32(&Zone[0]), AssetCount = U32(&Zone[0x20]);
        if (U64(&Zone[8]) != InlinePtr && StringCount) { Error = "unexpected string table pointer"; return false; }
        if (U64(&Zone[0x28]) != InlinePtr && AssetCount) { Error = "unexpected asset list pointer"; return false; }
        if (StringCount > 0x1000000 || AssetCount > 0x1000000) { Error = "implausible counts"; return false; }
        size_t Pos = 0x30;
        const size_t PtrBytes = (size_t)StringCount * 8;
        if (Pos + PtrBytes > Zone.size()) { Error = "string pointer table past end of zone"; return false; }
        std::vector<uint64_t> Ptrs(StringCount);
        for (uint32_t i = 0; i < StringCount; i++) Ptrs[i] = U64(&Zone[Pos + 8 * (size_t)i]);
        Pos += PtrBytes;
        H.Strings.reserve(StringCount);
        for (uint32_t i = 0; i < StringCount; i++)
        {
            if (Ptrs[i] != InlinePtr) { H.Strings.emplace_back(); continue; }
            size_t E = Pos;
            while (E < Zone.size() && Zone[E] != 0) E++;
            if (E >= Zone.size()) { Error = "unterminated script string " + std::to_string(i); return false; }
            H.Strings.emplace_back((const char*)&Zone[Pos], E - Pos);
            Pos = E + 1;
        }
        H.AssetListOffset = Pos;
        if (Pos + (size_t)AssetCount * 16 > Zone.size()) { Error = "asset list past end of zone"; return false; }
        H.Assets.reserve(AssetCount);
        for (uint32_t i = 0; i < AssetCount; i++)
        {
            AssetEntry E{ U32(&Zone[Pos + 16 * (size_t)i]), U64(&Zone[Pos + 16 * (size_t)i + 8]) };
            H.Assets.push_back(E);
            H.TypeCounts[E.Type]++;
        }
        H.AssetListEnd = Pos + (size_t)AssetCount * 16;
        return true;
    }

    // BO3 asset type ids as they appear in the zone manifest (same as the pool indices Greyhound reads from memory)
    inline const char* TypeName(uint32_t T)
    {
        switch (T)
        {
        case 3: return "xanim";
        case 4: return "xmodel";
        case 9: return "ximage";
        case 0x26: return "fx";
        case 0x2F: return "rawfile";
        default: return "";
        }
    }

    inline uint32_t TypeFromName(const std::string& N)
    {
        if (N == "anim") return 3;
        if (N == "model") return 4;
        if (N == "image") return 9;
        if (N == "rawfile") return 0x2F;
        return 0xFFFFFFFFu;
    }

    // ---------------------------------------------------------------- offline "list" run (shared by the exe and the CLI)
    inline std::string JsonEsc(const std::string& S)
    {
        std::string O;
        for (unsigned char C : S)
        {
            if (C == '"') O += "\\\""; else if (C == '\\') O += "\\\\";
            else if (C < 0x20) { char B[8]; std::snprintf(B, sizeof(B), "\\u%04x", C); O += B; }
            else O.push_back((char)C);
        }
        return O;
    }

    // The .ff files of ZoneDir (sorted), optionally only those whose zone name (file name without .ff) is in Names
    inline std::vector<std::string> FindZoneFiles(const std::string& ZoneDir, const std::vector<std::string>& Names)
    {
        std::vector<std::string> Out;
        WIN32_FIND_DATAA FD;
        HANDLE H = FindFirstFileA((ZoneDir + "\\*.ff").c_str(), &FD);
        if (H == INVALID_HANDLE_VALUE) return Out;
        do
        {
            if (FD.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            std::string File = FD.cFileName;
            if (File.size() < 4 || File.compare(File.size() - 3, 3, ".ff") != 0) continue; // "*.ff" also matches ".ffx" short names
            const std::string Stem = File.substr(0, File.size() - 3);
            if (!Names.empty() && std::find(Names.begin(), Names.end(), Stem) == Names.end()) continue;
            Out.push_back(ZoneDir + "\\" + File);
        } while (FindNextFileA(H, &FD));
        FindClose(H);
        std::sort(Out.begin(), Out.end());
        return Out;
    }

    // Lists the zones (*.ff of ZoneDir, or only the comma separated ones in Zones) with their manifest. Returns the JSON report.
    // Never writes into ZoneDir. Exit code in Code: 0 ok, 3 nothing found, 4 a zone failed to parse.
    inline std::string ListZones(const std::string& ZoneDir, const std::vector<std::string>& Files, int& Code)
    {
        std::string J = "{\n  \"mode\": \"offline\",\n  \"game\": \"black_ops_3\",\n  \"zones\": [";
        Code = Files.empty() ? 3 : 0;
        bool First = true;
        for (auto& Path : Files)
        {
            std::vector<uint8_t> Data; std::string Err; FastFile FF; ZoneHead H;
            bool Ok = ReadFile(Path, Data, Err) && ParseFastFile(Data, FF, Err) && ParseZoneHead(FF.Zone, H, Err);
            J += First ? "\n    " : ",\n    "; First = false;
            J += "{\"file\": \"" + JsonEsc(Path) + "\"";
            if (!Ok) { J += ", \"error\": \"" + JsonEsc(Err) + "\"}"; Code = 4; continue; }
            J += ", \"zone\": \"" + JsonEsc(FF.ZoneName) + "\", \"blocks\": " + std::to_string(FF.BlockCount)
               + ", \"zone_bytes\": " + std::to_string(FF.Zone.size()) + ", \"script_strings\": " + std::to_string(H.Strings.size())
               + ", \"assets\": " + std::to_string(H.Assets.size()) + ", \"types\": {";
            bool F2 = true;
            for (auto& KV : H.TypeCounts)
            {
                std::string N = TypeName(KV.first);
                if (N.empty()) N = "type" + std::to_string(KV.first);
                J += (F2 ? "\"" : ", \"") + N + "\": " + std::to_string(KV.second); F2 = false;
            }
            J += "}}";
        }
        J += "\n  ]\n}\n";
        (void)ZoneDir;
        return J;
    }
}
