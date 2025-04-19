#include <stdint.h>

uint64_t util_get_time_ms(void);
void util_sleep_ms(int64_t ms);
char *util_get_string_from_file(const char *path);