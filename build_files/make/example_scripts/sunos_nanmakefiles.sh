#!/bin/sh
# This is an example build script for SunOS5.8

rm -f /tmp/.nanguess
export MAKE=make
export NANBLENDERHOME=`pwd`
export MAKEFLAGS="-w -I $NANBLENDERHOME/source --no-print-directory"
export HMAKE="$NANBLENDERHOME/source/tools/hmake/hmake"

export NAN_PYTHON=/soft/python-2.2.2b1/SunOS5.8
export NAN_PYTHON_VERSION=2.2
export NAN_OPENAL=/usr/local
export NAN_JPEG=/usr/local
export NAN_PNG=/usr/local
export NAN_SDL=/usr/local
export NAN_ODE=/usr/local
export NAN_OPENSSL=/soft/ssl/openssl-0.9.6e
export NAN_ZLIB=/usr/local
export NAN_FREETYPE=/usr/local

export NAN_MOZILLA_INC=/usr/local/include/mozilla-1.0.1/
export NAN_MOZILLA_LIB=/usr/local/lib/mozilla-1.0.1/
export NAN_NSPR=/scratch/irulan/mein/nspr-4.2.2/mozilla/nsprpub/dist/
export CPPFLAGS="$CPPFLAGS"
export CFLAGS="$CFLAGS"
export INTERNATIONAL=true

$HMAKE -C intern/
if [ $? -eq 0 ]; then
        $HMAKE -C source/
fi
$HMAKE -C release

#cd release
#make
