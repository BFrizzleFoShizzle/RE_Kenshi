#include "Settings.h"

#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/prettywriter.h>
#include <fstream>

#include "Debug.h"

rapidjson::Document settingsDOM;

std::vector<float> GetDefaultGameSpeeds()
{
    std::vector<float> gameSpeeds;
    gameSpeeds.push_back(1.0f);
    gameSpeeds.push_back(3.0f);
    gameSpeeds.push_back(5.0f);
    gameSpeeds.push_back(10.0f);
    gameSpeeds.push_back(20.0f);
    return gameSpeeds;
}

rapidjson::Document GenerateDefaultSettings()
{
    rapidjson::Document defaultSettings;
    // TODO
    defaultSettings.SetObject();
    defaultSettings.AddMember("UseCompressedHeightmap", true, defaultSettings.GetAllocator());
    defaultSettings.AddMember("PreloadHeightmap", false, defaultSettings.GetAllocator());
    rapidjson::Value gameSpeeds(rapidjson::kArrayType);
    std::vector<float> defaultGameSpeeds = GetDefaultGameSpeeds();
    for(int i=0;i<defaultGameSpeeds.size(); ++i)
        gameSpeeds.PushBack(rapidjson::Value(defaultGameSpeeds[i]), defaultSettings.GetAllocator());
    defaultSettings.AddMember("GameSpeeds", gameSpeeds, defaultSettings.GetAllocator());
    return defaultSettings;
}

void SaveSettings()
{
    DebugLog("Saving settings...");
    std::ofstream outStream("RE_Kenshi.ini");
    rapidjson::OStreamWrapper osw(outStream);
    rapidjson::PrettyWriter<rapidjson::OStreamWrapper> writer(osw);
    settingsDOM.Accept(writer);
}

void Settings::Init()
{
    // Open settings file
    std::ifstream settingsFile("RE_Kenshi.ini");

    // If file doesn't exist, generate it
    if (!settingsFile.is_open())
    {
        // load default settings (to fill in blanks)
        DebugLog("Generating default settings...");
        settingsDOM = GenerateDefaultSettings();
        DebugLog("Settings file not found. Creating...");
        SaveSettings();
    }

    // override settings with values from filesystem
    if (settingsFile.is_open())
    {
        DebugLog("Loading settings file...");
        rapidjson::IStreamWrapper isw(settingsFile);
        settingsDOM.ParseStream(isw);
    }

    // TODO REMOVE AFTER TESTING
    SaveSettings();
}

bool Settings::UseHeightmapCompression()
{
    rapidjson::Value& val = settingsDOM["UseCompressedHeightmap"];
    return val.GetBool();
}


void Settings::SetUseHeightmapCompression(bool value)
{
    settingsDOM["UseCompressedHeightmap"].SetBool(value);
    SaveSettings();
}

bool Settings::PreloadHeightmap()
{
    rapidjson::Value& val = settingsDOM["PreloadHeightmap"];
    return val.GetBool();
}

const std::vector<float> Settings::GetGameSpeeds()
{
    rapidjson::Value& val = settingsDOM["GameSpeeds"];

    std::vector<float> gameSpeeds;
    if (val.Size() > 0)
    {
        for (rapidjson::Value::ConstValueIterator itr = val.Begin(); itr != val.End(); ++itr)
            gameSpeeds.push_back(itr->GetFloat());
    }
    else
    {
        gameSpeeds = GetDefaultGameSpeeds();
        SetGameSpeeds(gameSpeeds);
    }

    return gameSpeeds;
}

void Settings::SetGameSpeeds(std::vector<float> speeds)
{
    // Don't allow empty array
    if (speeds.size() == 0)
        return;

    rapidjson::Value gameSpeeds(rapidjson::kArrayType);
    for(int i=0;i<speeds.size();++i)
        gameSpeeds.PushBack(rapidjson::Value(speeds[i]), settingsDOM.GetAllocator());

    if (settingsDOM.HasMember("GameSpeeds"))
        settingsDOM["GameSpeeds"] = gameSpeeds;
    else
        settingsDOM.AddMember("GameSpeeds", gameSpeeds, settingsDOM.GetAllocator());

    SaveSettings();
}