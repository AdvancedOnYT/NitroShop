#---------------------------------------------------------------------------------
# Clear the implicit built in rules
#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

#---------------------------------------------------------------------------------
# Homebrew Menu attributes
#---------------------------------------------------------------------------------
APP_TITLE       :=  NitroShop
APP_AUTHOR      :=  Advanced / v1.0.1
APP_DESCRIPTION :=  A simple NDS rom downloader.
ifeq (build,$(notdir $(CURDIR)))
    APP_ICON    :=  $(CURDIR)/../ICON.png
else
    APP_ICON    :=  $(CURDIR)/ICON.png
endif

include $(DEVKITARM)/3ds_rules

#---------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files will be placed
# SOURCES is a list of directories containing source code
# DATA is a list of directories containing data files
# INCLUDES is a list of directories containing extra header files
# ROMFS is the directory containing data to be compiled into RomFS
#---------------------------------------------------------------------------------
TARGET		:=	NitroShop
BUILD		:=	build
SOURCES		:=	source
DATA		:=	data
INCLUDES	:=	include
ROMFS		:=	romfs

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
ARCH	:=	-march=armv6k -mtune=mpcore -mfloat-abi=hard -mfpu=vfp

CFLAGS	:=	-g -Wall -O2 -mword-relocations \
			-fomit-frame-pointer -ffunction-sections \
			$(ARCH)

CFLAGS	+=	$(DEFINES)

CXXFLAGS	:= $(CFLAGS) -std=gnu++17

ASFLAGS	:=	-g $(ARCH)
LDFLAGS	=	-specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $(basename $(@))).map

LIBS	:=	-lcitro2d -lcitro3d -lcurl -lmbedtls -lmbedx509 -lmbedcrypto -lz -lctru -lm

#---------------------------------------------------------------------------------
# list directories containing libraries for use with the linker
#---------------------------------------------------------------------------------
LIBDIRS	:=	$(PORTLIBS) $(DEVKITPRO)/libctru

#---------------------------------------------------------------------------------
# no modification needed below this line
#---------------------------------------------------------------------------------
ifeq ($(strip $(SRC_DIR)),)
	SRC_DIR := $(SOURCES)
endif

ifneq ($(BUILD),$(notdir $(CURDIR)))

#---------------------------------------------------------------------------------
export OUTPUT	:=	$(CURDIR)/$(TARGET)
export VPATH	:=	$(foreach dir,$(SRC_DIR),$(CURDIR)/$(dir)) \
					$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SRC_DIR),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SRC_DIR),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SRC_DIR),$(notdir $(wildcard $(dir)/*.s)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

#---------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
	export LD	:=	$(CC)
else
	export LD	:=	$(CXX)
endif

export OFILES	:=	$(addsuffix .o,$(bin2s)) $(CFILES:.c=.o) $(CPPFILES:.cpp=.o) $(SFILES:.s=.o)

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
					$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
					-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

ifeq ($(strip $(ROMFS)),)
	export _3DSXFLAGS	:=	--smdh=$(OUTPUT).smdh
else
	export _3DSXFLAGS	:=	--smdh=$(OUTPUT).smdh --romfs=$(CURDIR)/$(ROMFS)
endif

.PHONY: $(BUILD) clean all

#---------------------------------------------------------------------------------
all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $@ -f $(CURDIR)/Makefile

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).3dsx $(OUTPUT).elf $(TARGET).smdh

#---------------------------------------------------------------------------------
else

DEPENDS	:=	$(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
$(OUTPUT).3dsx	:	$(OUTPUT).elf $(OUTPUT).smdh

$(OUTPUT).elf	:	$(OFILES)

#---------------------------------------------------------------------------------
# rules for assembling, compiling and building
#---------------------------------------------------------------------------------
define binary_rule
$(strip $(1)).o: $(strip $(1))
	@echo $(notdir $<)
	@$(bin2o)
endef

$(foreach file,$(BINFILES),$(eval $(call binary_rule,$(file))))

%.o: %.cpp
	@echo $(notdir $<)
	@$(CXX) -MMD -MP -MF $(DEPSDIR)/$*.d $(CXXFLAGS) $(INCLUDE) -c $< -o $@

%.o: %.c
	@echo $(notdir $<)
	@$(CC) -MMD -MP -MF $(DEPSDIR)/$*.d $(CFLAGS) $(INCLUDE) -c $< -o $@

%.o: %.s
	@echo $(notdir $<)
	@$(AS) $(ASFLAGS) $< -o $@

-include $(DEPENDS)

#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------
