#include "Plugins.h"

#include "Escort.h"
#include <kenshi/Kenshi.h>
#include <kenshi/GameWorld.h>
#include <kenshi/ModInfo.h>
#include <kenshi/Globals.h>
#include <core/Functions.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

bool hasPreloaded = false;

typedef PVOID DLL_DIRECTORY_COOKIE, * PDLL_DIRECTORY_COOKIE;

// adds path to DLL load search path
void AddPluginPath(std::string path)
{
    // not included in our headers, have to get via GetProcAddress as per Microsoft's docs
    DLL_DIRECTORY_COOKIE(*AddDllDirectory)(PCWSTR NewDirectory);
    AddDllDirectory = (DLL_DIRECTORY_COOKIE(*)(PCWSTR))GetProcAddress(GetModuleHandleA("kernel32.dll"), "AddDllDirectory");
    AddDllDirectory(std::wstring(path.begin(), path.end()).c_str());
}

void LoadPlugin(std::string path)
{
    HMODULE plugin = LoadLibraryExA(path.c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!plugin)
    {
        ErrorLog("Could not load plugin: " + path);
        return;
    }
    FARPROC start = GetProcAddress(plugin, "?startPlugin@@YAXXZ");
    if (!start)
    {
        ErrorLog("Could not start plugin: " + path);
        return;
    }
    start();
}

void (*initModsList_orig)(GameWorld*);
void preload_init_hook(GameWorld* thisptr)
{
    // set up mod list
	initModsList_orig(thisptr);

    if (!hasPreloaded)
    {
        // Load preload plugins
        hasPreloaded = true;
        DebugLog("Loading preload plugins...");
        lektor<ModInfo*>& mods = ou->availabelModsOrderedList;
        for (int i = 0; i < mods.size(); ++i)
        {
            std::string settingsPath = mods[i]->path + "\\RE_Kenshi.json";
            std::ifstream settingsFile(settingsPath);

            // skip if settings file doesn't exist
            if (!settingsFile.is_open())
                continue;

            rapidjson::IStreamWrapper isw(settingsFile);
            rapidjson::Document modDOM;
            if (modDOM.ParseStream(isw).HasParseError())
                ErrorLog("Error parsing \"" + settingsPath + "\" : " + rapidjson::GetParseError_En(modDOM.GetParseError()));

            // parse output
            // FILE REBINDS
            if (modDOM.HasMember("PreloadPlugins") && modDOM["PreloadPlugins"].IsArray())
            {
                // TODO make force enabled in mod list
                AddPluginPath(mods[i]->path);
                const rapidjson::Value& item = modDOM["PreloadPlugins"];
                for (rapidjson::Value::ConstValueIterator itr = item.Begin(); itr != item.End(); ++itr)
                {
                    DebugLog(mods[i]->name + " -> " + itr->GetString());
                    LoadPlugin(mods[i]->path + "/" + itr->GetString());
                }
            }
        }
    }
}

void Plugins::Postload()
{
    DebugLog("Loading post-load plugins...");
    lektor<ModInfo*>& mods = ou->activeMods;
    for (int i = 0; i < mods.size(); ++i)
    {
        std::string settingsPath = mods[i]->path + "\\RE_Kenshi.json";
        std::ifstream settingsFile(settingsPath);

        // skip if settings file doesn't exist
        if (!settingsFile.is_open())
            continue;

        rapidjson::IStreamWrapper isw(settingsFile);
        rapidjson::Document modDOM;
        if (modDOM.ParseStream(isw).HasParseError())
            ErrorLog("Error parsing \"" + settingsPath + "\" : " + rapidjson::GetParseError_En(modDOM.GetParseError()));

        // parse output
        // FILE REBINDS
        if (modDOM.HasMember("Plugins") && modDOM["Plugins"].IsArray())
        {
            AddPluginPath(mods[i]->path);
            const rapidjson::Value& item = modDOM["Plugins"];
            for (rapidjson::Value::ConstValueIterator itr = item.Begin(); itr != item.End(); ++itr)
            {
                DebugLog(mods[i]->name + " -> " + itr->GetString());
                LoadPlugin(mods[i]->path + "/" + itr->GetString());
            }
        }
    }
}

void Plugins::Init()
{
    KenshiLib::AddHook(KenshiLib::GetRealAddress(&GameWorld::initModsList), preload_init_hook, &initModsList_orig);
}