#!/bin/sh

# This script invokes cuew_gen.py and updates the
# header and source files in the repository.

SCRIPT=`realpath -s $0`
DIR=`dirname $SCRIPT`

python ${DIR}/cuew_gen.py hdr $@ > $DIR/../include/cuew.h
python ${DIR}/cuew_gen.py impl $@ > $DIR/../src/cuew.c
