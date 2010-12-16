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

import os
from os.path import join, dirname, normpath, abspath, splitext, relpath, exists

base = join(os.path.dirname(__file__), "..", "..")
base = normpath(base)
base = abspath(base)

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
def is_c_header(filename):
    ext = splitext(filename)[1]
    return (ext in (".h", ".hpp", ".hxx"))

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

def is_svn_file(filename):
	dn, fn = os.path.split(filename)
	filename_svn = join(dn, ".svn", "text-base", "%s.svn-base" % fn)
	return exists(filename_svn)

def is_project_file(filename):
	return (is_c_any(filename) or is_cmake(filename)) and is_svn_file(filename)

files = list(source_list(base, filename_check=is_project_file))
files_rel = [relpath(f, start=base) for f in files]
files_rel.sort()


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

print("Project file written to: %s" % qtc_prj)
# --- end
