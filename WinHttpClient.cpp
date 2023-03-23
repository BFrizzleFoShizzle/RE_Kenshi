/**
 *  Copy right 2008 Cheng Shi.  All rights reserved.
 */

// From https://www.codeproject.com/Articles/28275/A-Simple-Windows-HTTP-Wrapper-Using-C
// Originally LGPLv3

#include "WinHttpClient.h"
#include <comutil.h>
#include <sstream>
#include <iomanip>

WinHttpClient::WinHttpClient(const wstring &URL)
    : m_URL(URL),
      m_HttpResponseHeader(L""),
      m_HttpResponse(L""),
      m_charset(L""),
      m_host(L"")
{
}

// from https://stackoverflow.com/questions/154536/encode-decode-urls-in-c
std::string WinHttpClient::UrlEncode(std::string str)
{
    ostringstream escaped;
    escaped.fill('0');
    escaped << hex;

    for (string::const_iterator i = str.begin(), n = str.end(); i != n; ++i) {
        string::value_type c = (*i);

        // Keep alphanumeric and other accepted characters intact
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
            continue;
        }

        // Any other characters are percent-encoded
        escaped << uppercase;
        escaped << '%' << std::setw(2) << int((unsigned char)c);
        escaped << nouppercase;
    }

    return escaped.str();
}

