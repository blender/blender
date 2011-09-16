# $Id:
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

__all__ = (
    "build_info",
    "SOURCE_DIR",
    )

import os
import sys
from os.path import join, dirname, normpath, abspath

SOURCE_DIR = join(dirname(__file__), "..", "..")
SOURCE_DIR = normpath(SOURCE_DIR)
SOURCE_DIR = abspath(SOURCE_DIR)


def is_c_header(filename):
    ext = os.path.splitext(filename)[1]
    return (ext in (".h", ".hpp", ".hxx"))


def is_c_header(filename):
    ext = os.path.splitext(filename)[1]
    return (ext in (".h", ".hpp", ".hxx"))


def is_c(filename):
    ext = os.path.splitext(filename)[1]
    return (ext in (".c", ".cpp", ".cxx", ".m", ".mm", ".rc"))


def is_c_any(filename):
    return os.path.s_c(filename) or is_c_header(filename)


# copied from project_info.py
CMAKE_DIR = "."


def cmake_cache_var(var):
    cache_file = open(join(CMAKE_DIR, "CMakeCache.txt"))
    lines = [l_strip for l in cache_file for l_strip in (l.strip(),) if l_strip if not l_strip.startswith("//") if not l_strip.startswith("#")]
    cache_file.close()

    for l in lines:
        if l.split(":")[0] == var:
            return l.split("=", 1)[-1]
    return None


def do_ignore(filepath, ignore_prefix_list):
    if ignore_prefix_list is None:
        return False

    relpath = os.path.relpath(filepath, SOURCE_DIR)
    return any([relpath.startswith(prefix) for prefix in ignore_prefix_list])


def makefile_log():
    import subprocess
    # Check blender is not 2.5x until it supports playback again
    print("running make with --dry-run ...")
    process = subprocess.Popen(["make", "--always-make", "--dry-run", "--keep-going", "VERBOSE=1"],
                                stdout=subprocess.PIPE,
                                )

    while process.poll():
        time.sleep(1)

    out = process.stdout.read()
    process.stdout.close()
    print("done!", len(out), "bytes")
    return out.decode("ascii").split("\n")


def build_info(use_c=True, use_cxx=True, ignore_prefix_list=None):

    makelog = makefile_log()

    source = []

    compilers = []
    if use_c:
        compilers.append(cmake_cache_var("CMAKE_C_COMPILER"))
    if use_cxx:
        compilers.append(cmake_cache_var("CMAKE_CXX_COMPILER"))

    print("compilers:", " ".join(compilers))

    fake_compiler = "%COMPILER%"

    print("parsing make log ...")

    for line in makelog:

        args = line.split()

        if not any([(c in args) for c in compilers]):
            continue

        # join args incase they are not.
        args = ' '.join(args)
        args = args.replace(" -isystem", " -I")
        args = args.replace(" -D ", " -D")
        args = args.replace(" -I ", " -I")

        for c in compilers:
            args = args.replace(c, fake_compiler)
        args = args.split()
        # end

        # remove compiler
        args[:args.index(fake_compiler) + 1] = []

        c_files = [f for f in args if is_c(f)]
        inc_dirs = [f[2:].strip() for f in args if f.startswith('-I')]
        defs = [f[2:].strip() for f in args if f.startswith('-D')]
        for c in sorted(c_files):

            if do_ignore(c, ignore_prefix_list):
                continue

            source.append((c, inc_dirs, defs))

        # safety check that our includes are ok
        for f in inc_dirs:
            if not os.path.exists(f):
                raise Exception("%s missing" % f)

    print("done!")

    return source


def main():
    if not os.path.exists(join(CMAKE_DIR, "CMakeCache.txt")):
        print("This script must run from the cmake build dir")
        return

    for s in build_info():
        print(s)

if __name__ == "__main__":
    main()
