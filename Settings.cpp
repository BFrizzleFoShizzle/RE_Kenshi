#include "Settings.h"

#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/error/en.h>
#include <fstream>

#include "WinHttpClient.h"

#include "kenshi/Kenshi.h"
#include "kenshi/GameWorld.h"
#include "Kenshi/ModInfo.h"

#include "Debug.h"

rapidjson::Document settingsDOM;

std::string version = "0.2.2";
const bool isPrerelease = true;

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
    defaultSettings.AddMember("LogFileIO", false, defaultSettings.GetAllocator());
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

    if (!IsCurrentVersion())
    {
        DebugLog("Opening browser...");
        system("start https://www.nexusmods.com/kenshi/mods/847?tab=files");
    }
}

std::unordered_map<std::string, std::string> fileOverrides;

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
    }
}

const std::unordered_map<std::string, std::string>* Settings::GetFileOverrides()
{
    return &fileOverrides;
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

std::string Settings::GetCurrentVersion()
{
    return version;
}

std::string Settings::GetDisplayVersion()
{
    return GetCurrentVersion() + (isPrerelease ? " prerelease" : "");
}

std::vector<int> ExtractVersion(std::string version)
{
    std::vector<int> versionNumbers;

    if (version[0] == 'v')
        version = version.substr(1);

    std::stringstream versionStream(version);
    while (versionStream.good())
    {
        int versionNum = -1;
        versionStream >> versionNum;

        if (versionStream.peek() == '.')
            versionStream.ignore();

        versionNumbers.push_back(versionNum);
    }

    return versionNumbers;
}

// is versionStr1 newer than versionStr2
bool IsVersionNewer(std::string versionStr1, std::string versionStr2)
{
    std::vector<int> version1 = ExtractVersion(versionStr1);
    std::vector<int> version2 = ExtractVersion(versionStr2);

    int count = min(version1.size(), version2.size());

    for (int i = 0; i < count; ++i)
    {
        if (version1[i] > version2[i])
            return true;
        if (version1[i] < version2[i])
            return false;
    }

    // versions are equal
    return false;
}

// get version number of latest release on GitHub
bool Settings::IsCurrentVersion()
{
    // get info of latest release on GitHub
    WinHttpClient client(L"https://api.github.com/repos/BFrizzleFoShizzle/RE_Kenshi/releases/latest");
    client.SendHttpRequest();
    // convert to regular string
    std::wstring responseWstr = client.GetHttpResponse();
    std::string responseStr(responseWstr.begin(), responseWstr.end());
    // parse json
    rapidjson::Document document;
    document.Parse(responseStr.c_str());
    // "tag_name" is version of release
    // "vx.x.x"
    std::string remoteVersion = document["tag_name"].GetString();
    DebugLog("Latest public release: " + remoteVersion);

    // parse + compare version number strings
    bool isRemoteNewer = IsVersionNewer(remoteVersion, GetCurrentVersion());
    if (!isRemoteNewer)
        DebugLog("We are up to date.");
    else
        DebugLog("Update is available from " + GetCurrentVersion() + " to " + remoteVersion);
    return !isRemoteNewer;
}