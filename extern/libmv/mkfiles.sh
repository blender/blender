#!/bin/sh

find ./libmv/ -type f | sed -r 's/^\.\///' > files.txt
