#!BPY

"""
Name: 'OFF...'
Blender: 232
Group: 'Export'
Tooltip: 'Export selected mesh to Object File Format (*.off)'
"""

# +---------------------------------------------------------+
# | Copyright (c) 2002 Anthony D'Agostino                   |
# | http://ourworld.compuserve.com/homepages/scorpius       |
# | scorpius@compuserve.com                                 |
# | February 3, 2001                                        |
# | Released under the Blender Artistic Licence (BAL)       |
# | Import Export Suite v0.5                                |
# +---------------------------------------------------------+
# | Read and write Object File Format (*.off)               |
# +---------------------------------------------------------+

import Blender
#import time
import mod_flags, mod_meshtools

# ==============================
# ====== Write OFF Format ======
# ==============================
def write(filename):
	#start = time.clock()
	file = open(filename, "wb")

	objects = Blender.Object.GetSelected()
	objname = objects[0].name
	meshname = objects[0].data.name
	mesh = Blender.NMesh.GetRaw(meshname)
	#mesh = Blender.NMesh.GetRawFromObject(meshname)	# for SubSurf
	obj = Blender.Object.Get(objname)

	# === OFF Header ===
	file.write("OFF\n")
	file.write("%d %d %d\n" % (len(mesh.verts), len(mesh.faces), 0))

	# === Vertex List ===
	for i in range(len(mesh.verts)):
		if not i%100 and mod_flags.show_progress:
			Blender.Window.DrawProgressBar(float(i)/len(mesh.verts), "Writing Verts")
		x, y, z = mesh.verts[i].co
		file.write("%f %f %f\n" % (x, y, z))

	# === Face List ===
	for i in range(len(mesh.faces)):
		if not i%100 and mod_flags.show_progress:
			Blender.Window.DrawProgressBar(float(i)/len(mesh.faces), "Writing Faces")
		file.write(`len(mesh.faces[i].v)`+' ')
		mesh.faces[i].v.reverse()
		for j in range(len(mesh.faces[i].v)):
			file.write(`mesh.faces[i].v[j].index`+' ')
		file.write("\n")


	Blender.Window.DrawProgressBar(1.0, '')  # clear progressbar
	file.close()
	#end = time.clock()
	#seconds = " in %.2f %s" % (end-start, "seconds")
	message = "Successfully exported " + Blender.sys.basename(filename)# + seconds
	mod_meshtools.print_boxed(message)

def fs_callback(filename):
	if filename.find('.off', -4) <= 0: filename += '.off'
	write(filename)

Blender.Window.FileSelector(fs_callback, "OFF Export")
