#!/usr/bin/python

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

import subprocess
import os
import sys

import time
import datetime


def man_format(data):
    data = data.replace("-", "\\-")
    data = data.replace("\t", "  ")
    return data

# allow passing blender as argument
if sys.argv[-1].endswith(os.sep + "blender"):
    blender_bin = sys.argv[-1]
else:
    blender_bin = os.path.join(os.path.dirname(__file__), "../../blender.bin")

cmd = [blender_bin, "--help"]
print("  executing:", " ".join(cmd))
blender_help = subprocess.Popen(cmd, stdout=subprocess.PIPE).communicate()[0].decode(encoding="utf-8")

blender_version = subprocess.Popen([blender_bin, "--version"], stdout=subprocess.PIPE).communicate()[0].decode(encoding="utf-8").strip()
blender_version = blender_version.split("Build")[0]

date_string = datetime.date.fromtimestamp(time.time()).strftime("%B %d, %Y")

filepath = os.path.splitext(__file__)[0]

file = open(filepath, "w")

fw = file.write

fw('.TH "BLENDER" "1" "%s" "Blender %s"\n' % (date_string, blender_version.replace(".", "\\&.")))

fw('''
.SH NAME
blender \- a 3D modelling and rendering package''')

fw('''
.SH SYNOPSIS
.B blender [args ...] [file] [args ...]''')

fw('''
.br
.SH DESCRIPTION
.PP
.B blender
is a 3D modelling and rendering package. Originating as the in-house software of a high quality animation studio, Blender has proven to be an extremely fast and versatile design instrument. The software has a personal touch, offering a unique approach to the world of Three Dimensions.

Use Blender to create TV commercials, to make technical visualizations, business graphics, to create content for games, or design user interfaces. You can easy build and manage complex environments. The renderer is versatile and extremely fast. All basic animation principles (curves & keys) are well implemented.

http://www.blender.org''')

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

print("written:", filepath)
