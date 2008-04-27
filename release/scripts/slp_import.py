#!BPY

"""
Name: 'Pro Engineer (.slp)...'
Blender: 232
Group: 'Import'
Tooltip: 'Import Pro Engineer (.slp) File Format'
"""

__author__ = "Anthony D'Agostino (Scorpius)"
__url__ = ("blender", "blenderartists.org",
"Author's homepage, http://www.redrival.com/scorpius")
__version__ = "Part of IOSuite 0.5"

__bpydoc__ = """\
This script imports Pro Engineer files to Blender.

This format can be exported from Pro/Engineer and most other CAD
applications. Written at the request of a Blender user. It is almost
identical to RAW format.

Usage:<br>
	Execute this script from the "File->Import" menu and choose an SLP file to
open.

Notes:<br>
	Generates the standard verts and faces lists, but without duplicate
verts. Only *exact* duplicates are removed, there is no way to specify a
tolerance.
"""

# $Id$
#
# +---------------------------------------------------------+
# | Copyright (c) 2004 Anthony D'Agostino                   |
# | http://www.redrival.com/scorpius                        |
# | scorpius@netzero.com                                    |
# | May 3, 2004                                             |
# | Read and write SLP Triangle File Format (*.slp)         |
# +---------------------------------------------------------+

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
# ***** END GPL LICENCE BLOCK *****

import Blender, meshtools
#import time

# ================================
# === Read SLP Triangle Format ===
# ================================
def read(filename):
	#start = time.clock()
	file = open(filename, "rb")

	raw = []
	for line in file: #.xreadlines():
		data = line.split()
		if data[0] == "vertex":
			vert = map(float, data[1:])
			raw.append(vert)
	
	tri = []
	for i in xrange(0, len(raw), 3):
		tri.append(raw[i] + raw[i+1] + raw[i+2])

	#$import pprint; pprint.pprint(tri)

	# Collect data from RAW format
	faces = []
	for line in tri:
		f1, f2, f3, f4, f5, f6, f7, f8, f9 = line
		faces.append([(f1, f2, f3), (f4, f5, f6), (f7, f8, f9)])

	# Generate verts and faces lists, without duplicates
	verts = []
	coords = {}
	index = 0
	for i in xrange(len(faces)):
		for j in xrange(len(faces[i])):
			vertex = faces[i][j]
			if not coords.has_key(vertex):
				coords[vertex] = index
				index += 1
				verts.append(vertex)
			faces[i][j] = coords[vertex]

	objname = Blender.sys.splitext(Blender.sys.basename(filename))[0]

	meshtools.create_mesh(verts, faces, objname)
	Blender.Window.DrawProgressBar(1.0, '')  # clear progressbar
	file.close()
	#end = time.clock()
	#seconds = " in %.2f %s" % (end-start, "seconds")
	message = "Successfully imported " + Blender.sys.basename(filename)# + seconds
	meshtools.print_boxed(message)

def fs_callback(filename):
	read(filename)

if __name__ == '__main__':
	Blender.Window.FileSelector(fs_callback, "Import SLP", "*.slp")
