// Standalone tests for BO3FastFile.h (inflate, container, zone head). Build + run: Tests\run_tests.bat
// Fixtures are generated in memory (stored zlib blocks wrapped in the real container layout) plus three
// zlib vectors produced with the Python zlib module (stored / fixed Huffman / dynamic Huffman).
// With GREYHOUND_BO3_ZONE_DIR set, also checks the retail core_common.ff (read only; skipped otherwise).
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include "../WraithXCOD/BO3FastFile.h"

static int Fails = 0, Skipped = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); Fails++; } } while (0)
using namespace BO3FF;

static const uint8_t ZDyn[] = {120,218,13,211,5,3,19,5,24,0,80,12,12,12,12,12,12,12,12,12,244,118,219,221,109,183,187,237,238,150,23,219,237,182,91,220,110,187,109,183,187,237,22,119,167,98,96,96,96,96,96,96,96,96,96,96,96,96,96,96,96,96,96,96,96,96,119,119,119,125,239,63,188,9,242,56,211,158,40,181,144,248,164,106,187,63,158,28,64,92,103,74,45,199,4,166,230,194,157,236,180,2,170,120,211,205,72,190,51,131,205,107,230,76,47,16,68,209,97,183,46,19,65,83,38,41,193,47,134,88,68,180,162,233,110,40,81,16,173,17,18,44,132,2,99,187,172,36,49,171,78,217,82,160,133,151,209,156,69,23,109,124,80,108,246,92,15,35,240,224,172,190,81,74,207,238,149,98,253,57,193,74,184,58,87,118,226,234,188,112,70,109,205,247,67,68,100,1,134,12,218,11,179,17,167,188,104,80,192,106,139,53,92,25,46,169,12,72,126,105,103,24,192,151,69,73,2,93,110,86,133,232,138,42,55,42,174,172,83,122,120,213,56,81,27,173,22,107,221,241,154,112,148,166,214,134,92,75,91,71,143,181,228,219,106,164,17,251,128,44,123,169,79,221,88,174,254,149,135,145,244,247,177,40,238,255,162,231,227,222,159,105,191,18,253,143,105,81,213,13,51,13,159,217,180,209,71,136,45,227,44,67,111,27,112,154,133,29,145,102,54,176,43,221,230,58,123,106,40,111,236,203,41,198,224,192,72,140,77,4,226,100,112,136,23,19,104,43,26,172,199,77,166,203,232,161,84,59,152,44,9,163,80,148,145,83,73,150,82,199,29,53,165,117,36,172,97,88,105,155,235,226,57,19,115,202,166,139,187,92,42,147,62,44,70,120,221,163,50,120,55,127,156,145,232,101,79,74,182,194,242,105,68,182,106,157,21,142,39,51,231,101,83,76,228,34,187,198,135,47,43,68,218,169,171,132,65,186,115,157,210,139,8,55,245,2,36,123,27,47,10,153,187,162,104,191,126,31,217,47,70,31,202,12,165,209,99,93,170,228,60,37,233,78,240,185,68,86,204,189,196,39,233,208,107,141,28,53,120,139,182,248,241,251,174,96,199,62,25,210,249,230,151,88,44,37,124,39,231,197,244,207,248,200,144,255,24,85,226,189,127,189,130,47,109,192,84,199,241,77,90,99,130,217,66,146,149,194,54,89,54,236,238,160,48,110,109,23,39,135,20,247,200,15,56,109,31,182,216,241,14,224,248,188,130,20,101,33,129,249,102,137,35,235,137,161,28,87,131,133,81,82,71,18,42,143,68,147,98,190,84,86,149,82,138,10,88,181,100,160,145,106,42,161,10,102,186,233,38,218,175,216,40,61,206,21,203,213,67,75,249,76,236,200,94,53,29,60,54,227,149,240,19,9,213,207,158,26,202,212,250,103,198,179,178,122,174,86,43,23,46,100,48,71,187,20,107,243,200,149,225,122,164,114,173,208,193,249,27,7,56,43,220,90,209,244,250,157,69,145,35,238,213,5,194,124,16,173,146,141,71,107,65,169,251,164,216,24,133,159,149,74,53,253,197,134,166,230,94,141,140,155,210,155,86,46,164,189,103,208,117,236,99,155,204,25,95,144,169,124,249,219,102,203,208,127,146,211,81,239,247,124,180,39,255,163,199,218,241,245,137,66,159,218,184,237,83,153,205,171,141,86,103,235,102,77,201,110,143,116,10,244,206,74,184,201,236,206,26,145,226,222,49,197,12,236,207,23,53,227,224,174,217,170,135,132,56,218,141,196,135,37,50,198,89,137,118,66,29,49,165,108,144,17,213,92,187,107,91,197,70,89,98,171,182,202,142,27,84,39,217,235,152,116,197,237,149,123,24,55,170,52,77,251,144,94,215,168,30,145,192,251,149,99,188,106,172,123,130,147,177,226,167,248,201,170,115,70,82,174,181,206,233,15,28,251,2,173,144,10,95,82,75,33,218,21,118,118,200,95,195,214,3,233,27,134,145,180,114,75,167,55,24,221,209,55,245,226,61,228,168,158,121,64,231,250,194,35,162,147,104,60,145,213,199,209,103,74,141,174,243,130,59,30,88,175,168,60,229,190,193,135,154,201,119,37,47,55,252,200,192,132,230,231,180,80,54,190,25,186,190,247,99,175,85,17,127,243,13,17,255,59,61,202,35,235,193,127,98,35,248,47,109,6,255,253,173,224,127,103,59,248,223,222,9,254,187,187,193,127,126,47,248,143,238,7,255,7,7,193,127,63,8,255,185,48,252,239,210,240,191,196,193,255,66,6,254,235,18,252,199,20,248,223,168,192,127,69,135,255,106,27,254,247,108,248,159,26,194,127,212,135,255,222,225,240,159,56,26,254,231,143,135,255,161,147,225,127,255,116,248,239,156,13,255,195,231,195,255,242,197,240,159,185,28,254,147,87,195,127,246,122,248,95,185,25,254,71,110,135,255,163,187,225,255,240,126,248,79,60,12,255,75,143,195,127,234,105,248,31,124,30,254,71,94,134,255,210,235,240,223,125,7,254,55,63,132,255,245,207,224,191,253,53,252,143,255,0,255,123,191,194,127,253,47,248,111,76,128,255,200,68,248,47,79,130,255,212,100,248,31,158,2,255,217,169,240,191,54,13,254,199,166,195,255,192,12,248,143,206,132,255,28,10,255,91,4,252,23,40,248,159,100,225,191,152,134,255,109,17,254,39,10,240,31,43,195,255,80,29,254,179,45,248,223,180,224,127,122,0,255,105,15,254,39,102,193,255,202,108,248,79,204,129,255,253,185,240,223,154,7,255,253,249,240,159,95,0,255,145,133,240,223,94,4,255,157,197,240,159,92,2,255,181,165,240,63,189,12,254,115,203,225,191,184,2,254,155,43,225,127,118,21,252,143,174,134,255,212,26,248,47,173,133,255,131,117,240,95,253,31,173,156,3,74};
static const uint8_t ZFixed[] = {120,1,203,72,205,201,201,87,200,192,32,203,243,139,114,82,0,163,138,10,249};
static const uint8_t ZStored[] = {120,1,1,17,0,238,255,115,116,111,114,101,100,32,98,108,111,99,107,32,100,97,116,97,59,205,6,119};
static std::vector<uint8_t> Gen(int n)
{
    std::vector<uint8_t> V;
    for (int i = 0; i < n; i++) V.push_back(i % 5 ? (uint8_t)(((i * i * 31 + (i >> 4)) & 0x3F) + 0x30) : (uint8_t)(i & 0xFF));
    return V;
}

