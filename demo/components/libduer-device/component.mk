#
# Component Makefile
#

# LightDuer Module
LIGHTDUER_PATH := lightduer
COMPONENT_ADD_INCLUDEDIRS += \
	$(LIGHTDUER_PATH)/include

COMPONENT_ADD_LDFLAGS := -L$(COMPONENT_PATH)/$(LIGHTDUER_PATH) -lduer-device
#COMPONENT_ADD_LDFLAGS := -L/home/julius/workspace/libduer-device/out/xtensa/modules/duer-device -lduer-device
