#!BPY
"""
Name: 'Cal3D (.cfg .xaf .xsf .xmf .xrf)...'
Blender: 243
Group: 'Export'
Tip: 'Export armature/bone/mesh/action data to the Cal3D format.'
"""

# export_cal3d.py
# Copyright (C) 2003-2004 Jean-Baptiste LAMY -- jibalamy@free.fr
# Copyright (C) 2004 Matthias Braun -- matze@braunis.de
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA


__version__ = '0.9f'
__author__  = 'Jean-Baptiste, Jiba, Lamy, Campbell Barton (Ideasman42)'
__email__   = ['Authors email, jibalamy:free*fr']
__url__     = ['Soya3ds homepage, http://home.gna.org/oomadness/en/soya/', 'Cal3d, http://cal3d.sourceforge.net']
__bpydoc__  =\
'''This script is a Blender => Cal3D converter.
(See http://blender.org and http://cal3d.sourceforge.net)

USAGE:

To install it, place the script in your $HOME/.blender/scripts directory.

Then open the File->Export->Cal3d v0.9 menu. And select the filename of the .cfg file.
The exporter will create a set of other files with same prefix (ie. bla.cfg, bla.xsf,
bla_Action1.xaf, bla_Action2.xaf, ...).

You should be able to open the .cfg file in cal3d_miniviewer.


NOT (YET) SUPPORTED:

	- Rotation, translation, or stretching Blender objects is still quite
buggy, so AVOID MOVING / ROTATING / RESIZE OBJECTS (either mesh or armature) !
Instead, edit the object (with tab), select all points / bones (with "a"),
and move / rotate / resize them.<br>
	- no support for exporting springs yet<br>
	- no support for exporting material colors (most games should only use images
I think...)


KNOWN ISSUES:

	- Cal3D versions <=0.9.1 have a bug where animations aren't played when the root bone
is not animated;<br>
	- Cal3D versions <=0.9.1 have a bug where objects that aren't influenced by any bones
are not drawn (fixed in Cal3D CVS).


NOTES:

It requires a very recent version of Blender (>= 2.44).

Build a model following a few rules:<br>
	- Use only a single armature;<br>
	- Use only a single rootbone (Cal3D doesn't support floating bones);<br>
	- Use only locrot keys (Cal3D doesn't support bone's size change);<br>
	- Don't try to create child/parent constructs in blender object, that gets exported
incorrectly at the moment;<br>
	- Objects or animations whose names start by "_" are not exported (hidden object).

You can pass as many parameters as you want at the end, "EXPORT_FOR_SOYA=1" is just an
example. The parameters are the same as below.
'''

# True (=1) to export for the Soya 3D engine
#     (http://oomadness.tuxfamily.org/en/soya).
# (=> rotate meshes and skeletons so as X is right, Y is top and -Z is front)
# EXPORT_FOR_SOYA = 0

# Enables LODs computation. LODs computation is quite slow, and the algo is
# surely not optimal :-(
LODS = 0

# Scale the model (not supported by Soya).

# See also BASE_MATRIX below, if you want to rotate/scale/translate the model at
# the exportation.

#########################################################################################
# Code starts here.
# The script should be quite re-useable for writing another Blender animation exporter.
# Most of the hell of it is to deal with Blender's head-tail-roll bone's definition.

import math
import Blender
import BPyMesh
import BPySys
import BPyArmature
import BPyObject
import bpy

def best_armature_root(armature):
	'''
	Find the armature root bone with the most children, return that bone
	'''
	
	bones = [bone for bone in armature.bones.values() if bone.hasChildren() == True]
	if len(bones) == 1:
		return bones[0]
	
	# Get the best root since we have more then 1
	bones = [(len(bone.getAllChildren()), bone) for bone in bones]
	bones.sort()
	return bones[-1][1] # bone with most children


Vector = Blender.Mathutils.Vector
Quaternion = Blender.Mathutils.Quaternion
Matrix = Blender.Mathutils.Matrix

# HACK -- it seems that some Blender versions don't define sys.argv,
# which may crash Python if a warning occurs.
# if not hasattr(sys, 'argv'): sys.argv = ['???']

def matrix_multiply(b, a):
	return [ [
		a[0][0] * b[0][0] + a[0][1] * b[1][0] + a[0][2] * b[2][0],
		a[0][0] * b[0][1] + a[0][1] * b[1][1] + a[0][2] * b[2][1],
		a[0][0] * b[0][2] + a[0][1] * b[1][2] + a[0][2] * b[2][2],
		0.0,
		], [
		a[1][0] * b[0][0] + a[1][1] * b[1][0] + a[1][2] * b[2][0],
		a[1][0] * b[0][1] + a[1][1] * b[1][1] + a[1][2] * b[2][1],
		a[1][0] * b[0][2] + a[1][1] * b[1][2] + a[1][2] * b[2][2],
		0.0,
		], [
		a[2][0] * b[0][0] + a[2][1] * b[1][0] + a[2][2] * b[2][0],
		a[2][0] * b[0][1] + a[2][1] * b[1][1] + a[2][2] * b[2][1],
		a[2][0] * b[0][2] + a[2][1] * b[1][2] + a[2][2] * b[2][2],
		 0.0,
		], [
		a[3][0] * b[0][0] + a[3][1] * b[1][0] + a[3][2] * b[2][0] + b[3][0],
		a[3][0] * b[0][1] + a[3][1] * b[1][1] + a[3][2] * b[2][1] + b[3][1],
		a[3][0] * b[0][2] + a[3][1] * b[1][2] + a[3][2] * b[2][2] + b[3][2],
		1.0,
		] ]

