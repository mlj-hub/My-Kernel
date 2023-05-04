#!/bin/sh

cd bootloader
riscv64-unknown-elf-gcc -O0 -march=rv64g -c sbi.S -o sbi.out
riscv64-unknown-elf-objcopy -S -O binary sbi.out sbi
rm sbi.out
cd ..