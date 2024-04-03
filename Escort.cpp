#include "Escort.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
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

// these were added in the Win10 SDK so we have to recreate them so it compiles with the 7.1 SDK
//////////////////////
#define MEM_EXTENDED_PARAMETER_TYPE_BITS    8
typedef struct DECLSPEC_ALIGN(8) MEM_EXTENDED_PARAMETER {

	struct {
		DWORD64 Type : MEM_EXTENDED_PARAMETER_TYPE_BITS;
		DWORD64 Reserved : 64 - MEM_EXTENDED_PARAMETER_TYPE_BITS;
	} DUMMYSTRUCTNAME;

	union {
		DWORD64 ULong64;
		PVOID Pointer;
		SIZE_T Size;
		HANDLE Handle;
		DWORD ULong;
	} DUMMYUNIONNAME;

} MEM_EXTENDED_PARAMETER, * PMEM_EXTENDED_PARAMETER;

typedef struct _MEM_ADDRESS_REQUIREMENTS {
	PVOID LowestStartingAddress;
	PVOID HighestEndingAddress;
	SIZE_T Alignment;
} MEM_ADDRESS_REQUIREMENTS, * PMEM_ADDRESS_REQUIREMENTS;

typedef enum MEM_EXTENDED_PARAMETER_TYPE {
	MemExtendedParameterInvalidType = 0,
	MemExtendedParameterAddressRequirements,
	MemExtendedParameterNumaNode,
	MemExtendedParameterPartitionHandle,
	MemExtendedParameterUserPhysicalHandle,
	MemExtendedParameterAttributeFlags,
	MemExtendedParameterImageMachine,
	MemExtendedParameterMax
} MEM_EXTENDED_PARAMETER_TYPE, * PMEM_EXTENDED_PARAMETER_TYPE;

typedef PVOID(VirtualAlloc2Type)(HANDLE process, PVOID baseAddress, SIZE_T size, ULONG allocationType, ULONG pageProtection, MEM_EXTENDED_PARAMETER* extendedParameters, ULONG ParameterCount) ;
static VirtualAlloc2Type(*VirtualAlloc2Ptr) = nullptr;
static SYSTEM_INFO sysInfo;
/////////////////////////

