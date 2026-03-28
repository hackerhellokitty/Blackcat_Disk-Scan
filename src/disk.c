#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "disk.h"
#include "eventlog.h"

/* -------------------------------------------------------------------------
 * Internal helpers
 * ---------------------------------------------------------------------- */

static void trim_wstr(wchar_t* s)
{
    if (!s) return;
    size_t len = wcslen(s);
    while (len > 0 && (s[len-1] == L' ' || s[len-1] == L'\r' || s[len-1] == L'\n' || s[len-1] == L'\t')) {
        s[--len] = L'\0';
    }
    wchar_t* p = s;
    while (*p == L' ' || *p == L'\t') p++;
    if (p != s) memmove(s, p, (wcslen(p) + 1) * sizeof(wchar_t));
}

/* -------------------------------------------------------------------------
 * list_drives  – use PowerShell to query WMI, fallback to IOCTL probe
 * ---------------------------------------------------------------------- */

static int list_drives_powershell(DriveInfo* out, int max_count)
{
    /*
     * PowerShell command:
     *   Get-PhysicalDisk | Sort-Object DeviceId | ForEach-Object {
     *     $idx = $_.DeviceId
     *     $size = $_.Size
     *     $model = $_.FriendlyName
     *     $serial = (Get-Disk -Number $idx).SerialNumber
     *     "$idx|$model|$serial|$size"
     *   }
     */
    const wchar_t* ps_cmd =
        L"powershell.exe -NoProfile -NonInteractive -Command \""
        L"Get-PhysicalDisk | Sort-Object DeviceId | ForEach-Object { "
        L"$idx=$_.DeviceId; $sz=$_.Size; $mdl=$_.FriendlyName; "
        L"$sn=''; try { $sn=(Get-Disk -Number ([int]$idx)).SerialNumber.Trim() } catch {}; "
        L"\\\"$idx|$mdl|$sn|$sz\\\" }\"";

    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE hRead = NULL, hWrite = NULL;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) return 0;
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = { 0 };
    si.cb          = sizeof(si);
    si.dwFlags     = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput  = hWrite;
    si.hStdError   = hWrite;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = { 0 };
    wchar_t cmd_buf[2048];
    wcsncpy_s(cmd_buf, _countof(cmd_buf), ps_cmd, _TRUNCATE);

    BOOL ok = CreateProcessW(NULL, cmd_buf, NULL, NULL, TRUE,
                             CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    CloseHandle(hWrite);

    if (!ok) {
        CloseHandle(hRead);
        return 0;
    }

    /* Read output into buffer */
    char output_buf[8192] = { 0 };
    DWORD total_read = 0, bytes_read = 0;
    while (total_read < sizeof(output_buf) - 1) {
        if (!ReadFile(hRead, output_buf + total_read,
                      (DWORD)(sizeof(output_buf) - 1 - total_read), &bytes_read, NULL) || bytes_read == 0)
            break;
        total_read += bytes_read;
    }
    output_buf[total_read] = '\0';

    WaitForSingleObject(pi.hProcess, 5000);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hRead);

    if (total_read == 0) return 0;

    /* Parse lines: "index|model|serial|size" */
    int count = 0;
    char* line = output_buf;
    while (*line && count < max_count) {
        char* eol = strchr(line, '\n');
        if (eol) *eol = '\0';
        /* strip \r */
        char* cr = strchr(line, '\r');
        if (cr) *cr = '\0';

        if (strlen(line) < 3) { line = eol ? eol + 1 : line + strlen(line); continue; }

        /* split by | */
        char* parts[4] = { NULL, NULL, NULL, NULL };
        char* p = line;
        int pi_idx = 0;
        parts[pi_idx++] = p;
        while (*p && pi_idx < 4) {
            if (*p == '|') {
                *p = '\0';
                parts[pi_idx++] = p + 1;
            }
            p++;
        }

        if (pi_idx >= 4) {
            int idx = atoi(parts[0]);
            DriveInfo* d = &out[count];
            d->index = idx;

            wchar_t wmodel[128] = { 0 };
            wchar_t wserial[128] = { 0 };
            MultiByteToWideChar(CP_UTF8, 0, parts[1], -1, wmodel,  _countof(wmodel));
            MultiByteToWideChar(CP_UTF8, 0, parts[2], -1, wserial, _countof(wserial));
            trim_wstr(wmodel);
            trim_wstr(wserial);

            wcsncpy_s(d->model,  _countof(d->model),  wmodel,  _TRUNCATE);
            wcsncpy_s(d->serial, _countof(d->serial), wserial, _TRUNCATE);

            /* size */
            long long sz = 0;
            if (parts[3]) sz = _atoi64(parts[3]);
            d->size_bytes = (int64_t)sz;

            /* Build label */
            if (d->size_bytes > 0) {
                double gb = (double)d->size_bytes / (1024.0*1024.0*1024.0);
                _snwprintf_s(d->label, _countof(d->label), _TRUNCATE,
                    L"PhysicalDrive%d  %s  (%.0f GB)", idx, wmodel, gb);
            } else {
                _snwprintf_s(d->label, _countof(d->label), _TRUNCATE,
                    L"PhysicalDrive%d  %s", idx, wmodel);
            }

            count++;
        }

        if (eol) line = eol + 1;
        else break;
    }

    return count;
}

