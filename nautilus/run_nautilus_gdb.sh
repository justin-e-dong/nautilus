#!/bin/sh
make isoimage && qemu-system-x86_64 -s -S -smp 2 -m 2048 -vga std -serial stdio -cdrom nautilus.iso

