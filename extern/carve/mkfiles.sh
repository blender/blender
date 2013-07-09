#!/bin/sh

find ./include/ -type f | sed -r 's/^\.\///' > files.txt
find ./lib/ -type f | sed -r 's/^\.\///' >> files.txt
