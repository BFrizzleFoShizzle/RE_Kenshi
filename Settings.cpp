#include "Settings.h"

#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/error/en.h>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/thread/lock_guard.hpp>
#include <fstream>

#include "kenshi/Kenshi.h"
#include "kenshi/GameWorld.h"
#include "Kenshi/ModInfo.h"

#include "Debug.h"

rapidjson::Document settingsDOM;

// would be nice if we could bypass this if settings haven't been changed...
boost::recursive_mutex settingsLock;

std::vector<float> GetDefaultGameSpeeds()
{
    std::vector<float> gameSpeeds;
    gameSpeeds.push_back(1.0f);
    gameSpeeds.push_back(2.0f);
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
    // the first call to HeightmapHook::GetRecommendedHeightmapMode() is expensive, 
    // and our function gets called at a time you really don't want to stall at, so
    // we hope the installer already set this to something sensible, else use AUTO
    // and detect optimal settings later
    defaultSettings.AddMember("HeightmapMode", HeightmapHook::AUTO, defaultSettings.GetAllocator());
    defaultSettings.AddMember("PreloadHeightmap", false, defaultSettings.GetAllocator());
    defaultSettings.AddMember("AttackSlots", -1, defaultSettings.GetAllocator());
    defaultSettings.AddMember("MaxFactionSize", -1, defaultSettings.GetAllocator());
    defaultSettings.AddMember("MaxSquadSize", -1, defaultSettings.GetAllocator());
    defaultSettings.AddMember("MaxSquads", -1, defaultSettings.GetAllocator());
    defaultSettings.AddMember("FixRNG", true, defaultSettings.GetAllocator());
    // TODO remove after dropping support for old versions
    defaultSettings.AddMember("FixFontSize", true, defaultSettings.GetAllocator());
    defaultSettings.AddMember("CacheShaders", true, defaultSettings.GetAllocator());
    defaultSettings.AddMember("LogFileIO", false, defaultSettings.GetAllocator());
    defaultSettings.AddMember("CheckUpdates", true, defaultSettings.GetAllocator());
    defaultSettings.AddMember("SkippedVersion", "", defaultSettings.GetAllocator());
    defaultSettings.AddMember("OpenSettingOnStart", true, defaultSettings.GetAllocator());
    defaultSettings.AddMember("LogAudio", false, defaultSettings.GetAllocator());
    defaultSettings.AddMember("ProfileLoads", false, defaultSettings.GetAllocator());
    defaultSettings.AddMember("EnableEmergencySaves", true, defaultSettings.GetAllocator());
    defaultSettings.AddMember("IncreaseMaxCameraDistance", false, defaultSettings.GetAllocator());
    rapidjson::Value gameSpeeds(rapidjson::kArrayType);
    std::vector<float> defaultGameSpeeds = GetDefaultGameSpeeds();
    for(int i=0;i<defaultGameSpeeds.size(); ++i)
        gameSpeeds.PushBack(rapidjson::Value(defaultGameSpeeds[i]), defaultSettings.GetAllocator());
    defaultSettings.AddMember("UseCustomGameSpeeds", false, defaultSettings.GetAllocator());
    defaultSettings.AddMember("GameSpeeds", gameSpeeds, defaultSettings.GetAllocator());
    return defaultSettings;
}

static bool IsSameType(rapidjson::Type t1, rapidjson::Type t2)
{
    // bools are the same type
    if (t1 == rapidjson::Type::kFalseType || t1 == rapidjson::Type::kTrueType)
        return t2 == rapidjson::Type::kFalseType || t2 == rapidjson::Type::kTrueType;

    // other
    return t1 == t2;
}

void AddDefaultSettings(rapidjson::Document &document)
{
    rapidjson::Document defaultSettings = GenerateDefaultSettings();

    // add missing properties
    for (rapidjson::Document::MemberIterator srcIt = defaultSettings.MemberBegin(); srcIt != defaultSettings.MemberEnd(); ++srcIt)
    {
        rapidjson::Type defaultType = srcIt->value.GetType();
        if (!document.HasMember(srcIt->name))
        {
            DebugLog("Adding default setting...");
            DebugLog(srcIt->name.GetString());
            rapidjson::Value &val = document.AddMember(srcIt->name, srcIt->value, document.GetAllocator());
        }
        if (!document.HasMember(srcIt->name))
        {
            ErrorLog("Error adding settings var \"" + std::string(srcIt->name.GetString()) + "\"");
        }
        else if (!IsSameType(defaultType, document[srcIt->name].GetType()))
        {
            ErrorLog("Incorrect type for settings var \"" + std::string(srcIt->name.GetString()) + "\"");
            std::stringstream str;
            str << defaultType << " " << document[srcIt->name].GetType();
            DebugLog(str.str());
            document[srcIt->name] = defaultSettings[srcIt->name];
        }
    }
}

