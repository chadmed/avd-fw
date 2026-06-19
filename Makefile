CC = clang
OBJCOPY = llvm-objcopy

VERSION := 3
TIER := 1

# avd-version = 3 in device tree
ifeq ($(VERSION),2)
PAD = 0xc000
endif
ifeq ($(VERSION),3)
PAD = 0x10000
endif
ifeq ($(VERSION),4)
PAD = 0x12000
endif
ifeq ($(VERSION),5)
ifeq ($(TIER),0)
PAD = 0x10000
else
PAD = 0x12000
endif
endif

LD_SCRIPT = avd-cm3.ld
CFLAGS = -Wall -O2 -nostdlib \
	 -mthumb -mcpu=cortex-m3 --target=arm-none-eabi \
	 -DVERSION=$(VERSION) -DTIER=$(TIER)

NAME = avd-fw-v$(VERSION)-t$(TIER)

OBJECTS := main.o \
	   tunable/tun-v$(VERSION)-t$(TIER).o

BUILD_OBJS := $(patsubst %,build/%,$(OBJECTS))

.PHONY: all clean
all: build/$(NAME).bin
clean:
	rm -rf build/*

build/$(NAME).elf: $(BUILD_OBJS)
	$(CC) $(CFLAGS) -T $(LD_SCRIPT) -o $@ $^

build/%.o: src/%.c
	mkdir -p "$(dir $@)"
	$(CC) $(CFLAGS) -c -o $@ $<

build/$(NAME).bin: build/$(NAME).elf
	$(OBJCOPY) -O binary --strip-debug --pad-to=$(PAD) $< $@
