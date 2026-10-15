# Clear built-in suffix rules so only the devkitPPC rules are used.
.SUFFIXES:

ifeq ($(strip $(DEVKITPPC)),)
$(error "Please set DEVKITPPC in your environment")
endif

include $(DEVKITPPC)/wii_rules

TARGET := boot
BUILD := build
SOURCES := src src/generated ../psxrecomp-v4/runtime/platform/wii
ifneq ($(strip $(PSX_WII_GAME_BUILD)),)
SOURCES += ../psxrecomp-v4/runtime/src
CFLAGS += -DPSX_WII_GAME_BUILD -DPSX_WII_USE_FRAMEWORK_MEMORY -DPSX_WII_USE_FRAMEWORK_GPU -ffunction-sections -fdata-sections
CXXFLAGS += -DPSX_WII_GAME_BUILD -DPSX_WII_USE_FRAMEWORK_MEMORY -DPSX_WII_USE_FRAMEWORK_GPU -ffunction-sections -fdata-sections
LDFLAGS += -Wl,--gc-sections
LIBS += -lfat
endif
DATA := data
INCLUDES := ../psxrecomp-v4/runtime/platform/wii ../psxrecomp-v4/runtime/include ../psxrecomp-v4/runtime/src

CFLAGS = -g -O2 -Wall -Wextra $(MACHDEP) $(INCLUDE)
CXXFLAGS = $(CFLAGS)
LDFLAGS = -g $(MACHDEP) -Wl,-Map,$(notdir $@).map
LIBS := -lwiiuse -lbte -logc -lm
LIBDIRS :=

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT := $(CURDIR)/$(TARGET)
export DOL_PAD_SCRIPT := $(CURDIR)/tools/pad_dol.py
export VPATH := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
                $(foreach dir,$(DATA),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)

# The public build stays a GPU/controller diagnostic. The private workflow sets
# PSX_WII_GAME_BUILD=1 only after staging generated game C and the private EXE.
ifeq ($(strip $(PSX_WII_GAME_BUILD)),1)
CFILES := main_apeescape.c psx_wii_input.c psx_wii_video.c psx_wii_optional_stubs.c psx_wii_optional_game_hooks.c psx_gpu_renderer.c psx_gpu_vram_dirty.c
CFILES += memory.c gpu.c gpu_render.c dirty_ram_interp.c psx_cycles.c interrupts.c traps.c psx_interpreter.c psx_icache.c psx_ram_geometry.c psx_video_timing.c
CFILES += dma.c cdrom.c mdec.c sio.c spu.c timers.c psx_bios_backend.c psx_bios_module.c bios_hle.c bios_hle_plan.c netplay_bios_settle.c gpu_timeline.c color_lut.c
CFILES += $(notdir $(wildcard $(CURDIR)/src/generated/SCUS_944.23_full_*.c)) SCUS_944.23_dispatch.c
CPPFILES := gte.cpp pgxp.cpp
else
CFILES := main.c psx_wii_input.c psx_wii_video.c psx_wii_gp0.c psx_wii_optional_stubs.c psx_wii_optional_game_hooks.c psx_gpu_renderer.c psx_gpu_vram_dirty.c
CPPFILES :=
endif
sFILES :=
SFILES :=
BINFILES :=

ifeq ($(strip $(CPPFILES)),)
export LD := $(CC)
else
export LD := $(CXX)
endif

export OFILES_BIN := $(addsuffix .o,$(BINFILES))
export OFILES_SOURCES := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(sFILES:.s=.o) $(SFILES:.S=.o)
export OFILES := $(OFILES_BIN) $(OFILES_SOURCES)
export HFILES := $(addsuffix .h,$(subst .,_,$(BINFILES)))

export INCLUDE := $(foreach dir,$(INCLUDES),-iquote $(CURDIR)/$(dir)) \
                  $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                  -I$(CURDIR)/$(BUILD) -I$(LIBOGC_INC)
export LIBPATHS := -L$(LIBOGC_LIB) $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean all prepare
all: $(BUILD)

prepare:
	@mkdir -p src/generated
	@cp ../psxrecomp-v4/runtime/src/gpu_sw_renderer.c src/generated/psx_gpu_renderer.c
	@cp ../psxrecomp-v4/runtime/src/gpu_vram_dirty.c src/generated/psx_gpu_vram_dirty.c

$(BUILD): prepare
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(OUTPUT).elf $(OUTPUT).dol src/generated/psx_gpu_renderer.c src/generated/psx_gpu_vram_dirty.c

else

DEPENDS := $(OFILES:.o=.d)

.PHONY: all
all: $(OUTPUT).dol
$(OUTPUT).dol: $(OUTPUT).elf
	@elf2dol $< $@
	@python3 "$(DOL_PAD_SCRIPT)" "$@"
$(OUTPUT).elf: $(OFILES)
$(OFILES_SOURCES): $(HFILES)

-include $(DEPENDS)

endif
