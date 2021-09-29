#include <Windows.h>
#include <string>

#include "FSHook.h"

#include "Escort.h"

// old assembler-based code
//extern "C" void fsopenHook();

FILE* (*_fsopen_orig)(const char* filename, const char* mode, int shflag);

int filesCounted = 0;

FILE* _fsopen_hook(const char* filename, const char* mode, int shflag)
{
	++filesCounted;
	if(filesCounted < 10)
		DebugLog("File opened: " + std::string(filename));
	return _fsopen_orig(filename, mode, shflag);
}


void FSHook::Init()
{
	// hook file open
	void* fsopen = Escort::GetFuncAddress("MSVCR100.dll", "_fsopen");

	std::string debugStr = "Address: " + std::to_string((uint64_t)fsopen);

	_fsopen_orig = Escort::JmpReplaceHook<FILE*(const char* filename, const char* mode, int shflag)>(fsopen, _fsopen_hook, 10);
	// Windows reserves the bottom 64k of the address space?
	//Escort::AllocateRWXNear((void*)0x10000, 5);
	//MessageBoxA(0, debugStr.c_str(), "Debug", MB_OK);
	//Escort::CallRAXHook(fsopen, fsopenHook, 15);
	//MessageBoxA(0, "Installed...", "Debug", MB_OK);

}