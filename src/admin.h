#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

BOOL is_admin(void);
void relaunch_as_admin(void);