void SaveSettings()
{
    DebugLog("Saving settings...");
    std::ofstream outStream("RE_Kenshi.ini");
    rapidjson::OStreamWrapper osw(outStream);
    rapidjson::PrettyWriter<rapidjson::OStreamWrapper> writer(osw);
    boost::lock_guard<boost::recursive_mutex> lock(settingsLock);
    settingsDOM.Accept(writer);
}

void Settings::Init()
{
    boost::lock_guard<boost::recursive_mutex> lock(settingsLock);

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
        if (settingsDOM.HasParseError())
        {
            ErrorLog("RE_Kenshi.ini has parsing error, replacing with default settings");
            settingsDOM = GenerateDefaultSettings();
        }
        else if (!settingsDOM.IsObject())
        {
            ErrorLog("Parsed settings do not form a JSON object, replacing with default settings");
            settingsDOM = GenerateDefaultSettings();
        }
        else
        {
            // Add missing settings
            AddDefaultSettings(settingsDOM);
        }
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

static bool modOverridesLoaded = false;

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
    modOverridesLoaded = true;
}

bool Settings::GetModOverridesLoaded()
{
    return modOverridesLoaded;
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

// internal functions
bool GetSettingBool(rapidjson::GenericStringRef<char> name)
{
    const char* namep = (const char*)name;

    boost::lock_guard<boost::recursive_mutex> lock(settingsLock);

    if (!settingsDOM.HasMember(name))
    {
        ErrorLog("Missing setting (bool get): " + std::string(name));
        // this will be set to default value at function end
        settingsDOM.AddMember(name, false, settingsDOM.GetAllocator());
    }
    else
    {
        rapidjson::Value& val = settingsDOM[namep];
        if (!val.IsBool())
            ErrorLog("Incorrect setting type (bool get): " + std::string(name));
        else
            // value exists and is of correct type
            return val.GetBool();
    }

    ErrorLog("Overwriting with default value (bool get): " + std::string(name));
    rapidjson::Document defaultSettings = GenerateDefaultSettings();
    settingsDOM[namep] = defaultSettings[namep].GetBool();

    return defaultSettings[namep].GetBool();
}

void SetSettingBool(rapidjson::GenericStringRef<char> name, bool value)
{
    boost::lock_guard<boost::recursive_mutex> lock(settingsLock);

    if (!settingsDOM.HasMember(name))
    {
        ErrorLog("Missing setting (bool set): " + std::string(name));
        settingsDOM.AddMember(name, value, settingsDOM.GetAllocator());
    }
    else
    {
        rapidjson::Value& val = settingsDOM[(const char*)name];
        if (!val.IsBool())
            ErrorLog("Incorrect setting type (bool set): " + std::string(name));
        val.SetBool(value);
    }

    SaveSettings();
}

int GetSettingInt(rapidjson::GenericStringRef<char> name)
{
    const char* namep = (const char*)name;

    boost::lock_guard<boost::recursive_mutex> lock(settingsLock);

    if (!settingsDOM.HasMember(name))
    {
        ErrorLog("Missing setting (int get): " + std::string(name));
        // this will be set to default value at function end
        settingsDOM.AddMember(name, -1, settingsDOM.GetAllocator());
    }
    else
    {
        rapidjson::Value& val = settingsDOM[namep];
        if (!val.IsInt())
            ErrorLog("Incorrect setting type (int get): " + std::string(name));
        else
        {
            // value exists and is of correct type
            return val.GetInt();
        }
    }

    ErrorLog("Overwriting with default value (int get): " + std::string(name));
    rapidjson::Document defaultSettings = GenerateDefaultSettings();
    settingsDOM[namep] = defaultSettings[namep].GetInt();

    return defaultSettings[namep].GetInt();
}

void SetSettingInt(rapidjson::GenericStringRef<char> name, int value)
{
    boost::lock_guard<boost::recursive_mutex> lock(settingsLock);

    if (!settingsDOM.HasMember(name))
    {
        ErrorLog("Missing setting (int set): " + std::string(name));
        settingsDOM.AddMember(name, value, settingsDOM.GetAllocator());
    }
    else
    {
        rapidjson::Value& val = settingsDOM[(const char*)name];
        if (!val.IsInt())
            ErrorLog("Incorrect setting type (int set): " + std::string(name));
        val.SetInt(value);
    }

    SaveSettings();
}

// read/write settings
void Settings::SetHeightmapMode(HeightmapHook::HeightmapMode value)
{
    SetSettingInt("HeightmapMode", value);
}

HeightmapHook::HeightmapMode Settings::GetHeightmapMode()
{
    return (HeightmapHook::HeightmapMode)GetSettingInt("HeightmapMode");
}

bool Settings::PreloadHeightmap()
{
    return GetSettingBool("PreloadHeightmap");
}

void Settings::SetProfileLoads(bool value)
{
    SetSettingBool("ProfileLoads", value);
}

bool Settings::GetProfileLoads()
{
    return GetSettingBool("ProfileLoads");
}

bool Settings::GetFixRNG()
{
    return GetSettingBool("FixRNG");
}
 
void Settings::SetFixRNG(bool value)
{
    SetSettingBool("FixRNG", value);
}

// TODO remove after dropping support for old versions
bool Settings::GetFixFontSize()
{
    return GetSettingBool("FixFontSize");
}

void Settings::SetFixFontSize(bool value)
{
    SetSettingBool("FixFontSize", value);
}

// -1 = use mod value
int Settings::GetAttackSlots()
{
    return GetSettingInt("AttackSlots");
}

void Settings::SetAttackSlots(int num)
{
    SetSettingInt("AttackSlots", num);
}

// -1 = use mod value
int Settings::GetMaxFactionSize()
{
    return GetSettingInt("MaxFactionSize");
}

void Settings::SetMaxFactionSize(int num)
{
    SetSettingInt("MaxFactionSize", num);
}

// -1 = use mod value
int Settings::GetMaxSquadSize()
{
    return GetSettingInt("MaxSquadSize");
}

void Settings::SetMaxSquadSize(int num)
{
    SetSettingInt("MaxSquadSize", num);
}

// -1 = use mod value
int Settings::GetMaxSquads()
{
    return GetSettingInt("MaxSquads");
}

void Settings::SetMaxSquads(int num)
{
    SetSettingInt("MaxSquads", num);
}

bool Settings::GetIncreaseMaxCameraDistance()
{
    return GetSettingBool("IncreaseMaxCameraDistance");
}

void Settings::SetIncreaseMaxCameraDistance(bool value)
{
    SetSettingBool("IncreaseMaxCameraDistance", value);
}

bool Settings::GetOpenSettingsOnStart()
{
    return GetSettingBool("OpenSettingOnStart");
}

void Settings::SetOpenSettingsOnStart(bool value)
{
    SetSettingBool("OpenSettingOnStart", value);
}

bool Settings::GetUseCustomGameSpeeds()
{
    return GetSettingBool("UseCustomGameSpeeds");
}

void Settings::SetUseCustomGameSpeeds(bool value)
{
    SetSettingBool("UseCustomGameSpeeds", value);
}

const std::vector<float> Settings::GetGameSpeeds()
{
    boost::lock_guard<boost::recursive_mutex> lock(settingsLock);

    rapidjson::Value& val = settingsDOM["GameSpeeds"];

    if (!val.IsArray())
        ErrorLog("Game speeds is not an array!");

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

    boost::lock_guard<boost::recursive_mutex> lock(settingsLock);

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
    return GetSettingBool("CacheShaders");
}

void Settings::SetCacheShaders(bool value)
{
    SetSettingBool("CacheShaders", value);
}

void Settings::SetLogFileIO(bool value)
{
    SetSettingBool("LogFileIO", value);
}

bool Settings::GetLogFileIO()
{
    return GetSettingBool("LogFileIO");
}

void Settings::SetLogAudio(bool value)
{
    SetSettingBool("LogAudio", value);
}

bool Settings::GetLogAudio()
{
    return GetSettingBool("LogAudio");
}

bool Settings::GetEnableEmergencySaves()
{
    return GetSettingBool("EnableEmergencySaves");
}

void Settings::SetEnableEmergencySaves(bool value)
{
    SetSettingBool("EnableEmergencySaves", value);
}

void Settings::SetCheckUpdates(bool value)
{
    SetSettingBool("CheckUpdates", value);
}

bool Settings::GetCheckUpdates()
{
    return GetSettingBool("CheckUpdates");
}

std::string Settings::GetSkippedVersion()
{
    boost::lock_guard<boost::recursive_mutex> lock(settingsLock);

    rapidjson::Value& val = settingsDOM["SkippedVersion"];
    if (!val.IsString())
    {
        SetSkippedVersion("");
        return "";
    }
    else
    {
        return val.GetString();
    }
}

void Settings::SetSkippedVersion(std::string version)
{
    boost::lock_guard<boost::recursive_mutex> lock(settingsLock);

    rapidjson::Value value;
    value.SetString(version.c_str(), settingsDOM.GetAllocator());
    if (!settingsDOM.HasMember("SkippedVersion"))
        settingsDOM.AddMember("SkippedVersion", value, settingsDOM.GetAllocator());
    else
        settingsDOM["SkippedVersion"] = value;

    SaveSettings();
}