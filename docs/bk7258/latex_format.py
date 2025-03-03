#!/usr/bin/env python3

import os
import subprocess
import sys
import re
import argparse
import subprocess
import time
from pathlib import Path

def run(command):
	# 执行sed命令
	result = subprocess.run(command, shell=True, text=True, capture_output=True)

	# 获取输出
	output = result.stdout.strip()
	return output

def convert_to_utf8(file_path):
    try:
        # 读取文件内容
        with open(file_path, 'r', encoding='utf-8') as file:
            content = file.read()

        # 将内容转换成UTF-8编码的字符串
        converted_content = content.encode('utf-8')

        # 写入新的文件
        with open(file_path, 'wb') as file:
            file.write(converted_content)
        print(f"File '{file_path}' has been converted to UTF-8 encoding.")
    except Exception as e:
        print(f"An error occurred: {e}")

def replace_chinese_symbols_with_english(file_path):
    # 使用正则表达式匹配所有中文字符
    pattern = re.compile(r'[\u4e00-\u9fa5]+')

    # 读取原始文件内容
    with open(file_path, 'r', encoding='utf-8') as file:
        content = file.read()

    # 替换中文字符为英文符号
    new_content = pattern.sub(lambda x: ''.join(['#' for _ in x.group()]), content)

    # 将修改后的内容写回文件
    with open(file_path, 'w', encoding='utf-8') as file:
        file.write(new_content)

def fileStringDel(file_name):
	pattern = r'English'
	regex = re.compile(pattern, re.MULTILINE | re.IGNORECASE)

	with open(file_name, 'r') as file:
		lines = file.readlines()

	with open(file_name, 'w') as file:
		file.writelines(filter(lambda x: not regex.match(x), lines))

def fileStringReplace(name, old, new):
	with open(name, 'r') as file :
		content = file.read()

		if old in content:
			print("Found in the string.")
		else:
			print("Not found in the string: %s" % old)
			return

		content = content.replace(old, new)

		with open(name, 'w') as file :
			file.write(content)

def fileStringAppend(name, old, new):
	with open(name, 'r') as file :
		content = file.read()

		if old in content:
			print("Found in the string.")
		else:
			print("Not found in the string: %s" % old)
			return

		content = content.replace(old, old + "\n" + new)

		with open(name, 'w') as file :
			file.write(content)

def main(argv):
	parser = argparse.ArgumentParser()
	parser.add_argument('--tex', type=str)
	args = parser.parse_args()

	fileStringAppend(args.tex,
					"\pagestyle{empty}"	+ "\n"
					+ "\sphinxmaketitle" + "\n"
					+ "\pagestyle{plain}" + "\n"
					+ "\sphinxtableofcontents" + "\n"
					+ "\pagestyle{normal}"	+ "\n"
					+ "\phantomsection\label{\detokenize{index::doc}}"
                  	,
					"\pagestyle{fancy}"	+ "\n"
					+ "\\fancyhf{}" + "\n"
					+ "\\fancyhead[L]{\\textbf {Audio Video Development Kits Document}}" + "\n"
					+ "\\fancyfoot[L]{\\textbf {Beken Corporation Proprietary and Confidential}}" + "\n"
					+ "\\fancyfoot[R]{\\textbf {Page \\thepage}}" + "\n"
					+ "\\renewcommand{\\footrulewidth}{0.5pt}" + "\n"
					+ "\\renewcommand{\headheight}{14.5pt}" + "\n"
					+ "\\renewcommand{\headwidth}{\\textwidth}" + "\n"
                  )

	fileStringAppend(args.tex, "\\usepackage{babel}",
                  "\\usepackage{graphicx}" + '\n'
                  + "\\usepackage{hyperref}"
                  )

	#fileStringDel(args.tex)
	run("sed -i '/{{\[}English{\]}}$/d' " + args.tex)
	run("sed -i '/{{\[}中文{\]}}$/d' " + args.tex)
	#run("sed 's/：/:/g'" + args.tex)
	#replace_chinese_symbols_with_english(args.tex)

	makefile = os.path.dirname(args.tex) + "/Makefile"
	fileStringAppend(makefile, "PDFLATEX = latexmk",
                  "PDFLATEX = latexmk -interaction=nonstopmode"
                  )




if __name__ == "__main__":
    main(sys.argv)