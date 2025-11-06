#include <Windows.h>
#include <string>
#include <fstream>

#include "FSHook.h"

#include "Escort.h"
#include <core/Functions.h>
#include "Settings.h"

// old assembler-based code
//extern "C" void fsopenHook();

std::ofstream fileIOLog;

FILE* (*_fsopen_orig)(const char* filename, const char* mode, int shflag);

FILE* _fsopen_hook(const char* filename, const char* mode, int shflag)
{
	std::unordered_map<std::string, std::string>::const_iterator fileOverride = Settings::GetFileOverrides()->find(filename);

	if (Settings::GetLogFileIO())
		fileIOLog << "File open: \"" << filename << "\"" << std::endl;

	if (fileOverride != Settings::GetFileOverrides()->end())
	{
		if (Settings::GetLogFileIO())
			fileIOLog << "File open override: \"" << fileOverride->first << "\" -> \"" + fileOverride->second << "\"" << std::endl;

		if (fileOverride->second == "HIDE")
			// TODO SetError()?
			return NULL;
		else
			// Use overridden path
			return _fsopen_orig(fileOverride->second.c_str(), mode, shflag);
	}

	// No override
	return _fsopen_orig(filename, mode, shflag);
}

// this gets broken, don't call it
HANDLE (*FindFirstFileW_orig)(LPCWSTR lpFileName, LPWIN32_FIND_DATAW lpFindFileData);
// this still works
HANDLE(*FindFirstFileW_kernelbase)(LPCWSTR lpFileName, LPWIN32_FIND_DATAW lpFindFileData);

HANDLE FindFirstFileW_hook(LPCWSTR lpFileName, LPWIN32_FIND_DATAW lpFindFileData)
{
	std::wstring filenamew = lpFileName;
	std::string filename = std::string(filenamew.begin(), filenamew.end());
	std::unordered_map<std::string, std::string>::const_iterator fileOverride = Settings::GetFileOverrides()->find(filename);

	if (Settings::GetLogFileIO())
		fileIOLog << "File search: \"" << filename << "\"" << std::endl;

	if (fileOverride != Settings::GetFileOverrides()->end())
	{
		if (Settings::GetLogFileIO())
			fileIOLog << "File search override: \"" << fileOverride->first << "\" -> \"" << fileOverride->second << "\"" << std::endl;

		if(fileOverride->second == "HIDE")
			return FindFirstFileW_kernelbase(lpFileName, lpFindFileData);
		else
			// Use overridden path
			return FindFirstFileW_kernelbase(std::wstring(fileOverride->second.begin(), fileOverride->second.end()).c_str(), lpFindFileData);
	}

	// Normal behaviour
	return FindFirstFileW_kernelbase(lpFileName, lpFindFileData);
}


void FSHook::Init()
{
	// set up logging
	fileIOLog = std::ofstream("FileIOLog.txt");

	// hook file open
	void* fsopen = Escort::GetFuncAddress("MSVCR100.dll", "_fsopen");
	FindFirstFileW_kernelbase = (HANDLE(*)(LPCWSTR lpFileName, LPWIN32_FIND_DATAW lpFindFileData))Escort::GetFuncAddress("KernelBase.dll", "FindFirstFileW");
	void* findFirstFileW = Escort::GetFuncAddress("kernel32.dll", "FindFirstFileW");

	std::string debugStr = "Address: " + std::to_string((uint64_t)fsopen);

	KenshiLib::AddHook(fsopen, _fsopen_hook, &_fsopen_orig);
	
	// First instruction has a relative offset that gets bork'd by the hook, so we call the kernel32 function instead
	// I think this is a leaf function, so can't use SEH prologue lookup, need to hard-code replaced bytes...
	KenshiLib::AddHook(findFirstFileW, FindFirstFileW_hook, &FindFirstFileW_orig);
	
	// Windows reserves the bottom 64k of the address space?
	//Escort::AllocateRWXNear((void*)0x10000, 5);
	//MessageBoxA(0, debugStr.c_str(), "Debug", MB_OK);
	//Escort::CallRAXHook(fsopen, fsopenHook, 15);
	//MessageBoxA(0, "Installed...", "Debug", MB_OK);

}