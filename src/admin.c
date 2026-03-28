#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include "admin.h"

/* Link with shell32.lib */
#pragma comment(lib, "shell32.lib")

BOOL is_admin(void)
{
    BOOL result = FALSE;
    HANDLE token = NULL;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
        return FALSE;

    TOKEN_ELEVATION elev;
    DWORD cb = sizeof(elev);
    if (GetTokenInformation(token, TokenElevation, &elev, sizeof(elev), &cb)) {
        result = (BOOL)elev.TokenIsElevated;
    }

    CloseHandle(token);
    return result;
}

void relaunch_as_admin(void)
{
    wchar_t exe_path[MAX_PATH] = { 0 };
    GetModuleFileNameW(NULL, exe_path, MAX_PATH);

    SHELLEXECUTEINFOW sei = { 0 };
    sei.cbSize       = sizeof(sei);
    sei.fMask        = SEE_MASK_NOASYNC;
    sei.lpVerb       = L"runas";
    sei.lpFile       = exe_path;
    sei.lpParameters = NULL;
    sei.nShow        = SW_NORMAL;

    ShellExecuteExW(&sei);
}
