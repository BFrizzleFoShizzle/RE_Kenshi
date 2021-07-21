#pragma once

#include <vector>
#include <stdint.h>

namespace Escort
{
	void Hook(void* sourceAddr, void* targetAddr, size_t replacedBytes);
	void PushRetHook(void* sourceAddr, void* targetAddr, size_t replacedBytes);
	void CallRAXHook(void* sourceAddr, void* targetAddr, size_t replacedBytes);
	void NOP(void* codeAddr, size_t replacedBytes);
	// ASM methods, mostly used internally
	void PushRetHookASM(void* sourceAddr, void* targetAddr, size_t replacedBytes);
	void Push64ASM(void* value, std::vector<uint8_t> &bytes);
	void NOPASM(size_t replacedBytes, std::vector<uint8_t>& bytes);
	void RetASM(std::vector<uint8_t>& bytes);
	void PushRAX(std::vector<uint8_t>& bytes);
	void PopRAX(std::vector<uint8_t>& bytes);
	void MovAbsRAX(void* value, std::vector<uint8_t>& bytes);
	void CallRAX(std::vector<uint8_t>& bytes);
	void WriteProtected(void* sourceAddr, void* destAddr, size_t count);
}