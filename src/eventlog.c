#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include "eventlog.h"

#ifndef _countof
#define _countof(a) (sizeof(a)/sizeof((a)[0]))
#endif

#define SOURCE_NAME L"BlackCat DiskScanner"

void write_event_log(int disk_index, int64_t byte_offset, int bad_count)
{
    HANDLE h_event_source = RegisterEventSourceW(NULL, SOURCE_NAME);
    if (!h_event_source) return;

    wchar_t msg[512];
    _snwprintf_s(msg, _countof(msg), _TRUNCATE,
        L"Bad sector detected on PhysicalDrive%d | "
        L"Byte offset: %lld (0x%016llX) | "
        L"Bad sector count so far: %d",
        disk_index,
        (long long)byte_offset,
        (unsigned long long)byte_offset,
        bad_count);

    const wchar_t* strings[1] = { msg };

    ReportEventW(
        h_event_source,
        EVENTLOG_WARNING_TYPE,  /* type  */
        0,                      /* category */
        0x00000001L,            /* event ID */
        NULL,                   /* user SID */
        1,                      /* num strings */
        0,                      /* raw data size */
        strings,                /* strings */
        NULL                    /* raw data */
    );

    DeregisterEventSource(h_event_source);
}
