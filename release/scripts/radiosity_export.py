#!BPY

"""
Name: 'Radiosity (.radio)...'
Blender: 232
Group: 'Export'
Tooltip: 'Export selected mesh (with vertex colors) to Radiosity File Format (.radio)'
"""

# $Id$
#
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

# ================================
# ====== Write Radio Format ======
# ================================
def write(filename):
	#start = time.clock()
	file = open(filename, "wb")

	objects = Blender.Object.GetSelected()
	objname = objects[0].name
	meshname = objects[0].data.name
	mesh = Blender.NMesh.GetRaw(meshname)
	obj = Blender.Object.Get(objname)

	if not mod_meshtools.has_vertex_colors(mesh):
		message = "Please assign vertex colors before exporting. \n"
		message += objname + " object was not saved."
		mod_meshtools.print_boxed(message)
		Blender.Draw.PupMenu("ERROR%t|"+message)
		return

	# === Object Name ===
	file.write(struct.pack("<h", len(objname)))
	file.write(struct.pack("<"+`len(objname)`+"s", objname))

	# === Vertex List ===
	file.write(struct.pack("<l", len(mesh.verts)))
	for i in range(len(mesh.verts)):
		if not i%100 and mod_meshtools.show_progress:
			Blender.Window.DrawProgressBar(float(i)/len(mesh.verts), "Writing Verts")

		x, y, z = mesh.verts[i].co
		file.write(struct.pack("<fff", x, y, z))

	# === Face List ===
	file.write(struct.pack("<l", len(mesh.faces)))
	for i in range(len(mesh.faces)):
		if not i%100 and mod_meshtools.show_progress:
			Blender.Window.DrawProgressBar(float(i)/len(mesh.faces), "Writing Faces")

		file.write(struct.pack("<b", len(mesh.faces[i].v)))
		for j in range(len(mesh.faces[i].v)):
			file.write(struct.pack("<h", mesh.faces[i].v[j].index))

		for j in range(4): # .col always has a length of 4
			file.write(struct.pack("<BBBB", mesh.faces[i].col[j].r,
											mesh.faces[i].col[j].g,
											mesh.faces[i].col[j].b,
											mesh.faces[i].col[j].a))

	Blender.Window.DrawProgressBar(1.0, '')  # clear progressbar
	file.close()
	#end = time.clock()
	#seconds = " in %.2f %s" % (end-start, "seconds")
	message = "Successfully exported " + Blender.sys.basename(filename)# + seconds
	mod_meshtools.print_boxed(message)

def fs_callback(filename):
	if filename.find('.radio', -6) <= 0: filename += '.radio'
	write(filename)

Blender.Window.FileSelector(fs_callback, "Export Radio")