void Escort::Init() {
	// page size: 0x1000 
	// min addr: 0x10000 max addr: 0x7FFFFFFEFFFF
	// alloc granularity: 0x10000
	GetSystemInfo(&sysInfo);

	// https://stackoverflow.com/questions/8696653/dynamically-load-a-function-from-a-dll
	// try to get VirtalAlloc2 function if it exists (requires Win10+)
	// THis function solves all the excrutiatingly painful race conditions you get when implementing a near-memory allocator
	HINSTANCE hGetProcIDDLL = LoadLibrary(L"KERNELBASE.dll");

	if (!hGetProcIDDLL) {
		DebugLog("Warning: could not load `KERNELBASE.dll` - old memory allocator must be used");
		return;
	}

	// resolve function address here
	VirtualAlloc2Ptr = (VirtualAlloc2Type*)GetProcAddress(hGetProcIDDLL, "VirtualAlloc2");
	if (VirtualAlloc2Ptr != nullptr) {
		DebugLog("VirtualAlloc2 found.");
#if 0
		/* Backup of working code
		
		MEM_ADDRESS_REQUIREMENTS addressReqs = { 0 };
		MEM_EXTENDED_PARAMETER param = { 0 };

		addressReqs.Alignment = allocationGranularity;
		addressReqs.HighestEndingAddress = (PVOID)(ULONG_PTR)0x7fffffff;

		param.Type = MemExtendedParameterAddressRequirements;
		param.Pointer = &addressReqs;

		void* alloc = VirtualAlloc2Ptr(
			nullptr, nullptr,
			allocationGranularity,
			MEM_RESERVE | MEM_COMMIT,
			PAGE_READWRITE,
			&param, 1);
			*/
		
		// TODO REMOVE
		MEM_ADDRESS_REQUIREMENTS addressReqs = { 0 };
		MEM_EXTENDED_PARAMETER param = { 0 };

		/*
		ULONG_PTR maxTestAddr = (ULONG_PTR)sysInfo.lpMaximumApplicationAddress - sysInfo.dwPageSize;
		ULONG_PTR minTestAddr = (ULONG_PTR)sysInfo.lpMinimumApplicationAddress;
		minTestAddr += (sysInfo.dwAllocationGranularity) >>1
		std::stringstream sstr;
		sstr << "Max: " << std::hex << maxTestAddr;
		DebugLog(sstr.str());
		sstr.str("");
		sstr << "Min: " << std::hex << minTestAddr;
		DebugLog(sstr.str());

		ULONG_PTR testAlignment = sysInfo.dwAllocationGranularity;
		sstr.str("");
		sstr << "Align: " << std::hex << testAlignment;
		DebugLog(sstr.str());
		*/

		// Microsofts docs for some of these are a bit suspicious, so I manually tested to see what the exact requirements of these are
		// (as of 2 March 2024)
		// alignment has to be a multiple of dwAllocationGranularity
		// set alignment to 0 for auto which should default to dwAllocationGranularity (AKA the minimum supported alignment)
		addressReqs.Alignment = 0;
		// Low address has to be above lpMinimumApplicationAddress and rounded to a multiple of dwAllocationGranularity
		addressReqs.LowestStartingAddress = (PVOID)0x7ff6cee30000;
		//addressReqs.LowestStartingAddress = (PVOID)sysInfo.lpMinimumApplicationAddress;
		// High address has to be below lpMaximumApplicationAddress and rounded to a multiple of dwPageSize
		addressReqs.HighestEndingAddress =  (PVOID)0x7ff8aee30FFF;// sysInfo.lpMaximumApplicationAddress;
		//addressReqs.HighestEndingAddress = (PVOID)sysInfo.lpMaximumApplicationAddress;
		param.Type = MemExtendedParameterAddressRequirements;
		param.Pointer = &addressReqs;

		ULONG_PTR allocateSize = sysInfo.dwPageSize;//1;// sysInfo.dwAllocationGranularity >> 2;

		void* alloc = VirtualAlloc2Ptr(
			nullptr, nullptr,
			// TODO should we allocate dwAllocationGranularity? read up on committing vs reserving and whether the OS actually allocates all pages in our allocated block
			// ^ looking at the raw memory, unallocated pages within the allocation granularity aren't actually mapped, so maybe it's not a problem?
			// Value has to be >= 1, but will be rounded up to a multiple of dwPageSize
			allocateSize,
			MEM_RESERVE | MEM_COMMIT,
			PAGE_EXECUTE_READWRITE,
			&param, 1);

		std::stringstream sstr;
		sstr << "Alloc1: " << std::hex << (ULONG_PTR)alloc;
		DebugLog(sstr.str());


		/*
		* 
		VirtualAlloc2Ptr(
		nullptr, nullptr,
		paddedSize,
		MEM_RESERVE | MEM_COMMIT,
		PAGE_EXECUTE_READWRITE,
		&param, 1);
		*/
		if (alloc == nullptr)
		{
			ErrorLog("TEST FAILED");
			DebugLog(GetLastErrorStdStr());
		}
		else
		{
			sstr.str("");
			sstr << "Addr: " << std::hex << (ULONG_PTR)alloc;
			DebugLog(sstr.str());
			DebugLog("TEST SUCCEEDED!!!!");
		}
#endif
		return;
	}
	else
	{
		// else, need to use old implementation
		DebugLog("Warning: could not load `VirtualAlloc2()` - old memory allocator must be used");
	}

}


// Returns true if we can do a relative jmp between
inline bool Escort::IsNear(void* ptr1, void* ptr2)
{
	return abs(int64_t((uint8_t*)ptr1 - (uint8_t*)ptr2)) < 0x7F000000;
}

static size_t roundUp(size_t numToRound, size_t multiple)
{
	return ((numToRound + multiple - 1) / multiple) * multiple;
}

