# $Id$
#
# +---------------------------------------------------------+
# | Copyright (c) 2001 Anthony D'Agostino                   |
# | http://www.redrival.com/scorpius                        |
# | scorpius@netzero.com                                    |
# | September 28, 2002                                      |
# +---------------------------------------------------------+
# | Common Functions & Global Variables For All IO Modules  |
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

import Blender
import sys

show_progress = 1			# Set to 0 for faster performance
average_vcols = 1			# Off for per-face, On for per-vertex
overwrite_mesh_name = 0 	# Set to 0 to increment object-name version

blender_version = Blender.Get('version')
blender_version_str = `blender_version`[0] + '.' + `blender_version`[1:]

try:
	import operator
except:
	msg = "Error: you need a full Python install to run this script."
	meshtools.print_boxed(msg)
	Blender.Draw.PupMenu("ERROR%t|"+msg)

# =================================
# === Append Faces To Face List ===
# =================================
def append_faces(mesh, faces, facesuv, uvcoords):
	for i in xrange(len(faces)):
		if not i%100 and show_progress: Blender.Window.DrawProgressBar(float(i)/len(faces), "Generating Faces")
		numfaceverts=len(faces[i])
		if numfaceverts == 2: #This is not a face is an edge
			if mesh.edges == None:  #first run
				mesh.addEdgeData()
			#rev_face = revert(cur_face)
			i1 = faces[i][0]
			i2 = faces[i][1]
			ee = mesh.addEdge(mesh.verts[i1],mesh.verts[i2])
			ee.flag |= Blender.NMesh.EdgeFlags.EDGEDRAW
			ee.flag |= Blender.NMesh.EdgeFlags.EDGERENDER
		elif numfaceverts in [3,4]:				# This face is a triangle or quad
			face = Blender.NMesh.Face()
			for j in xrange(numfaceverts):
				index = faces[i][j]
				face.v.append(mesh.verts[index])
				if len(uvcoords) > 1:
					uvidx = facesuv[i][j]
					face.uv.append(uvcoords[uvidx])
					face.mode = 0
					face.col = [Blender.NMesh.Col()]*4
			mesh.faces.append(face)
		else:								# Triangulate n-sided convex polygon.
			a, b, c = 0, 1, 2				# Indices of first triangle.
			for j in xrange(numfaceverts-2): # Number of triangles in polygon.
				face = Blender.NMesh.Face()
				face.v.append(mesh.verts[faces[i][a]])
				face.v.append(mesh.verts[faces[i][b]])
				face.v.append(mesh.verts[faces[i][c]])
				b = c; c += 1
				mesh.faces.append(face)
		#face.smooth = 1

# ===================================
# === Append Verts to Vertex List ===
# ===================================
def append_verts(mesh, verts, normals):
	#print "Number of normals:", len(normals)
	#print "Number of verts  :", len(verts)
	for i in xrange(len(verts)):
		if not i%100 and show_progress: Blender.Window.DrawProgressBar(float(i)/len(verts), "Generating Verts")
		x, y, z = verts[i]
		mesh.verts.append(Blender.NMesh.Vert(x, y, z))
		if normals:
			mesh.verts[i].no[0] = normals[i][0]
			mesh.verts[i].no[1] = normals[i][1]
			mesh.verts[i].no[2] = normals[i][2]

# ===========================
# === Create Blender Mesh ===
# ===========================
def create_mesh(verts, faces, objname, facesuv=[], uvcoords=[], normals=[]):
	if normals: normal_flag = 0
	else: normal_flag = 1
	mesh = Blender.NMesh.GetRaw()
	append_verts(mesh, verts, normals)
	append_faces(mesh, faces, facesuv, uvcoords)
	if not overwrite_mesh_name:
		objname = versioned_name(objname)
	ob= Blender.NMesh.PutRaw(mesh, objname, normal_flag)	# Name the Mesh
	ob.name= objname		# Name the Object
	Blender.Redraw()

# ==============================
# === Increment Name Version ===
# ==============================
def versioned_name(objname):
	existing_names = []
	for object in Blender.Object.Get():
		existing_names.append(object.name)
		existing_names.append(object.getData(name_only=1))
	if objname in existing_names: # don't over-write other names
		try:
			name, ext = objname.split('.')
		except ValueError:
			name, ext = objname, ''
		try:
			num = int(ext)
			root = name
		except ValueError:
			root = objname
		for i in xrange(1, 1000):
			objname = "%s.%03d" % (root, i)
			if objname not in existing_names:
				break
	return objname

