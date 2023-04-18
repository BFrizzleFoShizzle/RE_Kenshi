#include "Escort.h"

#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#include <stdint.h>
#include <array>
#include <string>
#include <map>

#include "Debug.h"

// internal functions
namespace Escort
{
	// Returns true if we can do a relative jmp between
	inline bool IsNear(void* ptr1, void* ptr2);
	void* AllocateRWXPageNear(void* targetAddr, size_t allocSize);
	size_t GetPageSize(void* ptr);
}

void* Escort::GetFuncAddress(std::string moduleName, std::string functionName)
{
	HMODULE hMod = GetModuleHandleA(moduleName.c_str());
	std::string debugStr = "DLL handle: " + std::to_string((uint64_t)hMod);
	//MessageBoxA(0, debugStr.c_str(), "Debug", MB_OK);
	return GetProcAddress(hMod, functionName.c_str());
}

void Escort::WriteProtected(void* destAddr, void* sourceAddr, size_t count)
{
	DWORD oldProtect;
	bool success = VirtualProtect(destAddr, count, PAGE_EXECUTE_READWRITE, &oldProtect) == 0;
	if (!success)
	{
		DWORD error = GetLastError();
		std::string errorMsg = "Protection change failed! " + std::to_string((long long)error);
		ErrorLog(errorMsg.c_str());
	}
	memcpy(destAddr, sourceAddr, count);
}

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

// Returns true if we can do a relative jmp between
inline bool Escort::IsNear(void* ptr1, void* ptr2)
{
	return abs(int64_t((uint8_t*)ptr1 - (uint8_t*)ptr2)) < 0x7F000000;
}

