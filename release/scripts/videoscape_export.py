#!BPY

"""
Name: 'Videoscape (with Vertex Colors)...'
Blender: 232
Group: 'Export'
Tooltip: 'Export selected mesh to VideoScape File Format (*.obj)'
"""

# +---------------------------------------------------------+
# | Copyright (c) 2001 Anthony D'Agostino                   |
# | http://ourworld.compuserve.com/homepages/scorpius       |
# | scorpius@compuserve.com                                 |
# | June 5, 2001                                            |
# | Released under the Blender Artistic Licence (BAL)       |
# | Import Export Suite v0.5                                |
# +---------------------------------------------------------+
# | Write Videoscape File Format (*.obj NOT WAVEFRONT OBJ)  |
# | Includes a *fast* algorithm for averaging vertex colors |
# | Blender's a|w doesn't export proper vertex colors       |
# +---------------------------------------------------------+

import Blender, mod_meshtools
#import time
import mod_flags

# =====================================
# ====== Write VideoScape Format ======
# =====================================
def write(filename):
	#start = time.clock()
	file = open(filename, "wb")

	objects = Blender.Object.GetSelected()
	objname = objects[0].name
	meshname = objects[0].data.name
	mesh = Blender.NMesh.GetRaw(meshname)
	obj = Blender.Object.Get(objname)

	if not mesh.hasVertexColours():
		message = "Please assign vertex colors before exporting.\n"
		message += objname + " object was not saved."
		mod_meshtools.print_boxed(message)
		return

	vcols = average_vertexcolors(mesh)

	# === Write Videoscape Header ===
	file.write("GOUR\n")
	file.write("%d\n" % len(mesh.verts))

	# === Write Vertex List & Vertex Colors ===
	for i in range(len(mesh.verts)):
		if not i%100 and mod_flags.show_progress:
			Blender.Window.DrawProgressBar(float(i)/len(mesh.verts), "Writing Verts")
		file.write("% f % f % f 0x" % tuple(mesh.verts[i].co))
		for j in range(len(vcols[i])):
			file.write("%02X" % vcols[i][j])
		file.write("\n")

	# === Write Face List ===
	for i in range(len(mesh.faces)):
		if not i%100 and mod_flags.show_progress:
			Blender.Window.DrawProgressBar(float(i)/len(mesh.faces), "Writing Faces")
		file.write("%d " % len(mesh.faces[i].v)) # numfaceverts
		for j in range(len(mesh.faces[i].v)):
			file.write("%d " % mesh.faces[i].v[j].index)
		file.write("\n")

	Blender.Window.DrawProgressBar(1.0, '')    # clear progressbar
	file.close()
	#end = time.clock()
	#seconds = " in %.2f %s" % (end-start, "seconds")
	message = "Successfully exported " + Blender.sys.basename(filename)# + seconds
	mod_meshtools.print_boxed(message)

# ===========================================
# === Vector Operations for Vertex Colors ===
# ===========================================
vcolor_add = lambda u, v: [u[0]+v[0], u[1]+v[1], u[2]+v[2], u[3]+v[3]]
vcolor_div = lambda u, s: [u[0]/s, u[1]/s, u[2]/s, u[3]/s]

# ========================================
# === Average All Vertex Colors (Fast) ===
# ========================================
def average_vertexcolors(mesh, debug=0):
	vertexcolors = {}
	for i in range(len(mesh.faces)):	# get all vcolors that share this vertex
		if not i%100 and mod_flags.show_progress: Blender.Window.DrawProgressBar(float(i)/len(mesh.verts), "Finding Shared VColors")
		for j in range(len(mesh.faces[i].v)):
			index = mesh.faces[i].v[j].index
			color = mesh.faces[i].col[j]
			r,g,b,a = color.r, color.g, color.b, color.a
			vertexcolors.setdefault(index, []).append([r,g,b,a])
	if debug: print 'before'; vcprint(vertexcolors)

	for i in range(len(vertexcolors)):	# average them
		if not i%100 and mod_flags.show_progress: Blender.Window.DrawProgressBar(float(i)/len(mesh.verts), "Averaging Vertex Colors")
		vcolor = [0,0,0,0]	# rgba
		for j in range(len(vertexcolors[i])):
			vcolor = vcolor_add(vcolor, vertexcolors[i][j])
		shared = len(vertexcolors[i])
		vertexcolors[i] = vcolor_div(vcolor, shared)
	if debug: print 'after'; vcprint(vertexcolors)
	return vertexcolors

