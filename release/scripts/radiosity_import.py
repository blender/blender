#!BPY

"""
Name: 'Radiosity...'
Blender: 232
Group: 'Import'
Tooltip: 'Import Radiosity File Format (*.radio) with vertex colors'
"""

# +---------------------------------------------------------+
# | Copyright (c) 2002 Anthony D'Agostino                   |
# | http://www.redrival.com/scorpius                        |
# | scorpius@netzero.com                                    |
# | April 11, 2002                                          |
# | Released under the Blender Artistic Licence (BAL)       |
# | Import Export Suite v0.5                                |
# +---------------------------------------------------------+
# | Read and write Radiosity File Format (*.radio)          |
# +---------------------------------------------------------+

import Blender, mod_meshtools
#import time

try:
	import struct
except:
	msg = "Error: you need a full Python install to run this script."
	mod_meshtools.print_boxed(msg)
	Blender.Draw.PupMenu("ERROR%t|"+msg)

# ===============================
# ====== Read Radio Format ======
# ===============================
def read(filename):
	#start = time.clock()
	file = open(filename, "rb")
	mesh = Blender.NMesh.GetRaw()
	#mesh.addMaterial(Blender.Material.New())

	# === Object Name ===
	namelen, = struct.unpack("<h",  file.read(2))
	objname, = struct.unpack("<"+`namelen`+"s", file.read(namelen))

	# === Vertex List ===
	numverts, = struct.unpack("<l", file.read(4))
	for i in range(numverts):
		if not i%100 and mod_meshtools.show_progress:
			Blender.Window.DrawProgressBar(float(i)/numverts, "Reading Verts")
		x, y, z = struct.unpack("<fff", file.read(12))
		mesh.verts.append(Blender.NMesh.Vert(x, y, z))

	# === Face List ===
	numfaces, = struct.unpack("<l", file.read(4))
	for i in range(numfaces):
		if not i%100 and mod_meshtools.show_progress:
			Blender.Window.DrawProgressBar(float(i)/numfaces, "Reading Faces")

		face = Blender.NMesh.Face()
		numfaceverts, = struct.unpack("<b", file.read(1))

		for j in range(numfaceverts):
			index, = struct.unpack("<h", file.read(2))
			face.v.append(mesh.verts[index])

		for j in range(4):
			r, g, b, a = struct.unpack("<BBBB", file.read(4))
			vertexcolor = Blender.NMesh.Col(r, g, b, a)
			face.col.append(vertexcolor)

		if len(face.v) == 3:
			face.uv = [ (0,0), (0,1), (1,1) ]
		else:
			face.uv = [ (0,0), (0,1), (1,1), (1,0) ]

		face.mode = 0
		mesh.faces.append(face)

	# ->tools.create_mesh(verts, faces, objname):
	Blender.NMesh.PutRaw(mesh, objname)
	object = Blender.Object.GetSelected()
	object[0].name=objname
	# ->tools.create_mesh(verts, faces, objname):

	Blender.Window.DrawProgressBar(1.0, '')  # clear progressbar
	file.close()
	#end = time.clock()
	#seconds = " in %.2f %s" % (end-start, "seconds")
	message = "Successfully imported " + Blender.sys.basename(filename)# + seconds
	mod_meshtools.print_boxed(message)

def fs_callback(filename):
	read(filename)

Blender.Window.FileSelector(fs_callback, "Radio Import")