static int list_drives_ioctl_fallback(DriveInfo* out, int max_count)
{
    int count = 0;
    for (int i = 0; i < 10 && count < max_count; i++) {
        wchar_t path[64];
        _snwprintf_s(path, _countof(path), _TRUNCATE, L"\\\\.\\PhysicalDrive%d", i);

        HANDLE h = CreateFileW(path,
            0,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL, OPEN_EXISTING, 0, NULL);

        if (h == INVALID_HANDLE_VALUE) continue;

        DriveInfo* d = &out[count];
        d->index = i;
        wcsncpy_s(d->model, _countof(d->model), L"Unknown", _TRUNCATE);
        wcsncpy_s(d->serial, _countof(d->serial), L"", _TRUNCATE);

        /* Get size via geometry */
        DISK_GEOMETRY_EX geom = { 0 };
        DWORD bytes_ret = 0;
        if (DeviceIoControl(h, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,
                NULL, 0, &geom, sizeof(geom), &bytes_ret, NULL)) {
            d->size_bytes = (int64_t)geom.DiskSize.QuadPart;
        }

        /* Try STORAGE_DEVICE_DESCRIPTOR for serial/model */
        STORAGE_PROPERTY_QUERY query = { 0 };
        query.PropertyId = StorageDeviceProperty;
        query.QueryType  = PropertyStandardQuery;
        char desc_buf[1024] = { 0 };
        if (DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY,
                &query, sizeof(query), desc_buf, sizeof(desc_buf), &bytes_ret, NULL)) {
            STORAGE_DEVICE_DESCRIPTOR* desc = (STORAGE_DEVICE_DESCRIPTOR*)desc_buf;
            if (desc->ProductIdOffset && desc->ProductIdOffset < sizeof(desc_buf)) {
                MultiByteToWideChar(CP_ACP, 0,
                    desc_buf + desc->ProductIdOffset, -1,
                    d->model, (int)_countof(d->model));
                trim_wstr(d->model);
            }
            if (desc->SerialNumberOffset && desc->SerialNumberOffset < sizeof(desc_buf)) {
                MultiByteToWideChar(CP_ACP, 0,
                    desc_buf + desc->SerialNumberOffset, -1,
                    d->serial, (int)_countof(d->serial));
                trim_wstr(d->serial);
            }
        }

        CloseHandle(h);

        if (d->size_bytes > 0) {
            double gb = (double)d->size_bytes / (1024.0*1024.0*1024.0);
            _snwprintf_s(d->label, _countof(d->label), _TRUNCATE,
                L"PhysicalDrive%d  %s  (%.0f GB)", i, d->model, gb);
        } else {
            _snwprintf_s(d->label, _countof(d->label), _TRUNCATE,
                L"PhysicalDrive%d  %s", i, d->model);
        }

        count++;
    }
    return count;
}

