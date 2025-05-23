
PROJECT?=$(notdir $(CURDIR))
ARMINO_DIR1=/home/g/04BK/02Prj/BK7258/bk_avdk_cl
ARMINO_DIR_TO_TOP=../../../../../../..
PROJECT_DIR?=$(ARMINO_DIR_TO_TOP)$(CURDIR)

$(info PROJECT: $(PROJECT))
$(info PROJECT_DIR: $(PROJECT_DIR))

all: cpu0

cpu0:
	mkdir -p build/
	@make -C $(ARMINO_DIR1) bk7258 PROJECT_DIR=$(PROJECT_DIR) PROJECT=media/doorbell_720p PROJECT_BUILD_DIR=$(PROJECT_DIR)/build/ APP_VERSION=test

build: all

clean:
	rm -rf build || true