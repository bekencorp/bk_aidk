
#include <stdint.h>
#include <string.h>
#include "foreign_model.h"

// 声明汇编生成的变量（由链接器分配地址）
extern unsigned int model0_rodata_begin;
extern unsigned int model0_rodata_end;
extern unsigned int model1_rodata_begin;
extern unsigned int model1_rodata_end;

typedef struct {
    const char *name;      // .bin 文件名
    unsigned int *start;       // 起始地址（指向 modelX_rodata_begin）
    unsigned int *end;         // 结束地址（指向 modelX_rodata_end）
} bin_info_t;

// 定义 bin_info 数组
bin_info_t bin_info[] = {
    [0] = {"japanese_air.bin", &model0_rodata_begin, &model0_rodata_end},
    [1] = {"english_air.bin", &model1_rodata_begin, &model1_rodata_end}
};


// 函数：根据 bin 名称返回起始和结束地址
void get_bin_addresses(const char *bin_name, unsigned int **start_addr, unsigned int **end_addr) {
    for (int i = 0; i < sizeof(bin_info) / sizeof(bin_info[0]); i++) {
        if (strcmp(bin_info[i].name, bin_name) == 0) {
            *start_addr = bin_info[i].start;
            *end_addr = bin_info[i].end;
            return;
        }
    }
    // 如果没找到，返回 NULL
    *start_addr = NULL;
    *end_addr = NULL;
}