int list_drives(DriveInfo* out, int max_count)
{
    int n = list_drives_powershell(out, max_count);
    if (n > 0) return n;
    return list_drives_ioctl_fallback(out, max_count);
}

/* -------------------------------------------------------------------------
 * get_disk_size
 * ---------------------------------------------------------------------- */

int64_t get_disk_size(int disk_index)
{
    wchar_t path[64];
    _snwprintf_s(path, _countof(path), _TRUNCATE, L"\\\\.\\PhysicalDrive%d", disk_index);

    HANDLE h = CreateFileW(path, GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING, 0, NULL);

    if (h == INVALID_HANDLE_VALUE) return -1;

    DISK_GEOMETRY_EX geom = { 0 };
    DWORD bytes_ret = 0;
    int64_t size = -1;

    if (DeviceIoControl(h, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,
            NULL, 0, &geom, sizeof(geom), &bytes_ret, NULL)) {
        size = (int64_t)geom.DiskSize.QuadPart;
    }

    CloseHandle(h);
    return size;
}

/* -------------------------------------------------------------------------
 * Scan thread
 * ---------------------------------------------------------------------- */

typedef struct {
    ScanParams     params;
    PfnOnBlock     on_block;
    PfnOnProgress  on_progress;
    PfnOnFinish    on_finish;
    HANDLE         stop_event;
    void*          ctx;
} ScanThreadArgs;

/* Demo mode: simulate scan */
static DWORD WINAPI scan_thread_demo(ScanThreadArgs* args)
{
    const int DEMO_BLOCKS = 200;
    srand((unsigned)GetTickCount());

    int bad_count = 0;
    double total_mb_fake = 500000.0; /* fake 500 GB */
    double block_mb  = total_mb_fake / DEMO_BLOCKS;

    ScanProgress prog = { 0 };
    prog.total_mb  = (int64_t)total_mb_fake;
    prog.scanned_mb = 0;
    prog.speed_mbps = 0.0;

    LARGE_INTEGER freq, t0, t1;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t0);

    for (int bi = 0; bi < DEMO_BLOCKS; bi++) {
        /* check stop */
        if (WaitForSingleObject(args->stop_event, 0) == WAIT_OBJECT_0) {
            args->on_finish(SCAN_RESULT_STOPPED, bad_count, args->ctx);
            return 0;
        }

        /* random bad: ~5% chance on demo */
        BlockState state = BLOCK_OK;
        int r = rand() % 100;
        if (r < 3) {
            state = BLOCK_BAD;
            bad_count++;
        }

        int64_t byte_offset = (int64_t)bi * (int64_t)(block_mb * 1024.0 * 1024.0);
        args->on_block(bi, state, byte_offset, args->ctx);

        if (state == BLOCK_BAD) {
            write_event_log(args->params.disk_index, byte_offset, bad_count);
            if (args->params.stop_on_bad) {
                prog.block_idx        = bi;
                prog.block_state      = state;
                prog.block_byte_offset = byte_offset;
                prog.scanned_mb       = (int64_t)((bi + 1) * block_mb);
                prog.bad_count        = bad_count;

                QueryPerformanceCounter(&t1);
                double elapsed = (double)(t1.QuadPart - t0.QuadPart) / (double)freq.QuadPart;
                prog.speed_mbps = (elapsed > 0.001) ? (double)prog.scanned_mb / elapsed : 0.0;
                args->on_progress(&prog, args->ctx);
                args->on_finish(SCAN_RESULT_DONE_WITH_ERRORS, bad_count, args->ctx);
                return 0;
            }
        }

        prog.block_idx         = bi;
        prog.block_state       = state;
        prog.block_byte_offset = byte_offset;
        prog.scanned_mb        = (int64_t)((bi + 1) * block_mb);
        prog.bad_count         = bad_count;

        QueryPerformanceCounter(&t1);
        double elapsed = (double)(t1.QuadPart - t0.QuadPart) / (double)freq.QuadPart;
        prog.speed_mbps = (elapsed > 0.001) ? (double)prog.scanned_mb / elapsed : 0.0;

        args->on_progress(&prog, args->ctx);

        /* simulate IO delay: ~50-150 ms per block */
        DWORD delay_ms = 30 + (rand() % 80);
        if (WaitForSingleObject(args->stop_event, delay_ms) == WAIT_OBJECT_0) {
            args->on_finish(SCAN_RESULT_STOPPED, bad_count, args->ctx);
            return 0;
        }
    }

    ScanResult res = (bad_count > 0) ? SCAN_RESULT_DONE_WITH_ERRORS : SCAN_RESULT_SUCCESS;
    args->on_finish(res, bad_count, args->ctx);
    return 0;
}

