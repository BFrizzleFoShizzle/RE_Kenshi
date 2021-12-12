#include "Version.h"

#include <vector>
#include <sstream>
#include <rapidjson/document.h>

#include "WinHttpClient.h"
#include "Debug.h"
#include "Settings.h"

std::string version = "0.2.2";
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
    return false;
}

void Version::Init()
{
    if (Settings::GetCheckUpdates())
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
        latestVersionCache = document["tag_name"].GetString();
        DebugLog("Latest public release: " + latestVersionCache);

        // parse + compare version number strings
        bool isRemoteNewer = IsVersionNewer(latestVersionCache, GetCurrentVersion());
        if (!isRemoteNewer)
            DebugLog("We are up to date.");
        else
            DebugLog("Update is available from " + GetCurrentVersion() + " to " + latestVersionCache);

        // Ask if user wants to update if needed
        if (!IsCurrentVersion())
        {
            int result = MessageBoxA(NULL, "A new version of  RE_Kenshi has been released.\nDo you want to update?", "RE_Kenshi update", MB_YESNO | MB_TOPMOST);
            if (result == IDYES)
            {
                DebugLog("Opening browser...");
                system("start https://www.nexusmods.com/kenshi/mods/847?tab=files");
            }
        }
    }
}

// get version number of latest release on GitHub
bool Version::IsCurrentVersion()
{
    return !IsVersionNewer(latestVersionCache, GetCurrentVersion());
}