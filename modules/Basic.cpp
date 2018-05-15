
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#if defined(_WIN32)
#include <varargs.h>
#include <windows.h>
#define DLLEXPORT __declspec(dllexport)
#else 
#include <stdarg.h>
#define DLLEXPORT 
#define vprintf_s vprintf
#endif

struct string {
    char *data;
    unsigned long size;
};

extern "C" DLLEXPORT void func()
{
    return;
}

int _strlen(const char *s)
{
    int r = 0;
    while (*s++) r++;
    return r;
}

extern "C" DLLEXPORT int print(char *data, unsigned long size, ...)
{
    DWORD dwRet;
    char buffer[256];

    va_list args;
    va_start(args, size);

    wvsprintf(buffer, data, args);
    WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), buffer, _strlen(buffer), &dwRet, 0);

    // int ret = vprintf_s(data, args);
    va_end(args);

    return dwRet;
}

#if defined(_WIN32)

extern "C" BOOL WINAPI DllMain(
    _In_ HINSTANCE hinstDLL,
    _In_ DWORD     fdwReason,
    _In_ LPVOID    lpvReserved
) 
{
    return TRUE;
}

#include "ProcessThreadsAPI.h"

extern "C" DLLEXPORT void end()
{
    print("Calling exit process now\n", 10);
    ExitProcess(0);
}
#endif 