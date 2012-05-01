#!/usr/bin/env python3.2

# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>


def print_help(targets):
    print("CMake quicky wrapper, no valid targets given.")
    print(" * targets can contain a subset of the full target name.")
    print(" * arguments with a '-' prefix are passed onto make.")
    print(" * this must run from the cmake build dir")
    print(" * alias this with a short command for speedy access, in bash:")
    print("   alias mk='../blender/build_files/cmake/example_scripts/make_quicky.py'")
    print("")
    print(" eg: make_quicky.py -j3 extern python")
    print(" ...will execute")
    print(" make -j3 extern_binreloc extern_glew bf_python bf_python_ext blender/fast")
    print("")
    print("Target List:")
    for t in targets:
        print("    %s" % t)
    print("...exiting")


def main():
    targets = set()

    # collect targets
    makefile = open("Makefile", "r")
    for line in makefile:
        line = line.rstrip()
        if not line or line[0] in ". \t@$#":
            continue

        line = line.split("#", 1)[0]
        if ":" not in line:
            continue

        line = line.split(":", 1)[0]

        if "/" in line:  # cmake terget options, dont need these
            continue

        targets.add(line)
    makefile.close()

    # remove cmake targets
    bad = set([
        "help",
        "clean",
        "all",
        "preinstall",
        "install",
        "default_target",
        "edit_cache",
        "cmake_force",
        "rebuild_cache",
        "depend",
        "cmake_check_build_system",
        ])

    targets -= set(bad)

    # parse args
    targets = list(targets)
    targets.sort()

    import sys
    if len(sys.argv) == 1:
        print_help(targets)
        return

    targets_new = []
    args = []
    for arg in sys.argv[1:]:
        if arg[0] in "/-":
            args.append(arg)
        else:
            found = False
            for t in targets:
                if arg in t and t not in targets_new:
                    targets_new.append(t)
                    found = True

            if not found:
                print("Error '%s' not found in...")
                for t in targets:
                    print("    %s" % t)
                print("...aborting.")
                return

    # execute
    cmd = "make %s %s blender/fast" % (" ".join(args), " ".join(targets_new))
    print("cmake building with targets: %s" % " ".join(targets_new))
    print("executing: %s" % cmd)

    import os
    os.system(cmd)

if __name__ == "__main__":
    main()