# multiplies 2 quaternions in x,y,z,w notation
def quaternion_multiply(q1, q2):
	return Quaternion(\
		q2[3] * q1[0] + q2[0] * q1[3] + q2[1] * q1[2] - q2[2] * q1[1],
		q2[3] * q1[1] + q2[1] * q1[3] + q2[2] * q1[0] - q2[0] * q1[2],
		q2[3] * q1[2] + q2[2] * q1[3] + q2[0] * q1[1] - q2[1] * q1[0],
		q2[3] * q1[3] - q2[0] * q1[0] - q2[1] * q1[1] - q2[2] * q1[2],\
		)

def matrix_translate(m, v):
	m[3][0] += v[0]
	m[3][1] += v[1]
	m[3][2] += v[2]
	return m

def matrix2quaternion(m):
	s = math.sqrt(abs(m[0][0] + m[1][1] + m[2][2] + m[3][3]))
	if s == 0.0:
		x = abs(m[2][1] - m[1][2])
		y = abs(m[0][2] - m[2][0])
		z = abs(m[1][0] - m[0][1])
		if   (x >= y) and (x >= z): return Quaternion(1.0, 0.0, 0.0, 0.0)
		elif (y >= x) and (y >= z): return Quaternion(0.0, 1.0, 0.0, 0.0)
		else:                       return Quaternion(0.0, 0.0, 1.0, 0.0)
			
	q = Quaternion([
		-(m[2][1] - m[1][2]) / (2.0 * s),
		-(m[0][2] - m[2][0]) / (2.0 * s),
		-(m[1][0] - m[0][1]) / (2.0 * s),
		0.5 * s,
		])
	q.normalize()
	#print q
	return q

def vector_by_matrix_3x3(p, m):
	return [p[0] * m[0][0] + p[1] * m[1][0] + p[2] * m[2][0],
					p[0] * m[0][1] + p[1] * m[1][1] + p[2] * m[2][1],
					p[0] * m[0][2] + p[1] * m[1][2] + p[2] * m[2][2]]

def vector_add(v1, v2):
	return [v1[0]+v2[0], v1[1]+v2[1], v1[2]+v2[2]]

def vector_sub(v1, v2):
	return [v1[0]-v2[0], v1[1]-v2[1], v1[2]-v2[2]]

def quaternion2matrix(q):
	xx = q[0] * q[0]
	yy = q[1] * q[1]
	zz = q[2] * q[2]
	xy = q[0] * q[1]
	xz = q[0] * q[2]
	yz = q[1] * q[2]
	wx = q[3] * q[0]
	wy = q[3] * q[1]
	wz = q[3] * q[2]
	return Matrix([1.0 - 2.0 * (yy + zz),       2.0 * (xy + wz),       2.0 * (xz - wy), 0.0],
					[      2.0 * (xy - wz), 1.0 - 2.0 * (xx + zz),       2.0 * (yz + wx), 0.0],
					[      2.0 * (xz + wy),       2.0 * (yz - wx), 1.0 - 2.0 * (xx + yy), 0.0],
					[0.0                  , 0.0                  , 0.0                  , 1.0])

def matrix_invert(m):
	det = (m[0][0] * (m[1][1] * m[2][2] - m[2][1] * m[1][2])
			 - m[1][0] * (m[0][1] * m[2][2] - m[2][1] * m[0][2])
			 + m[2][0] * (m[0][1] * m[1][2] - m[1][1] * m[0][2]))
	if det == 0.0: return None
	det = 1.0 / det
	r = [ [
			det * (m[1][1] * m[2][2] - m[2][1] * m[1][2]),
		- det * (m[0][1] * m[2][2] - m[2][1] * m[0][2]),
			det * (m[0][1] * m[1][2] - m[1][1] * m[0][2]),
			0.0,
		], [
		- det * (m[1][0] * m[2][2] - m[2][0] * m[1][2]),
			det * (m[0][0] * m[2][2] - m[2][0] * m[0][2]),
		- det * (m[0][0] * m[1][2] - m[1][0] * m[0][2]),
			0.0
		], [
			det * (m[1][0] * m[2][1] - m[2][0] * m[1][1]),
		- det * (m[0][0] * m[2][1] - m[2][0] * m[0][1]),
			det * (m[0][0] * m[1][1] - m[1][0] * m[0][1]),
			0.0,
		] ]
	r.append([
		-(m[3][0] * r[0][0] + m[3][1] * r[1][0] + m[3][2] * r[2][0]),
		-(m[3][0] * r[0][1] + m[3][1] * r[1][1] + m[3][2] * r[2][1]),
		-(m[3][0] * r[0][2] + m[3][1] * r[1][2] + m[3][2] * r[2][2]),
		1.0,
		])
	return r


def point_by_matrix(p, m):
	return [p[0] * m[0][0] + p[1] * m[1][0] + p[2] * m[2][0] + m[3][0],
					p[0] * m[0][1] + p[1] * m[1][1] + p[2] * m[2][1] + m[3][1],
					p[0] * m[0][2] + p[1] * m[1][2] + p[2] * m[2][2] + m[3][2]]

# Hack for having the model rotated right.
# Put in BASE_MATRIX your own rotation if you need some.

BASE_MATRIX = None


# Cal3D data structures

CAL3D_VERSION = 910
MATERIALS = {} # keys are (mat.name, img.name)