// Internal function for 
void* Escort::AllocateRWXPageNear(void* targetAddr, size_t allocSize)
{
	// HACK there's a race condition with this - I think if the game allocates a block 
	// of memory while this is running, it breaks
	for (int attempts = 0; attempts < 10; ++attempts)
	{
		// scan up to 2GB before giving up
		uint8_t* allocAddr = (uint8_t*)targetAddr;
		for (int i = 0; IsNear(allocAddr, targetAddr); ++i)
		{
			MEMORY_BASIC_INFORMATION output;
			size_t written = VirtualQuery(allocAddr, &output, sizeof(MEMORY_BASIC_INFORMATION));

			// Let me tell you the tale of my people
			// So, windows allows you to query memory as 4KB pages
			// However, under the hood, memory is reserved/allocated in 64KB chunks
			// In this code block, we have scanned memory for a 4KB page that is marked as "free"
			// to quote their docs: 
			// "Indicates free pages not accessible to the calling process and available to be allocated"
			// So, you'd think you'd be able to call VirtualAlloc and allocate that page, as it's marked
			// as free, and the docs say pages marked as free can be allocated.
			// You'd be WRONG. Why, you ask?
			// Depending on how the 64KB block is set up, you may have regions marked as free that cannot be allocated.
			// Now, you'd think there'd be an easy solution to this. Surely if we keep scanning for free pages,
			// we should find a 4KB page in a 64KB block that we can allocate in?
			// Oh, how naive. 
			// I, too, was once a sweet summer child.
			// Microsoft, in all their wisdom, decided that VirtualQuery should report any continuous blocks
			// of 4KB pages with identical attribures as a single item.
			// Did you catch the issue?
			// Let's say there's a chunk with 60KB allocated, so there's a single 4KB page at the end of it
			// marked as free. That last 4KB page cannot be allocated because god knows why.
			// The entire next 64KB of pages after that free page CAN be allocated as it's in a different
			// 64KB block.
			// But VirtualQuery WON'T give you a pointer to the 64KB of ACTUALLY ALLOCATEABLE MEMORY.
			// It'll ONLY give you the pointer to the start of the free block, the 4KB of memory YOU ARE NOT ABLE TO ALLOCATE.
			// WHAT IN THE HOLY FUCK.
			// So, we have to take the pointer we get out of VirtualQuery and round up to the next 64KB
			// WHY IN GODS NAME IS NONE OF THIS IN THE DOCS FOR VirutalAlloc AND VirtualQuery?!?

			uint8_t* allocateTargetAddress = (uint8_t*)output.BaseAddress;
			size_t remainder = (intptr_t)output.BaseAddress % 0x10000;

			if (remainder != 0)
				allocateTargetAddress += 0x10000 - remainder;

			if (written == 0)
			{
				DebugLog("AllocateRWXNear: 0 written");
				return nullptr;
			}
			else if (output.RegionSize == 0)
			{
				DebugLog("AllocateRWXNear: size 0");
				return nullptr;
			}
			else if (output.State == MEM_FREE
				&& allocateTargetAddress + allocSize < (uint8_t*)output.BaseAddress + output.RegionSize
				&& IsNear(allocateTargetAddress, targetAddr))
			{
				// Edge case - if we're already in an allocable block, closest address is the target address
				/*
				if (allocLocation < targetAddr)
					allocLocation = targetAddr;
				*/
				// found free page, try and alloc
				void* alloc = (uint8_t*)VirtualAlloc(allocateTargetAddress, allocSize, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
				if (alloc == NULL)
				{
					std::stringstream str;
					str << std::hex << "Failed RWX alloc at: " << allocateTargetAddress << ", continuing...";
					DebugLog(str.str());
				}
				else
				{
					// success
					return alloc;
				}
			}

			// HACK
			if (i > 100)
				break;

			if ((uint8_t*)output.BaseAddress + output.RegionSize == 0)
				break;

			allocAddr = (uint8_t*)output.BaseAddress + output.RegionSize;
		}
		DebugLog("RWX alloc failed, backing off and trying again...");
		// wait 10ms before retrying
		Sleep(10);
	}

	ErrorLog("Near RWX alloc failed!");
	return nullptr;
}

size_t Escort::GetPageSize(void* ptr)
{
	MEMORY_BASIC_INFORMATION output;
	size_t written = VirtualQuery((uint8_t*)ptr, &output, sizeof(MEMORY_BASIC_INFORMATION));
	if (written == 0)
		return 0;
	return output.RegionSize;
}

// Shitty memory "heap" (it's not a heap)
// No freeing/garbage collection, alignment is done through skipping bytes
class RWXPage
{
public:
	RWXPage(void* base, size_t allocatedSize)
		: base((uint8_t*)base), allocatedSize(allocatedSize), nextOffset(0)
	{

	}
	uint8_t* GetNextOffset(bool align = false)
	{
		if(!align)
			return &base[nextOffset];
		else
			return &base[nextOffset + GetBytesToAlign()];
	}
	bool CanAllocate(size_t size, bool align = false)
	{
		if(!align)
			return size < allocatedSize - nextOffset;
		else
			return CanAllocate(size + GetBytesToAlign());
	}
	uint8_t* Allocate(size_t size, bool align = false)
	{
		if (align)
		{
			// skip till aligned
			Allocate(GetBytesToAlign());
		}
		// TODO error-checking
		uint8_t* allocated = &base[nextOffset];
		nextOffset += size;
		return allocated;
	}
private:
	// 64-bit/8-byte alignment
	size_t GetBytesToAlign()
	{
		size_t remainder = nextOffset % 8;
		if (remainder == 0)
			return 0;
		else
			return 8 - remainder;
	}
	uint8_t* base;
	ptrdiff_t nextOffset;
	size_t allocatedSize;
};
// TODO locks/threading?
std::vector<RWXPage> allocatedPages;

void* Escort::AllocateRWXNear(void* targetAddr, size_t allocSize, bool align)
{
	// Check if we can alloc from existing pages
	for (int i = 0; i < allocatedPages.size(); ++i)
	{
		if (IsNear(targetAddr, allocatedPages[i].GetNextOffset(align))
			&& allocatedPages[i].CanAllocate(allocSize, align))
		{
			// Allocate from existing page
			return allocatedPages[i].Allocate(allocSize, align);
		}
	}

	// Need to allocate new page
	void* rwxMemory = AllocateRWXPageNear(targetAddr, allocSize);
	size_t newPageSize = GetPageSize(rwxMemory);

	// TODO error-checking/threading
	RWXPage newPage = RWXPage(rwxMemory, newPageSize);
	allocatedPages.push_back(newPage);
	return allocatedPages.back().Allocate(allocSize, align);
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

// rel call: 
// call +0x12345678
// Offset = 0x12345673 (-5 from instruction size)
// 0xE8 0x73 0x56 0x34 0x12
void Escort::CallRel(std::vector<uint8_t>& bytes, int32_t offset)
{
	// Bitshift of signed int is technically implementation-dependant
	// so we convert to uint. Pretty sure the C++ standard guarantees 
	// this will work
	// Currently untested for negative numbers, but theoretically works
	uint32_t offsetUint = offset;
	// subtract instruction length
	offsetUint -= 5;
	bytes.push_back(0xE8);
	bytes.push_back(offsetUint & 0xFF);
	bytes.push_back((offsetUint >> 8) & 0xFF);
	bytes.push_back((offsetUint >> 16) & 0xFF);
	bytes.push_back(offsetUint >> 24);
}

// rel jmp:
// jmp +0x12345678
// Offset = 0x12345673 (-5 from instruction size)
// 0xE9 0x73 0x56 0x34 0x12
void Escort::JmpRel(std::vector<uint8_t>& bytes, int32_t offset)
{
	// subtract instruction length
	offset -= 5;
	// Same as CallRel
	uint32_t offsetUint = offset;
	bytes.push_back(0xE9);
	bytes.push_back(offsetUint & 0xFF);
	bytes.push_back((offsetUint >> 8) & 0xFF);
	bytes.push_back((offsetUint >> 16) & 0xFF);
	bytes.push_back(offsetUint >> 24);
}

// jmp to addr held in ptr
// rex.W jmp FWORD PTR [0x12345678]
// 0x48 0xFF 0x2C 0x25 0x78 0x56 0x32 0x12
void Escort::JmpAbsPtr(std::vector<uint8_t>& bytes, int32_t offset)
{
	// subtract instruction length
	offset -= 6;
	// Same as CallRel
	uint32_t offsetUint = offset;
	//bytes.push_back(0x48);
	bytes.push_back(0xFF);
	//bytes.push_back(0x2C);
	bytes.push_back(0x25);
	bytes.push_back(offsetUint & 0xFF);
	bytes.push_back((offsetUint >> 8) & 0xFF);
	bytes.push_back((offsetUint >> 16) & 0xFF);
	bytes.push_back(offsetUint >> 24);
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


size_t Escort::GetPrologueSize(void* function)
{
	// get size of function prologue via SEH unwind info
	uint8_t* imageBase;
	UNWIND_HISTORY_TABLE historyTable;
	PRUNTIME_FUNCTION runtimeFunction = RtlLookupFunctionEntry((DWORD64)function, reinterpret_cast<PDWORD64>(&imageBase), &historyTable);

	if (runtimeFunction == nullptr)
	{
		ErrorLog("RtlLookup failed, function probably lacks SEH info");
		return 0;
	}

	UNWIND_INFO* unwindInfo = reinterpret_cast<UNWIND_INFO*>(imageBase + runtimeFunction->UnwindData);

	return unwindInfo->SizeOfProlog;
}