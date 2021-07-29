#!/bin/sh

find ./include/ -type f | sed -r 's/^\.\///' | grep -v /config.h > files.txt
find ./lib/ -type f | sed -r 's/^\.\///' >> files.txt
