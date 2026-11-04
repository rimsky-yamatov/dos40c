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

# ---------------- host tests ----------------
HOST_SRCS := kernel/dos_alloc.c kernel/dos_fat.c kernel/dos_dir.c \
             kernel/dos_handle.c tests/hal_host.c tests/test_core.c

$(B)/test_core: $(HOST_SRCS) $(B)/boot.bin | $(B)
	$(HOSTCC) -g -O0 -Wall -Wextra -DHOST_TEST -DBOOT_BIN='"$(B)/boot.bin"' \
		-Ikernel -o $@ $(HOST_SRCS)

.PHONY: test
test: $(B)/test_core
	$(B)/test_core