# ========================================
# === Average all Vertex Colors Slow 1 ===
# ========================================
def average_vertexcolors_slow_1(mesh, debug=0):
	vertexcolors = []
	i = 0
	for vertex in mesh.verts:
		if not i%100 and mod_flags.show_progress: Blender.Window.DrawProgressBar(float(i)/len(mesh.verts), "Averaging Vertex Colors")
		i += 1
		vcolor = [0,0,0,0]	# rgba
		shared = 0
		for face in mesh.faces:
			if vertex in face.v:
				index = face.v.index(vertex)
				color = face.col[index]
				r,g,b,a = color.r, color.g, color.b, color.a
				vcolor = vcolor_add(vcolor, [r,g,b,a])
				shared += 1
		if not shared: print "Error, vertex %d is not shared." % i; shared += 1
		vertexcolors.append(vcolor_div(vcolor, shared))
	if debug: print 'after'; vcprint(vertexcolors)
	return vertexcolors

# ========================================
# === Average all Vertex Colors Slow 2 ===
# ========================================
def average_vertexcolors_slow_2(mesh, debug=0):
	vertexcolors = []
	for i in range(len(mesh.verts)):
		if not i%100 and mod_flags.show_progress: Blender.Window.DrawProgressBar(float(i)/len(mesh.verts), "Averaging Vertex Colors")
		vcolor = [0,0,0,0]	# rgba
		shared = 0
		for j in range(len(mesh.faces)):
			if mesh.verts[i] in mesh.faces[j].v:
				index = mesh.faces[j].v.index(mesh.verts[i])
				color = mesh.faces[j].col[index]
				r,g,b,a = color.r, color.g, color.b, color.a
				vcolor = vcolor_add(vcolor, [r,g,b,a])
				shared += 1
		vertexcolors.append(vcolor_div(vcolor, shared))
	if debug: print 'after'; vcprint(vertexcolors)
	return vertexcolors

# ========================================
# === Average all Vertex Colors Slow 3 ===
# ========================================
def average_vertexcolors_slow_3(mesh, debug=0):
	vertexcolors = []
	for i in range(len(mesh.verts)):
		if not i%100 and mod_flags.show_progress: Blender.Window.DrawProgressBar(float(i)/len(mesh.verts), "Averaging Vertex Colors")
		vcolor = [0,0,0,0]	# rgba
		shared = 0
		for j in range(len(mesh.faces)):
			if len(mesh.faces[j].v) == 4:
				v1,v2,v3,v4 = mesh.faces[j].v
				faceverts = v1.index, v2.index, v3.index, v4.index
			else:
				v1,v2,v3 = mesh.faces[j].v
				faceverts = v1.index, v2.index, v3.index

			if i in faceverts:
				index = mesh.faces[j].v.index(mesh.verts[i])
				color = mesh.faces[j].col[index]
				r,g,b,a = color.r, color.g, color.b, color.a
				vcolor = vcolor_add(vcolor, [r,g,b,a])
				shared += 1
		vertexcolors.append(vcolor_div(vcolor, shared))
	if debug: print 'after'; vcprint(vertexcolors)
	return vertexcolors

def fs_callback(filename):
	if filename.find('.obj', -4) <= 0: filename += '.VIDEOSCAPE.obj'
	write(filename)

Blender.Window.FileSelector(fs_callback, "Videoscape Export")


# filename = "VIDEOSCAPE_" + objname + ".obj"
# filename = 'nul'
# file = open(filename, "wb")
# debug = 0
# time_functions = 1
# time_loop = 0
#
# if time_functions:
#	  funcs = [ average_vertexcolors,
#				average_vertexcolors_slow_1,
#				average_vertexcolors_slow_2,
#				average_vertexcolors_slow_3 ]
#
#	  print
#	  for func in funcs:
#		  start = time.clock()
#		  vcols = func(mesh, debug)
#		  end = time.clock()
#		  seconds = "in %.2f %s" % (end-start, "seconds")
#		  print func.__name__, "finished in", seconds
#
# elif time_loop:
#	  total = 0
#	  loops = 6
#	  for i in range(loops):
#		  start = time.clock()
#		  vcols = average_vertexcolors(mesh, debug)
#		  end = time.clock()
#		  total += (end-start)
#	  print "Total: %5.2f Avg: %.2f " % (total, total/loops)
# else:
#	  start = time.clock()
#	  vcols = average_vertexcolors(mesh, debug)

# # =====================================
# # === Print Vertex Colors for Debug ===
# # =====================================
# def vcprint(data):
#	  print type(data)
#	  for i in range(len(data)):
#		  print "%2d" % i,
#		  for j in range(len(data[i])):
#			  try:
#				  print "[%3d %3d %3d %3d]" % tuple(data[i][j]),  # before
#			  except:
#				  print "[%3d]" % data[i][j],                     # after
#		  print
#	  print
#
