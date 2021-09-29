#pragma once

#include <vector>
#include <stdint.h>

namespace Escort
{
	void Hook(void* sourceAddr, void* targetAddr, size_t replacedBytes);
	void PushRetHook(void* sourceAddr, void* targetAddr, size_t replacedBytes);
	// relative call, your call replaces func, args are passed over
	// returns pointer to replaced function so you can call the original
	// in your code
	template<typename T>
	//  Must be at least 5 bytes
	T* JmpReplaceHook(void* sourceAddr, void* targetAddr, size_t replacedBytes);
	// this messes up the stack so should only be used for naked/assembler calls
	void CallRAXHook(void* sourceAddr, void* targetAddr, size_t replacedBytes);
	void NOP(void* codeAddr, size_t replacedBytes);
	// Allocate RWX page near address, useful for relative jmps
	void* AllocateRWXNear(void* targetAddr, size_t allocSize);
	// ASM methods, mostly used internally
	void PushRetHookASM(void* sourceAddr, void* targetAddr, size_t replacedBytes);
	void Push64ASM(void* value, std::vector<uint8_t> &bytes);
	void NOPASM(size_t replacedBytes, std::vector<uint8_t>& bytes);
	void RetASM(std::vector<uint8_t>& bytes);
	void PushRAX(std::vector<uint8_t>& bytes);
	void PopRAX(std::vector<uint8_t>& bytes);
	void MovAbsRAX(void* value, std::vector<uint8_t>& bytes);
	void CallRAX(std::vector<uint8_t>& bytes);
	void CallRel(std::vector<uint8_t>& bytes, int32_t offset);
	void JmpRel(std::vector<uint8_t>& bytes, int32_t offset);
	void WriteProtected(void* destAddr, void* sourceAddr, size_t count);
	void* GetFuncAddress(std::string moduleName, std::string functionName);
}

// template functions

template<typename T>
T* Escort::JmpReplaceHook(void* sourceAddr, void* targetAddr, size_t replacedBytes)
{

	uint8_t* sourceInstructionsPtr = (uint8_t*)sourceAddr;
	uint8_t* sourceContinuePtr = sourceInstructionsPtr + replacedBytes;
	if (abs(int64_t((uint8_t*)targetAddr - sourceInstructionsPtr)) > 0x7F000000)
	{
		ErrorLog("Address delta 1 too large for hook");
		return nullptr;
	}

	int64_t hookStartRel = (int64_t)targetAddr - (int64_t)sourceInstructionsPtr;

	std::vector<uint8_t> rwxBytes;

	// allocate ptr for hook + ptr for continue + start of continue function
	rwxBytes.resize(replacedBytes);

	// add instructions copy
	memcpy(&rwxBytes[0], sourceInstructionsPtr, replacedBytes);
	
	// jmp to continue execution of old function (ptr filled in after alloc)
	JmpRel(rwxBytes, 0);

	uint8_t* rwxAlloc = (uint8_t*)Escort::AllocateRWXNear((void*)0x10000, rwxBytes.size());

	// write continue pointer in jmp instruction
	uint32_t continuePtrAddr = uint32_t(sourceContinuePtr - &rwxAlloc[replacedBytes]) - 5;
	rwxBytes[rwxBytes.size() - 1] = (continuePtrAddr >> 24) & 0xFF;
	rwxBytes[rwxBytes.size() - 2] = (continuePtrAddr >> 16) & 0xFF;
	rwxBytes[rwxBytes.size() - 3] = (continuePtrAddr >> 8) & 0xFF;
	rwxBytes[rwxBytes.size() - 4] = continuePtrAddr & 0xFF;

	if (abs(int64_t((uint8_t*)continuePtrAddr - rwxAlloc)) > 0x7F000000)
	{
		ErrorLog("Address delta 2 too large for hook");
		return nullptr;
	}

	// write bytes to alloc'd memory
	memcpy(rwxAlloc, &rwxBytes[0], rwxBytes.size());

	// bytes to overwrite the start of the function with
	std::vector<uint8_t> hookBytes;
	JmpRel(hookBytes, (int32_t)((uint8_t*)targetAddr - sourceInstructionsPtr));

	// copy hook instructions 
	Escort::WriteProtected(sourceAddr, &hookBytes[0], hookBytes.size());
	
	// return address of old backup
	return (T*)rwxAlloc;
}
