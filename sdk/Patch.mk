# Shared build logic for Polyend Endless FxPatchSDK patches in this repo.
#
# A patch directory includes this file and sets:
#   PATCH_NAME     - output binary base name (required)
#   PATCH_SOURCES  - list of the patch's own .cpp files, e.g. PatchImpl.cpp (required)
#   EXTRA_INCLUDES - additional -I include dirs, e.g. the dsp/ library (optional)
#
# Example patch Makefile:
#   PATCH_NAME := lexicon_hall
#   PATCH_SOURCES := PatchImpl.cpp
#   EXTRA_INCLUDES := $(REPO_ROOT)/dsp/include
#   include $(REPO_ROOT)/sdk/Patch.mk

TOOLCHAIN ?= arm-none-eabi-
CUSTOM_COMPILER_OPTIONS ?=
PATCH_LOAD_ADDR ?= 0x80000000

SDK_ROOT := $(REPO_ROOT)/sdk

CC = $(TOOLCHAIN)gcc
CXX = $(TOOLCHAIN)g++
LD = $(CXX)
OBJCOPY = $(TOOLCHAIN)objcopy

BUILD_DIR := build

COMMON_FLAGS := -Wall -Wextra -Werror -fno-builtin -fno-common -ffunction-sections -fdata-sections \
                -fsingle-precision-constant -g -mfloat-abi=hard -mthumb \
                -fstack-usage -specs=nano.specs -Wdouble-promotion -mcpu=cortex-m7 -mfpu=fpv5-sp-d16 \
                -mfp16-format=ieee -O3 -DNDEBUG

CFLAGS := $(COMMON_FLAGS) -std=c11 $(CUSTOM_COMPILER_OPTIONS)
CXXFLAGS := $(COMMON_FLAGS) -std=c++20 -includecstdint -felide-constructors -Wno-psabi -fno-exceptions -fno-rtti $(CUSTOM_COMPILER_OPTIONS)

INCLUDES := -I$(SDK_ROOT)/source -I$(SDK_ROOT)/internal $(addprefix -I,$(EXTRA_INCLUDES))

SRC_C := $(wildcard $(SDK_ROOT)/internal/*.c)
SRC_CPP := $(wildcard $(SDK_ROOT)/internal/*.cpp) $(PATCH_SOURCES)

OBJ := $(patsubst %.c,$(BUILD_DIR)/%.o,$(notdir $(SRC_C))) \
       $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(notdir $(SRC_CPP)))

vpath %.c $(SDK_ROOT)/internal
vpath %.cpp $(SDK_ROOT)/internal .

LDFLAGS = -nostartfiles --specs=nosys.specs -Wl,-gc-sections -T $(SDK_ROOT)/internal/patch_imx.ld \
           -Wl,--defsym=PATCH_LOAD_ADDR=$(PATCH_LOAD_ADDR) -Wl,--defsym=end=__patch_bss_end

LIBS := -Wl,--start-group -lstdc++ -lc -lm -lgcc -Wl,--end-group

RM = rm -rf
MKDIR = mkdir -p
PATCH_TIMESTAMP := $(shell date +"%Y%m%d_%H%M%S")

PATCH_ELF := $(BUILD_DIR)/$(PATCH_NAME).elf
PATCH_BIN := $(BUILD_DIR)/$(PATCH_NAME)_$(PATCH_TIMESTAMP).endl

all: $(PATCH_BIN)

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/%.o: %.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(PATCH_ELF): $(OBJ) $(SDK_ROOT)/internal/patch_imx.ld | $(BUILD_DIR)
	$(LD) $(CXXFLAGS) $(OBJ) -o $@ $(LDFLAGS) $(LIBS)

$(PATCH_BIN): $(PATCH_ELF) | $(BUILD_DIR)
	$(OBJCOPY) -O binary $< $@

$(BUILD_DIR):
	$(MKDIR) $(BUILD_DIR)

clean:
	$(RM) $(BUILD_DIR)

.PHONY: all clean
