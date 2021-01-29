#!/usr/bin/env python3

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

'''
This script generates the blender.1 man page, embedding the help text
from the Blender executable itself. Invoke it as follows:

    blender.1.py <path-to-blender> <output-filename>

where <path-to-blender> is the path to the Blender executable,
and <output-filename> is where to write the generated man page.
'''

# <pep8 compliant>

import os
import subprocess
import sys

import time


def man_format(data):
    data = data.replace("-", "\\-")
    data = data.replace("\t", "  ")
    return data


if len(sys.argv) != 3:
    import getopt
    raise getopt.GetoptError("Usage: %s <path-to-blender> <output-filename>" % sys.argv[0])

blender_bin = sys.argv[1]
outfilename = sys.argv[2]

cmd = [blender_bin, "--help"]
print("  executing:", " ".join(cmd))
ASAN_OPTIONS = "exitcode=0:" + os.environ.get("ASAN_OPTIONS", "")
blender_help = subprocess.run(
    cmd, env={"ASAN_OPTIONS": ASAN_OPTIONS}, check=True, stdout=subprocess.PIPE).stdout.decode(encoding="utf-8")
blender_version = subprocess.run(
    [blender_bin, "--version"], env={"ASAN_OPTIONS": ASAN_OPTIONS}, check=True, stdout=subprocess.PIPE).stdout.decode(encoding="utf-8").strip()
blender_version, blender_date = (blender_version.split("build") + [None, None])[0:2]
blender_version = blender_version.rstrip().partition(" ")[2]  # remove 'Blender' prefix.
if blender_date is None:
    # Happens when built without WITH_BUILD_INFO e.g.
    date_string = time.strftime("%B %d, %Y", time.gmtime(int(os.environ.get('SOURCE_DATE_EPOCH', time.time()))))
else:
    blender_date = blender_date.strip().partition(" ")[2]  # remove 'date:' prefix
    date_string = time.strftime("%B %d, %Y", time.strptime(blender_date, "%Y-%m-%d"))

outfile = open(outfilename, "w")
fw = outfile.write

fw('.TH "BLENDER" "1" "%s" "Blender %s"\n' % (date_string, blender_version.replace(".", "\\&.")))

fw('''
.SH NAME
blender \- a full-featured 3D application''')

fw('''
.SH SYNOPSIS
.B blender [args ...] [file] [args ...]''')

fw('''
.br
.SH DESCRIPTION
.PP
.B blender
is a full-featured 3D application. It supports the entirety of the 3D pipeline - modeling, rigging, animation, simulation, rendering, compositing, motion tracking, and video editing.

Use Blender to create 3D images and animations, films and commercials, content for games, architectural and industrial visualizatons, and scientific visualizations.

https://www.blender.org''')

fw('''
.SH OPTIONS''')

fw("\n\n")

lines = [line.rstrip() for line in blender_help.split("\n")]

while lines:
    l = lines.pop(0)
    if l.startswith("Environment Variables:"):
        fw('.SH "ENVIRONMENT VARIABLES"\n')
    elif l.endswith(":"):  # one line
        fw('.SS "%s"\n\n' % l)
    elif l.startswith("-") or l.startswith("/"):  # can be multi line

        fw('.TP\n')
        fw('.B %s\n' % man_format(l))

        while lines:
            # line with no
            if lines[0].strip() and len(lines[0].lstrip()) == len(lines[0]):  # no white space
                break

            if not l:  # second blank line
                fw('.IP\n')
            else:
                fw('.br\n')

            l = lines.pop(0)
            l = l[1:]  # remove first whitespace (tab)

            fw('%s\n' % man_format(l))

    else:
        if not l.strip():
            fw('.br\n')
        else:
            fw('%s\n' % man_format(l))

# footer

fw('''
.br
.SH SEE ALSO
.B luxrender(1)

.br
.SH AUTHORS
This manpage was written for a Debian GNU/Linux system by Daniel Mester
<mester@uni-bremen.de> and updated by Cyril Brulebois
<cyril.brulebois@enst-bretagne.fr> and Dan Eicher <dan@trollwerks.org>.
''')

outfile.close()
print("written:", outfilename)
