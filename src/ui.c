/*
 * ui.c  –  BlackCat Disk Scanner  –  Win32 native UI
 *
 *  Layout (960 x 760):
 *    Header bar (title, admin badge, EN/TH, theme toggle)
 *    Left panel  : configuration controls + action buttons
 *    Right panel : disk map + status + event log
 */

#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "ui.h"
#include "disk.h"
#include "pdf.h"
#include "i18n.h"
#include "utils.h"
#include "admin.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")

/* =========================================================================
 * Color palette (dark theme / light theme)
 * ====================================================================== */
typedef struct {
    COLORREF bg;
    COLORREF panel;
    COLORREF card;
    COLORREF border;
    COLORREF text;
    COLORREF sec_text;
    COLORREF accent;
    COLORREF green;
    COLORREF orange;
    COLORREF grey_block;
    COLORREF header_bg;
} Theme;

/* Dark: พื้นหลัง #1A1A2E โทนน้ำเงินเข้ม ตัดกับแดง อ่านง่ายกว่าดำล้วน */
static const Theme k_dark = {
    .bg          = RGB(0x18, 0x18, 0x1E),
    .panel       = RGB(0x22, 0x22, 0x2E),
    .card        = RGB(0x2A, 0x2A, 0x3A),
    .border      = RGB(0x3A, 0x3A, 0x50),
    .text        = RGB(0xE8, 0xE8, 0xF0),
    .sec_text    = RGB(0xA0, 0xA0, 0xB8),
    .accent      = RGB(0xFF, 0x3B, 0x30),
    .green       = RGB(0x30, 0xD1, 0x58),
    .orange      = RGB(0xFF, 0x9F, 0x0A),
    .grey_block  = RGB(0x38, 0x38, 0x48),
    .header_bg   = RGB(0x10, 0x10, 0x18),
};

/* Light: พื้นหลัง warm-grey ไม่ขาวจ้า */
static const Theme k_light = {
    .bg          = RGB(0xF0, 0xEF, 0xEB),
    .panel       = RGB(0xFF, 0xFF, 0xFF),
    .card        = RGB(0xE8, 0xE7, 0xE3),
    .border      = RGB(0xC8, 0xC6, 0xC0),
    .text        = RGB(0x20, 0x20, 0x28),
    .sec_text    = RGB(0x70, 0x70, 0x80),
    .accent      = RGB(0xCC, 0x2A, 0x20),
    .green       = RGB(0x20, 0xA0, 0x45),
    .orange      = RGB(0xC0, 0x70, 0x00),
    .grey_block  = RGB(0xC0, 0xBE, 0xB8),
    .header_bg   = RGB(0x28, 0x28, 0x38),
};

static Theme g_theme;
static BOOL  g_dark = TRUE;

/* =========================================================================
 * Control IDs
 * ====================================================================== */
#define ID_COMBO_DRIVE     1001
#define ID_RADIO_QUICK     1002
#define ID_RADIO_FULL      1003
#define ID_RADIO_STOP_BAD  1004
#define ID_RADIO_CONTINUE  1005
#define ID_CHK_DEMO        1006
#define ID_BTN_START       1007
#define ID_BTN_STOP        1008
#define ID_BTN_PDF         1009
#define ID_BTN_RELAUNCH    1010
#define ID_BTN_EN          1011
#define ID_BTN_TH          1012
#define ID_BTN_THEME       1013
#define ID_PROGRESS        1014
#define ID_LBL_SPEED       1015
#define ID_LBL_MB          1016
#define ID_LBL_BAD         1017
#define ID_LBL_STATUS      1018
#define ID_LISTBOX_LOG     1019
#define ID_LBL_ADMIN       1020
#define ID_LBL_TITLE       1021

/* =========================================================================
 * Window geometry constants
 * ====================================================================== */
#define WIN_W   1100
#define WIN_H    800
#define HDR_H     52
#define LEFT_W   220
#define GAP        8
#define PANEL_X   (LEFT_W + GAP * 2)
#define PANEL_W   (WIN_W - PANEL_X - GAP * 2)

/* =========================================================================
 * Global state
 * ====================================================================== */
static HWND  g_hwnd = NULL;
static HWND  g_hCombo     = NULL;
static HWND  g_hRadioQ    = NULL;   /* Quick */
static HWND  g_hRadioF    = NULL;   /* Full */
static HWND  g_hRadioStop = NULL;
static HWND  g_hRadioCont = NULL;
static HWND  g_hChkDemo   = NULL;
static HWND  g_hBtnStart  = NULL;
static HWND  g_hBtnStop   = NULL;
static HWND  g_hBtnPdf    = NULL;
static HWND  g_hProgress  = NULL;
static HWND  g_hLblSpeed  = NULL;
static HWND  g_hLblMb     = NULL;
static HWND  g_hLblBad    = NULL;
static HWND  g_hLblStatus = NULL;
static HWND  g_hListLog   = NULL;
static HWND  g_hLblAdmin  = NULL;
static HWND  g_hBtnEn     = NULL;
static HWND  g_hBtnTh     = NULL;
static HWND  g_hBtnTheme  = NULL;
static HWND  g_hBtnRelaunch = NULL;

/* Scan state */
static DriveInfo  g_drives[MAX_DRIVES];
static int        g_drive_count = 0;
static HANDLE     g_scan_thread  = NULL;
static HANDLE     g_stop_event   = NULL;
static BOOL       g_scanning     = FALSE;
static ULONGLONG  g_scan_start_tick = 0;  /* GetTickCount64 at scan start */

/* Block map state (for painting) */
static BlockState g_map_blocks[MAX_BLOCKS];
static int        g_map_block_count = 0;
static RECT       g_map_rect;   /* position of map area in client coords */

/* Report accumulation */
static ScanReport g_report;
static BOOL       g_has_report = FALSE;

