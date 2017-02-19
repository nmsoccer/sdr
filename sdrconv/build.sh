#!/bin/bash
gcc -g -W -I.. sdrconvlib.c sdrconv.c ../sdr.c -o sdrconv