bool WinHttpClient::SendHttpRequest(const wstring &httpVerb, const wstring& httpRequestHeader, const string& httpRequestData)
{
    // Make verb uppercase.
    wstring verb = httpVerb;
    if (wcsicmp(verb.c_str(), L"GET") == 0)
    {
        verb = L"GET";
    }
    else if (wcsicmp(verb.c_str(), L"POST") == 0)
    {
        verb = L"POST";
    }
    else
    {
        return false;
    }
    bool bRetVal = true;
    HINTERNET hSession = NULL;
    hSession = ::WinHttpOpen(SZ_AGENT,  
                             WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                             WINHTTP_NO_PROXY_NAME, 
                             WINHTTP_NO_PROXY_BYPASS,
                             0);
    if (hSession == NULL)
    {
        return false;
    }

    wchar_t szHostName[MAX_PATH] = L"";
    wchar_t szURLPath[MAX_PATH * 5] = L"";
    URL_COMPONENTS urlComp;
    memset(&urlComp, 0, sizeof(urlComp));
    urlComp.dwStructSize = sizeof(urlComp);
    urlComp.lpszHostName = szHostName;
    urlComp.dwHostNameLength = MAX_PATH;
    urlComp.lpszUrlPath = szURLPath;
    urlComp.dwUrlPathLength = MAX_PATH * 5;
    urlComp.dwSchemeLength = -1;

    if (::WinHttpCrackUrl(m_URL.c_str(), m_URL.size(), 0, &urlComp))
    {
        m_host = szHostName;
        HINTERNET hConnect = NULL;
        hConnect = ::WinHttpConnect(hSession, szHostName, urlComp.nPort, 0);
        if (hConnect != NULL)
        {
            DWORD dwOpenRequestFlag = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
            HINTERNET hRequest = NULL;
            hRequest = ::WinHttpOpenRequest(hConnect,
                                            verb.c_str(),
                                            urlComp.lpszUrlPath,
                                            NULL,
                                            WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES,
                                            dwOpenRequestFlag);
            if (hRequest != NULL)
            {
                bool bGetReponseSucceed = false;
                int iRetryTimes = 0;

                // Retry for several times if fails.
                while (!bGetReponseSucceed && iRetryTimes++ < INT_RETRYTIMES)
                {
                    // Send HTTP requeest.
                    LPVOID requestData = WINHTTP_NO_REQUEST_DATA;
                    size_t requestDataLength = 0;
                    if (httpRequestData != "")
                    {
                        requestData = (LPVOID)httpRequestData.c_str();
                        requestDataLength = httpRequestData.size();
                    }
                    LPWSTR requestHeaders = WINHTTP_NO_ADDITIONAL_HEADERS;
                    size_t requestHeadersLength = 0;
                    if (httpRequestHeader != L"")
                    {
                        requestHeaders = (LPWSTR)httpRequestHeader.c_str();
                        requestHeadersLength = -1;
                    }
                    if (::WinHttpSendRequest(hRequest,
                                             requestHeaders,
                                             requestHeadersLength,
                                             requestData,
                                             requestDataLength,
                                             requestDataLength,
                                             NULL))
                    {
                        if (::WinHttpReceiveResponse(hRequest, NULL))
                        {
                            DWORD dwStatusCode = 0;
                            DWORD dwSize = sizeof(DWORD);

                            // get the status code (to handle 204 no data)
                            BOOL bResult = ::WinHttpQueryHeaders(hRequest,
                                WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                WINHTTP_HEADER_NAME_BY_INDEX,
                                &dwStatusCode,
                                &dwSize,
                                WINHTTP_NO_HEADER_INDEX);

                            // "no data" - success
                            if (dwStatusCode == 204)
                            {
                                bGetReponseSucceed = true;
                                break;
                            }

                            // Get the buffer size of the HTTP response header.
                            bResult &= ::WinHttpQueryHeaders(hRequest,
                                                                 WINHTTP_QUERY_RAW_HEADERS_CRLF,
                                                                 WINHTTP_HEADER_NAME_BY_INDEX,
                                                                 NULL,
                                                                 &dwSize,
                                                                 WINHTTP_NO_HEADER_INDEX);

                            if (bResult || (!bResult && (::GetLastError() == ERROR_INSUFFICIENT_BUFFER)))
                            {
                                wchar_t *szHeader = new wchar_t[dwSize];
                                if (szHeader != NULL)
                                {
                                    memset(szHeader, 0, dwSize* sizeof(wchar_t));

                                    // Get HTTP response header.
                                    if (::WinHttpQueryHeaders(hRequest,
                                                              WINHTTP_QUERY_RAW_HEADERS_CRLF,
                                                              WINHTTP_HEADER_NAME_BY_INDEX,
                                                              szHeader,
                                                              &dwSize,
                                                              WINHTTP_NO_HEADER_INDEX))
                                    {
                                        m_HttpResponseHeader.assign(szHeader);
                                        _wcslwr(szHeader);
                                        wstring lwrHeader = szHeader;

                                        // Try to get charset from HTTP reponse header.
                                        int iCharsetIndex = lwrHeader.find(L"charset=");
                                        if (iCharsetIndex != wstring::npos)
                                        {
                                            iCharsetIndex += 8;     // "charset=" has 8 characters.
                                            int iCharsetLength = 0;
                                            for (int i = iCharsetIndex; i < lwrHeader.size(); i++)
                                            {
                                                if (i == lwrHeader.size()-1
                                                    || lwrHeader[i] == ' '
                                                    || lwrHeader[i] == ';'
                                                    || lwrHeader[i] == '\n')
                                                {
                                                    iCharsetLength = i - iCharsetIndex;
                                                    break;
                                                }
                                            }
                                            m_charset = lwrHeader.substr(iCharsetIndex, iCharsetLength);
                                        }
                                        delete[] szHeader;

                                        wstring resource = L"";
                                        do
                                        {
                                            dwSize = 0;
                                            if (::WinHttpQueryDataAvailable(hRequest, &dwSize))
                                            {
                                                BYTE *pResponse = new BYTE[dwSize];
                                                if (pResponse != NULL && dwSize > 0)
                                                {
                                                    memset(pResponse, 0, dwSize);
                                                    DWORD dwRead = 0;
                                                    if (::WinHttpReadData(hRequest,
                                                                          pResponse,
                                                                          dwSize,
                                                                          &dwRead))
                                                    {
                                                        // If the web page is encoded in UTF-8, it needs to be takn care of or garbage characters in the string.
                                                        if (m_charset.find(L"utf-8") != wstring::npos)
                                                        {
                                                            int iLength = ::MultiByteToWideChar(CP_UTF8,
                                                                                                MB_ERR_INVALID_CHARS, 
                                                                                                (LPCSTR)pResponse, 
                                                                                                dwSize, 
                                                                                                NULL, 
                                                                                                0);

                                                            if (iLength > 0)
                                                            {
                                                                wchar_t *wideChar = new wchar_t[iLength + 1];
                                                                if (wideChar != NULL)
                                                                {
                                                                    memset(wideChar, 0, iLength * sizeof(wchar_t));
                                                                    iLength = ::MultiByteToWideChar(CP_UTF8,
                                                                                                    MB_ERR_INVALID_CHARS, 
                                                                                                    (LPCSTR)pResponse, 
                                                                                                    dwSize, 
                                                                                                    wideChar, 
                                                                                                    iLength);
                                                                    if (iLength > 0)
                                                                    {
                                                                        wchar_t oldChar = wideChar[iLength];
                                                                        wideChar[iLength] = 0;
                                                                        resource.append(wideChar);
                                                                        wideChar[iLength] = oldChar;    // Hack.  Set back the original char or the delete operation leads a crash.
                                                                    }
                                                                    delete[] wideChar;
                                                                }
                                                            }
                                                        }
                                                        else
                                                        {
                                                            int iLength = ::MultiByteToWideChar(CP_ACP,
                                                                                                MB_PRECOMPOSED, 
                                                                                                (LPCSTR)pResponse, 
                                                                                                dwSize, 
                                                                                                NULL, 
                                                                                                0);

                                                            if (iLength > 0)
                                                            {
                                                                wchar_t *wideChar = new wchar_t[iLength + 1];
                                                                if (wideChar != NULL)
                                                                {
                                                                    memset(wideChar, 0, iLength * sizeof(wchar_t));
                                                                    iLength = ::MultiByteToWideChar(CP_ACP,
                                                                                                    MB_PRECOMPOSED, 
                                                                                                    (LPCSTR)pResponse, 
                                                                                                    dwSize, 
                                                                                                    wideChar, 
                                                                                                    iLength);
                                                                    if (iLength > 0)
                                                                    {
                                                                        wchar_t oldChar = wideChar[iLength];
                                                                        wideChar[iLength] = 0;
                                                                        resource.append(wideChar);
                                                                        wideChar[iLength] = oldChar;    // Hack.  Set back the original char or the delete operation leads a crash.
                                                                    }
                                                                    delete[] wideChar;
                                                                }
                                                            }
                                                        }
                                                    }
                                                    delete[] pResponse;
                                                }
                                            }
                                        }
                                        while (dwSize > 0);

                                        // The smallest web page must contain more than 1 characters.
                                        if (resource.size() > 1)
                                        {
                                            bGetReponseSucceed = true;
                                            m_HttpResponse = resource;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                if (!bGetReponseSucceed)
                {
                    bRetVal = false;
                }

                ::WinHttpCloseHandle(hRequest);
            }
            ::WinHttpCloseHandle(hConnect);
        }

    }

    ::WinHttpCloseHandle(hSession);

    return bRetVal;
}

wstring WinHttpClient::GetHttpResponseHeader(void)
{
    return m_HttpResponseHeader;
}

wstring WinHttpClient::GetHttpResponse(void)
{
    return m_HttpResponse;
}

wstring WinHttpClient::GetCharset(void)
{
    return m_charset;
}

wstring WinHttpClient::GetHost(void)
{
    return m_host;
}
