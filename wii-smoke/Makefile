ifeq ($(strip $(DEVKITPRO)),)
$(error "DEVKITPRO is not set; use a devkitPro devkitPPC environment")
endif
ifeq ($(strip $(DEVKITPPC)),)
$(error "DEVKITPPC is not set; use a devkitPro devkitPPC environment")
endif

TARGET := boot
BUILD := build
SOURCES := src
DATA :=
INCLUDES :=

CFLAGS = -g -O2 -Wall -Wextra $(MACHDEP) $(INCLUDE)
CXXFLAGS = $(CFLAGS)
LDFLAGS = -g $(MACHDEP) -Wl,-Map,$(notdir $@).map
LIBS := -lwiiuse -lbte -logc

include $(DEVKITPPC)/wii_rules