class Cal3DMaterial(object):
	__slots__ = 'amb', 'diff', 'spec', 'shininess', 'maps_filenames', 'id'
	def __init__(self, blend_world, blend_material, blend_images):
		
		# Material Settings
		if blend_world:		amb = [ int(c*255) for c in blend_world.amb ]
		else:				amb = [0,0,0] # Default value
		
		if blend_material:
			self.amb  = tuple([int(c*blend_material.amb) for c in amb] + [255])
			self.diff = tuple([int(c*255) for c in blend_material.rgbCol] + [int(blend_material.alpha*255)])
			self.spec = tuple([int(c*255) for c in blend_material.rgbCol] + [int(blend_material.alpha*255)])
			self.shininess = (float(blend_material.hard)-1)/5.10
		else:
			self.amb  = tuple(amb + [255])
			self.diff = (255,255,255,255)
			self.spec = (255,255,255,255)
			self.shininess = 1.0
		
		self.maps_filenames = []
		for image in blend_images:
			if image:
				self.maps_filenames.append( image.filename.split('\\')[-1].split('/')[-1] )
		
		self.id = len(MATERIALS)
		MATERIALS[blend_material, blend_images] = self
	
	# new xml format
	def writeCal3D(self, file):
		file.write('<?xml version="1.0"?>\n')
		file.write('<HEADER MAGIC="XRF" VERSION="%i"/>\n' % CAL3D_VERSION)
		file.write('<MATERIAL NUMMAPS="%s">\n' % len(self.maps_filenames))
		file.write('\t<AMBIENT>%i %i %i %i</AMBIENT>\n' % self.amb)
		file.write('\t<DIFFUSE>%i %i %i %i</DIFFUSE>\n' % self.diff)
		file.write('\t<SPECULAR>%i %i %i %i</SPECULAR>\n' % self.spec)
		file.write('\t<SHININESS>%.6f</SHININESS>\n' % self.shininess)
		
		for map_filename in self.maps_filenames:
			file.write('\t<MAP>%s</MAP>\n' % map_filename)
		
		file.write('</MATERIAL>\n')


class Cal3DMesh(object):
	__slots__ = 'name', 'submeshes', 'matrix', 'matrix_normal'
	def __init__(self, ob, blend_mesh, blend_world):
		self.name      = ob.name
		self.submeshes = []
		
		BPyMesh.meshCalcNormals(blend_mesh)
		
		self.matrix = ob.matrixWorld
		self.matrix_normal = self.matrix.copy().rotationPart()
		
		#if BASE_MATRIX:
		#	matrix = matrix_multiply(BASE_MATRIX, matrix)
		
		face_groups = {}
		blend_materials = blend_mesh.materials
		uvlayers = ()
		mat = None # incase we have no materials
		if blend_mesh.faceUV:
			uvlayers = blend_mesh.getUVLayerNames()
			if len(uvlayers) == 1:
				for f in blend_mesh.faces:
					image =						(f.image,) # bit in a tuple so we can match multi UV code
					if blend_materials:	mat =	blend_materials[f.mat] # if no materials, mat will always be None
					face_groups.setdefault( (mat,image), (mat,image,[]) )[2].append( f )
			else:
				# Multi UV's
				face_multi_images = [[] for i in xrange(len(blend_mesh.faces))]
				face_multi_uvs = [[[] for i in xrange(len(f))  ] for f in blend_mesh.faces]
				for uvlayer in uvlayers:
					blend_mesh.activeUVLayer = uvlayer
					for i, f in enumerate(blend_mesh.faces):
						face_multi_images[i].append(f.image)
						if f.image:
							for j, uv in enumerate(f.uv):
								face_multi_uvs[i][j].append( tuple(uv) )
				
				# Convert UV's to tuples so they can be compared with eachother
				# when creating new verts
				for fuv in face_multi_uvs:
					for i, uv in enumerate(fuv):
						fuv[i] = tuple(uv)
				
				for i, f in enumerate(blend_mesh.faces):
					image =						tuple(face_multi_images[i])
					if blend_materials: mat =	blend_materials[f.mat]
					face_groups.setdefault( (mat,image), (mat,image,[]) )[2].append( f )
		else:
			# No UV's
			for f in blend_mesh.faces:
				if blend_materials: mat =	blend_materials[f.mat]
				face_groups.setdefault( (mat,()), (mat,(),[]) )[2].append( f )
		
		for blend_material, blend_images, faces in face_groups.itervalues():
			
			try:		material = MATERIALS[blend_material, blend_images]
			except:		material = MATERIALS[blend_material, blend_images] = Cal3DMaterial(blend_world, blend_material, blend_images)
			
			submesh = Cal3DSubMesh(self, material, len(self.submeshes))
			self.submeshes.append(submesh)
			
			# Check weather we need to write UVs, dont do it if theres no image
			# Multilayer UV's have alredy checked that they have images when 
			# building face_multi_uvs
			if len(uvlayers) == 1:
				if blend_images == (None,):
					write_single_layer_uvs = False
				else:
					write_single_layer_uvs = True
			
			
			for face in faces:
				
				if not face.smooth:
					normal = face.no
				
				face_vertices = []
				face_v = face.v
				
				
				if len(uvlayers)>1:
					for i, blend_vert in enumerate(face_v):
						if face.smooth:		normal = blend_vert.no
						vertex = submesh.getVertex(blend_mesh, blend_vert, normal, face_multi_uvs[face.index][i])
						face_vertices.append(vertex)
				
				elif len(uvlayers)==1:
					if write_single_layer_uvs:
						face_uv = face.uv
					
					for i, blend_vert in enumerate(face_v):
						if face.smooth:		normal = blend_vert.no
						if write_single_layer_uvs:	uvs = (tuple(face_uv[i]),)
						else:						uvs = ()
						
						vertex = submesh.getVertex(blend_mesh, blend_vert, normal, uvs )	
						face_vertices.append(vertex)
				else:
					# No UVs
					for i, blend_vert in enumerate(face_v):
						if face.smooth:		normal = blend_vert.no
						vertex = submesh.getVertex(blend_mesh, blend_vert, normal, () )
						face_vertices.append(vertex)
				
				
				# Split faces with more than 3 vertices
				for i in xrange(1, len(face) - 1):
					submesh.faces.append(Cal3DFace(face_vertices[0], face_vertices[i], face_vertices[i + 1]))
	
	def writeCal3D(self, file):
		file.write('<?xml version="1.0"?>\n')
		file.write('<HEADER MAGIC="XMF" VERSION="%i"/>\n' % CAL3D_VERSION)
		file.write('<MESH NUMSUBMESH="%i">\n' % len(self.submeshes))
		for submesh in self.submeshes:
			submesh.writeCal3D(file, self.matrix, self.matrix_normal)
		file.write('</MESH>\n')


