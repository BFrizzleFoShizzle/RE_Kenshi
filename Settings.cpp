#include "Settings.h"

#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/error/en.h>
#include <fstream>

#include "kenshi/Kenshi.h"
#include "kenshi/GameWorld.h"
#include "Kenshi/ModInfo.h"

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
    defaultSettings.AddMember("AttackSlots", -1, defaultSettings.GetAllocator());
    defaultSettings.AddMember("FixRNG", true, defaultSettings.GetAllocator());
    defaultSettings.AddMember("CacheShaders", true, defaultSettings.GetAllocator());
    defaultSettings.AddMember("LogFileIO", false, defaultSettings.GetAllocator());
    defaultSettings.AddMember("CheckUpdates", true, defaultSettings.GetAllocator());
    defaultSettings.AddMember("LogAudio", false, defaultSettings.GetAllocator());
    defaultSettings.AddMember("IncreaseMaxCameraDistance", false, defaultSettings.GetAllocator());
    rapidjson::Value gameSpeeds(rapidjson::kArrayType);
    std::vector<float> defaultGameSpeeds = GetDefaultGameSpeeds();
    for(int i=0;i<defaultGameSpeeds.size(); ++i)
        gameSpeeds.PushBack(rapidjson::Value(defaultGameSpeeds[i]), defaultSettings.GetAllocator());
    defaultSettings.AddMember("GameSpeeds", gameSpeeds, defaultSettings.GetAllocator());
    return defaultSettings;
}

void AddDefaultSettings(rapidjson::Document &document)
{
    rapidjson::Document defaultSettings = GenerateDefaultSettings();

    // add missing properties
    for (rapidjson::Document::MemberIterator srcIt = defaultSettings.MemberBegin(); srcIt != defaultSettings.MemberEnd(); ++srcIt)
    {
        if (!document.HasMember(srcIt->name))
        {
            document.AddMember(srcIt->name, srcIt->value, document.GetAllocator());
        }
    }
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

        // Add missing settings
        AddDefaultSettings(settingsDOM);
    }

    // TODO REMOVE AFTER TESTING
    SaveSettings();
}

std::unordered_map<std::string, std::string> fileOverrides;
std::vector<std::string> soundbanks;

// crappy system for inserting mod folder root dynamically
std::string ParsePath(std::string path)
{
    size_t namePos = path.find("$(modroot \"");
    if (namePos != std::string::npos)
    {
        size_t nameEnd = path.find("\")");
        if (nameEnd != std::string::npos)
        {
            size_t nameStart = namePos + 11;
            std::string modName = path.substr(nameStart, nameEnd - nameStart);

            // find mod
            Kenshi::lektor<Kenshi::ModInfo*> &loadedMods = Kenshi::GetGameWorld().loadedMods;
            for (int i = 0; i < loadedMods.size(); ++i)
            {
                if (loadedMods[i]->modName == modName)
                {
                    // mod found, insert into path
                    return path.substr(0, namePos) + loadedMods[i]->fileDir + path.substr(nameEnd + 2);
                }
            }
            ErrorLog("Path override modroot not found: \"" + modName + "\"");
        }
    }
    else if (path.find("$") != std::string::npos)
    {
        ErrorLog("Error parsing override path: \"" + path + "\"");
    }

    return path;
}

// Load settings from mods
void Settings::LoadModOverrides()
{
    Kenshi::lektor<Kenshi::ModInfo*>& loadedMods = Kenshi::GetGameWorld().loadedMods;
    for (int i = 0; i < loadedMods.size(); ++i)
    {
        // attempt to load RE_Kenshi settings from mod dir
        std::string settingsPath = loadedMods[i]->fileDir + "\\RE_Kenshi.json";
        std::ifstream settingsFile(settingsPath);

        // skip if settings file doesn't exist
        if (!settingsFile.is_open())
            continue;

        DebugLog("Loading mod overrides from \"" + loadedMods[i]->modName + "\"...");
        rapidjson::IStreamWrapper isw(settingsFile);
        rapidjson::Document modDOM;
        if (modDOM.ParseStream(isw).HasParseError())
            ErrorLog("Error parsing \"" + settingsPath + "\" : " + rapidjson::GetParseError_En(modDOM.GetParseError()));

        // parse output
        // FILE REBINDS
        if (modDOM.HasMember("FileRebinds") && modDOM["FileRebinds"].IsObject())
        {
            const rapidjson::Value& itemn = modDOM["FileRebinds"];
            for (rapidjson::Value::ConstMemberIterator itr = itemn.MemberBegin();
                itr != itemn.MemberEnd(); ++itr)
            {
                if (itr->value.IsString())
                {
                    std::string origPath = itr->name.GetString();
                    std::string newPath = ParsePath(itr->value.GetString());

                    // Note: mods lower (?) in the mod order get precedence
                    fileOverrides[origPath] = newPath;
                    DebugLog("Override: \"" + origPath + "\" -> \"" + newPath + "\"");
                }
            }
        }
        // SOUND BANKS
        if (modDOM.HasMember("SoundBanks") && modDOM["SoundBanks"].IsArray())
        {
            const rapidjson::Value& item = modDOM["SoundBanks"];
            for (rapidjson::Value::ConstValueIterator itr = item.Begin(); itr != item.End(); ++itr)
                soundbanks.push_back(ParsePath(itr->GetString()));
            DebugLog("Queued soundbank: " + soundbanks.back());
        }
    }
}

