#  copyright (c) 2015 seeed.cc
#

APP?=0
SPI_SPEED?=40
SPI_MODE?=QIO
SPI_SIZE?=512

GROVES ?=  #it's a list of grove driver directions, passed in by caller
#GROVES ?= grove_example #it's a list of grove driver directions, passed in by caller
USER_PROGRAM_DIRS ?= #it's a list of sub-dirs of the user written programe

BASE_DIR = ../..
SDK_DIR = $(BASE_DIR)/esp8266_sdk
LD_DIR = $(SDK_DIR)/ld
SULI_DIR = $(BASE_DIR)/suli
GROVES_DIR = $(BASE_DIR)/grove_drivers
RPC_SERVER_DIR = $(BASE_DIR)/rpc_server
BUILD_DIR = .



############################################################################### 
### DO NOT CHANGE THE FOLLOWING CONTENT
############################################################################### 

boot = new

ifeq ($(APP), 1)
    app = 1
else
    ifeq ($(APP), 2)
        app = 2
    else
        app = 0
    endif
endif

ifeq ($(SPI_SPEED), 26.7)
    freqdiv = 1
else
    ifeq ($(SPI_SPEED), 20)
        freqdiv = 2
    else
        ifeq ($(SPI_SPEED), 80)
            freqdiv = 15
        else
            freqdiv = 0
        endif
    endif
endif


ifeq ($(SPI_MODE), QOUT)
    mode = 1
else
    ifeq ($(SPI_MODE), DIO)
        mode = 2
    else
        ifeq ($(SPI_MODE), DOUT)
            mode = 3
        else
            mode = 0
        endif
    endif
endif

# flash larger than 1024KB only use 1024KB to storage user1.bin and user2.bin
ifeq ($(SPI_SIZE), 256)
    size = 1
    flash = 256
else
    ifeq ($(SPI_SIZE), 1024)
        size = 2
        flash = 1024
    else
        ifeq ($(SPI_SIZE), 2048)
            size = 3
            flash = 1024
        else
            ifeq ($(SPI_SIZE), 4096)
                size = 4
                flash = 1024
            else
                size = 0
                flash = 512
            endif
        endif
    endif
endif

ifeq ($(flash), 512)
  ifeq ($(app), 1)
    addr = 0x01000
  else
    ifeq ($(app), 2)
      addr = 0x41000
    endif
  endif
else
  ifeq ($(flash), 1024)
    ifeq ($(app), 1)
      addr = 0x01000
    else
      ifeq ($(app), 2)
        addr = 0x81000
      endif
    endif
  endif
endif

LD_FILE = $(LD_DIR)/eagle.app.v6.$(boot).$(flash).app$(app).ld
BIN_NAME = user$(app)#.$(flash).$(boot)
OUTPUT_BIN = $(BIN_NAME)
OUTPUT = $(BUILD_DIR)/$(OUTPUT_BIN)


rwildcard=$(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))
#ALL_HTMLS := $(call rwildcard,foo/,*.html)

C_SRC = $(notdir $(call rwildcard,./,*.c))
CPP_SRC = $(notdir $(call rwildcard,./,*.cpp))
s_SRC = $(notdir $(call rwildcard,./,*.s))
#S_SRC = $(notdir $(call rwildcard,./,*.S))

OBJECTS = user_main.o
#OBJECTS = user_main.o suli2.o rpc_server.o rpc_stream.o rpc_server_registration.o
OBJECTS += $(C_SRC:%.c=%.o) $(CPP_SRC:%.cpp=%.o) $(s_SRC:%.s=%.o) $(S_SRC:%.S=%.o) 

INCLUDE_PATHS = -I. -I$(BASE_DIR) -I$(SDK_DIR)/include -I$(SULI_DIR) -I$(RPC_SERVER_DIR) 


## for each grove driver to be compiled
OBJECTS += $(foreach g,$(GROVES),$(g).o $(g)_class.o $(g)_gen.o)

INCLUDE_PATHS += $(foreach g,$(GROVES),-I$(GROVES_DIR)/$(g))
GROVES_DIR_LIST = $(foreach g,$(GROVES),$(GROVES_DIR)/$(g))

LIBRARY_PATHS = -L$(SDK_DIR)/lib -L$(SDK_DIR)/ld
LIBRARIES = -lm -lc -lgcc \
		-lat \
		-lhal \
		-lphy	\
		-lpp	\
		-lnet80211 \
		-llwip	\
		-lwpa	\
		-lmain	\
		-ljson	\
		-lssl	\
		-lssc   \
		-lupgrade \
		-lsmartconfig 

GCC_PATH ?=

