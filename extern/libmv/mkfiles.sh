#!/bin/sh

find ./libmv/ -type f | sed -r 's/^\.\///' | sort > files.txt
find ./third_party/ -mindepth 2 -type f | \
    grep -v third_party/ceres | \
    grep -v third_party/gflags/CMakeLists.txt | \
    grep -v third_party/glog/CMakeLists.txt | \
    sed -r 's/^\.\///' | sort >> files.txt
