#include "io.h"

#include <Debug.h>

#define _WIN32_DCOM
#include <comdef.h>
#include <Wbemidl.h>
#include <sstream>
#include <unordered_map>
#include <boost/filesystem.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/lock_guard.hpp>

#pragma comment(lib, "wbemuuid.lib")

// cache because the query(s) for checking a drive take like 250ms
static std::unordered_map<char, IO::DriveType> typeCache;
static boost::mutex driveTypeMtx;
static bool securityIsInitialized = false;

// sets up COM stuff, which has to be done ASAP
// TODO this would be safer to do in a hook
void IO::Init()
{
    boost::lock_guard<boost::mutex> lock(driveTypeMtx);
    // NOTE: The rest of this function is very slow, takes maybe 250ms
    HRESULT hres;

    // Step 1: --------------------------------------------------
    // Initialize COM. ------------------------------------------

    hres = CoInitializeEx(0, COINIT_MULTITHREADED);

    // Step 2: --------------------------------------------------
    // Set general COM security levels --------------------------
    // NOTE: this has to be called GLOBALLY AT MOST ONCE FOR A PROCESS
    // DOUBLE NOTE: if you don't do this early enough, combase will call it for you and cause problems
    if (!securityIsInitialized)
    {
        hres = CoInitializeSecurity(
            NULL,
            -1,                          // COM authentication
            NULL,                        // Authentication services
            NULL,                        // Reserved
            RPC_C_AUTHN_LEVEL_DEFAULT,   // Default authentication 
            RPC_C_IMP_LEVEL_IMPERSONATE, // Default Impersonation  
            NULL,                        // Authentication info
            EOAC_NONE,                   // Additional capabilities 
            NULL                         // Reserved
        );

        if (FAILED(hres))
        {
            std::stringstream error;
            error << "Failed to initialize security. Error code = 0x"
                << std::hex << hres;
            ErrorLog(error.str());
            CoUninitialize();
            return;                    // Program has failed.
        }
        securityIsInitialized = true;
    }

    CoUninitialize();
}

