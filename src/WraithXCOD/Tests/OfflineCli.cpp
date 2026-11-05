// Standalone offline BO3 zone lister: no game, no MFC. Build: Tests\build_offline_cli.bat
//   greyhound_offline.exe --zone-dir "<BO3>\zone" [--zones core_common,zm_zod] [--report out.json]
// Same logic and JSON as `Greyhound.exe --offline --list-only`. Read only: nothing is written into the zone dir.
#include <cstdio>
#include <fstream>
#include "../WraithXCOD/BO3FastFile.h"

int main(int argc, char** argv)
{
    std::string Dir, Report;
    std::vector<std::string> Names;
    for (int i = 1; i < argc; i++)
    {
        std::string A = argv[i];
        auto Next = [&](std::string& O) { if (i + 1 >= argc) return false; O = argv[++i]; return true; };
        std::string V;
        if (A == "--zone-dir") { if (!Next(Dir)) return 2; }
        else if (A == "--report") { if (!Next(Report)) return 2; }
        else if (A == "--zones") { if (!Next(V)) return 2; std::string C; for (char ch : V + ",") { if (ch == ',') { if (!C.empty()) Names.push_back(C); C.clear(); } else C += ch; } }
        else { std::fprintf(stderr, "unknown argument %s\n", A.c_str()); return 2; }
    }
    if (Dir.empty()) { std::fprintf(stderr, "usage: greyhound_offline --zone-dir DIR [--zones a,b] [--report FILE]\n"); return 2; }
    int Code = 0;
    std::string J = BO3FF::ListZones(Dir, BO3FF::FindZoneFiles(Dir, Names), Code);
    if (!Report.empty()) { std::ofstream(Report, std::ios::binary | std::ios::trunc) << J; }
    std::fputs(J.c_str(), stdout);
    return Code;
}
