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
# ***** END GPL LICENSE BLOCK *****

# <pep8 compliant>

"""
Example linux usage
 python3 ~/blender-git/blender/build_files/cmake/cmake_netbeans_project.py ~/blender-git/cmake

Windows not supported so far
"""

import sys

# until we have arg parsing
import project_info
if not project_info.init(sys.argv[-1]):
    sys.exit(1)

from project_info import (
    SIMPLE_PROJECTFILE,
    SOURCE_DIR,
    CMAKE_DIR,
    PROJECT_DIR,
    source_list,
    is_project_file,
    is_c_header,
    # is_py,
    cmake_advanced_info,
    cmake_compiler_defines,
    cmake_cache_var,
    project_name_get,
)


import os
from os.path import join, dirname, normpath, relpath, exists


def create_nb_project_main():
    from xml.sax.saxutils import escape

    files = list(source_list(SOURCE_DIR, filename_check=is_project_file))
    files_rel = [relpath(f, start=PROJECT_DIR) for f in files]
    files_rel.sort()

    if SIMPLE_PROJECTFILE:
        pass
    else:
        includes, defines = cmake_advanced_info()

        if (includes, defines) == (None, None):
            return

        # for some reason it doesn't give all internal includes
        includes = list(set(includes) | set(dirname(f) for f in files if is_c_header(f)))
        includes.sort()

        if 0:
            PROJECT_NAME = "Blender"
        else:
            # be tricky, get the project name from git if we can!
            PROJECT_NAME = project_name_get()

        make_exe = cmake_cache_var("CMAKE_MAKE_PROGRAM")
        make_exe_basename = os.path.basename(make_exe)

        # --------------- NB specific
        defines = [("%s=%s" % cdef) if cdef[1] else cdef[0] for cdef in defines]
        defines += [cdef.replace("#define", "").strip() for cdef in cmake_compiler_defines()]

        def file_list_to_nested(files):
            # convert paths to hierarchy
            paths_nested = {}

            def ensure_path(filepath):
                filepath_split = filepath.split(os.sep)

                pn = paths_nested
                for subdir in filepath_split[:-1]:
                    pn = pn.setdefault(subdir, {})
                pn[filepath_split[-1]] = None

            for path in files:
                ensure_path(path)
            return paths_nested

        PROJECT_DIR_NB = join(PROJECT_DIR, "nbproject")
        if not exists(PROJECT_DIR_NB):
            os.mkdir(PROJECT_DIR_NB)

        # SOURCE_DIR_REL = relpath(SOURCE_DIR, PROJECT_DIR)

        f = open(join(PROJECT_DIR_NB, "project.xml"), 'w')

        f.write('<?xml version="1.0" encoding="UTF-8"?>\n')
        f.write('<project xmlns="http://www.netbeans.org/ns/project/1">\n')
        f.write('    <type>org.netbeans.modules.cnd.makeproject</type>\n')
        f.write('    <configuration>\n')
        f.write('        <data xmlns="http://www.netbeans.org/ns/make-project/1">\n')
        f.write('            <name>%s</name>\n' % PROJECT_NAME)
        f.write('            <c-extensions>c,m</c-extensions>\n')
        f.write('            <cpp-extensions>cpp,cxx,cc,mm</cpp-extensions>\n')
        f.write('            <header-extensions>h,hxx,hh,hpp,inl</header-extensions>\n')
        f.write('            <sourceEncoding>UTF-8</sourceEncoding>\n')
        f.write('            <make-dep-projects/>\n')
        f.write('            <sourceRootList>\n')
        f.write('                <sourceRootElem>%s</sourceRootElem>\n' % SOURCE_DIR)  # base_root_rel
        f.write('            </sourceRootList>\n')
        f.write('            <confList>\n')
        f.write('                <confElem>\n')
        f.write('                    <name>Default</name>\n')
        f.write('                    <type>0</type>\n')
        f.write('                </confElem>\n')
        f.write('            </confList>\n')
        f.write('            <formatting>\n')
        f.write('                <project-formatting-style>false</project-formatting-style>\n')
        f.write('            </formatting>\n')
        f.write('        </data>\n')
        f.write('    </configuration>\n')
        f.write('</project>\n')

        f.close()

        f = open(join(PROJECT_DIR_NB, "configurations.xml"), 'w')

        f.write('<?xml version="1.0" encoding="UTF-8"?>\n')
        f.write('<configurationDescriptor version="95">\n')
        f.write('  <logicalFolder name="root" displayName="root" projectFiles="true" kind="ROOT">\n')
        f.write('    <df root="%s" name="0">\n' % SOURCE_DIR)  # base_root_rel

        # write files!
        files_rel_local = [normpath(relpath(join(CMAKE_DIR, path), SOURCE_DIR)) for path in files_rel]
        files_rel_hierarchy = file_list_to_nested(files_rel_local)
        # print(files_rel_hierarchy)

        def write_df(hdir, ident):
            dirs = []
            files = []
            for key, item in sorted(hdir.items()):
                if item is None:
                    files.append(key)
                else:
                    dirs.append((key, item))

            for key, item in dirs:
                f.write('%s  <df name="%s">\n' % (ident, key))
                write_df(item, ident + "    ")
                f.write('%s  </df>\n' % ident)

            for key in files:
                f.write('%s<in>%s</in>\n' % (ident, key))

        write_df(files_rel_hierarchy, ident="    ")

        f.write('    </df>\n')

        f.write('    <logicalFolder name="ExternalFiles"\n')
        f.write('                   displayName="Important Files"\n')
        f.write('                   projectFiles="false"\n')
        f.write('                   kind="IMPORTANT_FILES_FOLDER">\n')
        # f.write('      <itemPath>../GNUmakefile</itemPath>\n')
        f.write('    </logicalFolder>\n')

        f.write('  </logicalFolder>\n')
        # default, but this dir is infact not in blender dir so we can ignore it
        # f.write('  <sourceFolderFilter>^(nbproject)$</sourceFolderFilter>\n')
        f.write('  <sourceFolderFilter>^(nbproject|__pycache__|.*\.py|.*\.html|.*\.blend)$</sourceFolderFilter>\n')

        f.write('  <sourceRootList>\n')
        f.write('    <Elem>%s</Elem>\n' % SOURCE_DIR)  # base_root_rel
        f.write('  </sourceRootList>\n')

        f.write('  <projectmakefile>Makefile</projectmakefile>\n')

        # paths again
        f.write('  <confs>\n')
        f.write('    <conf name="Default" type="0">\n')

        f.write('      <toolsSet>\n')
        f.write('        <compilerSet>default</compilerSet>\n')
        f.write('        <dependencyChecking>false</dependencyChecking>\n')
        f.write('        <rebuildPropChanged>false</rebuildPropChanged>\n')
        f.write('      </toolsSet>\n')
        f.write('      <codeAssistance>\n')
        f.write('      </codeAssistance>\n')
        f.write('      <makefileType>\n')

        f.write('        <makeTool>\n')
        f.write('          <buildCommandWorkingDir>.</buildCommandWorkingDir>\n')

        if make_exe_basename == "ninja":
            build_cmd = "ninja"
            clean_cmd = "ninja -t clean"
        else:
            build_cmd = "${MAKE} -f Makefile"
            clean_cmd = "${MAKE} -f Makefile clean"

        f.write('          <buildCommand>%s</buildCommand>\n' % escape(build_cmd))
        f.write('          <cleanCommand>%s</cleanCommand>\n' % escape(clean_cmd))
        f.write('          <executablePath>./bin/blender</executablePath>\n')
        del build_cmd, clean_cmd

        def write_toolinfo():
            f.write('            <incDir>\n')
            for inc in includes:
                f.write('              <pElem>%s</pElem>\n' % inc)
            f.write('            </incDir>\n')
            f.write('            <preprocessorList>\n')
            for cdef in defines:
                f.write('              <Elem>%s</Elem>\n' % escape(cdef))
            f.write('            </preprocessorList>\n')

        f.write('          <cTool>\n')
        write_toolinfo()
        f.write('          </cTool>\n')

        f.write('          <ccTool>\n')
        write_toolinfo()
        f.write('          </ccTool>\n')

        f.write('        </makeTool>\n')
        f.write('      </makefileType>\n')
        # finished makefile info

        f.write('    \n')

        for path in files_rel_local:
            is_c = path.endswith(".c")
            f.write('      <item path="%s"\n' % path)
            f.write('            ex="false"\n')
            f.write('            tool="%d"\n' % (0 if is_c else 1))
            f.write('            flavor2="%d">\n' % (3 if is_c else 0))
            f.write('      </item>\n')

        f.write('      <runprofile version="9">\n')
        f.write('        <runcommandpicklist>\n')
        f.write('        </runcommandpicklist>\n')
        f.write('        <runcommand>%s</runcommand>\n' % os.path.join(CMAKE_DIR, "bin/blender"))
        f.write('        <rundir>%s</rundir>\n' % SOURCE_DIR)
        f.write('        <buildfirst>false</buildfirst>\n')
        f.write('        <terminal-type>0</terminal-type>\n')
        f.write('        <remove-instrumentation>0</remove-instrumentation>\n')
        f.write('        <environment>\n')
        f.write('        </environment>\n')
        f.write('      </runprofile>\n')

        f.write('    </conf>\n')
        f.write('  </confs>\n')

        # todo

        f.write('</configurationDescriptor>\n')

        f.close()


def main():
    create_nb_project_main()


if __name__ == "__main__":
    main()
