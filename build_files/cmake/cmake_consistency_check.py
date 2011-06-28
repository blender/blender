#!/usr/bin/env python

# $Id$
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
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# Contributor(s): Campbell Barton
#
# ***** END GPL LICENSE BLOCK *****

# <pep8 compliant>

from cmake_consistency_check_config import IGNORE, UTF8_CHECK, SOURCE_DIR

import os
from os.path import join, dirname, normpath, abspath, splitext

print("Scanning:", SOURCE_DIR)

global_h = set()
global_c = set()
global_refs = {}


def source_list(path, filename_check=None):
    for dirpath, dirnames, filenames in os.walk(path):

        # skip '.svn'
        if dirpath.startswith("."):
            continue

        for filename in filenames:
            if filename_check is None or filename_check(filename):
                yield os.path.join(dirpath, filename)


# extension checking
def is_cmake(filename):
    ext = splitext(filename)[1]
    return (ext == ".cmake") or (filename == "CMakeLists.txt")


def is_c_header(filename):
    ext = splitext(filename)[1]
    return (ext in (".h", ".hpp", ".hxx"))


def is_c(filename):
    ext = splitext(filename)[1]
    return (ext in (".c", ".cpp", ".cxx", ".m", ".mm", ".rc"))


def is_c_any(filename):
    return is_c(filename) or is_c_header(filename)


def cmake_get_src(f):

    sources_h = []
    sources_c = []

    filen = open(f, "r", encoding="utf8")
    it = iter(filen)
    found = False
    i = 0
    # print(f)
    while it is not None:
        while it is not None:
            i += 1
            try:
                l = next(it)
            except StopIteration:
                it = None
                break
            l = l.strip()
            if not l.startswith("#"):
                if 'set(SRC' in l or ('set(' in l and l.endswith("SRC")):
                    if len(l.split()) > 1:
                        raise Exception("strict formatting not kept 'set(SRC*' %s:%d" % (f, i))
                    found = True
                    break

                if "list(APPEND SRC" in l or ('list(APPEND ' in l and l.endswith("SRC")):
                    if l.endswith(")"):
                        raise Exception("strict formatting not kept 'list(APPEND SRC...)' on 1 line %s:%d" % (f, i))
                    found = True
                    break

        if found:
            cmake_base = dirname(f)

            while it is not None:
                i += 1
                try:
                    l = next(it)
                except StopIteration:
                    it = None
                    break

                l = l.strip()

                if not l.startswith("#"):

                    if ")" in l:
                        if l.strip() != ")":
                            raise Exception("strict formatting not kept '*)' %s:%d" % (f, i))
                        break

                    # replace dirs
                    l = l.replace("${CMAKE_CURRENT_SOURCE_DIR}", cmake_base)

                    if not l:
                        pass
                    elif l.startswith("$"):
                        # assume if it ends with SRC we know about it
                        if not l.split("}")[0].endswith("SRC"):
                            print("Can't use var '%s' %s:%d" % (l, f, i))
                    elif len(l.split()) > 1:
                        raise Exception("Multi-line define '%s' %s:%d" % (l, f, i))
                    else:
                        new_file = normpath(join(cmake_base, l))

                        if is_c_header(new_file):
                            sources_h.append(new_file)
                            global_refs.setdefault(new_file, []).append((f, i))
                        elif is_c(new_file):
                            sources_c.append(new_file)
                            global_refs.setdefault(new_file, []).append((f, i))
                        elif l in ("PARENT_SCOPE", ):
                            # cmake var, ignore
                            pass
                        elif new_file.endswith(".list"):
                            pass
                        elif new_file.endswith(".def"):
                            pass
                        else:
                            raise Exception("unknown file type - not c or h %s -> %s" % (f, new_file))

                        # print(new_file)

            global_h.update(set(sources_h))
            global_c.update(set(sources_c))
            '''
            if not sources_h and not sources_c:
                raise Exception("No sources %s" % f)

            sources_h_fs = list(source_list(cmake_base, is_c_header))
            sources_c_fs = list(source_list(cmake_base, is_c))
            '''
            # find missing C files:
            '''
            for ff in sources_c_fs:
                if ff not in sources_c:
                    print("  missing: " + ff)
            '''

    filen.close()


for cmake in source_list(SOURCE_DIR, is_cmake):
    cmake_get_src(cmake)


def is_ignore(f):
    for ig in IGNORE:
        if ig in f:
            return True
    return False


# First do stupid check, do these files exist?
print("\nChecking for missing references:")
import sys
is_err = False
errs = []
for f in (global_h | global_c):
    if f.endswith("dna.c"):
        continue

    if not os.path.exists(f):
        refs = global_refs[f]
        if refs:
            for cf, i in refs:
                errs.append((cf, i))
        else:
            raise Exception("CMake referenecs missing, internal error, aborting!")
        is_err = True

errs.sort()
errs.reverse()
for cf, i in errs:
    print("%s:%d" % (cf, i))
    # Write a 'sed' script, useful if we get a lot of these
    # print("sed '%dd' '%s' > '%s.tmp' ; mv '%s.tmp' '%s'" % (i, cf, cf, cf, cf))


if is_err:
    raise Exception("CMake referenecs missing files, aborting!")
del is_err
del errs

# now check on files not accounted for.
print("\nC/C++ Files CMake doesnt know about...")
for cf in sorted(source_list(SOURCE_DIR, is_c)):
    if not is_ignore(cf):
        if cf not in global_c:
            print("missing_c: ", cf)

        # check if automake builds a corrasponding .o file.
        '''
        if cf in global_c:
            out1 = os.path.splitext(cf)[0] + ".o"
            out2 = os.path.splitext(cf)[0] + ".Po"
            out2_dir, out2_file = out2 = os.path.split(out2)
            out2 = os.path.join(out2_dir, ".deps", out2_file)
            if not os.path.exists(out1) and not os.path.exists(out2):
                print("bad_c: ", cf)
        '''

print("\nC/C++ Headers CMake doesnt know about...")
for hf in sorted(source_list(SOURCE_DIR, is_c_header)):
    if not is_ignore(hf):
        if hf not in global_h:
            print("missing_h: ", hf)

if UTF8_CHECK:
    # test encoding
    import traceback
    for files in (global_c, global_h):
        for f in sorted(files):
            if os.path.exists(f):
                # ignore outside of our source tree
                if "extern" not in f:
                    i = 1
                    try:
                        for l in open(f, "r", encoding="utf8"):
                            i += 1
                    except:
                        print("Non utf8: %s:%d" % (f, i))
                        if i > 1:
                            traceback.print_exc()