# ===========================
# === Print Text In A Box ===
# ===========================
def print_boxed(text):
	lines = text.splitlines()
	maxlinelen = max(map(len, lines))
	if sys.platform[:3] == "win":
		print chr(218)+chr(196) + chr(196)*maxlinelen + chr(196)+chr(191)
		for line in lines:
			print chr(179) + ' ' + line.ljust(maxlinelen) + ' ' + chr(179)
		print chr(192)+chr(196) + chr(196)*maxlinelen + chr(196)+chr(217)
	else:
		print '+-' + '-'*maxlinelen + '-+'
		for line in lines: print '| ' + line.ljust(maxlinelen) + ' |'
		print '+-' + '-'*maxlinelen + '-+'
	print '\a\r', # beep when done

# ===============================================
# === Get euler angles from a rotation matrix ===
# ===============================================
def mat2euler(mat):
	angle_y = -math.asin(mat[0][2])
	c = math.cos(angle_y)
	if math.fabs(c) > 0.005:
		angle_x = math.atan2(mat[1][2]/c, mat[2][2]/c)
		angle_z = math.atan2(mat[0][1]/c, mat[0][0]/c)
	else:
		angle_x = 0.0
		angle_z = -math.atan2(mat[1][0], mat[1][1])
	return (angle_x, angle_y, angle_z)

# ==========================
# === Transpose A Matrix ===
# ==========================
def transpose(A):
	S = len(A)
	T = len(A[0])
	B = [[None]*S for i in xrange(T)]
	for i in xrange(T):
		for j in xrange(S):
			B[i][j] = A[j][i]
	return B

# =======================
# === Apply Transform ===
# =======================
def apply_transform(vertex, matrix):
	x, y, z = vertex
	xloc, yloc, zloc = matrix[3][0], matrix[3][1], matrix[3][2]
	xcomponent = x*matrix[0][0] + y*matrix[1][0] + z*matrix[2][0] + xloc
	ycomponent = x*matrix[0][1] + y*matrix[1][1] + z*matrix[2][1] + yloc
	zcomponent = x*matrix[0][2] + y*matrix[1][2] + z*matrix[2][2] + zloc
	vertex = [xcomponent, ycomponent, zcomponent]
	return vertex

# =========================
# === Has Vertex Colors ===
# =========================
def has_vertex_colors(mesh):
	# My replacement/workaround for hasVertexColours()
	# The docs say:
	# "Warning: If a mesh has both vertex colours and textured faces,
	# this function will return False. This is due to the way Blender
	# deals internally with the vertex colours array (if there are
	# textured faces, it is copied to the textured face structure and
	# the original array is freed/deleted)."
	try:
		return mesh.faces[0].col[0]
	except:
		return 0

# ===========================
# === Generate Edge Table ===
# ===========================
def generate_edgetable(mesh):
	edge_table = {}
	numfaces = len(mesh.faces)

	for i in xrange(numfaces):
		if not i%100 and show_progress:
			Blender.Window.DrawProgressBar(float(i)/numfaces, "Generating Edge Table")
		if len(mesh.faces[i].v) == 4:	# Process Quadrilaterals
			generate_entry_from_quad(mesh, i, edge_table)
		elif len(mesh.faces[i].v) == 3: # Process Triangles
			generate_entry_from_tri(mesh, i, edge_table)
		else:							# Skip This Face
			print "Face #", i, "was skipped."

	# === Sort Edge_Table Keys & Add Edge Indices ===
	i = 0
	keys = edge_table.keys()
	keys.sort()
	for key in keys:
		edge_table[key][6] = i
		i += 1

	# === Replace Tuples With Indices ===
	for key in keys:
		for i in [2,3,4,5]:
			if edge_table.has_key(edge_table[key][i]):
				edge_table[key][i] = edge_table[edge_table[key][i]][6]
			else:
				keyrev = (edge_table[key][i][1], edge_table[key][i][0])
				edge_table[key][i] = edge_table[keyrev][6]

	return edge_table

