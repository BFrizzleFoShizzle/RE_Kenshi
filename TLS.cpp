#include "TLS.h"

#include <boost/thread/thread_guard.hpp>
#include <vector>

#include <iostream>

// take this if you're accessing allocatedSlotIdxs
static boost::mutex TLSGCLock;
// what it says on the tin - used for GC
static std::vector<DWORD> allocatedSlotIdxs;

// in-thread API
// returns null if not set
TLS::GCTLSObj* TLS::GetThreadSlotGeneric(DWORD index)
{
	return (GCTLSObj*)TlsGetValue(index);
};

void TLS::SetThreadSlotGeneric(DWORD index, TLS::GCTLSObj* value)
{
	TlsSetValue(index, value);
}

// does nothing if the slot is alrea
void TLS::AllocateSlotIfNeeded(DWORD& slot)
{
    // this read isn't thread-safe
    if (slot == TLS_OUT_OF_INDEXES)
    {
        std::cout << "Allocating" << std::endl;
        boost::lock_guard<boost::mutex> lock(TLSGCLock);
        // read again after we've taken the lock to ensure we NEVER allocate a slot twice
        if (slot == TLS_OUT_OF_INDEXES)
        {
            slot = TlsAlloc();
            // TODO better error-checking
            if (slot == TLS_OUT_OF_INDEXES)
                allocatedSlotIdxs.push_back(slot);
        }
    }
}

// TLS garbage collection
// for the current thread, deletes all allocated TLS objects and sets all slots to nullptr
static void TLSGC()
{
    std::cout << "GC" << std::endl;
    boost::lock_guard<boost::mutex> lock(TLSGCLock);
    for (size_t i = 0; i < allocatedSlotIdxs.size(); ++i)
    {
        // this calls the virtual destructor for implementation-specific cleanup
        delete TLS::GetThreadSlotGeneric(allocatedSlotIdxs[i]);
        TLS::SetThreadSlotGeneric(allocatedSlotIdxs[i], nullptr);
    }
}

// weird having the DllMain here, but this is the only code that needs it
BOOL WINAPI DllMain(HINSTANCE hinstDLL,
    DWORD fdwReason,
    LPVOID lpvReserved)
{
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        // slots are allocated via static initializers which will run before this
        break;

    case DLL_THREAD_ATTACH:
        // we have to do late initialization because DLL_THREAD_ATTACH isn't
        // triggered for threads that were created before our DLL is loaded
        break;

    case DLL_THREAD_DETACH:
        TLSGC();
        break;

    case DLL_PROCESS_DETACH:
        TLSGC();
        break;

    default:
        break;
    }

    return TRUE;
    UNREFERENCED_PARAMETER(hinstDLL);
    UNREFERENCED_PARAMETER(lpvReserved);
}