#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stddef.h>

typedef enum { LANG_EN = 0, LANG_TH = 1 } AppLang;

typedef struct {
    const wchar_t* admin_warn;
    const wchar_t* cfg_title;
    const wchar_t* select_drive;
    const wchar_t* scan_mode;
    const wchar_t* mode_quick;
    const wchar_t* mode_full;
    const wchar_t* on_bad;
    const wchar_t* stop_first;
    const wchar_t* scan_finish;
    const wchar_t* btn_start;
    const wchar_t* btn_stop;
    const wchar_t* status_title;
    const wchar_t* ready;
    const wchar_t* map_title;
    const wchar_t* leg_unscan;
    const wchar_t* leg_ok;
    const wchar_t* leg_bad;
    const wchar_t* log_title;
    const wchar_t* log_ready;
    const wchar_t* scanning;
    const wchar_t* log_start;
    const wchar_t* speed_init;
    const wchar_t* bad_zero;
    const wchar_t* stopping;
    const wchar_t* stop_user;
    const wchar_t* dlg_title;
    const wchar_t* dlg_body;
    const wchar_t* dlg_cancel;
    const wchar_t* dlg_confirm;
    const wchar_t* res_ok;
    const wchar_t* res_errors;
    const wchar_t* res_failed;
    const wchar_t* res_stopped;
    const wchar_t* res_admin;
    const wchar_t* res_size;
    const wchar_t* demo_mode;
    const wchar_t* demo_badge;
    const wchar_t* admin_ok;
    const wchar_t* admin_no;
    const wchar_t* btn_relaunch;
    const wchar_t* btn_pdf;
    const wchar_t* pdf_saving;
    const wchar_t* pdf_saved;
    const wchar_t* pdf_error;
    const wchar_t* pdf_no_report;
    const wchar_t* pdf_sec_info;
    const wchar_t* pdf_sec_stats;
    const wchar_t* pdf_sec_map;
    const wchar_t* pdf_sec_log;
    const wchar_t* pdf_disk;
    const wchar_t* pdf_serial;
    const wchar_t* pdf_mode;
    const wchar_t* pdf_sob;
    const wchar_t* pdf_start;
    const wchar_t* pdf_end;
    const wchar_t* pdf_duration;
    const wchar_t* pdf_result;
    const wchar_t* pdf_total;
    const wchar_t* pdf_scanned;
    const wchar_t* pdf_bad;
    const wchar_t* pdf_speed;
} LangStrings;

extern const LangStrings g_lang[2];
extern AppLang g_cur_lang;

#define TS(field) (g_lang[g_cur_lang].field)
