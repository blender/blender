#!BPY

"""
Name: 'TrueSpace (.cob)...'
Blender: 232
Group: 'Export'
Tooltip: 'Export selected meshes to TrueSpace File Format (.cob)'
"""

# $Id$
#
# +---------------------------------------------------------+
# | Copyright (c) 2001 Anthony D'Agostino                   |
# | http://www.redrival.com/scorpius                        |
# | scorpius@netzero.com                                    |
# | June 12, 2001                                           |
# | Released under the Blender Artistic Licence (BAL)       |
# | Import Export Suite v0.5                                |
# +---------------------------------------------------------+
# | Read and write Caligari trueSpace File Format (*.cob)   |
# +---------------------------------------------------------+

import Blender, mod_meshtools
import struct, os, cStringIO, time

# ==============================
# === Write trueSpace Format ===
# ==============================
def write(filename):
	start = time.clock()
	file = open(filename, "wb")
	objects = Blender.Object.GetSelected()

	write_header(file)

	G,P,V,U,M = 1000,2000,3000,4000,5000
	for object in objects:
		objname = object.name
		meshname = object.data.name
		mesh = Blender.NMesh.GetRaw(meshname)
		obj = Blender.Object.Get(objname)
		if not mesh: continue

		grou = generate_grou('Group ' + `objects.index(object)+1`)
		polh = generate_polh(objname, obj, mesh)
		if mod_meshtools.has_vertex_colors(mesh): vcol = generate_vcol(mesh)
		unit = generate_unit()
		mat1 = generate_mat1(mesh)

		if objects.index(object) == 0: X = 0

		write_chunk(file, "Grou", 0, 1, G, X, grou)
		write_chunk(file, "PolH", 0, 4, P, G, polh)
		if mod_meshtools.has_vertex_colors(mesh) and vcol:
			write_chunk(file, "VCol", 1, 0, V, P, vcol)
		write_chunk(file, "Unit", 0, 1, U, P, unit)
		write_chunk(file, "Mat1", 0, 5, M, P, mat1)

		X = G
		G,P,V,U,M = map(lambda x: x+1, [G,P,V,U,M])

	write_chunk(file, "END ", 1, 0, 0, 0, '') # End Of File Chunk

	Blender.Window.DrawProgressBar(1.0, '')  # clear progressbar
	file.close()
	end = time.clock()
	seconds = " in %.2f %s" % (end-start, "seconds")
	message = "Successfully exported " + os.path.basename(filename) + seconds
	mod_meshtools.print_boxed(message)

# =============================
# === Write COB File Header ===
# =============================
def write_header(file):
	file.write("Caligari V00.01BLH"+" "*13+"\n")

# ===================
# === Write Chunk ===
# ===================
def write_chunk(file, name, major, minor, chunk_id, parent_id, data):
	file.write(name)
	file.write(struct.pack("<2h", major, minor))
	file.write(struct.pack("<2l", chunk_id, parent_id))
	file.write(struct.pack("<1l", len(data)))
	file.write(data)

# ============================================
# === Generate PolH (Polygonal Data) Chunk ===
# ============================================
def generate_polh(objname, obj, mesh):
	data = cStringIO.StringIO()
	write_ObjectName(data, objname)
	write_LocalAxes(data, obj)
	write_CurrentPosition(data, obj)
	write_VertexList(data, mesh)
	uvcoords = write_UVCoordsList(data, mesh)
	write_FaceList(data, mesh, uvcoords)
	return data.getvalue()

# === Write Object Name ===
def write_ObjectName(data, objname):
	data.write(struct.pack("<h", 0))  # dupecount
	data.write(struct.pack("<h", len(objname)))
	data.write(objname)

# === Write Local Axes ===
def write_LocalAxes(data, obj):
	data.write(struct.pack("<fff", obj.mat[3][0], obj.mat[3][1], obj.mat[3][2]))
	data.write(struct.pack("<fff", obj.mat[0][0]/obj.SizeX, obj.mat[1][0]/obj.SizeX, obj.mat[2][0]/obj.SizeX))
	data.write(struct.pack("<fff", obj.mat[0][1]/obj.SizeY, obj.mat[1][1]/obj.SizeY, obj.mat[2][1]/obj.SizeY))
	data.write(struct.pack("<fff", obj.mat[0][2]/obj.SizeZ, obj.mat[1][2]/obj.SizeZ, obj.mat[2][2]/obj.SizeZ))

# === Write Current Position ===
def write_CurrentPosition(data, obj):
	data.write(struct.pack("<ffff", obj.mat[0][0], obj.mat[0][1], obj.mat[0][2], obj.mat[3][0]))
	data.write(struct.pack("<ffff", obj.mat[1][0], obj.mat[1][1], obj.mat[1][2], obj.mat[3][1]))
	data.write(struct.pack("<ffff", obj.mat[2][0], obj.mat[2][1], obj.mat[2][2], obj.mat[3][2]))

# === Write Vertex List ===
def write_VertexList(data, mesh):
	data.write(struct.pack("<l", len(mesh.verts)))
	for i in range(len(mesh.verts)):
		if not i%100 and mod_meshtools.show_progress:
			Blender.Window.DrawProgressBar(float(i)/len(mesh.verts), "Writing Verts")
		x, y, z = mesh.verts[i].co
		data.write(struct.pack("<fff", -y, x, z))