# ================================
# === Generate Entry From Quad ===
# ================================
def generate_entry_from_quad(mesh, i, edge_table):
	vertex4, vertex3, vertex2, vertex1 = mesh.faces[i].v

	if has_vertex_colors(mesh):
		vcolor4, vcolor3, vcolor2, vcolor1 = mesh.faces[i].col
		Acol = (vcolor1.r/255.0, vcolor1.g/255.0, vcolor1.b/255.0)
		Bcol = (vcolor2.r/255.0, vcolor2.g/255.0, vcolor2.b/255.0)
		Ccol = (vcolor3.r/255.0, vcolor3.g/255.0, vcolor3.b/255.0)
		Dcol = (vcolor4.r/255.0, vcolor4.g/255.0, vcolor4.b/255.0)

	# === verts are upper case, edges are lower case ===
	A, B, C, D = vertex1.index, vertex2.index, vertex3.index, vertex4.index
	a, b, c, d = (A, B), (B, C), (C, D), (D, A)

	if edge_table.has_key((B, A)):
		edge_table[(B, A)][1] = i
		edge_table[(B, A)][4] = d
		edge_table[(B, A)][5] = b
		if has_vertex_colors(mesh): edge_table[(B, A)][8] = Bcol
	else:
		if has_vertex_colors(mesh):
			edge_table[(A, B)] = [i, None, d, b, None, None, None, Bcol, None]
		else:
			edge_table[(A, B)] = [i, None, d, b, None, None, None]

	if edge_table.has_key((C, B)):
		edge_table[(C, B)][1] = i
		edge_table[(C, B)][4] = a
		edge_table[(C, B)][5] = c
		if has_vertex_colors(mesh): edge_table[(C, B)][8] = Ccol
	else:
		if has_vertex_colors(mesh):
			edge_table[(B, C)] = [i, None, a, c, None, None, None, Ccol, None]
		else:
			edge_table[(B, C)] = [i, None, a, c, None, None, None]

	if edge_table.has_key((D, C)):
		edge_table[(D, C)][1] = i
		edge_table[(D, C)][4] = b
		edge_table[(D, C)][5] = d
		if has_vertex_colors(mesh): edge_table[(D, C)][8] = Dcol
	else:
		if has_vertex_colors(mesh):
			edge_table[(C, D)] = [i, None, b, d, None, None, None, Dcol, None]
		else:
			edge_table[(C, D)] = [i, None, b, d, None, None, None]

	if edge_table.has_key((A, D)):
		edge_table[(A, D)][1] = i
		edge_table[(A, D)][4] = c
		edge_table[(A, D)][5] = a
		if has_vertex_colors(mesh): edge_table[(A, D)][8] = Acol
	else:
		if has_vertex_colors(mesh):
			edge_table[(D, A)] = [i, None, c, a, None, None, None, Acol, None]
		else:
			edge_table[(D, A)] = [i, None, c, a, None, None, None]

# ====================================
# === Generate Entry From Triangle ===
# ====================================
def generate_entry_from_tri(mesh, i, edge_table):
	vertex3, vertex2, vertex1 = mesh.faces[i].v

	if has_vertex_colors(mesh):
		vcolor3, vcolor2, vcolor1, _vcolor4_ = mesh.faces[i].col
		Acol = (vcolor1.r/255.0, vcolor1.g/255.0, vcolor1.b/255.0)
		Bcol = (vcolor2.r/255.0, vcolor2.g/255.0, vcolor2.b/255.0)
		Ccol = (vcolor3.r/255.0, vcolor3.g/255.0, vcolor3.b/255.0)

	# === verts are upper case, edges are lower case ===
	A, B, C = vertex1.index, vertex2.index, vertex3.index
	a, b, c = (A, B), (B, C), (C, A)

	if edge_table.has_key((B, A)):
		edge_table[(B, A)][1] = i
		edge_table[(B, A)][4] = c
		edge_table[(B, A)][5] = b
		if has_vertex_colors(mesh): edge_table[(B, A)][8] = Bcol
	else:
		if has_vertex_colors(mesh):
			edge_table[(A, B)] = [i, None, c, b, None, None, None, Bcol, None]
		else:
			edge_table[(A, B)] = [i, None, c, b, None, None, None]

	if edge_table.has_key((C, B)):
		edge_table[(C, B)][1] = i
		edge_table[(C, B)][4] = a
		edge_table[(C, B)][5] = c
		if has_vertex_colors(mesh): edge_table[(C, B)][8] = Ccol
	else:
		if has_vertex_colors(mesh):
			edge_table[(B, C)] = [i, None, a, c, None, None, None, Ccol, None]
		else:
			edge_table[(B, C)] = [i, None, a, c, None, None, None]

	if edge_table.has_key((A, C)):
		edge_table[(A, C)][1] = i
		edge_table[(A, C)][4] = b
		edge_table[(A, C)][5] = a
		if has_vertex_colors(mesh): edge_table[(A, C)][8] = Acol
	else:
		if has_vertex_colors(mesh):
			edge_table[(C, A)] = [i, None, b, a, None, None, None, Acol, None]
		else:
			edge_table[(C, A)] = [i, None, b, a, None, None, None]

