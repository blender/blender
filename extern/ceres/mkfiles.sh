#!/bin/sh

find ./include/ -type f | sed -r 's/^\.\///' | sort > files.txt
find ./internal/ -type f | sed -r 's/^\.\///' | sort >> files.txt
find ./config/ -type f | sed -r 's/^\.\///' | sort >> files.txt
