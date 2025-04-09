#!/bin/bash

clang -O2 -march=native -mtune=native -o tinyboot tinyboot.c
clang -O2 -march=native -mtune=native -o kernel.elf kernel.c