static void Put32(std::vector<uint8_t>& V, uint32_t X) { for (int i = 0; i < 4; i++) V.push_back((uint8_t)(X >> (8 * i))); }
static void Put64(std::vector<uint8_t>& V, uint64_t X) { for (int i = 0; i < 8; i++) V.push_back((uint8_t)(X >> (8 * i))); }

// zlib stream made of stored deflate blocks (valid zlib, adler32 not checked by the reader)
static std::vector<uint8_t> StoredZlib(const std::vector<uint8_t>& Data)
{
    std::vector<uint8_t> Z = { 0x78, 0x01 };
    size_t Pos = 0;
    do
    {
        size_t N = std::min<size_t>(Data.size() - Pos, 0xFFFF);
        bool Last = Pos + N >= Data.size();
        Z.push_back(Last ? 1 : 0);
        Z.push_back((uint8_t)N); Z.push_back((uint8_t)(N >> 8)); Z.push_back((uint8_t)~N); Z.push_back((uint8_t)(~N >> 8));
        Z.insert(Z.end(), Data.begin() + Pos, Data.begin() + Pos + N);
        Pos += N;
    } while (Pos < Data.size());
    Z.insert(Z.end(), 4, 0);
    return Z;
}

// A zone stream: 2 script strings (one inline, one absent), then assets {type, ptr}, then fake bodies
static std::vector<uint8_t> MakeZone(const std::vector<std::pair<uint32_t, uint64_t>>& Assets, size_t Pad = 0)
{
    std::vector<uint8_t> Z;
    Put32(Z, 2); Put32(Z, 0); Put64(Z, InlinePtr);
    Put32(Z, 0); Put32(Z, 0); Put64(Z, 0);
    Put32(Z, (uint32_t)Assets.size()); Put32(Z, 0); Put64(Z, InlinePtr);
    Put64(Z, InlinePtr); Put64(Z, 0);
    const char* S = "tag_origin"; Z.insert(Z.end(), S, S + 11);
    for (auto& A : Assets) { Put32(Z, A.first); Put32(Z, 0); Put64(Z, A.second); }
    Z.insert(Z.end(), Pad, 0xAB);
    return Z;
}