class Cal3DSubMesh(object):
	__slots__ = 'material', 'vertices', 'vert_mapping', 'vert_count', 'faces', 'nb_lodsteps', 'springs', 'id'
	def __init__(self, mesh, material, id):
		self.material   = material
		self.vertices   = []
		self.vert_mapping = {} # map original indicies to local
		self.vert_count = 0
		self.faces      = []
		self.nb_lodsteps = 0
		self.springs    = []
		self.id = id
	
	def getVertex(self, blend_mesh, blend_vert, normal, maps):
		'''
		Request a vertex, and create a new one or return a matching vertex
		'''
		blend_index = blend_vert.index
		index_map = self.vert_mapping.get(blend_index)
		
		if index_map == None:
			vertex = Cal3DVertex(blend_vert.co, normal, maps, blend_mesh.getVertexInfluences(blend_index))
			self.vertices.append([vertex])
			self.vert_mapping[blend_index] = len(self.vert_mapping)
			self.vert_count +=1
			return vertex
		else:
			vertex_list = self.vertices[index_map]
			
			for v in vertex_list:
				if	v.normal == normal and\
					v.maps == maps:
						return v # reusing
			
			# No match, add a new vert
			# Use the first verts influences
			vertex = Cal3DVertex(blend_vert.co, normal, maps, vertex_list[0].influences)
			vertex_list.append(vertex)
			# self.vert_mapping[blend_index] = len(self.vert_mapping)
			self.vert_count +=1
			return vertex
		
	
	def compute_lods(self):
		'''Computes LODs info for Cal3D (there's no Blender related stuff here).'''
		
		print 'Start LODs computation...'
		vertex2faces = {}
		for face in self.faces:
			for vertex in (face.vertex1, face.vertex2, face.vertex3):
				l = vertex2faces.get(vertex)
				if not l: vertex2faces[vertex] = [face]
				else: l.append(face)
				
		couple_treated         = {}
		couple_collapse_factor = []
		for face in self.faces:
			for a, b in ((face.vertex1, face.vertex2), (face.vertex1, face.vertex3), (face.vertex2, face.vertex3)):
				a = a.cloned_from or a
				b = b.cloned_from or b
				if a.id > b.id: a, b = b, a
				if not couple_treated.has_key((a, b)):
					# The collapse factor is simply the distance between the 2 points :-(
					# This should be improved !!
					if vector_dotproduct(a.normal, b.normal) < 0.9: continue
					couple_collapse_factor.append((point_distance(a.loc, b.loc), a, b))
					couple_treated[a, b] = 1
			
		couple_collapse_factor.sort()
		
		collapsed    = {}
		new_vertices = []
		new_faces    = []
		for factor, v1, v2 in couple_collapse_factor:
			# Determines if v1 collapses to v2 or v2 to v1.
			# We choose to keep the vertex which is on the smaller number of faces, since
			# this one has more chance of being in an extrimity of the body.
			# Though heuristic, this rule yields very good results in practice.
			if   len(vertex2faces[v1]) <  len(vertex2faces[v2]): v2, v1 = v1, v2
			elif len(vertex2faces[v1]) == len(vertex2faces[v2]):
				if collapsed.get(v1, 0): v2, v1 = v1, v2 # v1 already collapsed, try v2
				
			if (not collapsed.get(v1, 0)) and (not collapsed.get(v2, 0)):
				collapsed[v1] = 1
				collapsed[v2] = 1
				
				# Check if v2 is already colapsed
				while v2.collapse_to: v2 = v2.collapse_to
				
				common_faces = filter(vertex2faces[v1].__contains__, vertex2faces[v2])
				
				v1.collapse_to         = v2
				v1.face_collapse_count = len(common_faces)
				
				for clone in v1.clones:
					# Find the clone of v2 that correspond to this clone of v1
					possibles = []
					for face in vertex2faces[clone]:
						possibles.append(face.vertex1)
						possibles.append(face.vertex2)
						possibles.append(face.vertex3)
					clone.collapse_to = v2
					for vertex in v2.clones:
						if vertex in possibles:
							clone.collapse_to = vertex
							break
						
					clone.face_collapse_count = 0
					new_vertices.append(clone)
	
				# HACK -- all faces get collapsed with v1 (and no faces are collapsed with v1's
				# clones). This is why we add v1 in new_vertices after v1's clones.
				# This hack has no other incidence that consuming a little few memory for the
				# extra faces if some v1's clone are collapsed but v1 is not.
				new_vertices.append(v1)
				
				self.nb_lodsteps += 1 + len(v1.clones)
				
				new_faces.extend(common_faces)
				for face in common_faces:
					face.can_collapse = 1
					
					# Updates vertex2faces
					vertex2faces[face.vertex1].remove(face)
					vertex2faces[face.vertex2].remove(face)
					vertex2faces[face.vertex3].remove(face)
				vertex2faces[v2].extend(vertex2faces[v1])
				
		new_vertices.extend(filter(lambda vertex: not vertex.collapse_to, self.vertices))
		new_vertices.reverse() # Cal3D want LODed vertices at the end
		for i in xrange(len(new_vertices)): new_vertices[i].id = i
		self.vertices = new_vertices
		
		new_faces.extend(filter(lambda face: not face.can_collapse, self.faces))
		new_faces.reverse() # Cal3D want LODed faces at the end
		self.faces = new_faces
		
		print 'LODs computed : %s vertices can be removed (from a total of %s).' % (self.nb_lodsteps, len(self.vertices))
	
	
	def writeCal3D(self, file, matrix, matrix_normal):
		
		file.write('\t<SUBMESH NUMVERTICES="%i" NUMFACES="%i" MATERIAL="%i" ' % \
				(self.vert_count, len(self.faces), self.material.id))
		file.write('NUMLODSTEPS="%i" NUMSPRINGS="%i" NUMTEXCOORDS="%i">\n' % \
				 (self.nb_lodsteps, len(self.springs),
				 len(self.material.maps_filenames)))
		
		i = 0
		for v in self.vertices:
			for item in v:
				item.id = i
				item.writeCal3D(file, matrix, matrix_normal)
				i += 1
		
		for item in self.springs:
			item.writeCal3D(file)
		for item in self.faces:
			item.writeCal3D(file)
		
		file.write('\t</SUBMESH>\n')

