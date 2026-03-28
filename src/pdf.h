#pragma once
#include <stdint.h>
#include <stdbool.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "disk.h"

#define MAX_LOG_EVENTS 1024

typedef struct {
    wchar_t timestamp[32];
    wchar_t message[256];
    int     color_type;   /* 0=normal, 1=red, 2=green */
} LogEvent;

typedef struct {
    wchar_t    disk_label[256];
    wchar_t    disk_serial[128];
    wchar_t    scan_mode[32];
    bool       stop_on_bad;
    wchar_t    start_time[32];
    wchar_t    end_time[32];
    double     duration_s;
    wchar_t    result_msg[128];
    int64_t    total_mb;
    int64_t    scanned_mb;
    int        bad_count;
    double     avg_speed;
    BlockState block_states[MAX_BLOCKS];
    int        block_count;
    LogEvent   log_events[MAX_LOG_EVENTS];
    int        log_count;
} ScanReport;

/* Returns 0 on success, -1 on error */
int generate_pdf_report(const ScanReport* report, const wchar_t* save_path, int lang);
