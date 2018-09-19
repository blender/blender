#!/usr/bin/env python3

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
# Contributor(s): Campbell Barton, M.G. Kishalmi
#
# ***** END GPL LICENSE BLOCK *****

# <pep8 compliant>

r"""
Example Linux usage:
 python ~/blender-git/blender/build_files/cmake/cmake_qtcreator_project.py --build-dir ~/blender-git/cmake

Example Win32 usage:
 c:\Python32\python.exe c:\blender_dev\blender\build_files\cmake\cmake_qtcreator_project.py --build-dir c:\blender_dev\cmake_build
"""


import os


def quote_define(define):
    if " " in define.strip():
        return '"%s"' % define
    else:
        return define


def create_qtc_project_main(name):
    from project_info import (
        SIMPLE_PROJECTFILE,
        SOURCE_DIR,
        # CMAKE_DIR,
        PROJECT_DIR,
        source_list,
        is_project_file,
        is_c_header,
        cmake_advanced_info,
        cmake_compiler_defines,
        project_name_get,
    )

    files = list(source_list(SOURCE_DIR, filename_check=is_project_file))
    files_rel = [os.path.relpath(f, start=PROJECT_DIR) for f in files]
    files_rel.sort()

    # --- qtcreator specific, simple format
    if SIMPLE_PROJECTFILE:
        # --- qtcreator specific, simple format
        PROJECT_NAME = name or "Blender"
        FILE_NAME = PROJECT_NAME.lower()
        with open(os.path.join(PROJECT_DIR, "%s.files" % FILE_NAME), 'w') as f:
            f.write("\n".join(files_rel))

        with open(os.path.join(PROJECT_DIR, "%s.includes" % FILE_NAME), 'w') as f:
            f.write("\n".join(sorted(list(set(os.path.dirname(f)
                                              for f in files_rel if is_c_header(f))))))

        qtc_prj = os.path.join(PROJECT_DIR, "%s.creator" % FILE_NAME)
        with open(qtc_prj, 'w') as f:
            f.write("[General]\n")

        qtc_cfg = os.path.join(PROJECT_DIR, "%s.config" % FILE_NAME)
        if not os.path.exists(qtc_cfg):
            with open(qtc_cfg, 'w') as f:
                f.write("// ADD PREDEFINED MACROS HERE!\n")
    else:
        includes, defines = cmake_advanced_info()

        if (includes, defines) == (None, None):
            return

        # for some reason it doesn't give all internal includes
        includes = list(set(includes) | set(os.path.dirname(f)
                                            for f in files_rel if is_c_header(f)))
        includes.sort()

        # be tricky, get the project name from CMake if we can!
        PROJECT_NAME = name or project_name_get()

        FILE_NAME = PROJECT_NAME.lower()
        with open(os.path.join(PROJECT_DIR, "%s.files" % FILE_NAME), 'w') as f:
            f.write("\n".join(files_rel))

        with open(os.path.join(PROJECT_DIR, "%s.includes" % FILE_NAME), 'w', encoding='utf-8') as f:
            f.write("\n".join(sorted(includes)))

        qtc_prj = os.path.join(PROJECT_DIR, "%s.creator" % FILE_NAME)
        with open(qtc_prj, 'w') as f:
            f.write("[General]\n")

        qtc_cfg = os.path.join(PROJECT_DIR, "%s.config" % FILE_NAME)
        with open(qtc_cfg, 'w') as f:
            f.write("// ADD PREDEFINED MACROS TO %s_custom.config!\n" % FILE_NAME)

            qtc_custom_cfg = os.path.join(PROJECT_DIR, "%s_custom.config" % FILE_NAME)
            if os.path.exists(qtc_custom_cfg):
                with open(qtc_custom_cfg, 'r') as fc:
                    f.write(fc.read())
                    f.write("\n")

            defines_final = [("#define %s %s" % (item[0], quote_define(item[1]))) for item in defines]
            if os.name != "nt":
                defines_final += cmake_compiler_defines()
            f.write("\n".join(defines_final))

    print("Blender project file written to: %r" % qtc_prj)
    # --- end


def create_qtc_project_python(name):
    from project_info import (
        SOURCE_DIR,
        # CMAKE_DIR,
        PROJECT_DIR,
        source_list,
        is_py,
        project_name_get,
    )

    files = list(source_list(SOURCE_DIR, filename_check=is_py))
    files_rel = [os.path.relpath(f, start=PROJECT_DIR) for f in files]
    files_rel.sort()

    # --- qtcreator specific, simple format
    # be tricky, get the project name from git if we can!
    PROJECT_NAME = (name or project_name_get()) + "_Python"

    FILE_NAME = PROJECT_NAME.lower()
    with open(os.path.join(PROJECT_DIR, "%s.files" % FILE_NAME), 'w') as f:
        f.write("\n".join(files_rel))

    qtc_prj = os.path.join(PROJECT_DIR, "%s.creator" % FILE_NAME)
    with open(qtc_prj, 'w') as f:
        f.write("[General]\n")

    qtc_cfg = os.path.join(PROJECT_DIR, "%s.config" % FILE_NAME)
    if not os.path.exists(qtc_cfg):
        with open(qtc_cfg, 'w') as f:
            f.write("// ADD PREDEFINED MACROS HERE!\n")

    print("Python project file written to:  %r" % qtc_prj)


def argparse_create():
    import argparse

    parser = argparse.ArgumentParser(
        description="This script generates Qt Creator project files for Blender",
    )

    parser.add_argument(
        "-n", "--name",
        dest="name",
        metavar='NAME', type=str,
        help="Override default project name (\"Blender\")",
        required=False,
    )

    parser.add_argument(
        "-b", "--build-dir",
        dest="build_dir",
        metavar='BUILD_DIR', type=str,
        help="Specify the build path (or fallback to the $PWD)",
        required=False,
    )

    return parser


def main():
    parser = argparse_create()
    args = parser.parse_args()
    name = args.name

    import project_info
    if not project_info.init(args.build_dir):
        return

    create_qtc_project_main(name)
    create_qtc_project_python(name)


if __name__ == "__main__":
    main()
