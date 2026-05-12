.PHONY: all run clean

all: boot.bin

boot.bin: boot/boot.asm
    nasm -f bin boot/boot.asm -o boot.bin

run: boot.bin
    qemu-system-i386 -drive format=raw,file=boot.bin

clean:
    rm -f *.bin