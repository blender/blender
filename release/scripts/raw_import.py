#!BPY

"""
Name: 'Raw Triangle...'
Blender: 232
Group: 'Import'
Tooltip: 'Import Raw Triangle File Format (*.raw)'
"""

# +---------------------------------------------------------+
# | Copyright (c) 2002 Anthony D'Agostino                   |
# | http://ourworld.compuserve.com/homepages/scorpius       |
# | scorpius@compuserve.com                                 |
# | April 28, 2002                                          |
# | Released under the Blender Artistic Licence (BAL)       |
# | Import Export Suite v0.5                                |
# +---------------------------------------------------------+
# | Read and write RAW Triangle File Format (*.raw)         |
# +---------------------------------------------------------+

import Blender
#import time
import mod_flags, mod_meshtools

# ==================================
# ==== Read RAW Triangle Format ====
# ==================================
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

Blender.Window.FileSelector(fs_callback, "Raw Import")