class Cal3DVertex(object):
	__slots__ = 'loc','normal','collapse_to','face_collapse_count','maps','influences','weight','cloned_from','clones','id'
	def __init__(self, loc, normal, maps, blend_influences):
		self.loc    = loc
		self.normal = normal
		self.collapse_to         = None
		self.face_collapse_count = 0
		self.maps       = maps
		self.weight = None
		
		self.cloned_from = None
		self.clones      = []
		
		self.id = -1
		
		if len(blend_influences) == 0 or isinstance(blend_influences[0], Cal3DInfluence): 
			# This is a copy from another vert
			self.influences = blend_influences
		else:
			# Pass the blender influences
			
			self.influences = []
			# should this really be a warning? (well currently enabled,
			# because blender has some bugs where it doesn't return
			# influences in python api though they are set, and because
			# cal3d<=0.9.1 had bugs where objects without influences
			# aren't drawn.
			#if not blend_influences:
			#	print 'A vertex of object "%s" has no influences.\n(This occurs on objects placed in an invisible layer, you can fix it by using a single layer)' % ob.name
			
			# sum of influences is not always 1.0 in Blender ?!?!
			sum = 0.0
			
			for bone_name, weight in blend_influences:
				if BONES.get(bone_name):
					sum += weight
			
			for bone_name, weight in blend_influences:
				bone = BONES.get(bone_name)
				if not bone: # keys
					# print 'Couldnt find bone "%s" which influences object "%s"' % (bone_name, ob.name)
					continue
				
				if weight:
					self.influences.append(Cal3DInfluence(bone, weight / sum))
	
	
	def writeCal3D(self, file, matrix, matrix_normal):
		if self.collapse_to:
			collapse_id = self.collapse_to.id
		else:
			collapse_id = -1
		file.write('\t\t<VERTEX ID="%i" NUMINFLUENCES="%i">\n' % \
				(self.id, len(self.influences)))
		file.write('\t\t\t<POS>%.6f %.6f %.6f</POS>\n' % tuple(self.loc*matrix))
		file.write('\t\t\t<NORM>%.6f %.6f %.6f</NORM>\n' % tuple( (self.normal*matrix_normal).normalize() ))
		if collapse_id != -1:
			file.write('\t\t\t<COLLAPSEID>%i</COLLAPSEID>\n' % collapse_id)
			file.write('\t\t\t<COLLAPSECOUNT>%i</COLLAPSECOUNT>\n' % \
					 self.face_collapse_count)
		
		for uv in self.maps:
			# we cant have more UV's then our materials image maps
			# check for this
			file.write('\t\t\t<TEXCOORD>%.6f %.6f</TEXCOORD>\n' % uv)
		
		for item in self.influences:
			item.writeCal3D(file)
		
		if self.weight != None:
			file.write('\t\t\t<PHYSIQUE>%.6f</PHYSIQUE>\n' % len(self.weight))
		file.write('\t\t</VERTEX>\n')

class Cal3DInfluence(object):
	__slots__ = 'bone', 'weight'
	def __init__(self, bone, weight):
		self.bone   = bone
		self.weight = weight
	
	def writeCal3D(self, file):
		file.write('\t\t\t<INFLUENCE ID="%i">%.6f</INFLUENCE>\n' % \
					 (self.bone.id, self.weight))

class Cal3DSpring(object):
	__slots__ = 'vertex1', 'vertex2', 'spring_coefficient', 'idlelength'
	def __init__(self, vertex1, vertex2):
		self.vertex1 = vertex1
		self.vertex2 = vertex2
		self.spring_coefficient = 0.0
		self.idlelength = 0.0
	
	def writeCal3D(self, file):
		file.write('\t\t<SPRING VERTEXID="%i %i" COEF="%.6f" LENGTH="%.6f"/>\n' % \
					 (self.vertex1.id, self.vertex2.id, self.spring_coefficient, self.idlelength))

class Cal3DFace(object):
	__slots__ = 'vertex1', 'vertex2', 'vertex3', 'can_collapse',
	def __init__(self, vertex1, vertex2, vertex3):
		self.vertex1 = vertex1
		self.vertex2 = vertex2
		self.vertex3 = vertex3
		self.can_collapse = 0
	
	def writeCal3D(self, file):
		file.write('\t\t<FACE VERTEXID="%i %i %i"/>\n' % \
					 (self.vertex1.id, self.vertex2.id, self.vertex3.id))

class Cal3DSkeleton(object):
	__slots__ = 'bones'
	def __init__(self):
		self.bones = []
	
	def writeCal3D(self, file):
		file.write('<?xml version="1.0"?>\n')
		file.write('<HEADER MAGIC="XSF" VERSION="%i"/>\n' % CAL3D_VERSION)
		file.write('<SKELETON NUMBONES="%i">\n' % len(self.bones))
		for item in self.bones:
			item.writeCal3D(file)
		
		file.write('</SKELETON>\n')

