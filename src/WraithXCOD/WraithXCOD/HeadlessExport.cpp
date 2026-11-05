#include "stdafx.h"
#include "spdlog/spdlog.h"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <thread>

#include "HeadlessExport.h"
#include "CoDAssets.h"
#include "FileSystems.h"
#include "SettingsManager.h"
#include "WraithX.h"

namespace
{
    // Output goes to the parent console (the exe is a GUI subsystem app) and to stdout if redirected
    void AttachConsoleOutput()
    {
        if (AttachConsole(ATTACH_PARENT_PROCESS))
        {
            FILE* Dummy = nullptr;
            // Only take over the standard handles when they are not already redirected to a file/pipe
            if (GetFileType(GetStdHandle(STD_OUTPUT_HANDLE)) == FILE_TYPE_UNKNOWN)
                freopen_s(&Dummy, "CONOUT$", "w", stdout);
            if (GetFileType(GetStdHandle(STD_ERROR_HANDLE)) == FILE_TYPE_UNKNOWN)
                freopen_s(&Dummy, "CONOUT$", "w", stderr);
        }
    }

    void Say(const std::string& Line)
    {
        std::printf("[headless] %s\n", Line.c_str());
        std::fflush(stdout);
        if (CoDAssets::Log)
            CoDAssets::Log->info("[headless] {0}", Line);
    }

    std::string JsonEscape(const std::string& S)
    {
        std::string Out;
        for (unsigned char C : S)
        {
            switch (C)
            {
            case '"': Out += "\\\""; break;
            case '\\': Out += "\\\\"; break;
            case '\n': Out += "\\n"; break;
            case '\r': Out += "\\r"; break;
            case '\t': Out += "\\t"; break;
            default:
                if (C < 0x20) { char B[8]; std::snprintf(B, sizeof(B), "\\u%04x", C); Out += B; }
                else Out.push_back((char)C);
            }
        }
        return Out;
    }

    WraithAssetType ToAssetType(const std::string& T)
    {
        if (T == "anim") return WraithAssetType::Animation;
        if (T == "model") return WraithAssetType::Model;
        if (T == "image") return WraithAssetType::Image;
        if (T == "sound") return WraithAssetType::Sound;
        if (T == "rawfile") return WraithAssetType::RawFile;
        return WraithAssetType::Material;
    }

    const char* StatusName(WraithAssetStatus S)
    {
        switch (S)
        {
        case WraithAssetStatus::Exported: return "exported";
        case WraithAssetStatus::Error: return "error";
        case WraithAssetStatus::Placeholder: return "placeholder";
        default: return "not_exported";
        }
    }

    // Writes the report; failures to write are logged, never fatal
    void WriteReport(const HeadlessOptions& O, int Code, const char* Game, const std::vector<CoDAsset_t*>& Matches, size_t Loaded)
    {
        if (O.ReportPath.empty()) return;
        FileSystems::CreateDirectory(FileSystems::GetDirectoryName(O.ReportPath));
        std::ofstream F(O.ReportPath, std::ios::binary | std::ios::trunc);
        if (!F) { Say("could not write report " + O.ReportPath); return; }
        size_t Ok = 0, Bad = 0;
        for (auto* A : Matches) { if (A->AssetStatus == WraithAssetStatus::Exported) Ok++; else if (A->AssetStatus == WraithAssetStatus::Error) Bad++; }
        F << "{\n  \"exit_code\": " << Code << ",\n  \"game\": \"" << Game << "\",\n  \"type\": \"" << O.AssetType
          << "\",\n  \"out_dir\": \"" << JsonEscape(O.OutDir) << "\",\n  \"assets_loaded\": " << Loaded
          << ",\n  \"matched\": " << Matches.size() << ",\n  \"exported\": " << Ok << ",\n  \"failed\": " << Bad
          << ",\n  \"list_only\": " << (O.ListOnly ? "true" : "false") << ",\n  \"assets\": [";
        for (size_t i = 0; i < Matches.size(); i++)
            F << (i ? ",\n    " : "\n    ") << "{\"name\": \"" << JsonEscape(Matches[i]->AssetName) << "\", \"status\": \"" << StatusName(Matches[i]->AssetStatus) << "\"}";
        F << "\n  ]\n}\n";
    }

