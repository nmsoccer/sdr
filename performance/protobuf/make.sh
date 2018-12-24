#!/bin/bash
c++ use_bag.cpp bag.pb.cc  -std=c++11 -pthread -lprotobuf -o use_bag
c++ unpack_bag.cpp bag.pb.cc -std=c++11 -pthread -lprotobuf -o unpack_bag
