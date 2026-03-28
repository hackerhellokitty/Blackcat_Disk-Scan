#pragma once
#include <stdint.h>

void write_event_log(int disk_index, int64_t byte_offset, int bad_count);