// inspired by https://learn.microsoft.com/en-us/windows/win32/wmisdk/example--getting-wmi-data-from-the-local-computer
// this function is really unsafe and problematic, but the main body is currnently only used ONCE for ONE FILE, so hopefully it shouldn't cause problems
IO::DriveType IO::GetDriveStorageType(std::string path)
{
    char driveLetter = toupper(boost::filesystem::absolute(path).string()[0]);
    std::string driveLetterStr = std::string(&driveLetter, 1);

    // HACK technically, this is unsafe without a lock
    if (typeCache.find(driveLetter) != typeCache.end())
        return typeCache[driveLetter];

    boost::lock_guard<boost::mutex> lock(driveTypeMtx);

    // check again after taking lock
    if (typeCache.find(driveLetter) != typeCache.end())
        return typeCache[driveLetter];

    // NOTE: The rest of this function is very slow, takes maybe 250ms
    HRESULT hres;

    // Step 1: --------------------------------------------------
    // Initialize COM. ------------------------------------------

    hres = CoInitializeEx(0, COINIT_MULTITHREADED);

    // Step 2: --------------------------------------------------
    // Set general COM security levels --------------------------
    // NOTE: this has to be called GLOBALLY AT MOST ONCE FOR A PROCESS
    // DOUBLE NOTE: if you don't do this early enough, combase will call it for you and cause problems
    if (!securityIsInitialized)
    {
        hres = CoInitializeSecurity(
            NULL,
            -1,                          // COM authentication
            NULL,                        // Authentication services
            NULL,                        // Reserved
            RPC_C_AUTHN_LEVEL_DEFAULT,   // Default authentication 
            RPC_C_IMP_LEVEL_IMPERSONATE, // Default Impersonation  
            NULL,                        // Authentication info
            EOAC_NONE,                   // Additional capabilities 
            NULL                         // Reserved
        );

        if (FAILED(hres))
        {
            std::stringstream error;
            error << "Failed to initialize security. Error code = 0x"
                << std::hex << hres;
            ErrorLog(error.str());
            CoUninitialize();
            return FAIL;                    // Program has failed.
        }
        securityIsInitialized = true;
    }

    // Step 3: ---------------------------------------------------
    // Obtain the initial locator to WMI -------------------------

    IWbemLocator* pLoc = NULL;

    // NOTE: this throws an error if it gets called on the DllMain thread
    hres = CoCreateInstance(
        CLSID_WbemLocator,
        0,
        CLSCTX_INPROC_SERVER,
        IID_IWbemLocator, (LPVOID*)&pLoc);

    if (FAILED(hres))
    {
        std::stringstream err;
        err << "Failed to create IWbemLocator object."
            << " Err code = 0x"
            << std::hex << hres;
        CoUninitialize();
        return FAIL;                 // Program has failed.
    }

    // Step 4: -----------------------------------------------------
    // Connect to WMI through the IWbemLocator::ConnectServer method

    IWbemServices* pSvc = NULL;

    // Connect to the root\cimv2 namespace with
    // the current user and obtain pointer pSvc
    // to make IWbemServices calls.
    hres = pLoc->ConnectServer(
        _bstr_t(L"ROOT\\microsoft\\windows\\storage"), // Object path of WMI namespace
        NULL,                    // User name. NULL = current user
        NULL,                    // User password. NULL = current
        0,                       // Locale. NULL indicates current
        NULL,                    // Security flags.
        0,                       // Authority (for example, Kerberos)
        0,                       // Context object 
        &pSvc                    // pointer to IWbemServices proxy
    );

    if (FAILED(hres))
    {
        std::stringstream err;
        err << "Could not connect. Error code = 0x"
            << std::hex << hres;
        ErrorLog(err.str());
        pLoc->Release();
        CoUninitialize();
        return FAIL;                // Program has failed.
    }

    // Step 5: --------------------------------------------------
    // Set security levels on the proxy -------------------------

    hres = CoSetProxyBlanket(
        pSvc,                        // Indicates the proxy to set
        RPC_C_AUTHN_WINNT,           // RPC_C_AUTHN_xxx
        RPC_C_AUTHZ_NONE,            // RPC_C_AUTHZ_xxx
        NULL,                        // Server principal name 
        RPC_C_AUTHN_LEVEL_CALL,      // RPC_C_AUTHN_LEVEL_xxx 
        RPC_C_IMP_LEVEL_IMPERSONATE, // RPC_C_IMP_LEVEL_xxx
        NULL,                        // client identity
        EOAC_NONE                    // proxy capabilities 
    );

    if (FAILED(hres))
    {
        std::stringstream err;
        err << "Could not set proxy blanket. Error code = 0x"
            << std::hex << hres;
        ErrorLog(err.str());
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return FAIL;               // Program has failed.
    }

    // Step 6: --------------------------------------------------
    // Use the IWbemServices pointer to make requests of WMI ----

    // For example, get the name of the operating system
    IEnumWbemClassObject* pEnumerator = NULL;
    hres = pSvc->ExecQuery(
        bstr_t("WQL"),
        bstr_t(("SELECT UniqueId FROM MSFT_Partition WHERE DriveLetter = '" + driveLetterStr + "'").c_str()),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL,
        &pEnumerator);

    if (FAILED(hres))
    {
        std::stringstream err;
        err << "Query for drive letter partition ID failed."
            << " Error code = 0x"
            << std::hex << hres << std::endl;
        ErrorLog(err.str());
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return FAIL;               // Program has failed.
    }

    // Step 7: -------------------------------------------------
    // Get the data from the query in step 6 -------------------

    IWbemClassObject* pclsObj = NULL;
    ULONG uReturn = 0;

    if (!pEnumerator)
    {
        ErrorLog("MSFT_Partition iterator null");
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return FAIL;
    }

    HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);

    // there should be at least one value
    if (uReturn == 0)
    {
        ErrorLog("MSFT_Partition iterator has no items");
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return FAIL;
    }

    VARIANT vtProp;
    VariantInit(&vtProp);
    // Get the value of the Name property
    hr = pclsObj->Get(bstr_t("UniqueId"), 0, &vtProp, 0, 0);
    std::wstring UUID = vtProp.bstrVal;
    UUID = UUID.substr(UUID.find_last_of('}') + 1);
    VariantClear(&vtProp);

    hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);

    // we expect EXACTLY ONE RESULT
    if (uReturn == 1)
    {
        ErrorLog("MSFT_Partition iterator has more than one item");
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return FAIL;
    }

    // Step 8: --------------------------------------------------
    // Do it all again to find the drive type -------------------
    // For example, get the name of the operating system

    hres = pSvc->ExecQuery(
        bstr_t("WQL"),
        bstr_t((L"SELECT MediaType FROM MSFT_PhysicalDisk WHERE UniqueId = '" + UUID + L"'").c_str()),
        //bstr_t("SELECT * FROM MSFT_PhysicalDisk"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL,
        &pEnumerator);

    if (FAILED(hres))
    {
        std::stringstream err;
        err << "Query for physical disk type failed."
            << " Error code = 0x"
            << std::hex << hres << std::endl;
        ErrorLog(err.str());
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return FAIL;               // Program has failed.
    }

    // Step 9: -------------------------------------------------
    // Get the data from the query in step 8 -------------------

    pclsObj = NULL;
    uReturn = 0;

    if (!pEnumerator)
    {
        ErrorLog("MSFT_PhysicalDisk iterator null");
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return FAIL;
    }

    hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);

    // there should be at least one value
    if (uReturn == 0)
    {
        ErrorLog("MSFT_PhysicalDisk iterator has no items");
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return FAIL;
    }

    VariantInit(&vtProp);
    // Get the value of the Name property
    hr = pclsObj->Get(L"MediaType", 0, &vtProp, 0, 0);
    short diskType = vtProp.iVal;
    VariantClear(&vtProp);

    hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);

    // we expect EXACTLY ONE RESULT
    if (uReturn == 1)
    {
        ErrorLog("MSFT_Partition iterator has more than one item");
        pSvc->Release();
        pLoc->Release();
        CoUninitialize();
        return FAIL;
    }


    // Cleanup
    // ========

    pclsObj->Release();
    pSvc->Release();
    pLoc->Release();
    pEnumerator->Release();
    CoUninitialize();

    DriveType driveType = (DriveType)diskType;

    typeCache[driveLetter] = driveType;

    return driveType;
}

