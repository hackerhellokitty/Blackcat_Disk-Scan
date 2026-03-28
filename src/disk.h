#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdbool.h>

#define MAX_DRIVES  32
#define MAX_BLOCKS  800
#define BLOCK_PX    14
#define GRID_COLS   40

typedef struct {
    int     index;
    wchar_t label[256];
    int64_t size_bytes;
    wchar_t serial[128];
    wchar_t model[128];
} DriveInfo;

typedef enum { BLOCK_UNSCAN = 0, BLOCK_OK, BLOCK_BAD } BlockState;
typedef enum { SCAN_MODE_QUICK = 0, SCAN_MODE_FULL }   ScanMode;

typedef enum {
    SCAN_RESULT_SUCCESS = 0,
    SCAN_RESULT_DONE_WITH_ERRORS,
    SCAN_RESULT_FAILED,
    SCAN_RESULT_STOPPED,
    SCAN_RESULT_ERROR_ADMIN,
    SCAN_RESULT_ERROR_SIZE,
} ScanResult;

typedef struct {
    int      disk_index;
    ScanMode mode;
    bool     stop_on_bad;
    bool     force_demo;
} ScanParams;

typedef struct {
    int64_t    scanned_mb;
    int64_t    total_mb;
    int        bad_count;
    double     speed_mbps;
    int        block_idx;
    BlockState block_state;
    int64_t    block_byte_offset;
} ScanProgress;

/* Callbacks (called from scan thread, use PostMessage to marshal to UI) */
typedef void (*PfnOnBlock)   (int block_idx, BlockState state, int64_t byte_offset, void* ctx);
typedef void (*PfnOnProgress)(const ScanProgress* prog, void* ctx);
typedef void (*PfnOnFinish)  (ScanResult result, int bad_count, void* ctx);

int     list_drives(DriveInfo* out, int max_count);
int64_t get_disk_size(int disk_index);

/* Scan runs in a thread. Returns thread handle. Call SetEvent(stop_event) to stop. */
HANDLE start_scan_thread(const ScanParams*  params,
                         PfnOnBlock         on_block,
                         PfnOnProgress      on_progress,
                         PfnOnFinish        on_finish,
                         HANDLE             stop_event,
                         void*              ctx);
