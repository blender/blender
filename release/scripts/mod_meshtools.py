# +---------------------------------------------------------+
# | Copyright (c) 2001 Anthony D'Agostino                   |
# | http://ourworld.compuserve.com/homepages/scorpius       |
# | scorpius@compuserve.                                    |
# | September 28, 2002                                      |
# | Released under the Blender Artistic Licence (BAL)       |
# | Import Export Suite v0.5                                |
# +---------------------------------------------------------+
# | Common Functions For All Modules                        |
# +---------------------------------------------------------+

import Blender
import sys#, random, operator
import mod_flags

try:
	import random, operator
# ===================================
# ==== Append Faces To Face List ====
# ===================================
	def append_faces(mesh, faces, facesuv, uvcoords):
		r = random.randrange(200, 255, 50)
		g = random.randrange(100, 200, 50)
		b = random.randrange(0, 100, 50)
		for i in range(len(faces)):
			if not i%100 and mod_flags.show_progress: Blender.Window.DrawProgressBar(float(i)/len(faces), "Generating Faces")
			numfaceverts=len(faces[i])
			if numfaceverts <= 4:				# This face is a triangle or quad
				face = Blender.NMesh.Face()
				for j in range(numfaceverts):
					index = faces[i][j]
					face.v.append(mesh.verts[index])
					if len(uvcoords) > 1:
						uvidx = facesuv[i][j]
						face.uv.append(uvcoords[uvidx])
						face.mode = 0
						#face.col = [Blender.NMesh.Col(r, g, b)]*4	 # Random color
						face.col = [Blender.NMesh.Col()]*4
				mesh.faces.append(face)
			else:								# Triangulate n-sided convex polygon.
				a, b, c = 0, 1, 2				# Indices of first triangle.
				for j in range(numfaceverts-2): # Number of triangles in polygon.
					face = Blender.NMesh.Face()
					face.v.append(mesh.verts[faces[i][a]])
					face.v.append(mesh.verts[faces[i][b]])
					face.v.append(mesh.verts[faces[i][c]])
					b = c; c += 1
					mesh.faces.append(face)
			#face.smooth = 1

# =====================================
# ==== Append Verts to Vertex List ====
# =====================================
	def append_verts(mesh, verts, normals):
		#print "Number of normals:", len(normals)
		#print "Number of verts  :", len(verts)
		for i in range(len(verts)):
			if not i%100 and mod_flags.show_progress: Blender.Window.DrawProgressBar(float(i)/len(verts), "Generating Verts")
			x, y, z = verts[i]
			mesh.verts.append(Blender.NMesh.Vert(x, y, z))
			if normals:
				mesh.verts[i].no[0] = normals[i][0]
				mesh.verts[i].no[1] = normals[i][1]
				mesh.verts[i].no[2] = normals[i][2]

# =============================
# ==== Create Blender Mesh ====
# =============================
	def create_mesh(verts, faces, objname, facesuv=[], uvcoords=[], normals=[]):
		if normals: normal_flag = 0
		else: normal_flag = 1
		mesh = Blender.NMesh.GetRaw()
		append_verts(mesh, verts, normals)
		append_faces(mesh, faces, facesuv, uvcoords)
		if not mod_flags.overwrite_mesh_name:
			objname = versioned_name(objname)
		Blender.NMesh.PutRaw(mesh, objname, normal_flag)	# Name the Mesh
		Blender.Object.GetSelected()[0].name=objname		# Name the Object
		Blender.Redraw()

except ImportError: pass

# ================================
# ==== Increment Name Version ====
# ================================
def versioned_name(objname):
	existing_names = []
	for object in Blender.Object.Get():
		existing_names.append(object.name)
		existing_names.append(object.data.name)
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

# =============================
# ==== Print Text In A Box ====
# =============================
def print_boxed(text):
	lines = text.splitlines()
	maxlinelen = max(map(len, lines))
	print '+-' + '-'*maxlinelen + '-+'
	for line in lines: print '| ' + line.ljust(maxlinelen) + ' |'
	print '+-' + '-'*maxlinelen + '-+'

	print '\a\r', # beep when done

# =================================================
# ==== Get Euler Angles From A Rotation Matrix ====
# =================================================
#def mat2euler(mat):
#	angle_y = -math.asin(mat[0][2])
#	c = math.cos(angle_y)
#	if math.fabs(c) > 0.005:
#		angle_x = math.atan2(mat[1][2]/c, mat[2][2]/c)
#		angle_z = math.atan2(mat[0][1]/c, mat[0][0]/c)
#	else:
#		angle_x = 0.0
#		angle_z = -math.atan2(mat[1][0], mat[1][1])
#	return (angle_x, angle_y, angle_z)

# ============================
# ==== Transpose A Matrix ====
# ============================
def transpose(A):
	S = len(A)
	T = len(A[0])
	B = [[None]*S for i in range(T)]
	for i in range(T):
		for j in range(S):
			B[i][j] = A[j][i]
	return B

#def append_ntimes(Seq, N):
#	Seq = reduce(operator.add, Seq)   # Flatten once
#	if N == 1: return Seq
#	return append_ntimes(Seq, N-1)



#	print "mesh.has_col           ", mesh.has_col
#	print "mesh.hasVertexColours()", mesh.hasVertexColours()
#	print "mesh.hasFaceUV()       ", mesh.hasFaceUV()
#	print "mesh.has_uvco          ", mesh.has_uvco

# # =============================
# # ==== Create Blender Mesh ====
# # =============================
# def create_mesh_old(verts, faces, objname):
#	mesh = Blender.NMesh.GetRaw()
#	# === Vertex List ===
#	for i in range(len(verts)):
#		x, y, z = verts[i]
#		mesh.verts.append(Blender.NMesh.Vert(x, y ,z))
#	# === Face List ===
#	for i in range(len(faces)):
#		face = Blender.NMesh.Face()
#		for j in range(len(faces[i])):
#			index = faces[i][j]
#			face.v.append(mesh.verts[index])
#		mesh.faces.append(face)
#	# === Name the Object ===
#	Blender.NMesh.PutRaw(mesh, objname)
#	object = Blender.Object.GetSelected()
#	object[0].name=objname
#	Blender.Redraw()

