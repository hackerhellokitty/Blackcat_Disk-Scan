#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "disk.h"
#include "pdf.h"

/* Window class name */
#define WC_MAIN L"BlackCatDiskScanner"

/* Custom WM messages */
#define WM_SCAN_BLOCK    (WM_USER + 1)   /* wParam=block_idx, lParam=state; byte_offset in g_pending */
#define WM_SCAN_PROGRESS (WM_USER + 2)   /* read from g_pending_progress */
#define WM_SCAN_FINISH   (WM_USER + 3)   /* wParam=ScanResult, lParam=bad_count */

int ui_run(HINSTANCE hInstance);
