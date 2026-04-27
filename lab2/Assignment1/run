nasm -f bin mbr.asm -o mbr.bin && dd if=mbr.bin of=hd.img bs=512 count=1 seek=0 conv=notrunc && qemu-system-i386 -drive format=raw,file=hd.img -vga std
