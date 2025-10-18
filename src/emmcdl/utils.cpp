#include "emmcdl/utils.h"


using namespace std;


#if defined(_WIN32) || defined(_WIN64)

#include <strsafe.h>
#include <windows.h>

wstring getErrorDescription(int status) {
    LPWSTR buffer = nullptr;
    DWORD size = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM | 
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        status,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPWSTR)&buffer,
        0,
        NULL
    );
    
    if (size == 0) {
        LPWSTR message = (LPWSTR)malloc(50 * sizeof(WCHAR));
        StringCchPrintfW(message, 50, L"Unknown error (%d)", status);
        return message;
    }
    
    while (buffer[size - 1] == L'\r' || buffer[size - 1] == L'\n') {
        buffer[size - 1] = L'\0';
        size--;
    }
    
    // 实际字符串长度
    size_t length = wcslen(buffer);
    
    LPWSTR message = (LPWSTR) malloc((length + 20) * sizeof(WCHAR));
    if (message == NULL) {
        LocalFree(buffer);
        return NULL;
    }
    
    // HRESULT原型是long（其实就是int）
    HRESULT hr = StringCchCopyW(message, length + 1, buffer);

    if (FAILED(hr)) {
        free(message);
        LocalFree(buffer);
        return NULL;
    }
    
    hr = StringCchPrintfW(message + length, 20, L" (%d)", status);
    if (FAILED(hr)) {
        free(message);
        LocalFree(buffer);
        return NULL;
    }
    
    LocalFree(buffer);
    return message;
}

#else

wstring getErrorDescription(int status) {
    string message = strerror(status);
    message += ("  (" + to_string(status) + ")");
    return wstring(message.begin(), message.end());
}

#endif