# === Write UV Vertex List ===
def write_UVCoordsList(data, mesh):
	if not mesh.hasFaceUV():
		data.write(struct.pack("<l", 1))
		data.write(struct.pack("<2f", 0,0))
		return {(0,0): 0}
		# === Default UV Coords (one image per face) ===
		# data.write(struct.pack("<l", 4))
		# data.write(struct.pack("<8f", 0,0, 0,1, 1,1, 1,0))
		# return {(0,0): 0, (0,1): 1, (1,1): 2, (1,0): 3}
		# === Default UV Coords (one image per face) ===

	# === collect, remove duplicates, add indices, and write the uv list ===
	uvdata = cStringIO.StringIO()
	uvcoords = {}
	uvidx = 0
	for i in range(len(mesh.faces)):
		if not i%100 and mod_meshtools.show_progress:
			Blender.Window.DrawProgressBar(float(i)/len(mesh.faces), "Writing UV Coords")
		numfaceverts = len(mesh.faces[i].v)
		for j in range(numfaceverts-1, -1, -1): 	# Reverse order
			u,v = mesh.faces[i].uv[j]
			if not uvcoords.has_key((u,v)):
				uvcoords[(u,v)] = uvidx
				uvidx += 1
				uvdata.write(struct.pack("<ff", u,v))
	uvdata = uvdata.getvalue()

	numuvcoords = len(uvdata)/8
	data.write(struct.pack("<l", numuvcoords))
	data.write(uvdata)
	#print "Number of uvcoords:", numuvcoords, '=', len(uvcoords)
	return uvcoords

# === Write Face List ===
def write_FaceList(data, mesh, uvcoords):
	data.write(struct.pack("<l", len(mesh.faces)))
	for i in range(len(mesh.faces)):
		if not i%100 and mod_meshtools.show_progress:
			Blender.Window.DrawProgressBar(float(i)/len(mesh.faces), "Writing Faces")
		numfaceverts = len(mesh.faces[i].v)
		data.write(struct.pack("<B", 0x10))         # Cull Back Faces Flag
		data.write(struct.pack("<h", numfaceverts))
		data.write(struct.pack("<h", 0))            # Material Index
		for j in range(numfaceverts-1, -1, -1): 	# Reverse order
			index = mesh.faces[i].v[j].index
			if mesh.hasFaceUV():
				uv = mesh.faces[i].uv[j]
				uvidx = uvcoords[uv]
			else:
				uvidx = 0
			data.write(struct.pack("<ll", index, uvidx))

# ===========================================
# === Generate VCol (Vertex Colors) Chunk ===
# ===========================================
def generate_vcol(mesh):
	data = cStringIO.StringIO()
	data.write(struct.pack("<l", len(mesh.faces)))
	uniquecolors = {}
	unique_alpha = {}
	for i in range(len(mesh.faces)):
		if not i%100 and mod_meshtools.show_progress:
			Blender.Window.DrawProgressBar(float(i)/len(mesh.faces), "Writing Vertex Colors")
		numfaceverts = len(mesh.faces[i].v)
		data.write(struct.pack("<ll", i, numfaceverts))
		for j in range(numfaceverts-1, -1, -1): 	# Reverse order
			r = mesh.faces[i].col[j].r
			g = mesh.faces[i].col[j].g
			b = mesh.faces[i].col[j].b
			a = 100  # 100 is opaque in ts
			uniquecolors[(r,g,b)] = None
			unique_alpha[mesh.faces[i].col[j].a] = None
			data.write(struct.pack("<BBBB", r,g,b,a))

	#print "uniquecolors:", uniquecolors.keys()
	#print "unique_alpha:", unique_alpha.keys()
	if len(uniquecolors) == 1:
		return None
	else:
		return data.getvalue()

# ==================================
# === Generate Unit (Size) Chunk ===
# ==================================
def generate_unit():
	data = cStringIO.StringIO()
	data.write(struct.pack("<h", 2))
	return data.getvalue()

# ======================================
# === Generate Mat1 (Material) Chunk ===
# ======================================
def generate_mat1(mesh):
	data = cStringIO.StringIO()
	data.write(struct.pack("<h", 0))
	data.write(struct.pack("<ccB", "p", "a", 0))
	data.write(struct.pack("<fff", 1.0, 1.0, 1.0))  # rgb (0.0 - 1.0)
	data.write(struct.pack("<fffff", 1, 1, 0, 0, 1))
	if mesh.hasFaceUV():
		tex_mapname = r"c:\image\maps\one-dot.tga"
		data.write("t:")
		data.write(struct.pack("<B", 0x00))
		data.write(struct.pack("<h", len(tex_mapname)))
		data.write(tex_mapname)
		data.write(struct.pack("<4f", 0,0, 1,1))
	return data.getvalue()

# ============================
# === Generate Group Chunk ===
# ============================
def generate_grou(name):
	data = cStringIO.StringIO()
	write_ObjectName(data, name)
	data.write(struct.pack("<12f", 0,0,0, 1,0,0, 0,1,0, 0,0,1))
	data.write(struct.pack("<12f", 1,0,0,0, 0,1,0,0, 0,0,1,0))
	return data.getvalue()

def fs_callback(filename):
	if filename.find('.cob', -4) <= 0: filename += '.cob'
	write(filename)

Blender.Window.FileSelector(fs_callback, "Export COB")

# === Matrix Differences between Blender & trueSpace ===
#
# For the 'Local Axes' values:
# The x, y, and z-axis represent a simple rotation matrix.
# This is equivalent to Blender's object matrix before it was
# combined with the object's scaling matrix.  Dividing each value
# by the appropriate scaling factor (and transposing at the same
# time) produces the original rotation matrix.
#
# For the 'Current Position' values:
# This is equivalent to Blender's object matrix except that the
# last row is omitted and the xyz location is used in the last
# column.  Binary format uses a 4x3 matrix, ascii format uses a 4x4
# matrix.
#
# For Cameras: The matrix is a little confusing.