const std::unordered_map<std::string, std::string>* Settings::GetFileOverrides()
{
    return &fileOverrides;
}

std::vector<std::string>* Settings::GetModSoundBanks()
{
    return &soundbanks;
}

std::string Settings::ResolvePath(std::string path)
{
    std::unordered_map<std::string, std::string>::const_iterator fileOverride = Settings::GetFileOverrides()->find(path);

    // match, return override
    if (fileOverride != Settings::GetFileOverrides()->end())
        return fileOverride->second;

    // no match
    return path;
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

bool Settings::GetFixRNG()
{
    rapidjson::Value& val = settingsDOM["FixRNG"];
    return val.GetBool();
}
 
void Settings::SetFixRNG(bool value)
{
    if (!settingsDOM.HasMember("FixRNG"))
        settingsDOM.AddMember("FixRNG", value, settingsDOM.GetAllocator());
    else
        settingsDOM["FixRNG"] = value;

    SaveSettings();
}

// -1 = use mod value
int Settings::GetAttackSlots()
{
    rapidjson::Value& val = settingsDOM["AttackSlots"];
    if (!val.IsInt())
    {
        SetAttackSlots(-1);
        return -1;
    }
    else
    {
        return val.GetInt();
    }
}

void Settings::SetAttackSlots(int num)
{
    if (!settingsDOM.HasMember("AttackSlots"))
        settingsDOM.AddMember("AttackSlots", num, settingsDOM.GetAllocator());
    else
        settingsDOM["AttackSlots"] = num;

    SaveSettings();
}

bool Settings::GetIncreaseMaxCameraDistance()
{
    rapidjson::Value& val = settingsDOM["IncreaseMaxCameraDistance"];
    if (!val.IsBool())
    {
        SetIncreaseMaxCameraDistance(false);
        return false;
    }
    else
    {
        return val.GetBool();
    }
}

void Settings::SetIncreaseMaxCameraDistance(bool value)
{
    if (!settingsDOM.HasMember("IncreaseMaxCameraDistance"))
        settingsDOM.AddMember("IncreaseMaxCameraDistance", value, settingsDOM.GetAllocator());
    else
        settingsDOM["IncreaseMaxCameraDistance"] = value;

    SaveSettings();
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

bool Settings::GetCacheShaders()
{
    rapidjson::Value& val = settingsDOM["CacheShaders"];
    return val.GetBool();
}

void Settings::SetCacheShaders(bool value)
{
    if (!settingsDOM.HasMember("CacheShaders"))
        settingsDOM.AddMember("CacheShaders", value, settingsDOM.GetAllocator());
    else
        settingsDOM["CacheShaders"] = value;

    SaveSettings();
}

void Settings::SetLogFileIO(bool value)
{
    if (!settingsDOM.HasMember("LogFileIO"))
        settingsDOM.AddMember("LogFileIO", value, settingsDOM.GetAllocator());
    else
        settingsDOM["LogFileIO"] = value;

    SaveSettings();
}

bool Settings::GetLogFileIO()
{
    rapidjson::Value& val = settingsDOM["LogFileIO"];
    return val.GetBool();
}

void Settings::SetLogAudio(bool value)
{
    if (!settingsDOM.HasMember("LogAudio"))
        settingsDOM.AddMember("LogAudio", value, settingsDOM.GetAllocator());
    else
        settingsDOM["LogAudio"] = value;

    SaveSettings();
}

bool Settings::GetLogAudio()
{
    rapidjson::Value& val = settingsDOM["LogAudio"];
    return val.GetBool();
}

void Settings::SetCheckUpdates(bool value)
{
    if (!settingsDOM.HasMember("CheckUpdates"))
        settingsDOM.AddMember("CheckUpdates", value, settingsDOM.GetAllocator());
    else
        settingsDOM["CheckUpdates"] = value;

    SaveSettings();
}

bool Settings::GetCheckUpdates()
{
    rapidjson::Value& val = settingsDOM["CheckUpdates"];
    return val.GetBool();
}
