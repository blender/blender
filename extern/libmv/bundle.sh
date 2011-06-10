#!/bin/sh

BRANCH="keir"

repo="git://github.com/${BRANCH}/libmv.git"
tmp=`mktemp -d`

git clone $repo $tmp/libmv

rm -rf libmv

cat "files.txt" | while f=`line`; do
  mkdir -p `dirname $f`
  cp $tmp/libmv/src/$f $f
done

rm -rf $tmp

sources=`find ./libmv -type f -iname '*.cc' | sed -r 's/^\.\//\t/'`
headers=`find ./libmv -type f -iname '*.h' | sed -r 's/^\.\//\t/'`

src_dir=`find ./libmv -type f -iname '*.cc' -exec dirname {} \; | sed -r 's/^\.\//\t/' | uniq`
src=""
for x in $src_dir; do
  t="src += env.Glob('`echo $x'/*.cc'`')"
  if [ -z "$src" ]; then
    src=$t
  else
    src=`echo "$src\n$t"`
  fi
done

cat > CMakeLists.txt << EOF
# \$Id\$
# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# The Original Code is Copyright (C) 2006, Blender Foundation
# All rights reserved.
#
# Contributor(s): Blender Foundation,
#                 Sergey Sharybin
#
# ***** END GPL LICENSE BLOCK *****

set(INC
	.
	../Eigen3
)

set(INC_SYS

)

set(SRC
	libmv-capi.cpp
${sources}

	libmv-capi.h
${headers}
)

blender_add_lib(extern_libmv "\${SRC}" "\${INC}" "\${INC_SYS}")
EOF

cat > libmv/logging/logging.h << EOF
// Copyright (c) 2007, 2008, 2009 libmv authors.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#ifndef LIBMV_LOGGING_LOGGING_H
#define LIBMV_LOGGING_LOGGING_H

#include <iostream>

class DummyLogger {
public:
	DummyLogger operator << (const ::std::wstring& wstr) { return *this; }
	DummyLogger operator << (const char *pchar) { return *this; }
	DummyLogger operator << (int a) { return *this; }
};

#define LG (DummyLogger())
#define V0 (DummyLogger())
#define V1 (DummyLogger())
#define V2 (DummyLogger())

#endif  // LIBMV_LOGGING_LOGGING_H

EOF

cat > SConscript << EOF
#!/usr/bin/python
import sys
import os

Import('env')

defs = ' -DUSE_FORTRAN_BLAS -DNOGUI'
cflags = []

src = env.Glob("*.cpp")
$src

incs = '. ../Eigen3 '

env.BlenderLib ( libname = 'extern_libmv', sources=src, includes=Split(incs), defines=Split(defs), libtype=['extern', 'player'], priority=[20,137], compileflags=cflags )
EOF