/*
// OLD CODE - SLOW AS HELL
#include <cstdio>
#include <string>
#include <assert.h>
// Adapted from https://stackoverflow.com/questions/478898/how-do-i-execute-a-command-and-get-the-output-of-the-command-within-c-using-po
static std::string exec(std::string cmd) 
{
    char buffer[128];
    std::string result = "";
    FILE* pipe = _popen(cmd.c_str(), "r");
    if (!pipe)
        return "POPEN_ERROR";
    try 
    {
        while (fgets(buffer, sizeof buffer, pipe) != NULL) 
        {
            result += buffer;
        }
    }
    catch (...) 
    {
        _pclose(pipe);
        throw;
    }
    _pclose(pipe);
    return result;
}
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
// taken from https://stackoverflow.com/questions/478898/how-do-i-execute-a-command-and-get-the-output-of-the-command-within-c-using-po
//
// Execute a command and get the results. (Only standard output)
//
std::string ExecCmd(std::string cmd)
{
    std::string strResult;
    HANDLE hPipeRead, hPipeWrite;

    SECURITY_ATTRIBUTES saAttr = { sizeof(SECURITY_ATTRIBUTES) };
    saAttr.bInheritHandle = TRUE; // Pipe handles are inherited by child process.
    saAttr.lpSecurityDescriptor = NULL;

    // Create a pipe to get results from child's stdout.
    if (!CreatePipe(&hPipeRead, &hPipeWrite, &saAttr, 0))
        return strResult;

    STARTUPINFOA si = { sizeof(STARTUPINFOA) };
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.hStdOutput = hPipeWrite;
    si.hStdError = hPipeWrite;
    si.wShowWindow = SW_HIDE; // Prevents cmd window from flashing.
                              // Requires STARTF_USESHOWWINDOW in dwFlags.

    PROCESS_INFORMATION pi = { 0 };

    BOOL fSuccess = CreateProcessA(NULL, (LPSTR)cmd.c_str(), NULL, NULL, TRUE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi);
    if (!fSuccess)
    {
        CloseHandle(hPipeWrite);
        CloseHandle(hPipeRead);
        return strResult;
    }

    bool bProcessEnded = false;
    for (; !bProcessEnded;)
    {
        // Give some timeslice (50 ms), so we won't waste 100% CPU.
        bProcessEnded = WaitForSingleObject(pi.hProcess, 50) == WAIT_OBJECT_0;

        // Even if process exited - we continue reading, if
        // there is some data available over pipe.
        for (;;)
        {
            char buf[1024];
            DWORD dwRead = 0;
            DWORD dwAvail = 0;

            if (!::PeekNamedPipe(hPipeRead, NULL, 0, NULL, &dwAvail, NULL))
                break;

            if (!dwAvail) // No data available, return
                break;

            if (!::ReadFile(hPipeRead, buf, min(sizeof(buf) - 1, dwAvail), &dwRead, NULL) || !dwRead)
                // Error, the child process might ended
                break;

            buf[dwRead] = 0;
            strResult += buf;
        }
    } //for

    CloseHandle(hPipeWrite);
    CloseHandle(hPipeRead);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return strResult;
} //ExecCmd


std::string IO::GetMediaType(std::string filePath)
{
    // Extract drive letter
    assert(isalpha(filePath[0]));
    assert(filePath[1] == ':');
    std::string driveLetter = filePath.substr(0, 1);

    // Powershell blob for checking the type of a drive
    std::string powershellCommand = "(Get-PhysicalDisk -UniqueId ((Get-Partition -DriveLetter " + driveLetter + " | Select UniqueId).UniqueId.split('}')[1]) | Select MediaType).MediaType";

    std::string output = exec("powershell.exe -windowstyle hidden \"" + powershellCommand + "\"");

    return output.substr(0, output.find("\n"));
}

*/