/* Real disk scan */
static DWORD WINAPI scan_thread_real(ScanThreadArgs* args)
{
    int disk_index = args->params.disk_index;
    wchar_t path[64];
    _snwprintf_s(path, _countof(path), _TRUNCATE, L"\\\\.\\PhysicalDrive%d", disk_index);

    HANDLE h = CreateFileW(path, GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING,
        FILE_FLAG_NO_BUFFERING | FILE_FLAG_SEQUENTIAL_SCAN, NULL);

    if (h == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        ScanResult res = (err == ERROR_ACCESS_DENIED) ? SCAN_RESULT_ERROR_ADMIN : SCAN_RESULT_FAILED;
        args->on_finish(res, 0, args->ctx);
        return 0;
    }

    int64_t disk_size = get_disk_size(disk_index);
    if (disk_size <= 0) {
        CloseHandle(h);
        args->on_finish(SCAN_RESULT_ERROR_SIZE, 0, args->ctx);
        return 0;
    }

    /* Block size: Quick=128MB, Full=512KB */
    int64_t block_bytes = (args->params.mode == SCAN_MODE_QUICK)
        ? (128LL * 1024LL * 1024LL)
        : (512LL * 1024LL);

    /* Sector-align block size (512 bytes) */
    block_bytes = ((block_bytes + 511LL) / 512LL) * 512LL;

    /* Allocate aligned buffer */
    DWORD  alloc_gran = 65536;
    SYSTEM_INFO si; GetSystemInfo(&si);
    alloc_gran = si.dwAllocationGranularity;
    if (alloc_gran < 4096) alloc_gran = 4096;

    /* Cap buffer at 128 MB to avoid huge allocations */
    if (block_bytes > 128LL * 1024LL * 1024LL)
        block_bytes = 128LL * 1024LL * 1024LL;

    void* read_buf = VirtualAlloc(NULL, (SIZE_T)block_bytes,
                                  MEM_COMMIT | MEM_RESERVE,
                                  PAGE_READWRITE);
    if (!read_buf) {
        CloseHandle(h);
        args->on_finish(SCAN_RESULT_FAILED, 0, args->ctx);
        return 0;
    }

    int64_t total_mb   = disk_size / (1024LL * 1024LL);
    int64_t scanned_mb = 0;
    int bad_count = 0;

    /* Determine block count for map */
    int64_t total_blocks = (disk_size + block_bytes - 1) / block_bytes;
    if (total_blocks > MAX_BLOCKS) total_blocks = MAX_BLOCKS;
    int64_t bytes_per_map_block = disk_size / (total_blocks > 0 ? total_blocks : 1);

    LARGE_INTEGER freq, t0, t1;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t0);

    int64_t  offset = 0;
    int      block_idx = 0;
    ScanResult final_result = SCAN_RESULT_SUCCESS;

    while (offset < disk_size) {
        /* Check stop event */
        if (WaitForSingleObject(args->stop_event, 0) == WAIT_OBJECT_0) {
            final_result = SCAN_RESULT_STOPPED;
            break;
        }

        int64_t remaining = disk_size - offset;
        DWORD   to_read   = (remaining >= block_bytes) ? (DWORD)block_bytes : (DWORD)remaining;
        /* Align to 512 */
        to_read = (to_read / 512) * 512;
        if (to_read == 0) break;

        /* Seek */
        LARGE_INTEGER li_offset;
        li_offset.QuadPart = offset;
        if (!SetFilePointerEx(h, li_offset, NULL, FILE_BEGIN)) {
            block_idx = (int)((offset * (total_blocks)) / disk_size);
            args->on_block(block_idx, BLOCK_BAD, offset, args->ctx);
            bad_count++;
            write_event_log(disk_index, offset, bad_count);
            if (args->params.stop_on_bad) { final_result = SCAN_RESULT_DONE_WITH_ERRORS; break; }
            offset += block_bytes;
            continue;
        }

        DWORD bytes_read = 0;
        BOOL  read_ok = ReadFile(h, read_buf, to_read, &bytes_read, NULL);

        BlockState bstate = BLOCK_OK;
        int cur_block = (int)((offset * (total_blocks)) / disk_size);
        if (cur_block >= MAX_BLOCKS) cur_block = MAX_BLOCKS - 1;

        if (!read_ok || bytes_read == 0) {
            bstate = BLOCK_BAD;
            bad_count++;
            write_event_log(disk_index, offset, bad_count);
        }

        args->on_block(cur_block, bstate, offset, args->ctx);

        if (bstate == BLOCK_BAD && args->params.stop_on_bad) {
            final_result = SCAN_RESULT_DONE_WITH_ERRORS;
            offset += block_bytes;
            break;
        }

        offset    += (int64_t)(bytes_read > 0 ? bytes_read : block_bytes);
        scanned_mb = offset / (1024LL * 1024LL);

        QueryPerformanceCounter(&t1);
        double elapsed = (double)(t1.QuadPart - t0.QuadPart) / (double)freq.QuadPart;

        ScanProgress prog;
        prog.scanned_mb        = scanned_mb;
        prog.total_mb          = total_mb;
        prog.bad_count         = bad_count;
        prog.speed_mbps        = (elapsed > 0.001) ? (double)scanned_mb / elapsed : 0.0;
        prog.block_idx         = cur_block;
        prog.block_state       = bstate;
        prog.block_byte_offset = offset - (int64_t)(bytes_read > 0 ? bytes_read : block_bytes);

        args->on_progress(&prog, args->ctx);

        block_idx = cur_block + 1;
    }

    VirtualFree(read_buf, 0, MEM_RELEASE);
    CloseHandle(h);

    if (final_result == SCAN_RESULT_SUCCESS && bad_count > 0)
        final_result = SCAN_RESULT_DONE_WITH_ERRORS;

    args->on_finish(final_result, bad_count, args->ctx);
    return 0;
}

static DWORD WINAPI scan_thread_entry(LPVOID param)
{
    ScanThreadArgs* args = (ScanThreadArgs*)param;
    DWORD ret;

    if (args->params.force_demo) {
        ret = scan_thread_demo(args);
    } else {
        ret = scan_thread_real(args);
    }

    HeapFree(GetProcessHeap(), 0, args);
    return ret;
}

HANDLE start_scan_thread(const ScanParams*  params,
                         PfnOnBlock         on_block,
                         PfnOnProgress      on_progress,
                         PfnOnFinish        on_finish,
                         HANDLE             stop_event,
                         void*              ctx)
{
    ScanThreadArgs* args = (ScanThreadArgs*)HeapAlloc(
        GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(ScanThreadArgs));
    if (!args) return NULL;

    args->params     = *params;
    args->on_block   = on_block;
    args->on_progress= on_progress;
    args->on_finish  = on_finish;
    args->stop_event = stop_event;
    args->ctx        = ctx;

    HANDLE ht = CreateThread(NULL, 0, scan_thread_entry, args, 0, NULL);
    return ht;
}
