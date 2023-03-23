#include "Bugs.h"

#include "Discord.h"

#include "WinHttpClient.h"
#include "Version.h"
#include "Escort.h"
#include <kenshi/Kenshi.h>
#include <boost/locale/message.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <sstream>
#include <iomanip>

// manual bug reporting
void Bugs::ReportUserBug(std::string description)
{
	WinHttpClient client(modBugDiscordWebHookURL);
	// embeds don't ping properly
	/*
	std::string message = EscapeJson("<@" + discordID + ">\nRE_Kenshi " + Version::GetDisplayVersion() + " / Kenshi " + Kenshi::GetKenshiVersion().ToString());
	std::string embed = EscapeJson(description);
	client.SendHttpRequest(L"POST", L"Content-Type: application/json\r\n", std::string("{\"content\": \"") + message + "\", \"embeds\":[{\"title\":\"Bug Description\", \"description\":\"" + embed + "\"}]}"); 
	*/
	std::string message = WinHttpClient::UrlEncode("<@" + discordID + ">\nRE_Kenshi " + Version::GetDisplayVersion() + " / Kenshi " + Kenshi::GetKenshiVersion().ToString() + " bug:\n" + description);
	client.SendHttpRequest(L"POST", L"Content-Type: application/x-www-form-urlencoded\r\n", "content=" + message);
}

// taken from https://stackoverflow.com/questions/7724448/simple-json-string-escape-for-c
std::string EscapeJson(const std::string& s) {
	std::ostringstream o;
	for (auto c = s.cbegin(); c != s.cend(); c++) {
		if (*c == '"' || *c == '\\' || ('\x00' <= *c && *c <= '\x1f')) {
			o << "\\u"
				<< std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(*c);
		}
		else {
			o << *c;
		}
	}
	return o.str();
}

// report /w crashdump
void Bugs::ReportCrash(std::string description, std::string crashDumpName)
{
	boost::uuids::uuid uuid = boost::uuids::random_generator()();

	std::vector<char> dumpBytes;
	std::ifstream crashDumpFile(crashDumpName, std::ifstream::binary | std::ifstream::ate);
	if (!crashDumpFile.fail())
	{
		size_t size = crashDumpFile.tellg();
		crashDumpFile.seekg(0);
		dumpBytes.resize(size);
		crashDumpFile.read(&dumpBytes[0], size);
	}

	// TODO
	WinHttpClient client(modCrashDiscordWebHookURL);
	std::string message = EscapeJson("<@" + discordID + ">\nRE_Kenshi " + Version::GetDisplayVersion() + " / Kenshi " + Kenshi::GetKenshiVersion().ToString() + " crash:\n" + description);
	std::wstring header = L"Content-Type: multipart/form-data; boundary=\"" + to_wstring(uuid) + L"\"\r\n";
	std::string body = "--" + to_string(uuid) + "\r\n"
		+ "Content-Disposition: form-data; name=\"payload_json\"\r\n"
		+ "Content-Type: application/json\r\n"
		+ "\r\n"
		+ "{\"content\": \"" + message + "\"}\r\n"
		+ "--" + to_string(uuid) + "\r\n";
	if (dumpBytes.size() > 0)
	{
		body += "Content-Disposition: form-data; name=\"file1\"; filename=\"" + crashDumpName + "\"\r\n"
			+ "Content-Type: application/octet-stream\r\n"
			+ "\r\n"
			+ std::string(&dumpBytes[0], dumpBytes.size())
			+ "\r\n"
			+ "--" + to_string(uuid) + "\r\n";
	}
	body += std::string("Content-Disposition: form-data; name=\"file2\"; filename=\"") + "RE_Kenshi_log.txt" + "\"\r\n"
		+ "Content-Type: application/octet-stream\r\n"
		+ "\r\n"
		+ GetDebugLog()
		+ "\r\n"
		+ "--" + to_string(uuid) + "--\r\n";

	client.SendHttpRequest(L"POST", header, body);
}

void* (*CrashReport_orig)(void* arg1, void* arg2);
static void* CrashReport_hook(void* arg1, void* arg2)
{
	ErrorLog("Crash detected");
	WIN32_FIND_DATAA foundCrashDump;
	std::string crashDumpSearchName = "crashDump" + Kenshi::GetKenshiVersion().GetVersion() + "_x64*.zip";
	HANDLE searchHandle = FindFirstFileA(crashDumpSearchName.c_str(), &foundCrashDump);
	std::string errorMessage = boost::locale::gettext("Kenshi has crashed.")
		+ boost::locale::gettext("\nWould you like to send a crash report to the RE_Kenshi team?")
		+ boost::locale::gettext("\nYour report will be sent to RE_Kenshi's developer (BFrizzleFoShizzle) with the following information:")
		+ boost::locale::gettext("\nRE_Kenshi version: ") + Version::GetDisplayVersion()
		+ boost::locale::gettext("\nKenshi version: ") + Kenshi::GetKenshiVersion().ToString()
		+ boost::locale::gettext("\nRE_Kenshi's log") + " (RE_Kenshi_log.txt)";

	if (searchHandle != INVALID_HANDLE_VALUE)
		errorMessage += boost::locale::gettext("\nKenshi's crashdump (") + foundCrashDump.cFileName + ")";
	
	// TODO figure out how to get a description from the user...
	int ID = MessageBoxA(NULL, errorMessage.c_str(), boost::locale::gettext("RE_Kenshi Crash Reporter").c_str(), MB_YESNO | MB_ICONWARNING);
	if (ID == IDYES)
	{
		Bugs::ReportCrash("", searchHandle != INVALID_HANDLE_VALUE ? foundCrashDump.cFileName : "");
	}
	return CrashReport_orig(arg1, arg2);
}

void Bugs::Init()
{
	if (discordID == "PUT_YOUR_ID_HERE")
		MessageBoxA(NULL, "Discord setup error", "Add your info to \"Discord.h\" and recompile to fix  bug reporting", MB_ICONWARNING);

	CrashReport_orig = Escort::JmpReplaceHook<void*(void*, void*)>(Kenshi::GetCrashReporterFunction(), &CrashReport_hook, 10);
}
