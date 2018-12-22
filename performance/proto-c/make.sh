#!/bin/bash
gcc -g use_bag.c bag.pb-c.c -lprotobuf-c -o use_bag
gcc -g unpack_bag.c bag.pb-c.c -lprotobuf-c -o unpack_bag
