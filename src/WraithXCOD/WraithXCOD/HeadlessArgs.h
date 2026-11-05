#pragma once

// Headless export: command line parsing and asset name filtering.
// Pure standard C++ (no MFC / WraithX dependencies) so it can be unit tested on its own,
// see ../Tests/HeadlessArgsTests.cpp.

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <iterator>
#include <string>
#include <vector>

// Exit codes of the headless mode (documented in the README)
enum HeadlessExitCode : int
{
    HeadlessOk = 0,             // everything that matched was exported
    HeadlessBadArgs = 2,        // invalid command line
    HeadlessGameNotFound = 3,   // no supported game running (after --wait)
    HeadlessGameInfoFailed = 4, // game found but its asset pools could not be located
    HeadlessNoMatches = 5,      // game loaded, but no asset matched the filters
    HeadlessExportFailed = 6,   // at least one matching asset failed to export
    HeadlessInternalError = 7,
    HeadlessOfflineUnsupported = 8 // --offline asked for something the offline reader cannot do yet (export of asset bodies)
};

struct HeadlessOptions
{
    // --headless / --export was present
    bool Enabled = false;
    // Asset type: anim, model, image, sound, rawfile, material
    std::string AssetType = "anim";
    // Name filters (glob, case-insensitive, '*' and '?'); an asset matches if ANY filter matches
    std::vector<std::string> Filters;
    // Output root (replaces <exe dir>/exported_files)
    std::string OutDir;
    // Where the JSON report is written (default <OutDir>/export_report.json)
    std::string ReportPath;
    // Formats for anims: seanim, cast
    std::vector<std::string> Formats = { "seanim" };
    // Seconds to wait for the game process / for matching assets to appear
    uint32_t WaitSeconds = 0;
    // Only list the matching assets, export nothing
    bool ListOnly = false;
    // Re-export files that already exist
    bool Overwrite = false;
    // --offline: read the game files on disk instead of attaching to a running game (BO3 only so far)
    bool Offline = false;
    // BO3 zone directory (<game>\zone) and optional zone names (file name without .ff) to restrict to
    std::string ZoneDir;
    std::vector<std::string> Zones;
};

namespace HeadlessArgs
{
    inline std::string Lower(std::string S)
    {
        std::transform(S.begin(), S.end(), S.begin(), [](unsigned char C) { return (char)std::tolower(C); });
        return S;
    }

    // Case-insensitive glob: '*' any run (also empty), '?' any single char. Whole-string match.
    inline bool GlobMatch(const std::string& Pattern, const std::string& Text)
    {
        const std::string P = Lower(Pattern), T = Lower(Text);
        size_t p = 0, t = 0, star = std::string::npos, mark = 0;
        while (t < T.size())
        {
            if (p < P.size() && (P[p] == '?' || P[p] == T[t])) { p++; t++; }
            else if (p < P.size() && P[p] == '*') { star = p++; mark = t; }
            else if (star != std::string::npos) { p = star + 1; t = ++mark; }
            else return false;
        }
        while (p < P.size() && P[p] == '*') p++;
        return p == P.size();
    }

    // A pattern without wildcards is treated as a substring (so "slide" == "*slide*")
    inline bool FilterMatches(const std::string& Filter, const std::string& Name)
    {
        if (Filter.find_first_of("*?") == std::string::npos)
            return Lower(Name).find(Lower(Filter)) != std::string::npos;
        return GlobMatch(Filter, Name);
    }

    // No filters = match everything
    inline bool MatchesAny(const std::vector<std::string>& Filters, const std::string& Name)
    {
        if (Filters.empty()) return true;
        for (auto& F : Filters)
            if (FilterMatches(F, Name)) return true;
        return false;
    }

    inline std::vector<std::string> SplitList(const std::string& S)
    {
        std::vector<std::string> Out;
        std::string Cur;
        for (char C : S)
        {
            if (C == ',' || C == ';') { if (!Cur.empty()) Out.push_back(Cur); Cur.clear(); }
            else Cur.push_back(C);
        }
        if (!Cur.empty()) Out.push_back(Cur);
        return Out;
    }

    // Returns false and fills Error on a bad command line. Options.Enabled stays false when
    // neither --headless nor --export is present (the GUI starts as before).
    inline bool Parse(const std::vector<std::string>& Args, HeadlessOptions& O, std::string& Error)
    {
        O = HeadlessOptions();
        auto Need = [&](size_t& i, const char* Name, std::string& Out) -> bool
        {
            if (i + 1 >= Args.size()) { Error = std::string("missing value for ") + Name; return false; }
            Out = Args[++i];
            return true;
        };
        for (size_t i = 0; i < Args.size(); i++)
        {
            const std::string& A = Args[i];
            std::string V;
            if (A == "--headless" || A == "--export") O.Enabled = true;
            else if (A == "--type") { if (!Need(i, "--type", V)) return false; O.AssetType = Lower(V); }
            else if (A == "--filter") { if (!Need(i, "--filter", V)) return false; for (auto& F : SplitList(V)) O.Filters.push_back(F); }
            else if (A == "--out") { if (!Need(i, "--out", V)) return false; O.OutDir = V; }
            else if (A == "--report") { if (!Need(i, "--report", V)) return false; O.ReportPath = V; }
            else if (A == "--formats") { if (!Need(i, "--formats", V)) return false; O.Formats.clear(); for (auto& F : SplitList(V)) O.Formats.push_back(Lower(F)); }
            else if (A == "--wait")
            {
                if (!Need(i, "--wait", V)) return false;
                if (V.empty() || V.find_first_not_of("0123456789") != std::string::npos || V.size() > 6) { Error = "--wait needs whole seconds"; return false; }
                O.WaitSeconds = (uint32_t)std::stoul(V);
            }
            else if (A == "--list-only") O.ListOnly = true;
            else if (A == "--offline") O.Offline = true;
            else if (A == "--zone-dir") { if (!Need(i, "--zone-dir", V)) return false; O.ZoneDir = V; }
            else if (A == "--zones") { if (!Need(i, "--zones", V)) return false; for (auto& F : SplitList(V)) O.Zones.push_back(F); }
            else if (A == "--overwrite") O.Overwrite = true;
        }
        if (!O.Enabled) return true;

        static const char* Types[] = { "anim", "model", "image", "sound", "rawfile", "material" };
        if (std::find(std::begin(Types), std::end(Types), O.AssetType) == std::end(Types)) { Error = "unknown --type " + O.AssetType; return false; }
        if (O.Offline && O.ZoneDir.empty()) { Error = "--offline needs --zone-dir <game dir>/zone"; return false; }
        if (O.OutDir.empty() && !O.ListOnly && !O.Offline) { Error = "--out is required"; return false; }
        for (auto& F : O.Formats)
            if (F != "seanim" && F != "cast") { Error = "unknown format " + F + " (seanim, cast)"; return false; }
        if (O.Formats.empty()) { Error = "--formats is empty"; return false; }
        if (O.ReportPath.empty() && !O.OutDir.empty()) O.ReportPath = O.OutDir + "\\export_report.json";
        return true;
    }
}
