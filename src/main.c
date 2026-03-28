#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "admin.h"
#include "ui.h"
#include "i18n.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    LPWSTR lpCmdLine, int nCmdShow)
{
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    if (!is_admin()) {
        int r = MessageBoxW(NULL,
            L"Administrator privileges required for raw disk access.\n"
            L"Relaunch as Administrator?",
            L"BlackCat Disk Scanner",
            MB_YESNO | MB_ICONWARNING);
        if (r == IDYES) {
            relaunch_as_admin();
            return 0;
        }
    }

    return ui_run(hInstance);
}
