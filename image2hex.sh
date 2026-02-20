#!/bin/bash
ia16-elf-objcopy -I binary -O ihex --change-addresses 0x10000 elks/arch/i86/boot/Image image.hex
ia16-elf-objcopy -I binary -O ihex --change-addresses 0x80000 image/romfs.bin romfs.hex
