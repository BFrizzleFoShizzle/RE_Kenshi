#include "Bugs.h"

#include "Discord.h"
#include <Debug.h>

#include "WinHttpClient.h"
#include "Version.h"
#include "Escort.h"
#include "Settings.h"
#include <kenshi/Kenshi.h>
#include <kenshi/Globals.h>
#include <core/Functions.h>
#include <boost/locale/message.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/filesystem.hpp>
#include <boost/atomic/atomic.hpp>
#include <sstream>
#include <iomanip>
#include <core/md5.h>
#include <windowsx.h>
#include <dbghelp.h>

#include <kenshi/SaveManager.h>
#include <kenshi/SaveFileSystem.h>
#include <ogre/OgreConfigFile.h>

#include "miniz.h"

static std::string GetUUID()
{
	HKEY hKey;
	if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Cryptography", 0, KEY_READ | KEY_WOW64_64KEY, &hKey) == ERROR_SUCCESS) 
	{
		// should be 37 characters + null
		char buffer[40];
		DWORD dwType, dwSize = sizeof(buffer);

		if (RegQueryValueExA(hKey, "MachineGuid", NULL, &dwType, reinterpret_cast<LPBYTE>(buffer), &dwSize) == ERROR_SUCCESS) 
		{
			if (dwType == REG_SZ)
			{
				RegCloseKey(hKey);
				return std::string(buffer, dwSize);
			}
			else
			{
				ErrorLog("UUID key has wrong type");
			}
		}
		else
		{
			ErrorLog("Failed to query UUID key ");
		}

		RegCloseKey(hKey);
	}
	else
	{
		ErrorLog("Failed to open UUID key");
	}
	return "";
}

std::string Bugs::GetUUIDHash()
{
	std::string uuid = GetUUID();

	if (uuid == "")
		return "No UUID";

	// calculate hash
	md5::md5_t hasher(uuid.c_str(), uuid.size());
	char hashStr[33];
	hasher.get_string(hashStr);

	// we don't need the full 32 chars to identify
	// 32 bits = 1% chance of collision at 10k bug reporters, plenty safe
	// 8 chars = 32 bits (1 byte = 2 chars)
	std::string finalHash = std::string(hashStr).substr(0, 8);
	transform(finalHash.begin(), finalHash.end(), finalHash.begin(), toupper);
	return finalHash;
}

