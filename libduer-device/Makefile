#
# Copyright (2017) Baidu Inc. All rights reserveed.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

CUSTOMER ?= mw300

CUSTOMERS := mw300 esp32 RTK linux mtk

ifeq ($(filter $(CUSTOMER),$(CUSTOMERS)),)
$(error Not support $(CUSTOMER), suppored for $(CUSTOMERS))
endif

ifeq ($(strip $(CUSTOMER)),mw300)
COMPILER := arm
COM_DEFS := DUER_PLATFORM_MARVELL
CFLAGS := -Wall -g -Os -MMD -ffunction-sections -fdata-sections -fno-common
CFLAGS += -ffreestanding -mcpu=cortex-m4 -mthumb -DDEBUG_LEVEL=3
else ifeq ($(strip $(CUSTOMER)),esp32)
COMPILER := xtensa
COM_DEFS := DUER_PLATFORM_ESPRESSIF DUER_VOICE_SEND_ASYNC CONFIG_TCP_OVERSIZE_MSS
else ifeq ($(strip $(CUSTOMER)),RTK)
COMPILER := arm
COM_DEFS := DUER_PLATFORM_RTK
CFLAGS := -DM3 -DGCC_ARMCM3 -mcpu=cortex-m3 -mthumb -g2 -w -Os -Wno-pointer-sign
CFLAGS += -fno-common -fmessage-length=0 -ffunction-sections -fdata-sections
CFLAGS += -fomit-frame-pointer -fno-short-enums -DF_CPU=166000000L -std=gnu99 -fsigned-char
else ifeq ($(strip $(CUSTOMER)),mtk)
COMPILER := poky
CFLAGS := --sysroot=$(OECORE_PATH)/sysroots/aarch64-poky-linux
LDFLAGS := --sysroot=$(OECORE_PATH)/sysroots/aarch64-poky-linux
else
COMPILER := linux
endif

ifeq ($(strip $(COMPILER)),arm)
TOOLCHAIN ?= arm-none-eabi-
else ifeq ($(strip $(COMPILER)),xtensa)
TOOLCHAIN ?= xtensa-esp32-elf-
else ifeq ($(strip $(COMPILER)),poky)
TOOLCHAIN ?= aarch64-poky-linux-
CFLAGS += -D__COMPILER_AARCH64__
endif

BASE_DIR := $(shell pwd)
OUT_DIR := $(BASE_DIR)/out/$(COMPILER)
CC := gcc

TARGET ?= duer-device

COM_DEFS += __TARGET_$(shell echo $(strip $(TARGET)) | tr a-z A-Z | sed 's/[^a-zA-Z0-9]/_/g')__

#include $(BASE_DIR)/Makefile

DEBUG ?= false
HIDE ?= @

DUER_DEBUG_LEVEL ?= 3
DUER_MEMORY_DEBUG ?= false
DUER_NSDL_DEBUG ?= false
DUER_MBEDTLS_DEBUG ?= 0

MBEDTLS_SUPPORT := dtls tls

BUILD_SYSTEM := $(BASE_DIR)/build/core
CLEAR_VAR := $(BUILD_SYSTEM)/clear_var.mak
BUILD_PRE := $(BUILD_SYSTEM)/build_pre.mak
BUILD_STATIC_LIB := $(BUILD_SYSTEM)/build_static_library.mak
BUILD_EXECUTABLE := $(BUILD_SYSTEM)/build_executable.mak

MODULES := mbedtls \
            nsdl

CFLAGS += -O2 -Wno-pointer-to-int-cast
ifeq ($(COMPILER),xtensa)
CFLAGS += -mlongcalls
else
CFLAGS += -std=c99
endif

# To be fixed later
CFLAGS += -DZ_SOLO

ifeq ($(COMPILER),linux)
CFLAGS += -D_GNU_SOURCE -lrt
endif

DUER_DEBUG_LEVEL_SUPPORT := 0 1 2 3 4 5

ifneq ($(filter $(DUER_DEBUG_LEVEL),$(DUER_DEBUG_LEVEL_SUPPORT)),)
COM_DEFS += DUER_DEBUG_LEVEL=$(strip $(DUER_DEBUG_LEVEL))

ifeq ($(strip $(DUER_MEMORY_DEBUG)),true)
COM_DEFS += DUER_MEMORY_DEBUG DUER_MEMORY_USAGE
endif

ifeq ($(strip $(DUER_NSDL_DEBUG)),true)
COM_DEFS += MBED_CONF_MBED_TRACE_ENABLE YOTTA_CFG_MBED_TRACE YOTTA_CFG_MBED_TRACE_FEA_IPV6=0
endif

ifneq ($(filter $(DUER_MBEDTLS_DEBUG),$(DUER_DEBUG_LEVEL_SUPPORT)),)
COM_DEFS += DUER_MBEDTLS_DEBUG=$(strip $(DUER_MBEDTLS_DEBUG))
else
$(error Please set the DUER_MBEDTLS_DEBUG with $(DUER_DEBUG_LEVEL_SUPPORT))
endif

CFLAGS += -g
else
$(warning The DUER_DEBUG_LEVEL not set, disable all debug features!!!)
endif

COM_DEFS += MBED_CONF_MBED_CLIENT_SN_COAP_MAX_BLOCKWISE_PAYLOAD_SIZE=1024

# open this if want to use the AES-CBC encrypted communication
#COM_DEFS += NET_TRANS_ENCRYPTED_BY_AES_CBC

ifneq ($(strip $(COM_DEFS)),)
COM_DEFS := $(foreach d,$(COM_DEFS),-D$(d))
endif

.PHONY: clean run clean_all

all: $(TARGET)

run: run_$(TARGET)

clean: clean_$(TARGET)

clean_all:
	$(HIDE)rm -rf $(OUT_DIR)

$(foreach mak,$(shell find $(BASE_DIR) -name duer.mak),$(eval include $(strip $(mak))))

