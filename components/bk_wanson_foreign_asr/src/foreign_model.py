import os

model_dir = "../../../bk_avdk/components/bk_thirdparty/asr/wanson_foreign/model"  # 替换为你的 Model 目录路径

# 获取所有 .bin 文件
bin_files = [f for f in os.listdir(model_dir) if f.endswith(".bin")]

# 1. 生成汇编代码
asm_code = """
.section .rodata
.align 4
 
"""

# 2. 生成 C 代码（结构体定义 + extern 声明 + 函数）
c_code = """
#include <stdint.h>
#include <string.h>
#include "foreign_model.h"

// 声明汇编生成的变量（由链接器分配地址）
"""

# 生成汇编代码和 C 代码
for i, bin_file in enumerate(bin_files):
    # 汇编部分：定义起始和结束地址
    asm_code += f".global model{i}_rodata_begin\n"
    asm_code += f".global model{i}_rodata_end\n"
    asm_code += f"model{i}_rodata_begin:\n"
    asm_code += f'.incbin "{bin_file}"\n'
    asm_code += f"model{i}_rodata_end:\n\n"

    # C 代码部分：声明 extern 变量
    c_code += f"extern unsigned int model{i}_rodata_begin;\n"
    c_code += f"extern unsigned int model{i}_rodata_end;\n"

# 结构体定义
c_code += """
typedef struct {
    const char *name;      // .bin 文件名
    unsigned int *start;       // 起始地址（指向 modelX_rodata_begin）
    unsigned int *end;         // 结束地址（指向 modelX_rodata_end）
} bin_info_t;

// 定义 bin_info 数组
"""
c_code += "bin_info_t bin_info[] = {\n"
for i, bin_file in enumerate(bin_files):
    c_code += f'    [{i}] = {{"{bin_file}", &model{i}_rodata_begin, &model{i}_rodata_end}}'
    if i < len(bin_files) - 1:
        c_code += ","
    c_code += "\n"
c_code += "};\n\n"

# 函数：根据 bin 名称返回起始和结束地址
c_code += """
// 函数：根据 bin 名称返回起始和结束地址
void get_bin_addresses(const char *bin_name, unsigned int **start_addr, unsigned int **end_addr) {
"""
c_code += "    for (int i = 0; i < sizeof(bin_info) / sizeof(bin_info[0]); i++) {\n"
c_code += "        if (strcmp(bin_info[i].name, bin_name) == 0) {\n"
c_code += "            *start_addr = bin_info[i].start;\n"
c_code += "            *end_addr = bin_info[i].end;\n"
c_code += "            return;\n"
c_code += "        }\n"
c_code += "    }\n"
c_code += "    // 如果没找到，返回 NULL\n"
c_code += "    *start_addr = NULL;\n"
c_code += "    *end_addr = NULL;\n"
c_code += "}\n"

# 生成头文件内容
h_code = """
#ifndef FOREIGN_MODEL_H
#define FOREIGN_MODEL_H

#include <stdint.h>

// 函数声明
void get_bin_addresses(const char *bin_name, unsigned int **start_addr, unsigned int **end_addr);

#endif // FOREIGN_MODEL_H
"""

# 最终生成文件
with open("foreign_model.S", "w") as f_asm, open("foreign_model.c", "w") as f_c, open("foreign_model.h", "w") as f_h:
    f_asm.write(asm_code)
    f_c.write(c_code)
    f_h.write(h_code)

print(f"Generated foreign_model.S, foreign_model.c, and foreign_model.h")
print(f"Found {len(bin_files)} .bin files.")
