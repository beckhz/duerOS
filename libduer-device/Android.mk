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

DUER_DEBUG_LEVEL ?= 3
DUER_MEMORY_DEBUG ?= false
DUER_NSDL_DEBUG ?= false
DUER_MBEDTLS_DEBUG ?= 0

MBEDTLS_SUPPORT := dtls tls

APP_CFLAGS :=
COM_DEFS :=

APP_CFLAGS +=  \
    -O2 -Wno-pointer-to-int-cast -std=c99 -D_GNU_SOURCE -lrt

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

    APP_CFLAGS += -g
else
$(warning The DUER_DEBUG_LEVEL not set, disable all debug features!!!)
endif

COM_DEFS += MBED_CONF_MBED_CLIENT_SN_COAP_MAX_BLOCKWISE_PAYLOAD_SIZE=1024

ifneq ($(strip $(COM_DEFS)),)
COM_DEFS := $(foreach d,$(COM_DEFS),-D$(d))
endif

APP_CFLAGS += $(COM_DEFS)
#$(warning $(APP_CFLAGS))

include $(call all-subdir-makefiles)

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := duer-device

LOCAL_WHOLE_STATIC_LIBRARIES := \
    cjson mbedtls speex nsdl \
    framework \
    alarm coap connagent dcs ntp play_event voice_engine \
    platform jni

#include $(BUILD_STATIC_LIBRARY)
LOCAL_LDLIBS += -L$(SYSROOT)/usr/lib -llog

include $(BUILD_SHARED_LIBRARY)

