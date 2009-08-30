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
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# The Original Code is Copyright (C) 2009 Blender Foundation.
# All rights reserved.
#
# Contributor(s): Jörg Müller
#
# ***** END GPL LICENCE BLOCK *****

import sys, os

if len(sys.argv) < 2:
	sys.stdout.write("Usage: datatoc <data_file>\n")
	sys.exit(1)

filename = sys.argv[1]

try:
	fpin = open(filename, "rb");
except:
	sys.stdout.write("Unable to open input %s\n" % sys.argv[1])
	sys.exit(1)

fpin.seek(0, os.SEEK_END)
size = fpin.tell()
fpin.seek(0)

if filename[0] == ".":
	filename = filename[1:]

cname = filename + ".c"
sys.stdout.write("Making C file <%s>\n" % cname)

filename = filename.replace(".", "_")
sys.stdout.write(str(size))
sys.stdout.write("\n")
try:
	fpout = open(cname, "w")
except:
	sys.stdout.write("Unable to open output %s\n" % cname)
	sys.exit(1)

fpout.write("/* DataToC output of file <%s> */\n\n" % filename)
fpout.write("int datatoc_%s_size= %d;\n" % (filename, size))

fpout.write("char datatoc_%s[]= {\n" % filename)

while size > 0:
	size -= 1
	if size % 32 == 31:
		fpout.write("\n")
	
	fpout.write("%3d," % ord(fpin.read(1)))

fpout.write("\n  0};\n\n")

fpin.close()
fpout.close()
