include config.mk

DEBUG ?= 1

CROSS_COMPILE ?= riscv32-unknown-elf-
CC = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)ld
AR = $(CROSS_COMPILE)ar
SIZE = $(CROSS_COMPILE)size
OBJCOPY = $(CROSS_COMPILE)objcopy
COMPILE_ASM = $(CC) -march=rv32imac -fsigned-char \
	-msmall-data-limit=8 -mno-save-restore \
	-DSTDIO_UART=$(DEBUG_UART) -DMEASURE_SP=1 -std=gnu99 \
	-Wall -Wextra -Wno-comment -Wno-unused-parameter
COMPILE_GC = $(COMPILE_ASM) -ffunction-sections -fdata-sections -Os -g

PROGRAMS = ch56x_stdio-stack.hex ch56x_stdio-perf.hex ch56x_stdio-demo.hex
OBJ = *.tmp *.o *.elf $(PROGRAMS)

.PHONY: all clean
.PRECIOUS: ch56x_stdio-%.elf

all: $(PROGRAMS)

clean:
	rm -f $(OBJ)

ch56x_stdio-%.hex: ch56x_stdio-%.elf
	$(OBJCOPY) -O ihex $< $@.tmp
	mv -f $@.tmp $@

ch56x_stdio-%.elf: ch56x_stdio-%.o startup_CH56x.o Link.ld ch56x_stdio.h ch56x_stdio.c stdio-metal.o
	$(COMPILE_GC) \
		-I ../../src -T Link.ld -nostartfiles --specs=picolibc.specs \
		-Wl,--print-memory-usage -Wl,--gc-sections \
		startup_CH56x.o stdio-metal.o ch56x_stdio.c \
		$< -o $@
	$(SIZE) $@

ch56x_stdio-%.o: ch56x_stdio-%.c ch56x_stdio-common.h ch56x_stdio.h
	$(COMPILE_GC) \
		-I ../../src -T Link.ld -nostartfiles --specs=picolibc.specs \
		-c $< -o $@

startup_CH56x.o: startup_CH56x.S
	$(COMPILE_ASM) -x assembler -c $< -o $@

stdio-metal.o: ../../src/stdio-metal.c ../../src/stdio-metal.h
	$(COMPILE_GC) $< -c -o stdio-metal.o
