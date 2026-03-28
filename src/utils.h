#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stddef.h>

void    fmt_mb(wchar_t* buf, size_t n, int64_t mb);
void    fmt_speed(wchar_t* buf, size_t n, double mbps);
void    fmt_duration(wchar_t* buf, size_t n, double seconds);
void    fmt_timestamp(wchar_t* buf, size_t n);   /* current time "YYYY-MM-DD HH:MM:SS" */
COLORREF hex_color(const char* hex);             /* "#FF3B30" -> COLORREF */
int     wchar_to_utf8(const wchar_t* wstr, char* buf, int buf_size);
