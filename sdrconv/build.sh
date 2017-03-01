#!/bin/bash
gcc -g -Werror -I.. sdrconvlib.c sdrconv.c ../sdr.c -o sdrconv