BONES = {}
POSEBONES= {}
class Cal3DBone(object):
	__slots__ = 'head', 'tail', 'name', 'cal3d_parent', 'loc', 'quat', 'children', 'matrix', 'lloc', 'lquat', 'id'
	def __init__(self, skeleton, blend_bone, arm_matrix, cal3d_parent=None):
		
		# def treat_bone(b, parent = None):
		head = blend_bone.head['BONESPACE']
		tail = blend_bone.tail['BONESPACE']
		#print parent.quat
		# Turns the Blender's head-tail-roll notation into a quaternion
		#quat = matrix2quaternion(blender_bone2matrix(head, tail, blend_bone.roll['BONESPACE']))
		quat = matrix2quaternion(blend_bone.matrix['BONESPACE'].copy().resize4x4())
		
		# Pose location
		ploc = POSEBONES[blend_bone.name].loc
		
		if cal3d_parent:
			# Compute the translation from the parent bone's head to the child
			# bone's head, in the parent bone coordinate system.
			# The translation is parent_tail - parent_head + child_head,
			# but parent_tail and parent_head must be converted from the parent's parent
			# system coordinate into the parent system coordinate.
			
			parent_invert_transform = matrix_invert(quaternion2matrix(cal3d_parent.quat))
			parent_head = vector_by_matrix_3x3(cal3d_parent.head, parent_invert_transform)
			parent_tail = vector_by_matrix_3x3(cal3d_parent.tail, parent_invert_transform)
			ploc = vector_add(ploc, blend_bone.head['BONESPACE'])
			
			# EDIT!!! FIX BONE OFFSET BE CAREFULL OF THIS PART!!! ??
			#diff = vector_by_matrix_3x3(head, parent_invert_transform)
			parent_tail= vector_add(parent_tail, head)
			# DONE!!!
			
			parentheadtotail = vector_sub(parent_tail, parent_head)
			# hmm this should be handled by the IPos, but isn't for non-animated
			# bones which are transformed in the pose mode...
			loc = parentheadtotail
			
		else:
			# Apply the armature's matrix to the root bones
			head = point_by_matrix(head, arm_matrix)
			tail = point_by_matrix(tail, arm_matrix)
			
			loc = head 
			quat = matrix2quaternion(matrix_multiply(arm_matrix, quaternion2matrix(quat))) # Probably not optimal
			
		self.head = head
		self.tail = tail
		
		self.cal3d_parent = cal3d_parent
		self.name   = blend_bone.name
		self.loc = loc
		self.quat = quat
		self.children = []
		
		self.matrix = matrix_translate(quaternion2matrix(quat), loc)
		if cal3d_parent:
			self.matrix = matrix_multiply(cal3d_parent.matrix, self.matrix)
		
		# lloc and lquat are the bone => model space transformation (translation and rotation).
		# They are probably specific to Cal3D.
		m = matrix_invert(self.matrix)
		self.lloc = m[3][0], m[3][1], m[3][2]
		self.lquat = matrix2quaternion(m)
		
		self.id = len(skeleton.bones)
		skeleton.bones.append(self)
		BONES[self.name] = self
		
		if not blend_bone.hasChildren():	return
		for blend_child in blend_bone.children:
			self.children.append(Cal3DBone(skeleton, blend_child, arm_matrix, self))
		

	def writeCal3D(self, file):
		file.write('\t<BONE ID="%i" NAME="%s" NUMCHILD="%i">\n' % \
				(self.id, self.name, len(self.children)))
		# We need to negate quaternion W value, but why ?
		file.write('\t\t<TRANSLATION>%.6f %.6f %.6f</TRANSLATION>\n' % \
				 (self.loc[0], self.loc[1], self.loc[2]))
		file.write('\t\t<ROTATION>%.6f %.6f %.6f %.6f</ROTATION>\n' % \
				 (self.quat[0], self.quat[1], self.quat[2], -self.quat[3]))
		file.write('\t\t<LOCALTRANSLATION>%.6f %.6f %.6f</LOCALTRANSLATION>\n' % \
				 (self.lloc[0], self.lloc[1], self.lloc[2]))
		file.write('\t\t<LOCALROTATION>%.6f %.6f %.6f %.6f</LOCALROTATION>\n' % \
				 (self.lquat[0], self.lquat[1], self.lquat[2], -self.lquat[3]))
		if self.cal3d_parent:
			file.write('\t\t<PARENTID>%i</PARENTID>\n' % self.cal3d_parent.id)
		else:
			file.write('\t\t<PARENTID>%i</PARENTID>\n' % -1)
		
		for item in self.children:
			file.write('\t\t<CHILDID>%i</CHILDID>\n' % item.id)
			
		file.write('\t</BONE>\n')

class Cal3DAnimation:
	def __init__(self, name, duration = 0.0):
		self.name     = name
		self.duration = duration
		self.tracks   = {} # Map bone names to tracks
	
	def writeCal3D(self, file):
		file.write('<?xml version="1.0"?>\n')
		file.write('<HEADER MAGIC="XAF" VERSION="%i"/>\n' % CAL3D_VERSION)
		file.write('<ANIMATION DURATION="%.6f" NUMTRACKS="%i">\n' % \
				 (self.duration, len(self.tracks)))
		
		for item in self.tracks.itervalues():
			item.writeCal3D(file)
		
		file.write('</ANIMATION>\n')

