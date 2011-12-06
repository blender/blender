#!/usr/bin/env python

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

"""
Example Win32 usage:
 c:\Python32\python.exe c:\blender_dev\blender\build_files\cmake\cmake_qtcreator_project.py c:\blender_dev\cmake_build

example linux usage
 python .~/blenderSVN/blender/build_files/cmake/cmake_qtcreator_project.py ~/blenderSVN/cmake
"""

from project_info import (SIMPLE_PROJECTFILE,
                          SOURCE_DIR,
                          # CMAKE_DIR,
                          PROJECT_DIR,
                          source_list,
                          is_project_file,
                          is_c_header,
                          is_py,
                          cmake_advanced_info,
                          cmake_compiler_defines,
                          project_name_get,
                          )

import os
import sys


def quote_define(define):
    if " " in define.strip():
        return '"%s"' % define
    else:
        return define


def create_qtc_project_main():
    files = list(source_list(SOURCE_DIR, filename_check=is_project_file))
    files_rel = [os.path.relpath(f, start=PROJECT_DIR) for f in files]
    files_rel.sort()

    # --- qtcreator specific, simple format
    if SIMPLE_PROJECTFILE:
        # --- qtcreator specific, simple format
        PROJECT_NAME = "Blender"
        f = open(os.path.join(PROJECT_DIR, "%s.files" % PROJECT_NAME), 'w')
        f.write("\n".join(files_rel))

        f = open(os.path.join(PROJECT_DIR, "%s.includes" % PROJECT_NAME), 'w')
        f.write("\n".join(sorted(list(set(os.path.dirname(f)
                          for f in files_rel if is_c_header(f))))))

        qtc_prj = os.path.join(PROJECT_DIR, "%s.creator" % PROJECT_NAME)
        f = open(qtc_prj, 'w')
        f.write("[General]\n")

        qtc_cfg = os.path.join(PROJECT_DIR, "%s.config" % PROJECT_NAME)
        if not os.path.exists(qtc_cfg):
            f = open(qtc_cfg, 'w')
            f.write("// ADD PREDEFINED MACROS HERE!\n")
    else:
        includes, defines = cmake_advanced_info()

        # for some reason it doesnt give all internal includes
        includes = list(set(includes) | set(os.path.dirname(f)
                        for f in files_rel if is_c_header(f)))
        includes.sort()

        if 0:
            PROJECT_NAME = "Blender"
        else:
            # be tricky, get the project name from SVN if we can!
            PROJECT_NAME = project_name_get(SOURCE_DIR)

        FILE_NAME = PROJECT_NAME.lower()
        f = open(os.path.join(PROJECT_DIR, "%s.files" % FILE_NAME), 'w')
        f.write("\n".join(files_rel))

        f = open(os.path.join(PROJECT_DIR, "%s.includes" % FILE_NAME), 'w', encoding='utf-8')
        f.write("\n".join(sorted(includes)))

        qtc_prj = os.path.join(PROJECT_DIR, "%s.creator" % FILE_NAME)
        f = open(qtc_prj, 'w')
        f.write("[General]\n")

        qtc_cfg = os.path.join(PROJECT_DIR, "%s.config" % FILE_NAME)
        f = open(qtc_cfg, 'w')
        f.write("// ADD PREDEFINED MACROS HERE!\n")
        defines_final = [("#define %s %s" % (item[0], quote_define(item[1]))) for item in defines]
        if sys.platform != "win32":
            defines_final += cmake_compiler_defines()
        f.write("\n".join(defines_final))

    print("Blender project file written to: %r" % qtc_prj)
    # --- end


def create_qtc_project_python():
    files = list(source_list(SOURCE_DIR, filename_check=is_py))
    files_rel = [os.path.relpath(f, start=PROJECT_DIR) for f in files]
    files_rel.sort()

    # --- qtcreator specific, simple format
    if 0:
        PROJECT_NAME = "Blender_Python"
    else:
        # be tricky, get the project name from SVN if we can!
        PROJECT_NAME = project_name_get(SOURCE_DIR) + "_Python"

    FILE_NAME = PROJECT_NAME.lower()
    f = open(os.path.join(PROJECT_DIR, "%s.files" % FILE_NAME), 'w')
    f.write("\n".join(files_rel))

    qtc_prj = os.path.join(PROJECT_DIR, "%s.creator" % FILE_NAME)
    f = open(qtc_prj, 'w')
    f.write("[General]\n")

    qtc_cfg = os.path.join(PROJECT_DIR, "%s.config" % FILE_NAME)
    if not os.path.exists(qtc_cfg):
        f = open(qtc_cfg, 'w')
        f.write("// ADD PREDEFINED MACROS HERE!\n")

    print("Python project file written to:  %r" % qtc_prj)


def main():
    create_qtc_project_main()
    create_qtc_project_python()


if __name__ == "__main__":
    main()
