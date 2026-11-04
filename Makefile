# DOS40C - MS-DOS 4.0 core ported to C
CC      ?= gcc
LD      ?= ld
OBJCOPY ?= objcopy
HOSTCC  ?= gcc

B := build

.PHONY: all clean
all: $(B)/boot.bin

$(B):
	mkdir -p $(B)

$(B)/boot.o: boot/boot.S | $(B)
	$(CC) -m16 -c -o $@ $<

$(B)/boot.bin: $(B)/boot.o
	$(LD) -m elf_i386 -nostdlib -Ttext=0x7C00 --oformat binary -o $@ $<

clean:
	rm -rf $(B)