class Cal3DTrack(object):
	__slots__ = 'bone', 'keyframes'
	def __init__(self, bone):
		self.bone      = bone
		self.keyframes = []

	def writeCal3D(self, file):
		file.write('\t<TRACK BONEID="%i" NUMKEYFRAMES="%i">\n' %
				(self.bone.id, len(self.keyframes)))
		for item in self.keyframes:
			item.writeCal3D(file)
		file.write('\t</TRACK>\n')

class Cal3DKeyFrame(object):
	__slots__ = 'time', 'loc', 'quat'
	def __init__(self, time, loc, quat):
		self.time = time
		self.loc  = loc
		self.quat = quat
	
	def writeCal3D(self, file):
		file.write('\t\t<KEYFRAME TIME="%.6f">\n' % self.time)
		file.write('\t\t\t<TRANSLATION>%.6f %.6f %.6f</TRANSLATION>\n' % \
				 (self.loc[0], self.loc[1], self.loc[2]))
		# We need to negate quaternion W value, but why ?
		file.write('\t\t\t<ROTATION>%.6f %.6f %.6f %.6f</ROTATION>\n' % \
				 (self.quat[0], self.quat[1], self.quat[2], -self.quat[3]))
		file.write('\t\t</KEYFRAME>\n')

def export_cal3d(filename, PREF_SCALE=0.1, PREF_BAKE_MOTION = True, PREF_ACT_ACTION_ONLY=True, PREF_SCENE_FRAMES=False):
	if not filename.endswith('.cfg'):
		filename += '.cfg'
	
	file_only = filename.split('/')[-1].split('\\')[-1]
	file_only_noext = file_only.split('.')[0]
	base_only = filename[:-len(file_only)]
	
	def new_name(dataname, ext):
		return file_only_noext + '_' + BPySys.cleanName(dataname) + ext
	
	#if EXPORT_FOR_SOYA:
	#	global BASE_MATRIX
	#	BASE_MATRIX = matrix_rotate_x(-math.pi / 2.0)
	# Get the sce
	
	sce = bpy.data.scenes.active
	blend_world = sce.world
	# ---- Export skeleton (armature) ----------------------------------------
	
	skeleton = Cal3DSkeleton()
	blender_armature = [ob for ob in sce.objects.context if ob.type == 'Armature']
	if len(blender_armature) > 1:	print "Found multiple armatures! using ",armatures[0].name
	if blender_armature: blender_armature = blender_armature[0]
	else:
		# Try find a meshes armature
		for ob in sce.objects.context:
			blender_armature = BPyObject.getObjectArmature(ob)
			if blender_armature:
				break
		
		if not blender_armature:
			Blender.Draw.PupMenu('Aborting%t|No Armature in selection')
			return

	# we need pose bone locations
	for pbone in blender_armature.getPose().bones.values():
		POSEBONES[pbone.name] = pbone

	Cal3DBone(skeleton, best_armature_root(blender_armature.getData()), blender_armature.matrixWorld)
	
	# ---- Export Mesh data ---------------------------------------------------
	meshes = []
	for ob in sce.objects.context:
		if ob.type != 'Mesh':		continue
		blend_mesh = ob.getData(mesh=1)
		
		if not blend_mesh.faces:			continue
		meshes.append( Cal3DMesh(ob, blend_mesh, blend_world) )
	
	# ---- Export animations --------------------------------------------------
	backup_action = blender_armature.action
	
	ANIMATIONS = []
	SUPPORTED_IPOS = 'QuatW', 'QuatX', 'QuatY', 'QuatZ', 'LocX', 'LocY', 'LocZ'
	
	if PREF_ACT_ACTION_ONLY:	action_items = [(blender_armature.action.name, blender_armature.action)]
	else:						action_items = Blender.Armature.NLA.GetActions().items()
	
	print len(action_items), 'action_items'
	
	for animation_name, blend_action in action_items:
		
		# get frame range
		if PREF_SCENE_FRAMES:
			action_start=	Blender.Get('staframe')
			action_end=		Blender.Get('endframe')
		else:
			_frames = blend_action.getFrameNumbers()
			action_start=	min(_frames);
			action_end=		max(_frames);
			del _frames
		
		blender_armature.action = blend_action
		
		if PREF_BAKE_MOTION:
			# We need to set the action active if we are getting baked data
			pose_data = BPyArmature.getBakedPoseData(blender_armature, action_start, action_end)
			
			# Fake, all we need is bone names
			blend_action_ipos_items = [(pbone, True) for pbone in POSEBONES.iterkeys()]
		else:
			# real (bone_name, ipo) pairs
			blend_action_ipos_items = blend_action.getAllChannelIpos().items()
		
			# Now we mau have some bones with no channels, easiest to add their names and an empty list here
			# this way they are exported with dummy keyfraames at teh first used frame
			action_bone_names = [name for name, ipo in blend_action_ipos_items]
			for bone_name in BONES: # iterkeys
				if bone_name not in action_bone_names:
					blend_action_ipos_items.append( (bone_name, []) )
		
		animation = Cal3DAnimation(animation_name)
		# ----------------------------
		ANIMATIONS.append(animation)
		animation.duration = 0.0
		
		for bone_name, ipo in blend_action_ipos_items:
			# Baked bones may have no IPO's width motion still
			if bone_name not in BONES:
				print '\tNo Bone "' + bone_name + '" in (from Animation "' + animation_name + '") ?!?'
				continue
			
			# So we can loop without errors
			if ipo==None: ipo = [] 
			
			bone = BONES[bone_name]
			track = animation.tracks[bone_name] = Cal3DTrack(bone)
			
			if PREF_BAKE_MOTION:
				for i in xrange(action_end - action_start):
					cal3dtime = i / 25.0 # assume 25FPS by default
					
					if cal3dtime > animation.duration:
						animation.duration = cal3dtime
					
					#print pose_data[i][bone_name], i
					loc, quat = pose_data[i][bone_name]
					
					loc = vector_by_matrix_3x3(loc, bone.matrix)
					loc = vector_add(bone.loc, loc)
					quat = quaternion_multiply(quat, bone.quat)
					quat = Quaternion(quat)
					
					quat.normalize()
					quat = tuple(quat)
					
					track.keyframes.append( Cal3DKeyFrame(cal3dtime, loc, quat) )
			
			else:
				#run 1: we need to find all time values where we need to produce keyframes
				times = set()
				for curve in ipo:
					curve_name = curve.name
					if curve_name in SUPPORTED_IPOS:
						for p in curve.bezierPoints:
							times.add( p.pt[0] )
				
				times = list(times)
				times.sort()
				
				# Incase we have no keys here or ipo==None
				if not times: times.append(action_start)

				# run2: now create keyframes
				for time in times:
					cal3dtime = (time-1) / 25.0 # assume 25FPS by default
					if cal3dtime > animation.duration:
						animation.duration = cal3dtime
					
					trans = Vector()
					quat  = Quaternion()
					
					for curve in ipo:
						val = curve.evaluate(time)
						# val = 0.0 
						curve_name= curve.name
						if   curve_name == 'LocX':  trans[0] = val
						elif curve_name == 'LocY':  trans[1] = val
						elif curve_name == 'LocZ':  trans[2] = val
						elif curve_name == 'QuatW': quat[3]  = val
						elif curve_name == 'QuatX': quat[0]  = val
						elif curve_name == 'QuatY': quat[1]  = val
						elif curve_name == 'QuatZ': quat[2]  = val
					
					transt = vector_by_matrix_3x3(trans, bone.matrix)
					loc = vector_add(bone.loc, transt)
					quat = quaternion_multiply(quat, bone.quat)
					quat = Quaternion(quat)
					
					quat.normalize()
					quat = tuple(quat)
					
					track.keyframes.append( Cal3DKeyFrame(cal3dtime, loc, quat) )
		
		
		if animation.duration <= 0:
			print 'Ignoring Animation "' + animation_name + '": duration is 0.\n'
			continue
	
	# Restore the original armature
	blender_armature.action = backup_action
	# ------------------------------------- End Animation
	
	
	
	cfg = open((filename), 'wb')
	cfg.write('# Cal3D model exported from Blender with export_cal3d.py\n# from %s\n' % Blender.Get('filename'))
	
	if PREF_SCALE != 1.0:	cfg.write('scale=%.6f\n' % PREF_SCALE)
	
	fname = file_only_noext + '.xsf'
	file = open( base_only +  fname, 'wb')
	skeleton.writeCal3D(file)
	file.close()
	
	cfg.write('skeleton=%s\n' % fname)
	
	for animation in ANIMATIONS:
		if not animation.name.startswith('_'):
			if animation.duration > 0.1: # Cal3D does not support animation with only one state
				fname = new_name(animation.name, '.xaf')
				file = open(base_only + fname, 'wb')
				animation.writeCal3D(file)
				file.close()
				cfg.write('animation=%s\n' % fname)
	
	for mesh in meshes:
		if not mesh.name.startswith('_'):
			fname = new_name(mesh.name, '.xmf')
			file = open(base_only + fname, 'wb')
			mesh.writeCal3D(file)
			file.close()
			
			cfg.write('mesh=%s\n' % fname)
	
	materials = MATERIALS.values()
	materials.sort(key = lambda a: a.id)
	for material in materials:
		# Just number materials, its less trouble
		fname = new_name(str(material.id), '.xrf')
		
		file = open(base_only + fname, 'wb')
		material.writeCal3D(file)
		file.close()
		
		cfg.write('material=%s\n' % fname)
	
	print 'Cal3D Saved to "%s.cfg"' % file_only_noext
	
	# Warnings
	if len(animation.tracks) < 2:
		Blender.Draw.PupMenu('Warning, the armature has less then 2 tracks, file may not load in Cal3d')