AS      = $(GCC_PATH)xtensa-lx106-elf-as
CC      = $(GCC_PATH)xtensa-lx106-elf-gcc
CPP     = $(GCC_PATH)xtensa-lx106-elf-g++
LD      = $(GCC_PATH)xtensa-lx106-elf-gcc
OBJCOPY = $(GCC_PATH)xtensa-lx106-elf-objcopy
OBJDUMP = $(GCC_PATH)xtensa-lx106-elf-objdump
SIZE 	= $(GCC_PATH)xtensa-lx106-elf-size

CC_FLAGS = -c -Os -Wpointer-arith -Wno-implicit-function-declaration -Wl,-EL -fno-inline-functions -nostdlib -mlongcalls -mtext-section-literals -falign-functions=4 -MMD -std=c99 
CPP_FLAGS = -c -Os -mlongcalls -mtext-section-literals -fno-exceptions -fno-rtti -falign-functions=4 -std=c++11 -MMD
S_FLAGS = -c -g -x assembler-with-cpp -MMD

LD_FLAGS = -nostdlib -Wl,--no-check-sections -u call_user_start -Wl,-static

DEFINES = -DICACHE_FLASH -DESP8266 -DF_CPU=80000000 -D__ets__

GET_FILESIZE ?= stat -f%z

#########################

VPATH = $(BASE_DIR):$(SULI_DIR):$(RPC_SERVER_DIR):$(BUILD_DIR):$(GROVES_DIR_LIST):$(USER_PROGRAM_DIRS)

.PHONY: all clean

all: $(OUTPUT).bin $(OUTPUT).hex 

clean:
	rm -f *.elf *.hex $(OBJECTS)  $(DEPS)

%.o: %.s
	$(CC) $(CC_FLAGS) -o $@ $<

#%.o: %.S
#	$(CC) $(CC_FLAGS) -D__ASSEMBLER__ -o $@ $<

%.o: %.c
	$(CC)  $(CC_FLAGS) $(DEFINES) $(INCLUDE_PATHS) -o $@ $<

%.o: %.cpp
	$(CPP) $(CPP_FLAGS) $(DEFINES) $(INCLUDE_PATHS) -o $@ $<


$(OUTPUT).elf: $(OBJECTS)
	$(LD) $(LD_FLAGS) $(LIBRARY_PATHS) -T$(LD_FILE) -Wl,--start-group $(LIBRARIES) $^ -Wl,--end-group -o $@ 
	@echo ""
	@echo "**********************"
	@echo "     Link Done"
	@echo ""
	@$(SIZE) $@
	@echo "**********************"

$(OUTPUT).bin: $(OUTPUT).elf
	@rm -rf $(OUTPUT).S $(OUTPUT).dump
	@$(OBJDUMP) -x -s $< > $(OUTPUT).dump
	@$(OBJDUMP) -S $< > $(OUTPUT).S

	@$(OBJCOPY) --only-section .text -O binary $< eagle.app.v6.text.bin
	@$(OBJCOPY) --only-section .data -O binary $< eagle.app.v6.data.bin
	@$(OBJCOPY) --only-section .rodata -O binary $< eagle.app.v6.rodata.bin
	@$(OBJCOPY) --only-section .irom0.text -O binary $< eagle.app.v6.irom0text.bin

	@echo ".irom0.text size: `$(GET_FILESIZE) eagle.app.v6.irom0text.bin`"
	@echo ".text size: `$(GET_FILESIZE) eagle.app.v6.text.bin`"

	@echo ""
	@echo "**********************"
	@echo " Gen Sections' Binary"
	@echo "**********************"
ifeq ($(boot), new)
	@python $(SDK_DIR)/tools/gen_appbin.py $< 2 $(mode) $(freqdiv) $(size)
	@echo "Support boot_v1.2 and +"
else
	@python $(SDK_DIR)/tools/gen_appbin.py $< 1 $(mode) $(freqdiv) $(size)
	@echo "Support boot_v1.1 and +"
endif

	@mv eagle.app.flash.bin $(OUTPUT).bin
	@rm eagle.app.v6.*
	@echo "Generate $(BIN_NAME).bin successully."
	@echo "boot.bin------------>0x00000"
	@echo "$(BIN_NAME).bin--->$(addr)"

$(OUTPUT).hex: $(OUTPUT).elf
	@$(OBJCOPY) -O ihex $< $@

$(OUTPUT).lst: $(OUTPUT).elf
	@$(OBJDUMP) -Sdh $< > $@

lst: $(OUTPUT).lst

size:
	$(SIZE) $(OUTPUT).elf

DEPS = $(OBJECTS:.o=.d) $(SYS_OBJECTS:.o=.d)
-include $(DEPS)