/* Pending progress (written by scan thread, read by UI thread) */
typedef struct {
    volatile LONG    version;   /* incremented each write */
    ScanProgress     prog;
} PendingProgress;
static PendingProgress g_pending_progress;

/* Pending block (written by scan thread) */
typedef struct {
    volatile LONG version;
    int        block_idx;
    BlockState state;
    int64_t    byte_offset;
} PendingBlock;
static PendingBlock g_pending_block;

/* Brushes (recreated on theme change) */
static HBRUSH g_br_bg      = NULL;
static HBRUSH g_br_panel   = NULL;
static HBRUSH g_br_card    = NULL;
static HBRUSH g_br_header  = NULL;
static HBRUSH g_br_accent  = NULL;

/* Fonts */
static HFONT g_font_title  = NULL;
static HFONT g_font_normal = NULL;
static HFONT g_font_small  = NULL;
static HFONT g_font_badge  = NULL;

/* Log colours */
#define LOG_COL_NORMAL  0
#define LOG_COL_RED     1
#define LOG_COL_GREEN   2

/* =========================================================================
 * Log event storage (for owner-draw listbox)
 * ====================================================================== */
#define MAX_LOG_UI  1024
typedef struct {
    wchar_t text[256];
    int     col_type;
} UiLogEntry;
static UiLogEntry g_log_entries[MAX_LOG_UI];
static int        g_log_entry_count = 0;

/* =========================================================================
 * Helper: add log entry
 * ====================================================================== */
static void log_add(const wchar_t* text, int col_type)
{
    wchar_t ts[32];
    fmt_timestamp(ts, _countof(ts));

    wchar_t full[288];
    _snwprintf_s(full, _countof(full), _TRUNCATE, L"[%s]  %s", ts, text);

    if (g_log_entry_count < MAX_LOG_UI) {
        wcsncpy_s(g_log_entries[g_log_entry_count].text,
                  _countof(g_log_entries[g_log_entry_count].text),
                  full, _TRUNCATE);
        g_log_entries[g_log_entry_count].col_type = col_type;
        g_log_entry_count++;
    }

    /* Append to report too */
    if (g_report.log_count < MAX_LOG_EVENTS) {
        LogEvent* ev = &g_report.log_events[g_report.log_count];
        wcsncpy_s(ev->timestamp, _countof(ev->timestamp), ts, _TRUNCATE);
        wcsncpy_s(ev->message, _countof(ev->message), text, _TRUNCATE);
        ev->color_type = col_type;
        g_report.log_count++;
    }

    /* Update listbox */
    if (g_hListLog) {
        SendMessageW(g_hListLog, LB_ADDSTRING, 0, (LPARAM)full);
        int count = (int)SendMessageW(g_hListLog, LB_GETCOUNT, 0, 0);
        SendMessageW(g_hListLog, LB_SETTOPINDEX, count - 1, 0);
        InvalidateRect(g_hListLog, NULL, TRUE);
    }
}

/* =========================================================================
 * Theme management
 * ====================================================================== */
static void apply_theme(void)
{
    g_theme = g_dark ? k_dark : k_light;

    if (g_br_bg)     DeleteObject(g_br_bg);
    if (g_br_panel)  DeleteObject(g_br_panel);
    if (g_br_card)   DeleteObject(g_br_card);
    if (g_br_header) DeleteObject(g_br_header);
    if (g_br_accent) DeleteObject(g_br_accent);

    g_br_bg     = CreateSolidBrush(g_theme.bg);
    g_br_panel  = CreateSolidBrush(g_theme.panel);
    g_br_card   = CreateSolidBrush(g_theme.card);
    g_br_header = CreateSolidBrush(g_theme.header_bg);
    g_br_accent = CreateSolidBrush(g_theme.accent);
}

/* =========================================================================
 * Font creation
 * ====================================================================== */