// manual bug reporting
bool Bugs::ReportUserBug(std::string description, std::string uuid)
{
	WinHttpClient client(modBugDiscordWebHookURL);

	std::string message = "<@" + discordID + ">\nRE_Kenshi " + Version::GetDisplayVersion() + " / Kenshi " + KenshiLib::GetKenshiVersion().ToString();
	if (uuid != "")
		message += "\nUUID: " + uuid;
	message += " bug:\n" + description;

	std::string encodedMessage = WinHttpClient::UrlEncode(message);
	return client.SendHttpRequest(L"POST", L"Content-Type: application/x-www-form-urlencoded\r\n", "content=" + encodedMessage);
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

char* pZipBuff = nullptr;
size_t zipBuffSize = 0;

// report /w crashdump
bool Bugs::ReportCrash(std::string description, std::string crashDumpName, std::string uuidHash)
{
	if (pZipBuff != nullptr && zipBuffSize > 0)
	{
		// send zip from RAM
		DebugLog("Sending RAM zip");
	}
	else
	{
		DebugLog("Sending disk zip");
		// send zip from disk
		std::ifstream crashDumpFile(crashDumpName, std::ifstream::binary | std::ifstream::ate);
		if (!crashDumpFile.fail())
		{
			zipBuffSize = crashDumpFile.tellg();
			pZipBuff = new char[zipBuffSize];
			crashDumpFile.seekg(0);
			crashDumpFile.read(&pZipBuff[0], zipBuffSize);
		}
	}

	WinHttpClient client(modCrashDiscordWebHookURL);
	std::string message = "\nRE_Kenshi " + Version::GetDisplayVersion() + " / Kenshi " + KenshiLib::GetKenshiVersion().ToString();
	if (uuidHash != "")
		message += "\nUUID: " + uuidHash;
	message += " crash:\n" + description;
	message = EscapeJson(message);

	boost::uuids::uuid uuid = boost::uuids::random_generator()();

	std::wstring header = L"Content-Type: multipart/form-data; boundary=\"" + to_wstring(uuid) + L"\"\r\n";
	std::string body = "--" + to_string(uuid) + "\r\n"
		+ "Content-Disposition: form-data; name=\"payload_json\"\r\n"
		+ "Content-Type: application/json\r\n"
		+ "\r\n"
		+ "{\"content\": \"" + message + "\"}\r\n"
		+ "--" + to_string(uuid) + "\r\n";
	if (zipBuffSize > 0)
	{
		body += "Content-Disposition: form-data; name=\"file1\"; filename=\"" + crashDumpName + "\"\r\n"
			+ "Content-Type: application/octet-stream\r\n"
			+ "\r\n"
			+ std::string(&pZipBuff[0], zipBuffSize)
			+ "\r\n"
			+ "--" + to_string(uuid) + "\r\n";
	}
	try
	{
		std::ifstream settingsFile("RE_Kenshi.ini", std::ifstream::ate);
		size_t size = settingsFile.tellg();
		std::vector<char> settingsBytes;
		settingsBytes.resize(size);
		settingsFile.seekg(0);
		settingsFile.read(&settingsBytes[0], size);
		body += std::string("Content-Disposition: form-data; name=\"file2\"; filename=\"") + "RE_Kenshi.ini" + "\"\r\n"
			+ "Content-Type: application/octet-stream\r\n"
			+ "\r\n"
			+ &settingsBytes[0]
			+ "\r\n"
			+ "--" + to_string(uuid) + "\r\n";
		settingsFile.close();
	}
	catch (std::exception e)
	{
		ErrorLog("Error opening settings file for report");
	}
	body += std::string("Content-Disposition: form-data; name=\"file3\"; filename=\"") + "RE_Kenshi_log.txt" + "\"\r\n"
		+ "Content-Type: application/octet-stream\r\n"
		+ "\r\n"
		+ GetDebugLog()
		+ "\r\n"
		+ "--" + to_string(uuid) + "--\r\n";

	return client.SendHttpRequest(L"POST", header, body);
}

static HWND uuidCheckbox = NULL;
static HWND yesButton = NULL;
static HWND noButton = NULL;
static HWND editbox = NULL;

static std::string crashDumpFileName = "";

enum CRASH_REPORT_STATUS
{
	SKIPPED,
	SUCCESS,
	FAIL
};

static CRASH_REPORT_STATUS crashReportStatus = SKIPPED;

enum BUTTON_ID
{
	UUID_CHECKBOX = 1,
	YES_BTN,
	NO_BTN,
	EDIT_BOX
};

void CheckFocusAllowed(HWND hwnd, HWND widget)
{
	// stop Kenshi's window from getting focus
	DWORD activeWindowProcessID = 0;
	GetWindowThreadProcessId(GetActiveWindow(), &activeWindowProcessID);
	if (GetActiveWindow() != hwnd && activeWindowProcessID == GetCurrentProcessId())
	{
		SetFocus(widget);
		FLASHWINFO flashInfo;
		flashInfo.cbSize = sizeof(FLASHWINFO);
		flashInfo.hwnd = hwnd;
		flashInfo.uCount = 2;
		flashInfo.dwTimeout = 50;
		flashInfo.dwFlags = FLASHW_CAPTION;
		FlashWindowEx(&flashInfo);
		MessageBeep(MB_ICONWARNING);
	}
}

// TODO refactor or something and remove this crap
static std::wstring to_wstr(const std::string s)
{
	return std::wstring(s.begin(), s.end());
}

// Fugly WINAPI code for making the crash report window, I don't want to add 
// a UI lib dependency just to make the *ONE* custom window RE_Kenshi needs :/
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// this has to be initialized after loading translations...
	const std::wstring placeholderText = boost::locale::gettext(L"Describe what caused the crash...");

	switch (msg)
	{
		case WM_CREATE:
		{
			HINSTANCE hInst = ((LPCREATESTRUCT)lParam)->hInstance;

			// disable close button
			EnableMenuItem(GetSystemMenu(hwnd, FALSE), SC_CLOSE,
				MF_BYCOMMAND | MF_DISABLED);

			// warning icon
			SHSTOCKICONINFO stockIconInfo;
			stockIconInfo.cbSize = sizeof(SHSTOCKICONINFO);
			HICON warningIcon = NULL;
			// attempt to get Win10 icon
			if (SHGetStockIconInfo(SIID_WARNING, SHGFI_ICON, &stockIconInfo) == S_OK)
			{
				warningIcon = stockIconInfo.hIcon;
			}
			else
			{
				warningIcon = LoadIcon(nullptr, IDI_WARNING);
			}

			ICONINFO iconInfo;
			BITMAP bitmapInfo;
			GetIconInfo(warningIcon, &iconInfo);
			GetObject(iconInfo.hbmColor, sizeof(BITMAP), &bitmapInfo);
			std::wstring errorMessage = boost::locale::gettext(L"Kenshi has crashed.")
				+ boost::locale::gettext(L"\nWould you like to send a crash report to the RE_Kenshi team?")
				+ boost::locale::gettext(L"\nYour report will be sent to RE_Kenshi's developer (BFrizzleFoShizzle) with the following information:")
				+ boost::locale::gettext(L"\n\nRE_Kenshi version: ") + to_wstr(Version::GetDisplayVersion())
				+ boost::locale::gettext(L"\nKenshi version: ") + to_wstr(KenshiLib::GetKenshiVersion().ToString())
				+ boost::locale::gettext(L"\nUUID hash: ") + to_wstr(Bugs::GetUUIDHash()) + boost::locale::gettext(L" (optional - allows the developer to know all your reports come from the same machine)")
				+ boost::locale::gettext(L"\nRE_Kenshi settings") + L" (RE_Kenshi.ini)"
				+ boost::locale::gettext(L"\nRE_Kenshi's log") + L" (RE_Kenshi_log.txt)";

			WIN32_FIND_DATAA foundCrashDump;
			std::string crashDumpSearchName = "crashDump" + KenshiLib::GetKenshiVersion().GetVersion() + "_x64*.zip";
			HANDLE searchHandle = FindFirstFileA(crashDumpSearchName.c_str(), &foundCrashDump);

			if (searchHandle != INVALID_HANDLE_VALUE)
			{
				crashDumpFileName = foundCrashDump.cFileName;
				errorMessage += boost::locale::gettext(L"\nKenshi's crashdump (") + to_wstr(crashDumpFileName) + L")";
			}

			errorMessage += boost::locale::gettext(L"\nYour bug description")
				+ boost::locale::gettext(L"\n\nPlease describe the bug:");

			HWND icon = CreateWindowA("static", "", WS_CHILD | WS_VISIBLE | SS_ICON,
				20, 20, bitmapInfo.bmWidth, bitmapInfo.bmHeight, hwnd, 0, hInst, NULL);
			HWND label = CreateWindowW(L"static", errorMessage.c_str(), WS_CHILD | WS_VISIBLE | SS_LEFT,
				30 + bitmapInfo.bmWidth, 20, 470 - (30 + bitmapInfo.bmWidth), 240, hwnd, 0, hInst, NULL);
			editbox = CreateWindow(L"EDIT", placeholderText.c_str(),
				WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_BORDER | WS_TABSTOP | ES_LEFT | ES_MULTILINE| ES_WANTRETURN | ES_AUTOVSCROLL,
				20,	250, 455, 150, hwnd, (HMENU)EDIT_BOX, hInst, NULL);
			uuidCheckbox = CreateWindowW(L"button", boost::locale::gettext(L"Include UUID hash").c_str(), WS_VISIBLE | WS_CHILD | WS_TABSTOP | BS_NOTIFY | BS_CHECKBOX,
				20, 410, 150, 20, hwnd, (HMENU)UUID_CHECKBOX, hInst, NULL);
			yesButton = CreateWindowW(L"button", boost::locale::gettext(L"Send").c_str(), WS_VISIBLE | WS_CHILD | WS_TABSTOP | BS_NOTIFY | BS_DEFPUSHBUTTON,
				200, 420, 100, 30, hwnd, (HMENU)YES_BTN, hInst, NULL);
			noButton = CreateWindowW(L"button", boost::locale::gettext(L"Don't send").c_str(), WS_VISIBLE | WS_CHILD | WS_TABSTOP | BS_NOTIFY | BS_PUSHBUTTON,
				320, 420, 100, 30, hwnd, (HMENU)NO_BTN, hInst, NULL);

			// update icon
			if(warningIcon != NULL)
				SendMessage(icon, STM_SETICON, WPARAM(warningIcon), TRUE);

			// get system font
			NONCLIENTMETRICS metrics = { 0 };
			metrics.cbSize = sizeof(NONCLIENTMETRICS);
			SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &metrics, 0);
			HFONT hFont = CreateFontIndirectW(&metrics.lfMessageFont);

			// update fonts
			SendMessage(label, WM_SETFONT, WPARAM(hFont), TRUE);
			SendMessage(uuidCheckbox, WM_SETFONT, WPARAM(hFont), TRUE);
			SendMessage(editbox, WM_SETFONT, WPARAM(hFont), TRUE);
			SendMessage(yesButton, WM_SETFONT, WPARAM(hFont), TRUE);
			SendMessage(noButton, WM_SETFONT, WPARAM(hFont), TRUE);

			// Cue banner
			// I jumped through hoops to make this work, it still had problems, so I'll do it a different way
			// *Yes* I did link to ComCtl32 Version 6
			// *Yes* I also tried ISOLATION_AWARE_ENABLED
			//SendMessage(editbox, EM_SETCUEBANNER, 0, (LPARAM)L"Describe what caused the crash...");

			// default send UUID
			CheckDlgButton(hwnd, 1, BST_CHECKED);

			SetFocus(yesButton);

			// play notification sound
			MessageBeep(MB_ICONWARNING);

			break;
		}

		case WM_COMMAND:
		{
			if (LOWORD(wParam) == UUID_CHECKBOX && HIWORD(wParam) == BN_CLICKED)
			{
				BOOL checked = IsDlgButtonChecked(hwnd, UUID_CHECKBOX);
				if (checked) 
				{
					CheckDlgButton(hwnd, UUID_CHECKBOX, BST_UNCHECKED);
				}
				else
				{
					CheckDlgButton(hwnd, UUID_CHECKBOX, BST_CHECKED);
				}
			}
			else if (LOWORD(wParam) == NO_BTN && HIWORD(wParam) == BN_CLICKED)
			{
				DestroyWindow(hwnd);
			}
			else if (LOWORD(wParam) == YES_BTN && HIWORD(wParam) == BN_CLICKED)
			{
				std::string uuid = "";
				if (IsDlgButtonChecked(hwnd, UUID_CHECKBOX))
					uuid = Bugs::GetUUIDHash();

				std::vector<wchar_t> descriptionChars;
				descriptionChars.resize(Edit_GetTextLength(editbox) + 1);
				bool success = Edit_GetText(editbox, &descriptionChars[0], descriptionChars.size());

				std::string description = "";
				if (success && &descriptionChars[0] != placeholderText)
					description = std::string(descriptionChars.begin(), descriptionChars.end());

				DestroyWindow(hwnd);

				if(Bugs::ReportCrash(description, crashDumpFileName, uuid))
					crashReportStatus = SUCCESS;
				else
					crashReportStatus = FAIL;
			}
			else if (LOWORD(wParam) == EDIT_BOX && HIWORD(wParam) == EN_SETFOCUS)
			{
				// clear default message if needed
				wchar_t text[40];
				bool success = Edit_GetText(editbox, &text[0], sizeof(text));
				if (success && text == placeholderText)
					Edit_SetText(editbox, L"");
			}
			else if (LOWORD(wParam) == UUID_CHECKBOX && HIWORD(wParam) == BN_KILLFOCUS)
			{
				CheckFocusAllowed(hwnd, uuidCheckbox);
			}
			else if (LOWORD(wParam) == YES_BTN && HIWORD(wParam) == BN_KILLFOCUS)
			{
				CheckFocusAllowed(hwnd, yesButton);
			}
			else if (LOWORD(wParam) == NO_BTN && HIWORD(wParam) == BN_KILLFOCUS)
			{
				CheckFocusAllowed(hwnd, noButton);
			}
			else if (LOWORD(wParam) == EDIT_BOX && HIWORD(wParam) == EN_KILLFOCUS)
			{
				CheckFocusAllowed(hwnd, editbox);
			}
			break;
		}

		case WM_KILLFOCUS:
		{
			CheckFocusAllowed(hwnd, hwnd);
			break;
		}

		case WM_DESTROY:
		{
			PostQuitMessage(0);
			break;
		}
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

static void CreateCrashReportWindow()
{
	MSG  msg;
	WNDCLASS wc = { 0 };
	wc.lpszClassName = TEXT("RE_Kenshi Crash Reporter");
	wc.hInstance = NULL;
	wc.hbrBackground = GetSysColorBrush(COLOR_3DFACE);
	wc.lpfnWndProc = WndProc;
	wc.hCursor = LoadCursor(0, IDC_ARROW);

	RegisterClass(&wc);

	HWND window = CreateWindowEx(WS_EX_TOPMOST, wc.lpszClassName, boost::locale::gettext(L"RE_Kenshi Crash Reporter").c_str(),
		WS_SYSMENU | WS_OVERLAPPED | WS_VISIBLE,
		CW_USEDEFAULT, CW_USEDEFAULT, 500, 500, NULL, 0, NULL, 0);

	// force cursor to be shown
	ShowCursor(true);

	while (GetMessage(&msg, NULL, 0, 0)) {
		if (!IsDialogMessage(window, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	if(crashReportStatus == SUCCESS)
		MessageBoxW(NULL, boost::locale::gettext(L"Crash report sent successfully").c_str(), boost::locale::gettext(L"Success").c_str(), MB_OK | MB_SYSTEMMODAL | MB_ICONINFORMATION);
	else if(crashReportStatus == FAIL)
		MessageBoxW(NULL, boost::locale::gettext(L"Crash report failed to send").c_str(), boost::locale::gettext(L"Error").c_str(), MB_OK | MB_SYSTEMMODAL | MB_ICONERROR);
}

// probably doesn't need to be atomic but who cares
static boost::atomic_bool crashHandlerExecuted(false);
static boost::recursive_mutex unhandledExceptionMx;
static LPTOP_LEVEL_EXCEPTION_FILTER vanillaFilter;
static LONG WINAPI UnhandledException(EXCEPTION_POINTERS* excpInfo = NULL)
{
	boost::recursive_mutex::scoped_lock lock(unhandledExceptionMx);
	DebugLog("Unhandled Exception Filter called");
	if (!crashHandlerExecuted)
	{
		crashHandlerExecuted = true;
		ErrorLog("Main crash handler did not pick up exception");

		// Generate crash dump
		std::string dumpFileName = "crashDump" + KenshiLib::GetKenshiVersion().GetVersion() + "_x64";
		std::string zipName = dumpFileName;
		if (KenshiLib::GetKenshiVersion().GetPlatform() == KenshiLib::BinaryVersion::GOG)
			zipName += "_ns";
		dumpFileName += ".dmp";
		zipName += ".zip";
		HANDLE hFile = CreateFileA(dumpFileName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile != INVALID_HANDLE_VALUE)
		{
			// TODO figure out correct _MINIDUMP_TYPE flags
			MINIDUMP_EXCEPTION_INFORMATION mdei;
			mdei.ClientPointers = FALSE;
			mdei.ThreadId = GetCurrentThreadId();
			mdei.ExceptionPointers = excpInfo;
			bool minidumpCreated = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, MiniDumpNormal, (excpInfo != 0) ? &mdei : 0, 0, 0);
			if (!minidumpCreated)
				ErrorLog("Couldn't write minidump");
			CloseHandle(hFile);
		}
		else
		{
			ErrorLog("Error creating minidump file");
		}

		// package zip
		DebugLog("Packaging crash info...");

		mz_zip_archive archive;
		memset(&archive, 0, sizeof(mz_zip_archive));
		if(mz_zip_writer_init_heap(&archive, 1024 * 1024, 0))
		{
			// Note: some of these may not send, probably because the logging pipes haven't been flushed
			if (boost::filesystem::exists(dumpFileName))
				mz_zip_writer_add_file(&archive, dumpFileName.c_str(), dumpFileName.c_str(), NULL, 0, MZ_BEST_COMPRESSION);
			if (boost::filesystem::exists("save.log"))
				mz_zip_writer_add_file(&archive, "save.log", "save.log", NULL, 0, MZ_BEST_COMPRESSION);
			if (boost::filesystem::exists("MyGUI.log"))
				mz_zip_writer_add_file(&archive, "MyGUI.log", "MyGUI.log", NULL, 0, MZ_BEST_COMPRESSION);
			// pretty sure Kenshi's logging destructors need to be called for these two files to be readable or something
			if (boost::filesystem::exists("kenshi_info.log"))
				mz_zip_writer_add_file(&archive, "kenshi_info.log", "kenshi_info.log", NULL, 0, MZ_BEST_COMPRESSION);
			if (boost::filesystem::exists("kenshi.log"))
				mz_zip_writer_add_file(&archive, "kenshi.log", "kenshi.log", NULL, 0, MZ_BEST_COMPRESSION);
			if (boost::filesystem::exists("kenshi.cfg"))
				mz_zip_writer_add_file(&archive, "kenshi.cfg", "kenshi.cfg", NULL, 0, MZ_BEST_COMPRESSION);
			if (boost::filesystem::exists("Havok.log"))
				mz_zip_writer_add_file(&archive, "Havok.log", "Havok.log", NULL, 0, MZ_BEST_COMPRESSION);
			if (boost::filesystem::exists("audio.log"))
				mz_zip_writer_add_file(&archive, "audio.log", "audio.log", NULL, 0, MZ_BEST_COMPRESSION);

			mz_zip_writer_finalize_heap_archive(&archive, (void**)&pZipBuff, &zipBuffSize);
			mz_zip_writer_end(&archive);
		}
		else
		{
			// TODO fail creating zip
			ErrorLog("Couldn't create dump .zip");
		}

		// trigger crashdump handler
		CreateCrashReportWindow();
	}
	// NOTE: the game doesn't appear to have a proper filter, so this actually causes a crash
	// but that is the CORRECT behaviour and will be forward-compatible if Lo-Fi ever change that
	vanillaFilter(excpInfo);
	return 0;
}

// called AFTER the game saves the crash dump
void* (*CrashReport_orig)(void* arg1, void* arg2);
static void* CrashReport_hook(void* arg1, void* arg2)
{
	ErrorLog("Crash detected");

	CreateCrashReportWindow();

	return CrashReport_orig(arg1, arg2);
}

bool inGame = false;

// called BEFORE the game saves the crash dump
void (*LogManager_destructor_orig)(void* thisptr);
void LogManager_destructor_hook(void* thisptr)
{
	boost::recursive_mutex::scoped_lock lock(unhandledExceptionMx);
	DebugLog("Log manager destructor, probably crashing");

	// stop the UnhandledExceptionFilter handler from double-executing crash handler stuff
	crashHandlerExecuted = true;

	if (inGame && Settings::GetEnableEmergencySaves())
	{
		// force cursor to be shown
		ShowCursor(true);

		// emergency save
		int result = MessageBoxW(NULL, (boost::locale::gettext(L"Kenshi is probably crashing.")
			+ L"\n" + boost::locale::gettext(L"Do you want to force Kenshi to save before it crashes?")).c_str(), boost::locale::gettext(L"RE_Kenshi crash handler").c_str(), MB_YESNO | MB_SYSTEMMODAL | MB_ICONWARNING);

		if (result == IDYES)
		{
			try
			{
				DebugLog("Attempting emergency save...");
				SaveManager* saveManager = SaveManager::getSingleton();

				int emergencySaveNum = 0;
				for (; emergencySaveNum < 10000; ++emergencySaveNum)
					if (!boost::filesystem::exists(saveManager->location + "emergency_save_" + std::to_string((uint64_t)emergencySaveNum)))
						break;

				if (emergencySaveNum < 10000)
				{
					if (!saveManager->saveGame(saveManager->location, "emergency_save_" + std::to_string((uint64_t)emergencySaveNum)))
					{
						SaveFileSystem* saveFileSystem = SaveFileSystem::getSingleton();
						WaitForSingleObject(saveFileSystem->threadHandle, INFINITE);

						// success? update continue save
						Ogre::ConfigFile config;
						config.load("settings.cfg");
						std::ofstream outSettings("settings.cfg");

						// copy out settings from default section
						Ogre::ConfigFile::SectionIterator settingsIter = config.getSectionIterator();
						for (Ogre::ConfigFile::SettingsMultiMap::iterator mapIter = settingsIter.current()->second->begin();
							mapIter != settingsIter.current()->second->end(); ++mapIter)
						{
							// update continue save
							if (mapIter->first == "continue")
								outSettings << "continue=emergency_save_" << emergencySaveNum << std::endl;
							else
								outSettings << mapIter->first << "=" << mapIter->second << std::endl;
						}

						DebugLog("Emergency save completed successfully");
						MessageBoxW(NULL, boost::locale::gettext(L"The game has saved successfully and will now crash.").c_str(), boost::locale::gettext(L"Save success").c_str(), MB_OK | MB_SYSTEMMODAL | MB_ICONINFORMATION);
					}
					else
					{
						ErrorLog("Couldn't create emergency save");
						MessageBoxW(NULL, boost::locale::gettext(L"Error creating save file.").c_str(), boost::locale::gettext(L"Save failed").c_str(), MB_OK | MB_SYSTEMMODAL | MB_ICONERROR);
					}
				}
				else
				{
					ErrorLog("Error finding unused emergency save slot");
					MessageBoxW(NULL, boost::locale::gettext(L"Error finding unused emergency save slot.").c_str(), boost::locale::gettext(L"Save failed").c_str(), MB_OK | MB_SYSTEMMODAL | MB_ICONERROR);
				}
			}
			catch (...)
			{
				ErrorLog("Unspecified emergency save error");
				MessageBoxW(NULL, boost::locale::gettext(L"An unspecified error occurred while saving.").c_str(), boost::locale::gettext(L"Save failed").c_str(), MB_OK | MB_SYSTEMMODAL | MB_ICONERROR);
			}
		}
	}

	LogManager_destructor_orig(thisptr);
}

// catches bugs that occur before Kenshi's crash handler gets set up
void Bugs::PreInit()
{
	if (discordID == "PUT_YOUR_ID_HERE")
		MessageBoxA(NULL, "Discord setup error", "Add your info to \"Discord.h\" and recompile to fix bug reporting", MB_ICONWARNING);

	// https://stackoverflow.com/questions/13591334/what-actions-do-i-need-to-take-to-get-a-crash-dump-in-all-error-scenarios
	// disable Windows default error handling behaviour
	SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
	// register our error handler
	vanillaFilter = SetUnhandledExceptionFilter(UnhandledException);
	if (vanillaFilter == nullptr)
	{
		ErrorLog("Expected a dummy UnhandledExceptionFilter but got NULL");
	}
	else
	{
		DebugLog("UEF registered successfully");
	}
}

void Bugs::UndoPreInit()
{
	// Sync version checker to make sure it no longer needs the global crash handler
	Version::SyncInit();

	// register our error handler
	if (vanillaFilter != nullptr)
	{
		LPTOP_LEVEL_EXCEPTION_FILTER filterCheck = SetUnhandledExceptionFilter(vanillaFilter);
		if (filterCheck != &UnhandledException)
			ErrorLog("Unexpected filter change in Bugs::UndoPreInit()");
		else
			DebugLog("UEF unregistered successfully");
	}
	else
	{
		ErrorLog("Attempted to register vanilla exception handler but got NULL");
	}
}

// 2nd init stage - run after KenshiLib init
void Bugs::Init()
{
	// secondary crash hook - this one uses Kenshi's crash handling code to generate most of the dump files
	KenshiLib::AddHook(KenshiLib::GetRealFunction(showErrorMessage), CrashReport_hook, &CrashReport_orig);
}

void Bugs::InitMenu()
{
	// I tried a few ways of hooking the root crash function
	// Kenshi's exception handler runs before the UnhandledExceptionFilter (probably implemented as a try/catch)
	// Other useless APIs: _set_invalid_parameter_handler, set_terminate, set_unexpected
	// Also VEH hooks get triggered by a bunch of non-crashing events, so AddVectoredExceptionHandler is also useless
	// When an unhandled exception occurs, execution jumps directly to an address inside the body of a function, 
	// meaning we can't use a regular function hook
	// The first thing the game's exception handler does is call the LogManager destructor, so we hook that instead
	// (nothing else seems to call it)
	// void MyGUI::LogManager::~LogManager(LogManager* thisptr)
	void* LogManagerDestructorPtr = Escort::GetFuncAddress("MyGUIEngine_x64.dll", "??1LogManager@MyGUI@@QEAA@XZ");
	// annoyingly, this is a 5-byte instruction in a packed jmp table, so we need to dereference offset
	// in order to find a place that can be safely hooked with our 6-byte hook
	// TODO can we use JmpReplaceHook32 instead?
	uint8_t* LogManagerDestructor_jmp_ptr = (uint8_t*)Escort::GetFuncAddress("MyGUIEngine_x64.dll", "??1LogManager@MyGUI@@QEAA@XZ");
	if (*LogManagerDestructor_jmp_ptr != 0xE9)
	{
		ErrorLog("LogManager::~LogManager has incorrect instruction");
		return;
	}
	// get address of offset in jmp instruction
	uint32_t* offsetPtr = (uint32_t*)(LogManagerDestructor_jmp_ptr + 1);
	// find target of jmp
	uint8_t* LogManagerDestructor_body_ptr = LogManagerDestructor_jmp_ptr + *offsetPtr + 5;
	KenshiLib::AddHook(LogManagerDestructor_body_ptr, LogManager_destructor_hook, &LogManager_destructor_orig);
}

void Bugs::InitInGame()
{
	// disable global crash handler so only crashes that trigger ~LogManager() are hooked
	LPTOP_LEVEL_EXCEPTION_FILTER filterCheck = SetUnhandledExceptionFilter(vanillaFilter);
	if (filterCheck != &UnhandledException)
		ErrorLog("Unexpected filter change in Bugs::UndoPreInit()");
	else
		DebugLog("UEF unregistered.");
	inGame = true;
}