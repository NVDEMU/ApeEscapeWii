# Clear built-in suffix rules so only the devkitPPC rules are used.
.SUFFIXES:

ifeq ($(strip $(DEVKITPPC)),)
$(error "Please set DEVKITPPC in your environment")
endif

include $(DEVKITPPC)/wii_rules

TARGET := boot
BUILD := build
SOURCES := src src/generated ../psxrecomp-v4/runtime/platform/wii
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

# Explicit list: never wildcard all framework runtime sources into the Wii app.
CFILES := main.c psx_wii_input.c psx_wii_video.c psx_gpu_renderer.c psx_gpu_vram_dirty.c
CPPFILES :=
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
