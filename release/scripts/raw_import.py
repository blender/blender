#!BPY

"""
Name: 'Raw Triangle (.raw)...'
Blender: 232
Group: 'Import'
Tooltip: 'Import Raw Triangle File Format (.raw)'
"""

__author__ = "Anthony D'Agostino (Scorpius)"
__url__ = ("blender", "elysiun",
"Author's homepage, http://www.redrival.com/scorpius")
__version__ = "Part of IOSuite 0.5"

__bpydoc__ = """\
This script imports Raw Triangle File format files to Blender.

The raw triangle format is very simple; it has no verts or faces lists.
It's just a simple ascii text file with the vertices of each triangle
listed on each line. There were some very old utilities (when the PovRay
forum was in existence on CompuServe) that preformed operations on these
files.

Usage:<br>
	Execute this script from the "File->Import" menu and choose a Raw file to
open.

Notes:<br>
	Generates the standard verts and faces lists, but without duplicate
verts. Only *exact* duplicates are removed, there is no way to specify a
tolerance.
"""

# $Id$
#
# +---------------------------------------------------------+
# | Copyright (c) 2002 Anthony D'Agostino                   |
# | http://www.redrival.com/scorpius                        |
# | scorpius@netzero.com                                    |
# | April 28, 2002                                          |
# | Released under the Blender Artistic Licence (BAL)       |
# | Import Export Suite v0.5                                |
# +---------------------------------------------------------+
# | Read and write RAW Triangle File Format (*.raw)         |
# +---------------------------------------------------------+

import Blender, mod_meshtools
#import time

# ================================
# === Read RAW Triangle Format ===
# ================================
def read(filename):
	#start = time.clock()
	file = open(filename, "rb")

	# Collect data from RAW format
	faces = []
	for line in file.readlines():
		try:
			f1, f2, f3, f4, f5, f6, f7, f8, f9 = map(float, line.split())
			faces.append([(f1, f2, f3), (f4, f5, f6), (f7, f8, f9)])
		except: # Quad
			f1, f2, f3, f4, f5, f6, f7, f8, f9, A, B, C = map(float, line.split())
			faces.append([(f1, f2, f3), (f4, f5, f6), (f7, f8, f9), (A, B, C)])

	# Generate verts and faces lists, without duplicates
	verts = []
	coords = {}
	index = 0
	for i in range(len(faces)):
		for j in range(len(faces[i])):
			vertex = faces[i][j]
			if not coords.has_key(vertex):
				coords[vertex] = index
				index += 1
				verts.append(vertex)
			faces[i][j] = coords[vertex]

	objname = Blender.sys.splitext(Blender.sys.basename(filename))[0]

	mod_meshtools.create_mesh(verts, faces, objname)
	Blender.Window.DrawProgressBar(1.0, '')  # clear progressbar
	file.close()
	#end = time.clock()
	#seconds = " in %.2f %s" % (end-start, "seconds")
	message = "Successfully imported " + Blender.sys.basename(filename)# + seconds
	mod_meshtools.print_boxed(message)

def fs_callback(filename):
	read(filename)

Blender.Window.FileSelector(fs_callback, "Import Raw")
