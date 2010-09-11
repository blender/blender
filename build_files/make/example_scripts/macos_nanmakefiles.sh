#!/bin/bash

rm -f /tmp/.nanguess
export MAKE=make
export NANBLENDERHOME=`pwd`
export MAKEFLAGS="-w -I $NANBLENDERHOME/source --no-print-directory"
export HMAKE="$NANBLENDERHOME/source/tools/hmake/hmake"
echo 
echo NANBLENDERHOME : ${NANBLENDERHOME}

export NAN_PYTHON=/sw

$HMAKE -C intern/
if [ $? -eq 0 ]; then
        $HMAKE -C source/
fi
cd release
make