static void create_fonts(void)
{
    if (g_font_title)  DeleteObject(g_font_title);
    if (g_font_normal) DeleteObject(g_font_normal);
    if (g_font_small)  DeleteObject(g_font_small);
    if (g_font_badge)  DeleteObject(g_font_badge);

    g_font_title  = CreateFontW(-20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

    g_font_normal = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

    g_font_small  = CreateFontW(-11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

    g_font_badge  = CreateFontW(-11, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
}

/* =========================================================================
 * Subclassed ListBox proc (owner-draw for coloured log entries)
 * ====================================================================== */
static WNDPROC g_orig_listbox_proc = NULL;

static LRESULT CALLBACK ListBoxSubclassProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    return CallWindowProcW(g_orig_listbox_proc, hwnd, msg, wp, lp);
}

/* =========================================================================
 * Populate drive combo box
 * ====================================================================== */
static void populate_drives(void)
{
    SendMessageW(g_hCombo, CB_RESETCONTENT, 0, 0);
    g_drive_count = list_drives(g_drives, MAX_DRIVES);

    for (int i = 0; i < g_drive_count; i++) {
        SendMessageW(g_hCombo, CB_ADDSTRING, 0, (LPARAM)g_drives[i].label);
    }

    if (g_drive_count > 0)
        SendMessageW(g_hCombo, CB_SETCURSEL, 0, 0);
}

/* =========================================================================
 * Status label update helpers
 * ====================================================================== */
static void set_status(const wchar_t* text)
{
    SetWindowTextW(g_hLblStatus, text);
}

static void update_stats_labels(const ScanProgress* p)
{
    wchar_t buf[64];

    /* MB/total */
    wchar_t mb_scanned[32], mb_total[32];
    fmt_mb(mb_scanned, _countof(mb_scanned), p->scanned_mb);
    fmt_mb(mb_total,   _countof(mb_total),   p->total_mb);
    _snwprintf_s(buf, _countof(buf), _TRUNCATE, L"%s / %s", mb_scanned, mb_total);
    SetWindowTextW(g_hLblMb, buf);

    /* Speed */
    fmt_speed(buf, _countof(buf), p->speed_mbps);
    SetWindowTextW(g_hLblSpeed, buf);

    /* Bad count */
    _snwprintf_s(buf, _countof(buf), _TRUNCATE, L"Bad: %d", p->bad_count);
    SetWindowTextW(g_hLblBad, buf);

    /* Progress bar */
    if (p->total_mb > 0) {
        int pct = (int)((p->scanned_mb * 100LL) / p->total_mb);
        SendMessageW(g_hProgress, PBM_SETPOS, (WPARAM)pct, 0);
    }
}

/* =========================================================================
 * Scan callbacks (called from scan thread → PostMessage)
 * ====================================================================== */
static void CALLBACK cb_on_block(int block_idx, BlockState state, int64_t byte_offset, void* ctx)
{
    (void)ctx;
    g_pending_block.block_idx   = block_idx;
    g_pending_block.state       = state;
    g_pending_block.byte_offset = byte_offset;
    InterlockedIncrement(&g_pending_block.version);
    PostMessageW(g_hwnd, WM_SCAN_BLOCK, (WPARAM)block_idx, (LPARAM)state);
}

static void CALLBACK cb_on_progress(const ScanProgress* prog, void* ctx)
{
    (void)ctx;
    g_pending_progress.prog = *prog;
    InterlockedIncrement(&g_pending_progress.version);
    PostMessageW(g_hwnd, WM_SCAN_PROGRESS, 0, 0);
}

static void CALLBACK cb_on_finish(ScanResult result, int bad_count, void* ctx)
{
    (void)ctx;
    PostMessageW(g_hwnd, WM_SCAN_FINISH, (WPARAM)result, (LPARAM)bad_count);
}

/* =========================================================================
 * Start / Stop scan
 * ====================================================================== */
static void start_scan(void)
{
    int sel = (int)SendMessageW(g_hCombo, CB_GETCURSEL, 0, 0);
    if (sel < 0 || sel >= g_drive_count) {
        MessageBoxW(g_hwnd, L"Please select a drive first.", L"BlackCat", MB_ICONWARNING);
        return;
    }

    /* Reset map */
    memset(g_map_blocks, 0, sizeof(g_map_blocks));
    g_map_block_count = MAX_BLOCKS;
    InvalidateRect(g_hwnd, &g_map_rect, TRUE);

    /* Reset log */
    g_log_entry_count = 0;
    g_report.log_count = 0;
    SendMessageW(g_hListLog, LB_RESETCONTENT, 0, 0);

    /* Reset progress */
    SendMessageW(g_hProgress, PBM_SETPOS, 0, 0);
    SetWindowTextW(g_hLblMb,    L"--");
    SetWindowTextW(g_hLblSpeed, L"--");
    SetWindowTextW(g_hLblBad,   L"Bad: 0");

    /* Setup report */
    memset(&g_report, 0, sizeof(g_report));
    wcsncpy_s(g_report.disk_label,  _countof(g_report.disk_label),
              g_drives[sel].label, _TRUNCATE);
    wcsncpy_s(g_report.disk_serial, _countof(g_report.disk_serial),
              g_drives[sel].serial, _TRUNCATE);
    fmt_timestamp(g_report.start_time, _countof(g_report.start_time));
    g_scan_start_tick    = GetTickCount64();
    g_report.total_mb    = g_drives[sel].size_bytes / (1024LL * 1024LL);
    g_report.block_count = MAX_BLOCKS;

    BOOL is_quick    = (SendMessageW(g_hRadioQ, BM_GETCHECK, 0, 0) == BST_CHECKED);
    BOOL stop_on_bad = (SendMessageW(g_hRadioStop, BM_GETCHECK, 0, 0) == BST_CHECKED);
    BOOL demo_mode   = (SendMessageW(g_hChkDemo,   BM_GETCHECK, 0, 0) == BST_CHECKED);

    wcsncpy_s(g_report.scan_mode, _countof(g_report.scan_mode),
              is_quick ? TS(mode_quick) : TS(mode_full), _TRUNCATE);
    g_report.stop_on_bad = (bool)stop_on_bad;

    ScanParams params = { 0 };
    params.disk_index  = g_drives[sel].index;
    params.mode        = is_quick ? SCAN_MODE_QUICK : SCAN_MODE_FULL;
    params.stop_on_bad = (bool)stop_on_bad;
    params.force_demo  = (bool)demo_mode;

    if (g_stop_event) CloseHandle(g_stop_event);
    g_stop_event = CreateEventW(NULL, TRUE, FALSE, NULL);

    g_scan_thread = start_scan_thread(&params,
                                      cb_on_block,
                                      cb_on_progress,
                                      cb_on_finish,
                                      g_stop_event,
                                      NULL);
    if (!g_scan_thread) {
        MessageBoxW(g_hwnd, L"Failed to start scan thread.", L"BlackCat", MB_ICONERROR);
        return;
    }

    g_scanning = TRUE;
    g_has_report = FALSE;

    EnableWindow(g_hBtnStart, FALSE);
    EnableWindow(g_hBtnStop,  TRUE);
    EnableWindow(g_hBtnPdf,   FALSE);
    EnableWindow(g_hCombo,    FALSE);

    set_status(TS(scanning));
    log_add(TS(log_start), LOG_COL_NORMAL);
    log_add(TS(speed_init), LOG_COL_NORMAL);
}

static void stop_scan(void)
{
    if (!g_scanning) return;
    set_status(TS(stopping));
    log_add(TS(stopping), LOG_COL_NORMAL);
    if (g_stop_event) SetEvent(g_stop_event);
}

/* =========================================================================
 * PDF export
 * ====================================================================== */
static void export_pdf(void)
{
    if (!g_has_report) {
        log_add(TS(pdf_no_report), LOG_COL_RED);
        return;
    }

    wchar_t file_path[MAX_PATH] = L"scan_report.pdf";

    OPENFILENAMEW ofn = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = g_hwnd;
    ofn.lpstrFilter = L"PDF Files\0*.pdf\0All Files\0*.*\0\0";
    ofn.lpstrFile   = file_path;
    ofn.nMaxFile    = MAX_PATH;
    ofn.lpstrDefExt = L"pdf";
    ofn.Flags       = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

    if (!GetSaveFileNameW(&ofn)) return;   /* cancelled */

    log_add(TS(pdf_saving), LOG_COL_NORMAL);

    /* Fill end time if not set */
    if (g_report.end_time[0] == L'\0')
        fmt_timestamp(g_report.end_time, _countof(g_report.end_time));

    int ret = generate_pdf_report(&g_report, file_path, (int)g_cur_lang);
    if (ret == 0) {
        wchar_t msg[MAX_PATH + 64];
        _snwprintf_s(msg, _countof(msg), _TRUNCATE, L"%s%s", TS(pdf_saved), file_path);
        log_add(msg, LOG_COL_GREEN);
    } else {
        wchar_t msg[128];
        _snwprintf_s(msg, _countof(msg), _TRUNCATE, L"%sgenerate_pdf_report() failed", TS(pdf_error));
        log_add(msg, LOG_COL_RED);
    }
}

/* =========================================================================
 * Disk map painting
 * ====================================================================== */
static void paint_disk_map(HDC hdc, const RECT* rc)
{
    int px   = BLOCK_PX;
    int gap  = 2;
    int cell = px + gap;
    int pad  = 6;

    /* Fill map background */
    HBRUSH br_bg = CreateSolidBrush(g_theme.card);
    FillRect(hdc, rc, br_bg);
    DeleteObject(br_bg);

    int count = g_map_block_count;
    if (count <= 0) return;

    /* คำนวณ cols จากความกว้างจริงของ rect เพื่อให้ block เต็มกรอบ */
    int avail_w = (rc->right - rc->left) - pad * 2;
    int cols = avail_w / cell;
    if (cols < 1) cols = 1;

    int start_x = rc->left + pad;
    int start_y = rc->top  + pad;

    for (int i = 0; i < count && i < MAX_BLOCKS; i++) {
        int row = i / cols;
        int col = i % cols;
        int bx  = start_x + col * cell;
        int by  = start_y + row * cell;

        if (by + px > rc->bottom - pad) break;

        COLORREF c;
        switch (g_map_blocks[i]) {
        case BLOCK_OK:     c = g_theme.green;      break;
        case BLOCK_BAD:    c = g_theme.accent;     break;
        default:           c = g_theme.grey_block; break;
        }

        RECT br = { bx, by, bx + px, by + px };
        HBRUSH hbr = CreateSolidBrush(c);
        FillRect(hdc, &br, hbr);
        DeleteObject(hbr);
    }
}

/* =========================================================================
 * Owner-draw ListBox
 * ====================================================================== */
static void handle_owner_draw_listbox(DRAWITEMSTRUCT* dis)
{
    if (dis->itemID == (UINT)-1) return;

    int idx = (int)dis->itemID;
    if (idx < 0 || idx >= g_log_entry_count) return;

    const UiLogEntry* entry = &g_log_entries[idx];

    /* Background */
    COLORREF bg_col = (dis->itemState & ODS_SELECTED)
        ? g_theme.card
        : (idx % 2 == 0 ? g_theme.panel : g_theme.bg);
    HBRUSH hbr = CreateSolidBrush(bg_col);
    FillRect(dis->hDC, &dis->rcItem, hbr);
    DeleteObject(hbr);

    /* Text colour */
    COLORREF fg_col;
    switch (entry->col_type) {
    case LOG_COL_RED:   fg_col = g_theme.accent; break;
    case LOG_COL_GREEN: fg_col = g_theme.green;  break;
    default:            fg_col = g_theme.text;   break;
    }

    SetTextColor(dis->hDC, fg_col);
    SetBkMode(dis->hDC, TRANSPARENT);
    SelectObject(dis->hDC, g_font_small);

    RECT tr = dis->rcItem;
    tr.left += 6;
    DrawTextW(dis->hDC, entry->text, -1, &tr, DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

/* =========================================================================
 * Update all control texts after language change
 * ====================================================================== */
static void refresh_ui_text(void)
{
    /* Buttons */
    SetWindowTextW(g_hBtnStart,   TS(btn_start));
    SetWindowTextW(g_hBtnStop,    TS(btn_stop));
    SetWindowTextW(g_hBtnPdf,     TS(btn_pdf));

    /* Radio buttons */
    SetWindowTextW(g_hRadioQ,    TS(mode_quick));
    SetWindowTextW(g_hRadioF,    TS(mode_full));
    SetWindowTextW(g_hRadioStop, TS(stop_first));
    SetWindowTextW(g_hRadioCont, TS(scan_finish));

    /* Demo checkbox */
    SetWindowTextW(g_hChkDemo,   TS(demo_mode));

    /* Admin label */
    if (is_admin()) {
        wchar_t buf[64];
        _snwprintf_s(buf, _countof(buf), _TRUNCATE, L"[%s]", TS(admin_ok));
        SetWindowTextW(g_hLblAdmin, buf);
    } else {
        wchar_t buf[64];
        _snwprintf_s(buf, _countof(buf), _TRUNCATE, L"[%s]", TS(admin_no));
        SetWindowTextW(g_hLblAdmin, buf);
    }

    if (g_hBtnRelaunch)
        SetWindowTextW(g_hBtnRelaunch, TS(btn_relaunch));

    if (!g_scanning)
        set_status(TS(ready));

    InvalidateRect(g_hwnd, NULL, TRUE);
}

/* =========================================================================
 * WM_CREATE: create all child controls
 * ====================================================================== */
static void on_create(HWND hwnd)
{
    g_hwnd = hwnd;
    create_fonts();
    apply_theme();

    int y = HDR_H + GAP;
    int lx = GAP;
    int lw = LEFT_W;
    int row_h = 22;
    int spacing = 28;

    /* ---- Left panel controls ---- */

    /* Section label: Drive */
    CreateWindowExW(0, L"STATIC", TS(select_drive),
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        lx, y, lw, 18, hwnd, NULL, NULL, NULL);
    y += 22;

    /* Drive ComboBox */
    g_hCombo = CreateWindowExW(0, L"COMBOBOX", NULL,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST | CBS_HASSTRINGS,
        lx, y, lw, 200, hwnd, (HMENU)(INT_PTR)ID_COMBO_DRIVE, NULL, NULL);
    SendMessageW(g_hCombo, WM_SETFONT, (WPARAM)g_font_small, TRUE);
    y += spacing + 4;

    /* Section label: Scan mode */
    CreateWindowExW(0, L"STATIC", TS(scan_mode),
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        lx, y, lw, 18, hwnd, NULL, NULL, NULL);
    y += 22;

    g_hRadioQ = CreateWindowExW(0, L"BUTTON", TS(mode_quick),
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP,
        lx, y, lw, row_h, hwnd, (HMENU)(INT_PTR)ID_RADIO_QUICK, NULL, NULL);
    SendMessageW(g_hRadioQ, BM_SETCHECK, BST_CHECKED, 0);
    SendMessageW(g_hRadioQ, WM_SETFONT, (WPARAM)g_font_small, TRUE);
    y += spacing;

    g_hRadioF = CreateWindowExW(0, L"BUTTON", TS(mode_full),
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
        lx, y, lw, row_h, hwnd, (HMENU)(INT_PTR)ID_RADIO_FULL, NULL, NULL);
    SendMessageW(g_hRadioF, WM_SETFONT, (WPARAM)g_font_small, TRUE);
    y += spacing + 4;

    /* Section label: On bad sector */
    CreateWindowExW(0, L"STATIC", TS(on_bad),
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        lx, y, lw, 18, hwnd, NULL, NULL, NULL);
    y += 22;

    g_hRadioStop = CreateWindowExW(0, L"BUTTON", TS(stop_first),
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP,
        lx, y, lw, row_h, hwnd, (HMENU)(INT_PTR)ID_RADIO_STOP_BAD, NULL, NULL);
    SendMessageW(g_hRadioStop, WM_SETFONT, (WPARAM)g_font_small, TRUE);
    y += spacing;

    g_hRadioCont = CreateWindowExW(0, L"BUTTON", TS(scan_finish),
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
        lx, y, lw, row_h, hwnd, (HMENU)(INT_PTR)ID_RADIO_CONTINUE, NULL, NULL);
    SendMessageW(g_hRadioCont, BM_SETCHECK, BST_CHECKED, 0);
    SendMessageW(g_hRadioCont, WM_SETFONT, (WPARAM)g_font_small, TRUE);
    y += spacing + 4;

    /* Demo checkbox */
    g_hChkDemo = CreateWindowExW(0, L"BUTTON", TS(demo_mode),
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        lx, y, lw, row_h, hwnd, (HMENU)(INT_PTR)ID_CHK_DEMO, NULL, NULL);
    SendMessageW(g_hChkDemo, WM_SETFONT, (WPARAM)g_font_small, TRUE);
    y += spacing + 12;

    /* Buttons */
    g_hBtnStart = CreateWindowExW(0, L"BUTTON", TS(btn_start),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        lx, y, lw, 32, hwnd, (HMENU)(INT_PTR)ID_BTN_START, NULL, NULL);
    SendMessageW(g_hBtnStart, WM_SETFONT, (WPARAM)g_font_normal, TRUE);
    y += 38;

    g_hBtnStop = CreateWindowExW(0, L"BUTTON", TS(btn_stop),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED,
        lx, y, lw, 32, hwnd, (HMENU)(INT_PTR)ID_BTN_STOP, NULL, NULL);
    SendMessageW(g_hBtnStop, WM_SETFONT, (WPARAM)g_font_normal, TRUE);
    y += 38;

    g_hBtnPdf = CreateWindowExW(0, L"BUTTON", TS(btn_pdf),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED,
        lx, y, lw, 28, hwnd, (HMENU)(INT_PTR)ID_BTN_PDF, NULL, NULL);
    SendMessageW(g_hBtnPdf, WM_SETFONT, (WPARAM)g_font_small, TRUE);
    y += 34;

    if (!is_admin()) {
        g_hBtnRelaunch = CreateWindowExW(0, L"BUTTON", TS(btn_relaunch),
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            lx, y, lw, 28, hwnd, (HMENU)(INT_PTR)ID_BTN_RELAUNCH, NULL, NULL);
        SendMessageW(g_hBtnRelaunch, WM_SETFONT, (WPARAM)g_font_small, TRUE);
        y += 34;
    }

    /* Admin badge */
    {
        wchar_t admin_txt[64];
        if (is_admin())
            _snwprintf_s(admin_txt, _countof(admin_txt), _TRUNCATE, L"[%s]", TS(admin_ok));
        else
            _snwprintf_s(admin_txt, _countof(admin_txt), _TRUNCATE, L"[%s]", TS(admin_no));

        g_hLblAdmin = CreateWindowExW(0, L"STATIC", admin_txt,
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            lx, WIN_H - HDR_H - 30, lw, 20, hwnd, (HMENU)(INT_PTR)ID_LBL_ADMIN, NULL, NULL);
        SendMessageW(g_hLblAdmin, WM_SETFONT, (WPARAM)g_font_badge, TRUE);
    }

    /* ---- Right panel controls ---- */
    int rx  = PANEL_X;
    int rw  = PANEL_W;
    int ry  = HDR_H + GAP;

    /* Status row */
    g_hLblStatus = CreateWindowExW(0, L"STATIC", TS(ready),
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        rx, ry, rw - 200, 20, hwnd, (HMENU)(INT_PTR)ID_LBL_STATUS, NULL, NULL);
    SendMessageW(g_hLblStatus, WM_SETFONT, (WPARAM)g_font_normal, TRUE);

    g_hLblMb = CreateWindowExW(0, L"STATIC", L"--",
        WS_CHILD | WS_VISIBLE | SS_RIGHT,
        rx + rw - 200, ry, 120, 20, hwnd, (HMENU)(INT_PTR)ID_LBL_MB, NULL, NULL);
    SendMessageW(g_hLblMb, WM_SETFONT, (WPARAM)g_font_small, TRUE);

    g_hLblSpeed = CreateWindowExW(0, L"STATIC", L"--",
        WS_CHILD | WS_VISIBLE | SS_RIGHT,
        rx + rw - 80, ry, 76, 20, hwnd, (HMENU)(INT_PTR)ID_LBL_SPEED, NULL, NULL);
    SendMessageW(g_hLblSpeed, WM_SETFONT, (WPARAM)g_font_small, TRUE);
    ry += 24;

    /* Progress bar */
    g_hProgress = CreateWindowExW(0, PROGRESS_CLASSW, NULL,
        WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
        rx, ry, rw, 16, hwnd, (HMENU)(INT_PTR)ID_PROGRESS, NULL, NULL);
    SendMessageW(g_hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessageW(g_hProgress, PBM_SETPOS,   0, 0);
    ry += 22;

    /* Bad count label */
    g_hLblBad = CreateWindowExW(0, L"STATIC", L"Bad: 0",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        rx, ry, 200, 18, hwnd, (HMENU)(INT_PTR)ID_LBL_BAD, NULL, NULL);
    SendMessageW(g_hLblBad, WM_SETFONT, (WPARAM)g_font_small, TRUE);
    ry += 24;

    /* Disk map area — คำนวณ height จาก cols จริงตาม panel width */
    int map_cols = (rw - 12) / (BLOCK_PX + 2);
    if (map_cols < 1) map_cols = 1;
    int map_rows = (MAX_BLOCKS + map_cols - 1) / map_cols;
    int map_h    = map_rows * (BLOCK_PX + 2) + 12;
    if (map_h > 380) map_h = 380;
    g_map_rect.left   = rx;
    g_map_rect.top    = ry;
    g_map_rect.right  = rx + rw;
    g_map_rect.bottom = ry + map_h;
    ry += map_h + GAP;

    /* Event log ListBox */
    int log_h = WIN_H - ry - GAP - 10;
    if (log_h < 80) log_h = 80;

    g_hListLog = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", NULL,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS,
        rx, ry, rw, log_h, hwnd, (HMENU)(INT_PTR)ID_LISTBOX_LOG, NULL, NULL);
    SendMessageW(g_hListLog, WM_SETFONT, (WPARAM)g_font_small, TRUE);
    SendMessageW(g_hListLog, LB_SETITEMHEIGHT, 0, 18);

    /* Subclass listbox */
    g_orig_listbox_proc = (WNDPROC)SetWindowLongPtrW(g_hListLog, GWLP_WNDPROC,
                                                     (LONG_PTR)ListBoxSubclassProc);

    /* ---- Header controls ---- */
    /* EN / TH buttons */
    g_hBtnEn = CreateWindowExW(0, L"BUTTON", L"EN",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        WIN_W - 130, (HDR_H - 24) / 2, 38, 24,
        hwnd, (HMENU)(INT_PTR)ID_BTN_EN, NULL, NULL);
    SendMessageW(g_hBtnEn, WM_SETFONT, (WPARAM)g_font_badge, TRUE);

    g_hBtnTh = CreateWindowExW(0, L"BUTTON", L"TH",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        WIN_W - 88, (HDR_H - 24) / 2, 38, 24,
        hwnd, (HMENU)(INT_PTR)ID_BTN_TH, NULL, NULL);
    SendMessageW(g_hBtnTh, WM_SETFONT, (WPARAM)g_font_badge, TRUE);

    /* Theme toggle */
    g_hBtnTheme = CreateWindowExW(0, L"BUTTON", L"\u25D1",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        WIN_W - 46, (HDR_H - 24) / 2, 38, 24,
        hwnd, (HMENU)(INT_PTR)ID_BTN_THEME, NULL, NULL);
    SendMessageW(g_hBtnTheme, WM_SETFONT, (WPARAM)g_font_normal, TRUE);

    /* Populate drives */
    populate_drives();

    /* Initial log entry */
    log_add(TS(log_ready), LOG_COL_NORMAL);
}

/* =========================================================================
 * WM_PAINT
 * ====================================================================== */
static void on_paint(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT rc;
    GetClientRect(hwnd, &rc);

    /* Main background */
    FillRect(hdc, &rc, g_br_bg);

    /* Header bar */
    RECT hdr_rc = { 0, 0, rc.right, HDR_H };
    FillRect(hdc, &hdr_rc, g_br_header);

    /* Red accent line under header */
    RECT accent_rc = { 0, HDR_H - 3, rc.right, HDR_H };
    FillRect(hdc, &accent_rc, g_br_accent);

    /* "BlackCat" title in red */
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, g_theme.accent);
    SelectObject(hdc, g_font_title);
    RECT title_rc = { GAP + 8, 10, 200, HDR_H - 4 };
    DrawTextW(hdc, L"BlackCat", -1, &title_rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    /* Version string */
    SetTextColor(hdc, g_theme.text);
    SelectObject(hdc, g_font_normal);
    RECT ver_rc = { 130, 14, 400, HDR_H - 8 };
    DrawTextW(hdc, L"Disk Scanner v4.2.0", -1, &ver_rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    /* Demo badge */
    BOOL demo = (g_hChkDemo && SendMessageW(g_hChkDemo, BM_GETCHECK, 0, 0) == BST_CHECKED);
    if (demo) {
        RECT badge_rc = { 340, 14, 400, HDR_H - 10 };
        HBRUSH br_orange = CreateSolidBrush(g_theme.orange);
        FillRect(hdc, &badge_rc, br_orange);
        DeleteObject(br_orange);
        SetTextColor(hdc, RGB(0,0,0));
        SelectObject(hdc, g_font_badge);
        DrawTextW(hdc, TS(demo_badge), -1, &badge_rc,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    /* Left panel border */
    RECT left_panel = { 0, HDR_H, LEFT_W + GAP + 4, rc.bottom };
    FillRect(hdc, &left_panel, g_br_panel);

    /* Vertical divider */
    RECT divider = { LEFT_W + GAP, HDR_H, LEFT_W + GAP + 1, rc.bottom };
    HBRUSH br_div = CreateSolidBrush(g_theme.border);
    FillRect(hdc, &divider, br_div);
    DeleteObject(br_div);

    /* Map title */
    SetTextColor(hdc, g_theme.sec_text);
    SelectObject(hdc, g_font_small);
    RECT map_title_rc = { g_map_rect.left, g_map_rect.top - 18, g_map_rect.right, g_map_rect.top };
    DrawTextW(hdc, TS(map_title), -1, &map_title_rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    /* Disk map */
    paint_disk_map(hdc, &g_map_rect);

    /* Map border */
    HPEN pen_border = CreatePen(PS_SOLID, 1, g_theme.border);
    HPEN old_pen = (HPEN)SelectObject(hdc, pen_border);
    HBRUSH old_br = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, g_map_rect.left, g_map_rect.top,
              g_map_rect.right, g_map_rect.bottom);
    SelectObject(hdc, old_pen);
    SelectObject(hdc, old_br);
    DeleteObject(pen_border);

    /* Map legend */
    {
        int lx = g_map_rect.left;
        int ly = g_map_rect.bottom + 4;

        struct { COLORREF c; const wchar_t* label; } legend[] = {
            { g_theme.grey_block, TS(leg_unscan) },
            { g_theme.green,      TS(leg_ok) },
            { g_theme.accent,     TS(leg_bad) },
        };

        for (int i = 0; i < 3; i++) {
            RECT sq = { lx, ly, lx + 12, ly + 12 };
            HBRUSH br = CreateSolidBrush(legend[i].c);
            FillRect(hdc, &sq, br);
            DeleteObject(br);

            SetTextColor(hdc, g_theme.sec_text);
            RECT lbl = { lx + 14, ly - 1, lx + 90, ly + 14 };
            DrawTextW(hdc, legend[i].label, -1, &lbl, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            lx += 90;
        }
    }

    EndPaint(hwnd, &ps);
}

/* =========================================================================
 * WM_CTLCOLOR* handlers (theme all child controls)
 * ====================================================================== */
static HBRUSH handle_ctl_color(HWND hwnd, HDC hdc, HWND hctl, UINT msg)
{
    (void)hwnd; (void)hctl; (void)msg;
    SetTextColor(hdc, g_theme.text);
    SetBkColor(hdc,   g_theme.panel);
    SetBkMode(hdc, OPAQUE);
    return g_br_panel;
}

/* =========================================================================
 * WM_COMMAND handler
 * ====================================================================== */
static void on_command(HWND hwnd, int id, int notify_code)
{
    (void)hwnd; (void)notify_code;

    switch (id) {
    case ID_BTN_START:
        start_scan();
        break;

    case ID_BTN_STOP:
        stop_scan();
        break;

    case ID_BTN_PDF:
        export_pdf();
        break;

    case ID_BTN_RELAUNCH:
        relaunch_as_admin();
        PostQuitMessage(0);
        break;

    case ID_BTN_EN:
        g_cur_lang = LANG_EN;
        refresh_ui_text();
        break;

    case ID_BTN_TH:
        g_cur_lang = LANG_TH;
        refresh_ui_text();
        break;

    case ID_BTN_THEME:
        g_dark = !g_dark;
        apply_theme();
        InvalidateRect(g_hwnd, NULL, TRUE);
        /* Refresh all controls */
        EnumChildWindows(g_hwnd, (WNDENUMPROC)(void*)InvalidateRect, (LPARAM)NULL);
        break;
    }
}

/* =========================================================================
 * Scan message handlers
 * ====================================================================== */
static void on_scan_block(int block_idx, BlockState state)
{
    if (block_idx >= 0 && block_idx < MAX_BLOCKS) {
        g_map_blocks[block_idx] = state;
        g_report.block_states[block_idx] = state;
        if (block_idx >= g_map_block_count)
            g_map_block_count = block_idx + 1;
    }
    InvalidateRect(g_hwnd, &g_map_rect, FALSE);

    if (state == BLOCK_BAD) {
        int64_t off = g_pending_block.byte_offset;
        wchar_t msg[128];
        _snwprintf_s(msg, _countof(msg), _TRUNCATE,
            L"Bad sector at block %d  offset 0x%llX", block_idx, (unsigned long long)off);
        log_add(msg, LOG_COL_RED);
    }
}

static void on_scan_progress(void)
{
    ScanProgress prog = g_pending_progress.prog;
    update_stats_labels(&prog);
    g_report.scanned_mb = prog.scanned_mb;
    g_report.avg_speed  = prog.speed_mbps;
}

static void on_scan_finish(ScanResult result, int bad_count)
{
    g_scanning = FALSE;

    EnableWindow(g_hBtnStart, TRUE);
    EnableWindow(g_hBtnStop,  FALSE);
    EnableWindow(g_hBtnPdf,   TRUE);
    EnableWindow(g_hCombo,    TRUE);

    fmt_timestamp(g_report.end_time, _countof(g_report.end_time));
    g_report.duration_s = (double)(GetTickCount64() - g_scan_start_tick) / 1000.0;
    g_report.bad_count  = bad_count;

    const wchar_t* res_msg = NULL;
    int log_col = LOG_COL_NORMAL;

    switch (result) {
    case SCAN_RESULT_SUCCESS:
        res_msg = TS(res_ok);
        log_col = LOG_COL_GREEN;
        break;
    case SCAN_RESULT_DONE_WITH_ERRORS:
        res_msg = TS(res_errors);
        log_col = LOG_COL_RED;
        break;
    case SCAN_RESULT_FAILED:
        res_msg = TS(res_failed);
        log_col = LOG_COL_RED;
        break;
    case SCAN_RESULT_STOPPED:
        res_msg = TS(res_stopped);
        log_col = LOG_COL_NORMAL;
        break;
    case SCAN_RESULT_ERROR_ADMIN:
        res_msg = TS(res_admin);
        log_col = LOG_COL_RED;
        break;
    case SCAN_RESULT_ERROR_SIZE:
        res_msg = TS(res_size);
        log_col = LOG_COL_RED;
        break;
    default:
        res_msg = TS(res_failed);
        log_col = LOG_COL_RED;
        break;
    }

    wcsncpy_s(g_report.result_msg, _countof(g_report.result_msg), res_msg, _TRUNCATE);
    set_status(res_msg);
    log_add(res_msg, log_col);

    g_has_report = TRUE;

    if (g_scan_thread) {
        CloseHandle(g_scan_thread);
        g_scan_thread = NULL;
    }
}

/* =========================================================================
 * Main window procedure
 * ====================================================================== */
static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE:
        on_create(hwnd);
        return 0;

    case WM_PAINT:
        on_paint(hwnd);
        return 0;

    case WM_ERASEBKGND:
        return 1; /* handled in WM_PAINT */

    case WM_COMMAND:
        on_command(hwnd, LOWORD(wp), HIWORD(wp));
        return 0;

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORLISTBOX:
        return (LRESULT)handle_ctl_color(hwnd, (HDC)wp, (HWND)lp, msg);

    case WM_MEASUREITEM:
        /* owner-draw listbox item height */
        {
            MEASUREITEMSTRUCT* mis = (MEASUREITEMSTRUCT*)lp;
            if (mis->CtlID == ID_LISTBOX_LOG)
                mis->itemHeight = 18;
        }
        return TRUE;

    case WM_DRAWITEM:
        {
            DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lp;
            if (dis->CtlID == ID_LISTBOX_LOG) {
                handle_owner_draw_listbox(dis);
                return TRUE;
            }
        }
        return FALSE;

    case WM_SCAN_BLOCK:
        on_scan_block((int)wp, (BlockState)lp);
        return 0;

    case WM_SCAN_PROGRESS:
        on_scan_progress();
        return 0;

    case WM_SCAN_FINISH:
        on_scan_finish((ScanResult)wp, (int)lp);
        return 0;

    case WM_SIZE:
        {
            /* Reposition log listbox on resize */
            if (g_hListLog) {
                int ry_log = g_map_rect.bottom + GAP + 22; /* legend space */
                int new_h  = HIWORD(lp) - ry_log - GAP - 10;
                if (new_h < 60) new_h = 60;
                SetWindowPos(g_hListLog, NULL,
                    g_map_rect.left, ry_log,
                    PANEL_W, new_h,
                    SWP_NOZORDER | SWP_NOACTIVATE);
            }
        }
        return 0;

    case WM_GETMINMAXINFO:
        {
            MINMAXINFO* mmi = (MINMAXINFO*)lp;
            mmi->ptMinTrackSize.x = 700;
            mmi->ptMinTrackSize.y = 500;
        }
        return 0;

    case WM_DESTROY:
        /* Clean up scan if running */
        if (g_scanning && g_stop_event) SetEvent(g_stop_event);
        if (g_scan_thread) {
            WaitForSingleObject(g_scan_thread, 3000);
            CloseHandle(g_scan_thread);
        }
        if (g_stop_event) CloseHandle(g_stop_event);

        /* Delete GDI objects */
        if (g_br_bg)     DeleteObject(g_br_bg);
        if (g_br_panel)  DeleteObject(g_br_panel);
        if (g_br_card)   DeleteObject(g_br_card);
        if (g_br_header) DeleteObject(g_br_header);
        if (g_br_accent) DeleteObject(g_br_accent);
        if (g_font_title)  DeleteObject(g_font_title);
        if (g_font_normal) DeleteObject(g_font_normal);
        if (g_font_small)  DeleteObject(g_font_small);
        if (g_font_badge)  DeleteObject(g_font_badge);

        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* =========================================================================
 * ui_run
 * ====================================================================== */
int ui_run(HINSTANCE hInstance)
{
    /* Init common controls */
    INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icex);

    /* Register window class */
    WNDCLASSEXW wc = { 0 };
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = MainWndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon         = LoadIconW(hInstance, MAKEINTRESOURCEW(1));
    wc.hIconSm       = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(1),
                           IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
    wc.hbrBackground = NULL;  /* we paint our own bg */
    wc.lpszClassName = WC_MAIN;
    RegisterClassExW(&wc);

    /* Create main window — adjust for title bar/borders so client area = WIN_W x WIN_H */
    RECT wr = { 0, 0, WIN_W, WIN_H };
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hwnd = CreateWindowExW(
        0,
        WC_MAIN,
        L"BlackCat Disk Scanner",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        wr.right - wr.left, wr.bottom - wr.top,
        NULL, NULL, hInstance, NULL);

    if (!hwnd) return 1;

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    /* Message loop */
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}