    const char* GameSlug()
    {
        switch (CoDAssets::GameID)
        {
        case SupportedGames::WorldAtWar: return "world_at_war";
        case SupportedGames::BlackOps: return "black_ops_1";
        case SupportedGames::BlackOps2: return "black_ops_2";
        case SupportedGames::BlackOps3: return "black_ops_3";
        case SupportedGames::BlackOps4: return "black_ops_4";
        case SupportedGames::BlackOpsCW: return "black_ops_cw";
        default: return "other";
        }
    }
}

int HeadlessExport::Run(const HeadlessOptions& O)
{
    AttachConsoleOutput();

    if (!WraithX::InitializeAPI(true))
    {
        std::fprintf(stderr, "[headless] WraithX API initialization failed\n");
        return HeadlessInternalError;
    }

    // Formats: only what was asked for, never the GUI's remembered settings
    auto Has = [&](const char* F) { return std::find(O.Formats.begin(), O.Formats.end(), F) != O.Formats.end(); };
    SettingsManager::SetSetting("export_seanim", Has("seanim") ? "true" : "false");
    SettingsManager::SetSetting("export_castanim", Has("cast") ? "true" : "false");
    SettingsManager::SetSetting("export_directxanim", "false");
    SettingsManager::SetSetting("skipprevanim", O.Overwrite ? "false" : "true");
    SettingsManager::SetSetting("skipprevmodel", O.Overwrite ? "false" : "true");
    SettingsManager::SetSetting("skipprevimg", O.Overwrite ? "false" : "true");
    SettingsManager::SetSetting("skipprevsound", O.Overwrite ? "false" : "true");
    CoDAssets::ExportRootOverride = O.OutDir;

    const auto Deadline = std::chrono::steady_clock::now() + std::chrono::seconds(O.WaitSeconds);
    const auto Type = ToAssetType(O.AssetType);

    int Code = HeadlessOk;
    std::vector<CoDAsset_t*> Matches;
    size_t Loaded = 0;
    bool Done = false;

    // The game may still be starting (process missing, pools empty): retry until the deadline
    while (!Done)
    {
        Matches.clear();
        auto Found = CoDAssets::BeginGameMode();
        if (Found == FindGameResult::Success && CoDAssets::GameAssets != nullptr)
        {
            Loaded = CoDAssets::GameAssets->LoadedAssets.size();
            for (auto* Asset : CoDAssets::GameAssets->LoadedAssets)
                if (Asset->AssetType == Type && HeadlessArgs::MatchesAny(O.Filters, Asset->AssetName))
                    Matches.push_back(Asset);
            Say("game " + std::string(GameSlug()) + ": " + std::to_string(Loaded) + " assets loaded, " + std::to_string(Matches.size()) + " match");
            Code = Matches.empty() ? HeadlessNoMatches : HeadlessOk;
        }
        else
        {
            Code = (Found == FindGameResult::NoGamesRunning) ? HeadlessGameNotFound : HeadlessGameInfoFailed;
            Say(Found == FindGameResult::NoGamesRunning ? "no supported game running" : "game found, asset pools not located");
        }

        Done = (Code == HeadlessOk) || std::chrono::steady_clock::now() >= Deadline;
        if (!Done)
        {
            CoDAssets::CleanUpGame();
            std::this_thread::sleep_for(std::chrono::seconds(3));
        }
    }

    if (Code == HeadlessOk && !O.ListOnly)
    {
        FileSystems::CreateDirectory(O.OutDir);
        uint32_t Index = 0;
        auto Queue = std::make_unique<std::vector<CoDAsset_t*>>();
        for (auto* A : Matches) { A->AssetLoadedIndex = Index++; Queue->push_back(A); }
        CoDAssets::AssetsToExportCount = (uint32_t)Queue->size();
        CoDAssets::ExportedAssetsCount = 0;
        CoDAssets::CanExportContinue = true;
        Say("exporting " + std::to_string(Queue->size()) + " assets to " + O.OutDir);
        CoDAssets::ExportSelectedAssets(nullptr, Queue);

        for (auto* A : Matches)
            if (A->AssetStatus == WraithAssetStatus::Error) { Code = HeadlessExportFailed; Say("FAILED: " + A->AssetName); }
    }
    else if (Code == HeadlessOk)
    {
        for (auto* A : Matches) Say("match: " + A->AssetName);
    }

    WriteReport(O, Code, GameSlug(), Matches, Loaded);
    Say("exit code " + std::to_string(Code));

    CoDAssets::CleanUpGame();
    WraithX::ShutdownAPI(true);
    return Code;
}
