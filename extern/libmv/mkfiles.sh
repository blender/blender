#!/bin/sh

find ./libmv/ -type f | sed -r 's/^\.\///' | sort > files.txt
find ./third_party/ -type f | sed -r 's/^\.\///' | sort >> files.txt