// TODO return how much memory is allocated?
static void* VirtualAllocNear(void* target, size_t allocSize)
{
	MEM_ADDRESS_REQUIREMENTS addressReqs = { 0 };
	MEM_EXTENDED_PARAMETER param = { 0 };
	ULONG_PTR targetAddr = (ULONG_PTR)target;
	// +-2GB so we can use relative 32-bit jmps
	ULONG_PTR lowestAddr = (targetAddr - 0x70000000ull);
	ULONG_PTR highestAddr = (targetAddr + 0x70000000ull);
	// round to a multiple of dwAllocationGranularity
	lowestAddr = lowestAddr - (lowestAddr % sysInfo.dwAllocationGranularity);
	// for some reason this needs to end in 0xFFF I add an extra F for luck :)
	highestAddr = highestAddr - (highestAddr % sysInfo.dwAllocationGranularity) + 0xFFFF;
	// handle underflow
	if (lowestAddr > targetAddr || lowestAddr < (ULONG_PTR)sysInfo.lpMinimumApplicationAddress)
		lowestAddr = (ULONG_PTR)sysInfo.lpMinimumApplicationAddress;
	// handle overflow
	if (highestAddr < targetAddr || highestAddr > (ULONG_PTR)sysInfo.lpMaximumApplicationAddress)
		highestAddr = (ULONG_PTR)sysInfo.lpMaximumApplicationAddress;
	/*
	if (highestAddr > (ULONG_PTR)sysInfo.lpMaximumApplicationAddress)
		ErrorLog("HIGHEST ADDR TOO HIGH");
	if (lowestAddr < (ULONG_PTR)sysInfo.lpMinimumApplicationAddress)
		ErrorLog("LOWEST ADDR TOO LOW");
	std::stringstream sstr;
	sstr << "alloc size: " << allocSize << " target: " << std::hex << targetAddr << " lowest: " << lowestAddr << " highest: " << highestAddr;
	DebugLog(sstr.str());
		*/

	// Microsofts docs for some of these are a bit suspicious, so I manually tested to see what the exact requirements/restrictions these have
	// (as of 2 March 2024)
	// alignment has to be a multiple of dwAllocationGranularity
	// set alignment to 0 for auto which should default to dwAllocationGranularity (AKA the minimum supported alignment)
	addressReqs.Alignment = sysInfo.dwAllocationGranularity;
	// Low address has to be above lpMinimumApplicationAddress and rounded to a multiple of dwAllocationGranularity
	addressReqs.LowestStartingAddress = (PVOID)lowestAddr;
	// High address has to be below lpMaximumApplicationAddress and rounded to a multiple of dwPageSize
	addressReqs.HighestEndingAddress = (PVOID)highestAddr;
	param.Type = MemExtendedParameterAddressRequirements;
	param.Pointer = &addressReqs;

	void* addr = VirtualAlloc2Ptr(
		nullptr, nullptr,
		// TODO should we allocate dwAllocationGranularity? read up on committing vs reserving and whether the OS actually allocates all pages in our allocated block
		// ^ looking at the raw memory, unallocated pages within the allocation granularity aren't actually mapped, so maybe it's not a problem?
		// TODO another workaround would be to just try and allocate more pages within the remaining pages in the allocated area
		// Value has to be >= 1, but will be rounded up to a multiple of dwPageSize
		sysInfo.dwPageSize,
		MEM_RESERVE | MEM_COMMIT,
		PAGE_EXECUTE_READWRITE,
		&param, 1);
	if (addr == nullptr)
	{
		ErrorLog("VirtualAlloc2: " + GetLastErrorStdStr());
	}
	return addr;

	/* wokring
		MEM_ADDRESS_REQUIREMENTS addressReqs = { 0 };
		MEM_EXTENDED_PARAMETER param = { 0 };

		addressReqs.Alignment = allocationGranularity;
		addressReqs.HighestEndingAddress = (PVOID)(ULONG_PTR)0x7fffffff;

		param.Type = MemExtendedParameterAddressRequirements;
		param.Pointer = &addressReqs;

		void* alloc = VirtualAlloc2Ptr(
			nullptr, nullptr,
			allocationGranularity,
			MEM_RESERVE | MEM_COMMIT,
			PAGE_EXECUTE_READWRITE,
			&param, 1);
			*/

	/*
	MEM_ADDRESS_REQUIREMENTS addressReqs = { 0 };
	MEM_EXTENDED_PARAMETER param = { 0 };
	// +-2GB so we can use relative 32-bit jmps
	ULONG_PTR targetAddr = (ULONG_PTR)target;
	ULONG_PTR lowestAddr = (targetAddr-0x7F000000ull);
	// handle underflow
	if (lowestAddr > targetAddr)
		lowestAddr = 0;
	ULONG_PTR highestAddr = (targetAddr+0x7F000000ull);
	// handle overflow
	if (highestAddr < targetAddr)
		highestAddr = std::numeric_limits<ULONG_PTR>::max();
	//addressReqs.LowestStartingAddress = (PVOID)lowestAddr;
	addressReqs.HighestEndingAddress = (PVOID)highestAddr;
	// TODO is this supposed to be dwAllocationGranularity or dwPageSize ?
	addressReqs.Alignment = sysInfo.dwAllocationGranularity;

	param.Type = MemExtendedParameterAddressRequirements;
	param.Pointer = &addressReqs;

	size_t paddedSize = sysInfo.dwAllocationGranularity;// roundUp(allocSize, allocationGranularity);
	std::stringstream sstr;
	sstr << "Alloc start: " << std::hex << lowestAddr
		<< " Alloc end: " << std::hex << highestAddr
		<< " Padded size: " << paddedSize;
	DebugLog(sstr.str());
	return VirtualAlloc2Ptr(
		nullptr, nullptr,
		paddedSize,
		MEM_RESERVE | MEM_COMMIT,
		PAGE_EXECUTE_READWRITE,
		&param, 1);
		*/
}

