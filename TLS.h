// bootleg thread-local storage library
// workaround for __declspec(thread) having issues with dynamically-loaded DLLs
// this only allocates ONE pointer/object per GCTLSObj type, which has a default value of nullptr
#include <stdint.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

// Main usage is like this:
//   class YourObj : public GCTLSObj
//   (Single-obj) typedef TLS::TLSSlot<YourObj> YourSlot
//   (Multi-obj)  typedef TLS::TLSSlot<YourObj, ObjIdx> YourSlot
//   DWORD YourSlot::TLSSlotIndex = TLS_OUT_OF_INDEXES;
//   YourSlot::SetPtr(new YourObj())
//   YourSlot::GetPtr()

namespace TLS
{
	// "Garbage-collected TLS object"
	class GCTLSObj
	{
	public:
		virtual ~GCTLSObj() {};
	};

	// usually used internally
	void AllocateSlotIfNeeded(DWORD &slot);
	void SetThreadSlotGeneric(DWORD index, GCTLSObj* obj);
	GCTLSObj* GetThreadSlotGeneric(DWORD index); // returns null if not set

	// usually used externally
	// A slot is uniquely identified by T and D, use unique D if you need multiple instances of the same GCTLSObj
	template<typename T, size_t D = -1>
	class TLSSlot
	{
	private:
		// NOTE: YOU MUST INITIALIZE THIS ELSEWHERE TO TLS_OUT_OF_INDEXES
		static DWORD TLSSlotIndex;
	public:
		static T* GetPtr() { AllocateSlotIfNeeded(TLSSlotIndex); return dynamic_cast<T*>(GetThreadSlotGeneric(TLSSlotIndex)); };
		static void SetPtr(T* obj) { AllocateSlotIfNeeded(TLSSlotIndex); SetThreadSlotGeneric(TLSSlotIndex, obj); };
	};
	// convenient auto-constructed singleton implementation - uses/requires default constructor
	template<typename T, size_t D = -1>
	class TLSSlotDefault : public TLSSlot<T, D>
	{
	public:
		static T* GetPtr() {
			T* ptr = TLSSlot<T, D>::GetPtr();
			// create TLS entry if needed
			if (ptr == nullptr)
			{
				ptr = new T();
				TLSSlot<T, D>::SetPtr(ptr);
			}
			return ptr;
		};
	};

}