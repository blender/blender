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
# The Original Code is Copyright (C) 2009 Blender Foundation.
# All rights reserved.
#
# Contributor(s): Campbell Barton
#
# ***** END GPL LICENCE BLOCK *****


# <pep8 compliant>

import sys

if len(sys.argv) < 2:
    sys.stdout.write("Usage: ctodata <c_file>\n")
    sys.exit(1)

filename = sys.argv[1]

try:
    fpin = open(filename, "r")
except:
    sys.stdout.write("Unable to open input %s\n" % sys.argv[1])
    sys.exit(1)

data = fpin.read().rsplit("{")[-1].split("}")[0]
data = data.replace(",", " ")
data = data.split()
data = bytes([int(v) for v in data])

dname = filename + ".ctodata"

sys.stdout.write("Making DATA file <%s>\n" % dname)

try:
    fpout = open(dname, "wb")
except:
    sys.stdout.write("Unable to open output %s\n" % dname)
    sys.exit(1)

size = fpout.write(data)

sys.stdout.write("%d\n" % size)