def export_cal3d_ui(filename):
	
	PREF_SCALE= Blender.Draw.Create(1.0)
	PREF_BAKE_MOTION = Blender.Draw.Create(1)
	PREF_ACT_ACTION_ONLY= Blender.Draw.Create(1)
	PREF_SCENE_FRAMES= Blender.Draw.Create(0)
	
	block = [\
	('Scale: ', PREF_SCALE, 0.01, 100, 'The scale to set in the Cal3d .cfg file (unsupported by soya)'),\
	('Baked Motion', PREF_BAKE_MOTION, 'use final pose position instead of ipo keyframes (IK and constraint support)'),\
	('Active Action', PREF_ACT_ACTION_ONLY, 'Only export action applied to this armature, else export all actions.'),\
	('Scene Frames', PREF_SCENE_FRAMES, 'Use scene frame range, else the actions start/end'),\
	]
	
	if not Blender.Draw.PupBlock('Cal3D Options', block):
		return
	
	Blender.Window.WaitCursor(1)
	export_cal3d(filename, 1.0/PREF_SCALE.val, PREF_BAKE_MOTION.val, PREF_ACT_ACTION_ONLY.val, PREF_SCENE_FRAMES.val)
	Blender.Window.WaitCursor(0)


#import os
if __name__ == '__main__':
	Blender.Window.FileSelector(export_cal3d_ui, 'Cal3D Export', Blender.Get('filename').replace('.blend', '.cfg'))
	#export_cal3d('/cally/data/skeleton/skeleton' + '.cfg', 1.0, True, False, False)
	#export_cal3d('/test' + '.cfg')
	#export_cal3d_ui('/test' + '.cfg')
	#os.system('cd /; wine /cal3d_miniviewer.exe /skeleton.cfg')
	#os.system('cd /cally/;wine cally')
