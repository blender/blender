#!/usr/bin/python
# -*- coding: utf-8 -*-

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
# The Original Code is Copyright (C) 2012 Blender Foundation.
# All rights reserved.
#
# Contributor(s): Jeroen Bakker
#
# ***** END GPL LICENCE BLOCK *****

# <pep8 compliant>

import sys
import os

if len(sys.argv) < 2:
    sys.stdout.write("Usage: clkernelstoh <cl_file>\n")
    sys.exit(1)

filename = sys.argv[1]

try:
    fpin = open(filename, "r")
except:
    sys.stdout.write("Unable to open input %s\n" % sys.argv[1])
    sys.exit(1)

if filename[0:2] == "." + os.sep:
    filename = filename[2:]

cname = filename + ".h"
sys.stdout.write("Making H file <%s>\n" % cname)

filename = filename.split("/")[-1].split("\\")[-1]
filename = filename.replace(".", "_")

try:
    fpout = open(cname, "w")
except:
    sys.stdout.write("Unable to open output %s\n" % cname)
    sys.exit(1)

fpout.write("/* clkernelstoh output of file <%s> */\n\n" % filename)
fpout.write("const char * clkernelstoh_%s = " % filename)

lines = fpin.readlines()
for line in lines:
    fpout.write("\"")
    fpout.write(line.rstrip())
    fpout.write("\\n\" \\\n")
fpout.write("\"\\0\";\n")

fpin.close()
fpout.close()
