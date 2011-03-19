#!/usr/bin/env python

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

"""
Exampel Win32 usage:
 c:\Python32\python.exe c:\blender_dev\blender\build_files\cmake\cmake_qtcreator_project.py c:\blender_dev\cmake_build
"""

import os
from os.path import join, dirname, normpath, abspath, splitext, relpath, exists

base = join(os.path.dirname(__file__), "..", "..")
base = normpath(base)
base = abspath(base)

SIMPLE_PROJECTFILE = False


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


def is_c(filename):
    ext = splitext(filename)[1]
    return (ext in (".c", ".cpp", ".cxx", ".m", ".mm", ".rc"))


def is_c_any(filename):
    return is_c(filename) or is_c_header(filename)


def is_svn_file(filename):
    dn, fn = os.path.split(filename)
    filename_svn = join(dn, ".svn", "text-base", "%s.svn-base" % fn)
    return exists(filename_svn)


def is_project_file(filename):
    return (is_c_any(filename) or is_cmake(filename))  # and is_svn_file(filename)


def cmake_advanced_info():
    """ Extracr includes and defines from cmake.
    """

    def create_eclipse_project(cmake_dir):
        import sys
        if sys.platform == "win32":
            cmd = 'cmake %r -G"Eclipse CDT4 - MinGW Makefiles"' % cmake_dir
        else:
            cmd = 'cmake %r -G"Eclipse CDT4 - Unix Makefiles"' % cmake_dir

        os.system(cmd)

    includes = []
    defines = []

    import os
    import sys

    cmake_dir = sys.argv[-1]

    if not os.path.exists(os.path.join(cmake_dir, "CMakeCache.txt")):
        cmake_dir = os.getcwd()
    if not os.path.exists(os.path.join(cmake_dir, "CMakeCache.txt")):
        print("CMakeCache.txt not found in %r or %r\n    Pass CMake build dir as an argument, or run from that dir, abording" % (cmake_dir, os.getcwd()))
        sys.exit(1)

    create_eclipse_project(cmake_dir)

    from xml.dom.minidom import parse
    tree = parse(os.path.join(cmake_dir, ".cproject"))
    '''
    f = open(".cproject_pretty", 'w')
    f.write(tree.toprettyxml(indent="    ", newl=""))
    '''
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


def main():
    files = list(source_list(base, filename_check=is_project_file))
    files_rel = [relpath(f, start=base) for f in files]
    files_rel.sort()

    # --- qtcreator spesific, simple format
    if SIMPLE_PROJECTFILE:
        # --- qtcreator spesific, simple format
        PROJECT_NAME = "Blender"
        f = open(join(base, "%s.files" % PROJECT_NAME), 'w')
        f.write("\n".join(files_rel))

        f = open(join(base, "%s.includes" % PROJECT_NAME), 'w')
        f.write("\n".join(sorted(list(set(dirname(f) for f in files_rel if is_c_header(f))))))

        qtc_prj = join(base, "%s.creator" % PROJECT_NAME)
        f = open(qtc_prj, 'w')
        f.write("[General]\n")

        qtc_cfg = join(base, "%s.config" % PROJECT_NAME)
        if not exists(qtc_cfg):
            f = open(qtc_cfg, 'w')
            f.write("// ADD PREDEFINED MACROS HERE!\n")
    else:
        includes, defines = cmake_advanced_info()

        # for some reason it doesnt give all internal includes
        includes = list(set(includes) | set(dirname(f) for f in files_rel if is_c_header(f)))
        includes.sort()

        PROJECT_NAME = "Blender"
        f = open(join(base, "%s.files" % PROJECT_NAME), 'w')
        f.write("\n".join(files_rel))

        f = open(join(base, "%s.includes" % PROJECT_NAME), 'w')
        f.write("\n".join(sorted(includes)))

        qtc_prj = join(base, "%s.creator" % PROJECT_NAME)
        f = open(qtc_prj, 'w')
        f.write("[General]\n")

        qtc_cfg = join(base, "%s.config" % PROJECT_NAME)
        f = open(qtc_cfg, 'w')
        f.write("// ADD PREDEFINED MACROS HERE!\n")
        f.write("\n".join([("#define %s %s" % item) for item in defines]))

    print("Project file written to: %s" % qtc_prj)
    # --- end

if __name__ == "__main__":
    main()
