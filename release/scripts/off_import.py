#!BPY

"""
Name: 'OFF...'
Blender: 232
Group: 'Import'
Tooltip: 'Import Object File Format (*.off)'
"""

# +---------------------------------------------------------+
# | Copyright (c) 2002 Anthony D'Agostino                   |
# | http://www.redrival.com/scorpius                        |
# | scorpius@netzero.com                                    |
# | February 3, 2001                                        |
# | Released under the Blender Artistic Licence (BAL)       |
# | Import Export Suite v0.5                                |
# +---------------------------------------------------------+
# | Read and write Object File Format (*.off)               |
# +---------------------------------------------------------+

import Blender, mod_meshtools
#import time

# =============================
# ====== Read OFF Format ======
# =============================
def read(filename):
	#start = time.clock()
	file = open(filename, "rb")

	verts = []
	faces = []

	# === OFF Header ===
	offheader = file.readline()
	numverts, numfaces, null = file.readline().split()
	numverts = int(numverts)
	numfaces = int(numfaces)

	# === Vertex List ===
	for i in range(numverts):
		if not i%100 and mod_meshtools.show_progress:
			Blender.Window.DrawProgressBar(float(i)/numverts, "Reading Verts")
		x, y, z = file.readline().split()
		x, y, z = float(x), float(y), float(z)
		verts.append((x, y, z))

	# === Face List ===
	for i in range(numfaces):
		if not i%100 and mod_meshtools.show_progress:
			Blender.Window.DrawProgressBar(float(i)/numfaces, "Reading Faces")
		line = file.readline().split()
		numfaceverts = len(line)-1
		facev = []
		for j in range(numfaceverts):
			index = int(line[j+1])
			facev.append(index)
		facev.reverse()
		faces.append(facev)

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

Blender.Window.FileSelector(fs_callback, "OFF Import")
