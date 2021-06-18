#include "Escort.h"

#include "windows.h"
#include <stdint.h>
#include <array>
#include <string>

#include "Debug.h"

// call &function (near, relative)
void Escort::Hook(void* sourceAddr, void* destAddr, size_t replacedBytes)
{
	// push rax
	// mov rax, (addr)
	// call rax
	// (need to pop rax after hook)
	uint8_t hookBytes[] = { 0x50, 0x48, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xd0 };
	// insert hook address
	memcpy(&hookBytes[3], destAddr, sizeof(void*));
	hookBytes[1] = ((size_t)destAddr >> 0) & 0xFF;
	hookBytes[2] = ((size_t)destAddr >> 8) & 0xFF;
	// allocate memory for copied instructions
	uint8_t* instructionsCopy = (uint8_t*)VirtualAlloc(NULL, replacedBytes + 6, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);

}

// push rax
// movabs rax, (addr)
// call rax
// (hook)
// pop rax
// (continue)
void Escort::CallRAXHook(void* sourceAddr, void* destAddr, size_t replacedBytes)
{
	uint8_t* sourceInstructionsPtr = (uint8_t*)sourceAddr;
	uint8_t* sourceContinuePtr = sourceInstructionsPtr + replacedBytes;

	std::vector<uint8_t> hookBytes;

	// backup old value
	PushRAX(hookBytes);
	// load hook address
	MovAbsRAX(destAddr, hookBytes);
	// call hook address
	CallRAX(hookBytes);
	// load old value
	PopRAX(hookBytes);

	// add instructions copy
	size_t hookPreambleEnd = hookBytes.size();
	hookBytes.resize(hookPreambleEnd + replacedBytes);
	memcpy(&hookBytes[hookPreambleEnd], sourceInstructionsPtr, replacedBytes);

	// push continue address
	Push64ASM(sourceContinuePtr, hookBytes);
	RetASM(hookBytes);

	// allocate memory for hook
	uint8_t* instructionsCopy = (uint8_t*)VirtualAlloc(NULL, hookBytes.size(), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	uint8_t* hookContinueAddr = &instructionsCopy[hookPreambleEnd];

	// copy hook instructions
	memcpy(instructionsCopy, &hookBytes[0], hookBytes.size());

	PushRetHookASM(sourceInstructionsPtr, instructionsCopy, replacedBytes);
}

// This is bad but took like 30 seconds to implement
// DON'T use this if you care about performance or good code...
// Pushes the pointer onto the stack using 2 32-bit pushes, then returns to the pushed address
void Escort::PushRetHook(void* sourceAddr, void* destAddr, size_t replacedBytes)
{
	uint8_t* sourceInstructionsPtr = (uint8_t*)sourceAddr;
	uint8_t* sourceContinuePtr = sourceInstructionsPtr + replacedBytes;

	std::vector<uint8_t> hookBytes;

	// push dummy address, to return to after hook
	Push64ASM(nullptr, hookBytes);

	// push hook address
	Push64ASM(destAddr, hookBytes);
	RetASM(hookBytes);

	// add instructions copy
	size_t hookPreambleEnd = hookBytes.size();
	hookBytes.resize(hookPreambleEnd + replacedBytes);
	memcpy(&hookBytes[hookPreambleEnd], sourceInstructionsPtr, replacedBytes);

	// push continue address
	Push64ASM(sourceContinuePtr, hookBytes);
	RetASM(hookBytes);

	// allocate memory for hook
	uint8_t* instructionsCopy = (uint8_t*)VirtualAlloc(NULL, hookBytes.size(), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	uint8_t* hookContinueAddr = &instructionsCopy[hookPreambleEnd];
	// hack to get 1st/2nd half of address
	uint8_t* hookContinueAddrPtr = reinterpret_cast<uint8_t*> (&hookContinueAddr);

	// HACK insert hook continue address into first address push
	memcpy(&hookBytes[1], &hookContinueAddrPtr[0], 4);
	memcpy(&hookBytes[9], &hookContinueAddrPtr[4], 4);

	// copy hook instructions
	memcpy(instructionsCopy, &hookBytes[0], hookBytes.size());

	PushRetHookASM(sourceInstructionsPtr, instructionsCopy, replacedBytes);
}

// stolen from https://www.codeproject.com/tips/479880/getlasterror-as-std-string
// Create a string with last error message
std::string GetLastErrorStdStr()
{
	DWORD error = GetLastError();
	if (error)
	{
		LPVOID lpMsgBuf;
		DWORD bufLen = FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			error,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPTSTR)&lpMsgBuf,
			0, NULL);
		if (bufLen)
		{
			LPCSTR lpMsgStr = (LPCSTR)lpMsgBuf;
			std::string result(lpMsgStr, lpMsgStr + bufLen);

			LocalFree(lpMsgBuf);

			return result;
		}
	}
	return std::string();
}

