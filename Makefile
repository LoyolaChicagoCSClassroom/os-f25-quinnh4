UNAME_M := $(shell uname -m)
ifeq ($(UNAME_M),aarch64)
PREFIX:=i386-unknown-elf-
BOOTIMG:=/usr/local/grub/lib/grub/i386-pc/boot.img
GRUBLOC:=/usr/local/grub/bin/
else
PREFIX:=
BOOTIMG:=/usr/lib/grub/i386-pc/boot.img
GRUBLOC:=
endif

CC := $(PREFIX)gcc
LD := $(PREFIX)ld
SIZE := $(PREFIX)size

CONFIGS := -DCONFIG_HEAP_SIZE=4096
CFLAGS := -ffreestanding -mgeneral-regs-only -mno-mmx -m32 -march=i386 -fno-pie -fno-stack-protector -g3 -Wall

ODIR = obj
SDIR = src
OBJS = kernel_main.o rprintf.o page.o paging.o fat.o ide.o
OBJ = $(patsubst %,$(ODIR)/%,$(OBJS))

$(ODIR)/%.o: $(SDIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $^

$(ODIR)/%.o: $(SDIR)/%.s
	nasm -f elf32 -g -o $@ $^

all: kernel rootfs.img

kernel: obj $(OBJ)
	$(LD) -melf_i386 obj/* -Tkernel.ld -o kernel
	$(SIZE) kernel
	@echo "Kernel built successfully."

obj:
	mkdir -p obj

# ---- Build a GRUB-bootable FAT16 image ----
rootfs.img:
	dd if=/dev/zero of=rootfs.img bs=1M count=32
	mkfs.vfat -F16 rootfs.img
	mmd -i rootfs.img ::/boot
	mcopy -i rootfs.img kernel ::/
	echo 'set timeout=0' > grub.cfg
	echo 'set default=0' >> grub.cfg
	echo 'menuentry "MyOS" {' >> grub.cfg
	echo '  multiboot2 /kernel' >> grub.cfg
	echo '  boot' >> grub.cfg
	echo '}' >> grub.cfg
	mcopy -i rootfs.img grub.cfg ::/boot/
	@echo "FAT filesystem test" > testfile.txt
	mcopy -i rootfs.img testfile.txt ::/
	@echo " -- rootfs.img built successfully --"

run:
	qemu-system-i386 -drive file=rootfs.img,format=raw,if=ide,index=0 -boot d -serial stdio

clean:
	rm -f kernel rootfs.img obj/* testfile.txt grub.cfg

