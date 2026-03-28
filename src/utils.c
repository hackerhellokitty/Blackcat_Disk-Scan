#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "utils.h"

void fmt_mb(wchar_t* buf, size_t n, int64_t mb)
{
    if (mb >= 1024LL * 1024LL) {
        double tb = (double)mb / (1024.0 * 1024.0);
        _snwprintf_s(buf, n, _TRUNCATE, L"%.2f TB", tb);
    } else if (mb >= 1024LL) {
        double gb = (double)mb / 1024.0;
        _snwprintf_s(buf, n, _TRUNCATE, L"%.1f GB", gb);
    } else {
        _snwprintf_s(buf, n, _TRUNCATE, L"%lld MB", (long long)mb);
    }
}

void fmt_speed(wchar_t* buf, size_t n, double mbps)
{
    if (mbps >= 1000.0) {
        _snwprintf_s(buf, n, _TRUNCATE, L"%.2f GB/s", mbps / 1024.0);
    } else if (mbps < 0.01) {
        _snwprintf_s(buf, n, _TRUNCATE, L"--");
    } else {
        _snwprintf_s(buf, n, _TRUNCATE, L"%.1f MB/s", mbps);
    }
}

void fmt_duration(wchar_t* buf, size_t n, double seconds)
{
    int total_s = (int)seconds;
    int hours   = total_s / 3600;
    int minutes = (total_s % 3600) / 60;
    int secs    = total_s % 60;

    if (hours > 0) {
        _snwprintf_s(buf, n, _TRUNCATE, L"%dh %02dm %02ds", hours, minutes, secs);
    } else if (minutes > 0) {
        _snwprintf_s(buf, n, _TRUNCATE, L"%dm %02ds", minutes, secs);
    } else {
        _snwprintf_s(buf, n, _TRUNCATE, L"%ds", secs);
    }
}

void fmt_timestamp(wchar_t* buf, size_t n)
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    _snwprintf_s(buf, n, _TRUNCATE,
        L"%04d-%02d-%02d %02d:%02d:%02d",
        st.wYear, st.wMonth, st.wDay,
        st.wHour, st.wMinute, st.wSecond);
}

COLORREF hex_color(const char* hex)
{
    /* Accepts "#RRGGBB" or "RRGGBB" */
    if (!hex) return RGB(0, 0, 0);
    if (*hex == '#') hex++;

    unsigned int r = 0, g = 0, b = 0;
    if (strlen(hex) >= 6) {
        char rbuf[3] = { hex[0], hex[1], 0 };
        char gbuf[3] = { hex[2], hex[3], 0 };
        char bbuf[3] = { hex[4], hex[5], 0 };
        r = (unsigned int)strtoul(rbuf, NULL, 16);
        g = (unsigned int)strtoul(gbuf, NULL, 16);
        b = (unsigned int)strtoul(bbuf, NULL, 16);
    }
    return RGB(r, g, b);
}

int wchar_to_utf8(const wchar_t* wstr, char* buf, int buf_size)
{
    if (!wstr || !buf || buf_size <= 0) return 0;
    int written = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, buf, buf_size, NULL, NULL);
    if (written <= 0) {
        buf[0] = '\0';
        return 0;
    }
    return written;
}