// Container: header, blocks of BlockSize uncompressed bytes (stride = comp rounded up to 4), then a trailer block
static std::vector<uint8_t> MakeFF(const std::vector<uint8_t>& Zone, const char* Name, size_t BlockSize, uint32_t Version = 0x251)
{
    std::vector<uint8_t> F(HeaderSize, 0);
    std::memcpy(F.data(), "TAff0000", 8);
    std::memcpy(&F[8], &Version, 4);
    std::memcpy(&F[ZoneNameOffset], Name, std::strlen(Name));
    for (size_t Pos = 0; Pos < Zone.size(); Pos += BlockSize)
    {
        size_t N = std::min(BlockSize, Zone.size() - Pos);
        auto Z = StoredZlib(std::vector<uint8_t>(Zone.begin() + Pos, Zone.begin() + Pos + N));
        uint32_t Stride = (uint32_t)((Z.size() + 3) & ~3u);
        Put32(F, (uint32_t)Z.size()); Put32(F, (uint32_t)N); Put32(F, Stride); Put32(F, (uint32_t)F.size());
        F.insert(F.end(), Z.begin(), Z.end());
        F.insert(F.end(), Stride - Z.size(), 0);
    }
    Put32(F, 1234); Put32(F, 0); Put32(F, 1236); Put32(F, 0); // trailer (uncompressed size 0): never inflated
    F.insert(F.end(), 64, 0x5A);
    return F;
}

