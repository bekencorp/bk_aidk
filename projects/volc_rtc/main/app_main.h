#pragma once

#include <stdint.h>

void volume_set_abs(uint8_t level, uint8_t has_precision);
uint32_t volume_get_current();
uint32_t volume_get_level_count();