// Internal function for allocating RWX memory within 2GB of an address so you can use 32-bit jmp/call/etc instructions at the target location
void* Escort::AllocateRWXPageNear(void* targetAddr, size_t allocSize)
{
	// On Windows 10+ VirtualAlloc2 can deterministically handle this
	if (VirtualAlloc2Ptr != nullptr)
	{
		void* alloc = VirtualAllocNear(targetAddr, allocSize);
		if (alloc == nullptr)
		{
			ErrorLog("VirtualAlloc2 failed to allocate near memory, reverting to old implementation");
			ErrorLog(GetLastErrorStdStr());
		}
		else if (!IsNear(alloc, targetAddr))
		{
			ErrorLog("VirtualAlloc2 allocated memory that wasn't near, reverting to old implementation");
			ErrorLog(GetLastErrorStdStr());
			VirtualFree(alloc, 0, MEM_RELEASE);
		}
		else
		{
			// Success!
			return alloc;
		}
	}

	// THis is the painful branch...
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
			// For whatever reason, the number of iterations needed here went up an order of magnitude in 1.0.65
			if (i > 1000)
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
	// "If the function succeeds, the return value is nonzero"
	bool success = VirtualProtect(destAddr, count, PAGE_EXECUTE_READWRITE, &oldProtect);
	if (!success)
	{
		DWORD error = GetLastError();
		std::string errorMsg = "Protection change failed! " + std::to_string((long long)error);
		ErrorLog(errorMsg.c_str());
	}
	memcpy(destAddr, sourceAddr, count);
	// revert protection
	DWORD oldProtect2;
	success = VirtualProtect(destAddr, count, oldProtect, &oldProtect2);
	if (!success)
	{
		DWORD error = GetLastError();
		std::string errorMsg = "Protection revert failed! " + std::to_string((long long)error);
		ErrorLog(errorMsg.c_str());
	}
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
	bool success = VirtualProtect(sourceAddr, hookSize, PAGE_EXECUTE_READWRITE, &oldProtect);
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