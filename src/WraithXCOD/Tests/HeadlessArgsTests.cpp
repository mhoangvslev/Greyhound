// Standalone tests for HeadlessArgs.h. Build + run: Tests\run_tests.bat
#include <cstdio>
#include "../WraithXCOD/HeadlessArgs.h"

static int Fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); Fails++; } } while (0)
using namespace HeadlessArgs;

int main()
{
    CHECK(GlobMatch("*slide*", "pb_stand_slide_f"));
    CHECK(GlobMatch("PB_*_SLIDE_?", "pb_stand_slide_f"));
    CHECK(!GlobMatch("pb_*_slide", "pb_stand_slide_f"));
    CHECK(GlobMatch("*", ""));
    CHECK(GlobMatch("a*b*c", "aXXbYYc"));
    CHECK(!GlobMatch("a*b*c", "aXXbYY"));
    CHECK(FilterMatches("slide", "mp_Slide_Start"));       // no wildcard = substring
    CHECK(!FilterMatches("slide_", "mp_slide"));
    CHECK(MatchesAny({}, "anything"));
    CHECK(MatchesAny({ "nope", "slide" }, "x_slide_y"));
    CHECK(!MatchesAny({ "nope" }, "x_slide_y"));
    CHECK(SplitList("a,b;;c").size() == 3);

    HeadlessOptions O; std::string E;
    CHECK(Parse({ "greyhound.exe" }, O, E) && !O.Enabled);                     // GUI untouched
    CHECK(Parse({ "greyhound.exe", "verifiedhashes" }, O, E) && !O.Enabled);
    CHECK(Parse({ "g", "--headless", "--type", "anim", "--filter", "slide,*_sl_*", "--out", "D:\\o" }, O, E));
    CHECK(O.Enabled && O.Filters.size() == 2 && O.OutDir == "D:\\o" && O.Formats.size() == 1 && O.Formats[0] == "seanim");
    CHECK(O.ReportPath == "D:\\o\\export_report.json");
    CHECK(Parse({ "g", "--export", "--out", "o", "--formats", "SEAnim,cast", "--wait", "120", "--overwrite" }, O, E));
    CHECK(O.Formats.size() == 2 && O.Formats[1] == "cast" && O.WaitSeconds == 120 && O.Overwrite);
    CHECK(!Parse({ "g", "--headless" }, O, E) && E.find("--out") != std::string::npos);
    CHECK(Parse({ "g", "--headless", "--list-only" }, O, E) && O.ListOnly);   // --out optional when listing
    CHECK(!Parse({ "g", "--headless", "--out", "o", "--type", "bogus" }, O, E));
    CHECK(!Parse({ "g", "--headless", "--out", "o", "--formats", "fbx" }, O, E));
    CHECK(!Parse({ "g", "--headless", "--out", "o", "--wait", "-3" }, O, E));
    CHECK(!Parse({ "g", "--headless", "--out" }, O, E));

    CHECK(Parse({ "g", "--headless", "--offline", "--zone-dir", "Z", "--zones", "core_common,zm_zod" }, O, E) && O.Offline && O.ZoneDir == "Z" && O.Zones.size() == 2);
    CHECK(!Parse({ "g", "--headless", "--offline", "--out", "o" }, O, E) && E.find("--zone-dir") != std::string::npos);
    CHECK(Parse({ "g", "--headless", "--out", "o" }, O, E) && !O.Offline);

    CHECK(Parse({ "g", "--headless", "--type", "model", "--out", "o", "--formats", "semodel,cast" }, O, E) && O.Formats.size() == 2);
    CHECK(!Parse({ "g", "--headless", "--type", "model", "--out", "o", "--formats", "seanim" }, O, E) && E.find("applies") != std::string::npos);
    CHECK(!Parse({ "g", "--headless", "--type", "anim", "--out", "o", "--formats", "semodel" }, O, E));
    CHECK(Parse({ "g", "--headless", "--type", "anim", "--out", "o", "--formats", "seanim,semodel" }, O, E));
    CHECK(Parse({ "g", "--headless", "--type", "image", "--out", "o", "--formats", "seanim" }, O, E));

    std::printf(Fails ? "%d FAILED\n" : "all passed\n", Fails);
    return Fails ? 1 : 0;
}
