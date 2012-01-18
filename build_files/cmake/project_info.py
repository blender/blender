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

__all__ = (
    "SIMPLE_PROJECTFILE",
    "SOURCE_DIR",
    "CMAKE_DIR",
    "PROJECT_DIR",
    "source_list",
    "is_project_file",
    "is_c_header",
    "is_py",
    "cmake_advanced_info",
    "cmake_compiler_defines",
    "project_name_get"
)


import sys
if not sys.version.startswith("3"):
    print("\nPython3.x needed, found %s.\nAborting!\n" %
          sys.version.partition(" ")[0])
    sys.exit(1)


import os
from os.path import join, dirname, normpath, abspath, splitext, exists

SOURCE_DIR = join(dirname(__file__), "..", "..")
SOURCE_DIR = normpath(SOURCE_DIR)
SOURCE_DIR = abspath(SOURCE_DIR)

SIMPLE_PROJECTFILE = False

# get cmake path
CMAKE_DIR = sys.argv[-1]

if not exists(join(CMAKE_DIR, "CMakeCache.txt")):
    CMAKE_DIR = os.getcwd()
if not exists(join(CMAKE_DIR, "CMakeCache.txt")):
    print("CMakeCache.txt not found in %r or %r\n    Pass CMake build dir as an argument, or run from that dir, aborting" % (CMAKE_DIR, os.getcwd()))
    sys.exit(1)


# could be either.
# PROJECT_DIR = SOURCE_DIR
PROJECT_DIR = CMAKE_DIR


def source_list(path, filename_check=None):
    for dirpath, dirnames, filenames in os.walk(path):

        # skip '.svn'
        if dirpath.startswith("."):
            continue

        for filename in filenames:
            filepath = join(dirpath, filename)
            if filename_check is None or filename_check(filepath):
                yield filepath


# extension checking
def is_cmake(filename):
    ext = splitext(filename)[1]
    return (ext == ".cmake") or (filename.endswith("CMakeLists.txt"))


def is_c_header(filename):
    ext = splitext(filename)[1]
    return (ext in (".h", ".hpp", ".hxx"))


def is_py(filename):
    ext = splitext(filename)[1]
    return (ext == ".py")


def is_glsl(filename):
    ext = splitext(filename)[1]
    return (ext == ".glsl")


def is_c(filename):
    ext = splitext(filename)[1]
    return (ext in (".c", ".cpp", ".cxx", ".m", ".mm", ".rc", ".cc", ".inl"))


def is_c_any(filename):
    return is_c(filename) or is_c_header(filename)


def is_svn_file(filename):
    dn, fn = os.path.split(filename)
    filename_svn = join(dn, ".svn", "text-base", "%s.svn-base" % fn)
    return exists(filename_svn)


def is_project_file(filename):
    return (is_c_any(filename) or is_cmake(filename) or is_glsl(filename))  # and is_svn_file(filename)


def cmake_advanced_info():
    """ Extracr includes and defines from cmake.
    """

    def create_eclipse_project():
        print("CMAKE_DIR %r" % CMAKE_DIR)
        if sys.platform == "win32":
            cmd = 'cmake "%s" -G"Eclipse CDT4 - MinGW Makefiles"' % CMAKE_DIR
        else:
            cmd = 'cmake "%s" -G"Eclipse CDT4 - Unix Makefiles"' % CMAKE_DIR

        os.system(cmd)

    includes = []
    defines = []

    create_eclipse_project()

    from xml.dom.minidom import parse
    tree = parse(join(CMAKE_DIR, ".cproject"))

    # to check on nicer xml
    # f = open(".cproject_pretty", 'w')
    # f.write(tree.toprettyxml(indent="    ", newl=""))

    ELEMENT_NODE = tree.ELEMENT_NODE

    cproject, = tree.getElementsByTagName("cproject")
    for storage in cproject.childNodes:
        if storage.nodeType != ELEMENT_NODE:
            continue

        if storage.attributes["moduleId"].value == "org.eclipse.cdt.core.settings":
            cconfig = storage.getElementsByTagName("cconfiguration")[0]
            for substorage in cconfig.childNodes:
                if substorage.nodeType != ELEMENT_NODE:
                    continue

                moduleId = substorage.attributes["moduleId"].value

                # org.eclipse.cdt.core.settings
                # org.eclipse.cdt.core.language.mapping
                # org.eclipse.cdt.core.externalSettings
                # org.eclipse.cdt.core.pathentry
                # org.eclipse.cdt.make.core.buildtargets

                if moduleId == "org.eclipse.cdt.core.pathentry":
                    for path in substorage.childNodes:
                        if path.nodeType != ELEMENT_NODE:
                            continue
                        kind = path.attributes["kind"].value

                        if kind == "mac":
                            # <pathentry kind="mac" name="PREFIX" path="" value="&quot;/opt/blender25&quot;"/>
                            defines.append((path.attributes["name"].value, path.attributes["value"].value))
                        elif kind == "inc":
                            # <pathentry include="/data/src/blender/blender/source/blender/editors/include" kind="inc" path="" system="true"/>
                            includes.append(path.attributes["include"].value)
                        else:
                            pass

    return includes, defines


def cmake_cache_var(var):
    cache_file = open(join(CMAKE_DIR, "CMakeCache.txt"), encoding='utf-8')
    lines = [l_strip for l in cache_file for l_strip in (l.strip(),) if l_strip if not l_strip.startswith("//") if not l_strip.startswith("#")]
    cache_file.close()

    for l in lines:
        if l.split(":")[0] == var:
            return l.split("=", 1)[-1]
    return None


def cmake_compiler_defines():
    compiler = cmake_cache_var("CMAKE_C_COMPILER")  # could do CXX too

    if compiler is None:
        print("Couldn't find the compiler, os defines will be omitted...")
        return

    import tempfile
    temp_c = tempfile.mkstemp(suffix=".c")[1]
    temp_def = tempfile.mkstemp(suffix=".def")[1]

    os.system("%s -dM -E %s > %s" % (compiler, temp_c, temp_def))

    temp_def_file = open(temp_def)
    lines = [l.strip() for l in temp_def_file if l.strip()]
    temp_def_file.close()

    os.remove(temp_c)
    os.remove(temp_def)
    return lines


def project_name_get(path, fallback="Blender", prefix="Blender_"):
    if not os.path.isdir(os.path.join(path, ".svn")):
        return fallback

    import subprocess
    info = subprocess.Popen(["svn", "info", path],
                            stdout=subprocess.PIPE).communicate()[0]
    # string version, we only want the URL
    info = info.decode(encoding="utf-8", errors="ignore")

    for l in info.split("\n"):
        l = l.strip()
        if l.startswith("URL"):
            # https://svn.blender.org/svnroot/bf-blender/branches/bmesh/blender
            # --> bmesh
            if "/branches/" in l:
                return prefix + l.rsplit("/branches/", 1)[-1].split("/", 1)[0]
    return fallback
