#include "emmcdl_new/utils.h"
#include <strsafe.h>
#include <cstring>

using namespace std;


#if defined(_WIN32) || defined(_WIN64)

#include <strsafe.h>
#include <windows.h>

string getErrorDescription(int status) {
    LPSTR buffer = nullptr;
    DWORD size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM | 
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        status,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&buffer,
        0,
        NULL
    );
    
    if (size == 0) {
        LPSTR message = (LPSTR)malloc(50 * sizeof(char));
        StringCchPrintfA(message, 50, "Unknown error (%d)", status);
        return message;
    }
    
    while (buffer[size - 1] == '\r' || buffer[size - 1] == '\n') {
        buffer[size - 1] = '\0';
        size--;
    }
    
    size_t length = strlen(buffer);
    
    LPSTR message = (LPSTR) malloc((length + 20) * sizeof(char));
    if (message == NULL) {
        LocalFree(buffer);
        return NULL;
    }
    
    HRESULT hr = StringCchCopyA(message, length + 1, buffer);

    if (FAILED(hr)) {
        free(message);
        LocalFree(buffer);
        return NULL;
    }
    
    hr = StringCchPrintfA(message + length, 20, " (%d)", status);
    if (FAILED(hr)) {
        free(message);
        LocalFree(buffer);
        return NULL;
    }
    
    LocalFree(buffer);
    return message;
}

char* StringReplace(char* inp, const char* find, const char* rep) {
    char tmpstr[2048];
    int max_len = 2048;
    char* tptr = tmpstr;
    char* sptr = strstr(inp, find);

    if (sptr != NULL) {
        strncpy_s(tptr, max_len, inp, (sptr - inp));
        max_len -= (sptr - inp);
        tptr += (sptr - inp);
        sptr += strlen(find);
        strncpy_s(tptr, max_len, rep, strlen(rep));
        max_len -= strlen(rep);
        tptr += strlen(rep);

        strncpy_s(tptr, max_len, sptr, strlen(sptr));

        strcpy_s(inp, 2048, tmpstr);
    }

    return inp;
}

#else

string getErrorDescription(int status) {
    string message = strerror(status);
    message += ("  (" + to_string(status) + ")");
    return message;
}

char* StringReplace(char* inp, const char* find, const char* rep) {
    char* result = inp;
    char* pos;
    size_t find_len = strlen(find);
    size_t rep_len = strlen(rep);
    
    while ((pos = strstr(result, find)) != NULL) {
        memmove(pos + rep_len, pos + find_len, strlen(pos + find_len) + 1);
        memcpy(pos, rep, rep_len);
        result = pos + rep_len;
    }
    
    return inp;
}

#endif
