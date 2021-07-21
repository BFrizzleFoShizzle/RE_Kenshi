#include "Settings.h"

#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/prettywriter.h>
#include <fstream>

#include "Debug.h"

rapidjson::Document settingsDOM;

rapidjson::Document GenerateDefaultSettings()
{
    rapidjson::Document defaultSettings;
    // TODO
    defaultSettings.SetObject();
    defaultSettings.AddMember("UseCompressedHeightmap", true, defaultSettings.GetAllocator());
    defaultSettings.AddMember("PreloadHeightmap", false, defaultSettings.GetAllocator());
    return defaultSettings;
}

void SaveSettings()
{
    std::ofstream outStream("RE_Kenshi.ini");
    rapidjson::OStreamWrapper osw(outStream);
    rapidjson::PrettyWriter<rapidjson::OStreamWrapper> writer(osw);
    settingsDOM.Accept(writer);
}

void Settings::Init()
{
    std::ifstream settingsFile("RE_Kenshi.ini");
    // load default settings (to fill in blanks)
    settingsDOM = GenerateDefaultSettings();
    if (!settingsFile.is_open())
    {
        DebugLog("Settings file not found. Creating...");
        // file doesn't exist - generate it
        SaveSettings();
    }
    // override settings with values from filesystem
    if (settingsFile.is_open())
        DebugLog("Loading settings file...");
    {
        rapidjson::IStreamWrapper isw(settingsFile);
        settingsDOM.ParseStream(isw);
    }
    // TODO REMOVE AFTER TESTING
    SaveSettings();
}

bool Settings::UseHeightmapcompression()
{
    rapidjson::Value& val = settingsDOM["UseCompressedHeightmap"];
    return val.GetBool();
}

bool Settings::PreloadHeightmap()
{
    rapidjson::Value& val = settingsDOM["PreloadHeightmap"];
    return val.GetBool();
}