int main()
{
    // ---- inflate
    { std::vector<uint8_t> O; CHECK(InflateZlib(ZStored, sizeof(ZStored), O) && std::string(O.begin(), O.end()) == "stored block data"); }
    { std::vector<uint8_t> O; CHECK(InflateZlib(ZFixed, sizeof(ZFixed), O) && std::string(O.begin(), O.end()) == "hello hello hello hello world"); }
    { std::vector<uint8_t> O; CHECK(InflateZlib(ZDyn, sizeof(ZDyn), O) && O == Gen(1500)); }
    { std::vector<uint8_t> B(ZDyn, ZDyn + sizeof(ZDyn)); B[40] ^= 0xFF; B[41] ^= 0xFF; std::vector<uint8_t> O; CHECK(!InflateZlib(B.data(), B.size(), O) || O != Gen(1500)); }
    { std::vector<uint8_t> O; CHECK(!InflateZlib(ZDyn, 30, O)); }                              // truncated
    { uint8_t Bad[] = { 0x78, 0x01, 0x07 }; std::vector<uint8_t> O; CHECK(!InflateZlib(Bad, 3, O)); } // block type 3

    // ---- container + zone head
    auto Assets = std::vector<std::pair<uint32_t, uint64_t>>{ {3, InlinePtr}, {3, InlinePtr}, {4, InlinePtr}, {9, InlinePtr}, {0x26, 0x5000000000001234ull} };
    auto Zone = MakeZone(Assets, 777);
    for (size_t BS : { size_t(64), size_t(100000) })
    {
        auto File = MakeFF(Zone, "unit_zone", BS);
        FastFile FF; std::string E;
        CHECK(ParseFastFile(File, FF, E));
        CHECK(FF.Zone == Zone && FF.ZoneName == "unit_zone" && FF.Version == 0x251);
        CHECK(FF.BlockCount == (Zone.size() + BS - 1) / BS);
        ZoneHead H;
        CHECK(ParseZoneHead(FF.Zone, H, E));
        CHECK(H.Strings.size() == 2 && H.Strings[0] == "tag_origin" && H.Strings[1].empty());
        CHECK(H.Assets.size() == 5 && H.TypeCounts[3] == 2 && H.TypeCounts[4] == 1 && H.TypeCounts[9] == 1 && H.TypeCounts[0x26] == 1);
        CHECK(H.Assets[4].Ptr == 0x5000000000001234ull);
        CHECK(H.AssetListEnd + 777 == FF.Zone.size());
    }

    // ---- negative controls
    {
        FastFile FF; std::string E;
        auto File = MakeFF(Zone, "z", 64);
        auto Bad = File; Bad[0] = 'X';           CHECK(!ParseFastFile(Bad, FF, E) && E.find("magic") != std::string::npos);
        CHECK(!ParseFastFile(MakeFF(Zone, "z", 64, 0x250), FF, E) && E.find("version") != std::string::npos);
        CHECK(!ParseFastFile(std::vector<uint8_t>(File.begin(), File.begin() + 100), FF, E));
        Bad = File; Bad.resize(HeaderSize + 40); CHECK(!ParseFastFile(Bad, FF, E) && E.find("truncated") != std::string::npos);
        Bad = File; Bad[HeaderSize + 16] = 0x00;  CHECK(!ParseFastFile(Bad, FF, E));
        ZoneHead H;
        auto ShortZone = Zone; ShortZone.resize(0x38);   CHECK(!ParseZoneHead(ShortZone, H, E));
        auto NoNul = Zone; NoNul.resize(0x30 + 16 + 5);  CHECK(!ParseZoneHead(NoNul, H, E));
        auto BigCount = Zone; BigCount[0x23] = 0x7F;     CHECK(!ParseZoneHead(BigCount, H, E));
        auto Ptr = Zone; Ptr[0x28] = 0;                  CHECK(!ParseZoneHead(Ptr, H, E));
    }

    // ---- JSON report on temp files (and the read-only guarantee: the input file is not modified)
    {
        auto Dir = std::filesystem::temp_directory_path() / "bo3ff_unit";
        std::filesystem::remove_all(Dir);
        std::filesystem::create_directories(Dir);
        auto P = (Dir / "unit_zone.ff").string();
        { auto File = MakeFF(Zone, "unit_zone", 64); std::ofstream(P, std::ios::binary).write((const char*)File.data(), (std::streamsize)File.size()); }
        auto Before = std::filesystem::last_write_time(P);
        auto Files = FindZoneFiles(Dir.string(), {});
        CHECK(Files.size() == 1);
        CHECK(FindZoneFiles(Dir.string(), { "other" }).empty());
        int Code = -1; auto J = ListZones(Dir.string(), Files, Code);
        CHECK(Code == 0 && J.find("\"xanim\": 2") != std::string::npos && J.find("\"assets\": 5") != std::string::npos && J.find("\"zone\": \"unit_zone\"") != std::string::npos);
        ListZones(Dir.string(), {}, Code); CHECK(Code == 3);
        { std::ofstream(Dir / "broken.ff", std::ios::binary) << "garbage"; }
        ListZones(Dir.string(), FindZoneFiles(Dir.string(), {}), Code); CHECK(Code == 4);
        CHECK(std::filesystem::last_write_time(P) == Before);
        std::filesystem::remove_all(Dir);
    }

    // ---- retail zone (optional)
    if (const char* ZD = std::getenv("GREYHOUND_BO3_ZONE_DIR"))
    {
        std::vector<uint8_t> D; std::string E; FastFile FF; ZoneHead H;
        bool Ok = ReadFile(std::string(ZD) + "\\core_common.ff", D, E) && ParseFastFile(D, FF, E) && ParseZoneHead(FF.Zone, H, E);
        CHECK(Ok);
        if (Ok)
        {
            CHECK(FF.ZoneName == "core_common" && FF.BlockCount == 70 && FF.Zone.size() == 16086560);
            CHECK(H.Strings.size() == 4835 && H.Assets.size() == 39087 && H.TypeCounts[3] == 7355 && H.TypeCounts[4] == 2937 && H.TypeCounts[9] == 11827);
            CHECK(H.Strings[0].empty() && H.Strings[1] == "tag_riot_shield" && H.AssetListOffset == 0x23C7D);
        }
        else std::printf("retail core_common: %s\n", E.c_str());
    }
    else Skipped++;

    if (Fails) std::printf("%d FAILED\n", Fails);
    else std::printf("all passed%s\n", Skipped ? " (retail zone check skipped: set GREYHOUND_BO3_ZONE_DIR)" : " (incl. retail core_common.ff)");
    return Fails ? 1 : 0;
}