// This is bad but took like 30 seconds to implement
// DON'T use this if you care about performance or good code...
// Pushes the pointer onto the stack using 2 32-bit pushes, and a ret to the pushed address
void Escort::PushRetHookASM(void* sourceAddr, void* destAddr, size_t replacedBytes)
{
	uint8_t hookBytes[] = { 0x68, 0x00, 0x00, 0x00, 0x00, 0xc7, 0x44, 0x24, 0x04, 0x00, 0x00, 0x00, 0x00, 0xc3 };
	const size_t hookSize = sizeof(hookBytes);
	// insert hook address
	uint8_t* destAddrBytes = reinterpret_cast<uint8_t*> (&destAddr);
	memcpy(&hookBytes[1], &destAddrBytes[0], 4);
	memcpy(&hookBytes[9], &destAddrBytes[4], 4);
	
	// allocate memory for copied instructions
	HANDLE handle = GetModuleHandle(NULL);
	size_t bytesWritten = -1;
	DWORD oldProtect;
	bool success = VirtualProtect(sourceAddr, hookSize, PAGE_EXECUTE_READWRITE, &oldProtect) == 0;
	if (!success)
	{
		DWORD error = GetLastError();
		std::string errorMsg = "Protection change failed! " + std::to_string((long long)error);
		ErrorLog(errorMsg.c_str());
		//MessageBoxA(0, errorMsg.c_str(), "Debug", MB_OK);
	}
	memcpy(sourceAddr, hookBytes, hookSize);
	// HACK nop remaining bytes
	uint8_t *sourceBytePtr = reinterpret_cast<uint8_t*>(sourceAddr);
	NOP(sourceBytePtr + hookSize, replacedBytes - hookSize);
}


void Escort::PushRAX(std::vector<uint8_t>& bytes)
{
	bytes.push_back(0x50);
}

void Escort::PopRAX(std::vector<uint8_t>& bytes)
{
	bytes.push_back(0x58);
}

void Escort::MovAbsRAX(void* value, std::vector<uint8_t>& bytes)
{
	std::array<uint8_t, 10> instructionBytes = { 0x48, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	memcpy(&instructionBytes[2], &value, sizeof(void*));
	bytes.insert(bytes.end(), instructionBytes.begin(), instructionBytes.end());
}

void Escort::CallRAX(std::vector<uint8_t>& bytes)
{
	bytes.push_back(0xff);
	bytes.push_back(0xd0);
}

void Escort::NOP(void* codeAddr, size_t replacedBytes)
{
	memset(codeAddr, 0x90, replacedBytes);
}

// methods for generating byte vectors
void Escort::Push64ASM(void* value, std::vector<uint8_t>& bytes)
{
	std::array<uint8_t, 13> hookBytes = { 0x68, 0x00, 0x00, 0x00, 0x00, 0xc7, 0x44, 0x24, 0x04, 0x00, 0x00, 0x00, 0x00 };
	const size_t hookSize = sizeof(hookBytes);
	// insert hook address
	uint8_t* valueBytes = reinterpret_cast<uint8_t*> (&value);
	memcpy(&hookBytes[1], &valueBytes[0], 4);
	memcpy(&hookBytes[9], &valueBytes[4], 4);

	bytes.insert(bytes.end(), hookBytes.begin(), hookBytes.end());
}

void Escort::NOPASM(size_t replacedBytes, std::vector<uint8_t>& bytes)
{
	for(size_t i=0;i<replacedBytes;++i)
		bytes.push_back(0x90);
}

void Escort::RetASM(std::vector<uint8_t>& bytes)
{
	bytes.push_back(0xc3);
}