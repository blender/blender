#!BPY

"""
Name: 'Pro Engineer (.slp)...'
Blender: 232
Group: 'Import'
Tooltip: 'Import Pro Engineer (.slp) File Format'
"""

# $Id$
#
# +---------------------------------------------------------+
# | Copyright (c) 2004 Anthony D'Agostino                   |
# | http://www.redrival.com/scorpius                        |
# | scorpius@netzero.com                                    |
# | May 3, 2004                                             |
# | Released under the Blender Artistic Licence (BAL)       |
# | Import Export Suite v0.5                                |
# +---------------------------------------------------------+
# | Read and write SLP Triangle File Format (*.slp)         |
# +---------------------------------------------------------+

import Blender, mod_meshtools
#import time

# ================================
# === Read SLP Triangle Format ===
# ================================
def read(filename):
	#start = time.clock()
	file = open(filename, "rb")

	raw = []
	for line in file.readlines():
		data = line.split()
		if data[0] == "vertex":
			vert = map(float, data[1:])
			raw.append(vert)

	tri = []
	for i in range(0, len(raw), 3):
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

Blender.Window.FileSelector(fs_callback, "Import SLP")
