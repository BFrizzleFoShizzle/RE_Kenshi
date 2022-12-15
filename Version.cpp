#include "Version.h"

#include <vector>
#include <sstream>
#include <rapidjson/document.h>

#include "WinHttpClient.h"
#include "Debug.h"
#include "Settings.h"

std::string version = "0.2.6";
std::string latestVersionCache = "0.0.0";
const bool isPrerelease = true;

std::string Version::GetCurrentVersion()
{
    return version;
}

std::string Version::GetDisplayVersion()
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
    if (isPrerelease)
        return true;

    return false;
}

DWORD WINAPI checkVersionThread(LPVOID param)
{
    DebugLog("Checking latest version...");

    // get info of latest release on GitHub
    WinHttpClient client(L"https://api.github.com/repos/BFrizzleFoShizzle/RE_Kenshi/releases/latest");
    bool success = client.SendHttpRequest();
    if (success)
    {
        // convert to regular string
        std::wstring responseWstr = client.GetHttpResponse();
        std::string responseStr(responseWstr.begin(), responseWstr.end());

        // parse json
        rapidjson::Document document;
        document.Parse(responseStr.c_str());
        // "tag_name" is version of release
        // "vx.x.x"
        latestVersionCache = document["tag_name"].GetString();
        DebugLog("Latest public release: " + latestVersionCache);
        if (Settings::GetSkippedVersion() == latestVersionCache)
        {
            DebugLog("Version " + latestVersionCache + " skipped.");
            return 0;
        }
        // parse + compare version number strings
        bool isRemoteNewer = IsVersionNewer(latestVersionCache, Version::GetCurrentVersion());
        if (!isRemoteNewer)
            DebugLog("We are up to date.");
        else
            DebugLog("Update is available from " + Version::GetCurrentVersion() + " to " + latestVersionCache);

        // Ask if user wants to update if needed
        if (!Version::IsCurrentVersion())
        {
            std::string msg = "A new version of RE_Kenshi (" + latestVersionCache + ") has been released.\nDo you want to update?";
            int result = MessageBoxA(NULL, msg.c_str(), "RE_Kenshi update", MB_YESNO | MB_TOPMOST);
            if (result == IDYES)
            {
                DebugLog("Opening browser...");
                system("start https://www.nexusmods.com/kenshi/mods/847?tab=files");
            }
            if (result == IDNO)
            {
                std::string msg = "Do you want to skip RE_Kenshi release " + latestVersionCache + "?\n(disables notifications for this release)";
                result = MessageBoxA(NULL, msg.c_str(), "RE_Kenshi update", MB_YESNO | MB_TOPMOST);
                if (result == IDYES)
                {
                    Settings::SetSkippedVersion(latestVersionCache);
                    MessageBoxA(NULL, "Version skipped!", "RE_Kenshi update", MB_OK | MB_TOPMOST);
                }
            }
        }
    }
    else
    {
        DebugLog("Could not connect to update server. Update check failed.");
    }
    return 0;
}

void Version::Init()
{
    if (Settings::GetCheckUpdates())
    {
        // moved to another thread as it causes other code to hang
        CreateThread(NULL, 0, checkVersionThread, 0, 0, 0);
    }
}

// get version number of latest release on GitHub
bool Version::IsCurrentVersion()
{
    return !IsVersionNewer(latestVersionCache, GetCurrentVersion());
}