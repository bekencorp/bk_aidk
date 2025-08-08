export ARMINO_AVDK_DIR := $(shell pwd)/bk_avdk

ARMINO_IDK_DIR := $(shell pwd)/bk_avdk/bk_idk
ARMINO_TOOL := @$(ARMINO_IDK_DIR)/tools/build_tools/armino
ARMINO_TOOL_WRAPPER := @$(ARMINO_IDK_DIR)/tools/build_tools/build.sh

# 1. soc_targets contains all supported SoCs
# 2. cmake_supported_targets contains all targets that can directly
#    passed to armino cmake build system
# 3. cmake_not_supported_targets contains all targets:
#    3.1> armino cmake doesn't support it, only implemented in this
#         Makefile
#    3.2> armino cmake supports it, but has different target name
soc_targets := $(shell find  bk_avdk/bk_idk/middleware/soc/ -name "*.defconfig" -exec basename {} \; | cut -f1 -d ".")

cmake_not_supported_targets = help clean doc
all_targets = cmake_not_supported_targets soc_targets cmake_supported_targets
export SOC_SUPPORTED_TARGETS := ${soc_targets}

export ARMINO_SOC := $(findstring $(MAKECMDGOALS), $(soc_targets))
export CMD_TARGET := $(MAKECMDGOALS)

ifeq ("$(APP_VERSION)", "")
	export APP_VERSION := unknown
else
	export APP_VERSION := test
endif

ifeq ("$(PROJECT)", "")
	export PROJECT := beken_genie
else
	export PROJECT := $(PROJECT)
endif


ifeq ("$(PROJECT_DIR)", "")
	PROJECT_DIR := ../../projects/$(PROJECT)
else
	PROJECT_DIR := $(PROJECT_DIR)
endif

ifeq ("$(ARMINO_SOC)", "")
ifeq ("$(ARMINO_SOC_LIB)", "")
	ARMINO_SOC := bk7258
	ARMINO_TARGET := $(MAKECMDGOALS)
endif
else
	ARMINO_TARGET := build
endif

PROJECT_NAME := $(notdir $(PROJECT_DIR))
ifdef BUILD_DIR
	PROJECT_BUILD_DIR := $(BUILD_DIR)
else
	PROJECT_BUILD_DIR := ../../build/$(PROJECT_NAME)
endif

ifdef USE_LIBS_DETERMINED_MODE
	export ARMINO_USE_WRAPPER := 1
	export ARMINO_WRAPPER_PATH := $(ARMINO_AVDK_DIR)
	export ARMINO_WRAPPER_NEW_PATH := /armino_avdk
endif

.PHONY: all_targets

help:
	@echo ""
	@echo " make bkxxx - build soc bkxxx"
	@echo " make all - build all soc"
	@echo " make clean - clean build"
	@echo " make help - display this help info"
	@echo " make doc [option] - generate aidk doc"
	@echo " 	example:"
	@echo " 		make doc DOCS_TARGET="bk7258" DOCS_TYPE="html" DOCS_VERSION="latest""
	@echo " 		make doc DOCS_TARGET="bk7258" DOCS_TYPE="pdf"  DOCS_VERSION="latest""
	@echo ""

common:
	@echo "ARMINO_SOC is set to $(ARMINO_SOC)"
	@echo "ARMINO_TARGET is set to $(ARMINO_TARGET)"
	@echo "armino project path=$(PROJECT_DIR)"
	@echo "armino path=$(ARMINO_IDK_DIR)"
	@echo "armino build path=$(PROJECT_BUILD_DIR)"


all: $(soc_targets)

$(soc_targets): common
	@make $(ARMINO_SOC) PROJECT_DIR=$(PROJECT_DIR) BUILD_DIR=$(PROJECT_BUILD_DIR) APP_NAME=$(APP_NAME) APP_VERSION=$(APP_VERSION) -C $(ARMINO_IDK_DIR)


DOCS_PARAMTERS:=
ifneq ("$(DOCS_TARGET)", "")
	DOCS_PARAMTERS += --target $(DOCS_TARGET)
endif
ifneq ("$(DOCS_TYPE)", "")
	DOCS_PARAMTERS += --type $(DOCS_TYPE)
endif
ifneq ("$(DOCS_VERSION)", "")
	DOCS_PARAMTERS += --version $(DOCS_VERSION)
endif

doc:
	@python3 ./tools/armino_doc.py $(DOCS_PARAMTERS)

clean:
	@echo "rm -rf ./build"
	@python3 ./tools/armino_doc.py --clean True
	@rm -rf ./build ./bk_avdk/build ./bk_avdk/bk_idk/build

