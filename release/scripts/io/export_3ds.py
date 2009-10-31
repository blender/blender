# coding: utf-8
__author__ = ["Campbell Barton", "Bob Holcomb", "Richard Lärkäng", "Damien McGinnes", "Mark Stijnman"]
__url__ = ("blenderartists.org", "www.blender.org", "www.gametutorials.com", "lib3ds.sourceforge.net/")
__version__ = "0.90a"
__bpydoc__ = """\

3ds Exporter

This script Exports a 3ds file.

Exporting is based on 3ds loader from www.gametutorials.com(Thanks DigiBen) and using information
from the lib3ds project (http://lib3ds.sourceforge.net/) sourcecode.
"""

# ***** BEGIN GPL LICENSE BLOCK *****
#
# Script copyright (C) Bob Holcomb 
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
# --------------------------------------------------------------------------


######################################################
# Importing modules
######################################################

import struct
import os
import time

import bpy

# import Blender
# from BPyMesh import getMeshFromObject
# from BPyObject import getDerivedObjects
# try: 
#     import struct
# except: 
#     struct = None

# also used by X3D exporter
# return a tuple (free, object list), free is True if memory should be freed later with free_derived_objects()
def create_derived_objects(ob):
	if ob.parent and ob.parent.dupli_type != 'NONE':
		return False, None

	if ob.dupli_type != 'NONE':
		ob.create_dupli_list()
		return True, [(dob.object, dob.matrix) for dob in ob.dupli_list]
	else:
		return False, [(ob, ob.matrix)]

# also used by X3D exporter
def free_derived_objects(ob):
	ob.free_dupli_list()

# So 3ds max can open files, limit names to 12 in length
# this is verry annoying for filenames!
name_unique = []
name_mapping = {}
def sane_name(name):
	name_fixed = name_mapping.get(name)
	if name_fixed != None:
		return name_fixed
	
	if len(name) > 12:
		new_name = name[:12]
	else:
		new_name = name
	
	i = 0
	
	while new_name in name_unique:
		new_name = new_name[:-4] + '.%.3d' % i
		i+=1
	
	name_unique.append(new_name)
	name_mapping[name] = new_name
	return new_name

######################################################
# Data Structures
######################################################

#Some of the chunks that we will export
#----- Primary Chunk, at the beginning of each file
PRIMARY= int("0x4D4D",16)

#------ Main Chunks
OBJECTINFO   =      int("0x3D3D",16);      #This gives the version of the mesh and is found right before the material and object information
VERSION      =      int("0x0002",16);      #This gives the version of the .3ds file
KFDATA       =      int("0xB000",16);      #This is the header for all of the key frame info

#------ sub defines of OBJECTINFO
MATERIAL=45055		#0xAFFF				// This stored the texture info
OBJECT=16384		#0x4000				// This stores the faces, vertices, etc...

#>------ sub defines of MATERIAL
MATNAME    =      int("0xA000",16);      # This holds the material name
MATAMBIENT   =      int("0xA010",16);      # Ambient color of the object/material
MATDIFFUSE   =      int("0xA020",16);      # This holds the color of the object/material
MATSPECULAR   =      int("0xA030",16);      # SPecular color of the object/material
MATSHINESS   =      int("0xA040",16);      # ??
MATMAP       =      int("0xA200",16);      # This is a header for a new material
MATMAPFILE    =      int("0xA300",16);      # This holds the file name of the texture

RGB1=	int("0x0011",16)
RGB2=	int("0x0012",16)

#>------ sub defines of OBJECT
OBJECT_MESH  =      int("0x4100",16);      # This lets us know that we are reading a new object
OBJECT_LIGHT =      int("0x4600",16);      # This lets un know we are reading a light object
OBJECT_CAMERA=      int("0x4700",16);      # This lets un know we are reading a camera object

#>------ sub defines of CAMERA
OBJECT_CAM_RANGES=   int("0x4720",16);      # The camera range values

#>------ sub defines of OBJECT_MESH
OBJECT_VERTICES =   int("0x4110",16);      # The objects vertices
OBJECT_FACES    =   int("0x4120",16);      # The objects faces
OBJECT_MATERIAL =   int("0x4130",16);      # This is found if the object has a material, either texture map or color
OBJECT_UV       =   int("0x4140",16);      # The UV texture coordinates
OBJECT_TRANS_MATRIX  =   int("0x4160",16); # The Object Matrix

#>------ sub defines of KFDATA
KFDATA_KFHDR            = int("0xB00A",16);
KFDATA_KFSEG            = int("0xB008",16);
KFDATA_KFCURTIME        = int("0xB009",16);
KFDATA_OBJECT_NODE_TAG  = int("0xB002",16);

#>------ sub defines of OBJECT_NODE_TAG
OBJECT_NODE_ID          = int("0xB030",16);
OBJECT_NODE_HDR         = int("0xB010",16);
OBJECT_PIVOT            = int("0xB013",16);
OBJECT_INSTANCE_NAME    = int("0xB011",16);
POS_TRACK_TAG			= int("0xB020",16);
ROT_TRACK_TAG			= int("0xB021",16);
SCL_TRACK_TAG			= int("0xB022",16);

def uv_key(uv):
	return round(uv[0], 6), round(uv[1], 6)
# 	return round(uv.x, 6), round(uv.y, 6)

# size defines:	
SZ_SHORT = 2
SZ_INT   = 4
SZ_FLOAT = 4

class _3ds_short(object):
	'''Class representing a short (2-byte integer) for a 3ds file.
	*** This looks like an unsigned short H is unsigned from the struct docs - Cam***'''
	__slots__ = 'value'
	def __init__(self, val=0):
		self.value=val
	
	def get_size(self):
		return SZ_SHORT

	def write(self,file):
		file.write(struct.pack("<H", self.value))
		
	def __str__(self):
		return str(self.value)

class _3ds_int(object):
	'''Class representing an int (4-byte integer) for a 3ds file.'''
	__slots__ = 'value'
	def __init__(self, val=0):
		self.value=val
	
	def get_size(self):
		return SZ_INT

	def write(self,file):
		file.write(struct.pack("<I", self.value))
	
	def __str__(self):
		return str(self.value)

class _3ds_float(object):
	'''Class representing a 4-byte IEEE floating point number for a 3ds file.'''
	__slots__ = 'value'
	def __init__(self, val=0.0):
		self.value=val
	
	def get_size(self):
		return SZ_FLOAT

	def write(self,file):
		file.write(struct.pack("<f", self.value))
	
	def __str__(self):
		return str(self.value)


class _3ds_string(object):
	'''Class representing a zero-terminated string for a 3ds file.'''
	__slots__ = 'value'
	def __init__(self, val=""):
		self.value=val
	
	def get_size(self):
		return (len(self.value)+1)

	def write(self,file):
		binary_format = "<%ds" % (len(self.value)+1)
		file.write(struct.pack(binary_format, self.value))
	
	def __str__(self):
		return self.value

class _3ds_point_3d(object):
	'''Class representing a three-dimensional point for a 3ds file.'''
	__slots__ = 'x','y','z'
	def __init__(self, point=(0.0,0.0,0.0)):
		self.x, self.y, self.z = point
		
	def get_size(self):
		return 3*SZ_FLOAT

	def write(self,file):
		file.write(struct.pack('<3f', self.x, self.y, self.z))
	
	def __str__(self):
		return '(%f, %f, %f)' % (self.x, self.y, self.z)
		
# Used for writing a track
"""
class _3ds_point_4d(object):
	'''Class representing a four-dimensional point for a 3ds file, for instance a quaternion.'''
	__slots__ = 'x','y','z','w'
	def __init__(self, point=(0.0,0.0,0.0,0.0)):
		self.x, self.y, self.z, self.w = point	
	
	def get_size(self):
		return 4*SZ_FLOAT

	def write(self,file):
		data=struct.pack('<4f', self.x, self.y, self.z, self.w)
		file.write(data)

	def __str__(self):
		return '(%f, %f, %f, %f)' % (self.x, self.y, self.z, self.w)
"""

class _3ds_point_uv(object):
	'''Class representing a UV-coordinate for a 3ds file.'''
	__slots__ = 'uv'
	def __init__(self, point=(0.0,0.0)):
		self.uv = point
	
	def __cmp__(self, other):
		return cmp(self.uv,other.uv)	
	
	def get_size(self):
		return 2*SZ_FLOAT
	
	def write(self,file):
		data=struct.pack('<2f', self.uv[0], self.uv[1])
		file.write(data)
	
	def __str__(self):
		return '(%g, %g)' % self.uv

class _3ds_rgb_color(object):
	'''Class representing a (24-bit) rgb color for a 3ds file.'''
	__slots__ = 'r','g','b'
	def __init__(self, col=(0,0,0)):
		self.r, self.g, self.b = col
	
	def get_size(self):
		return 3
	
	def write(self,file):
		file.write( struct.pack('<3B', int(255*self.r), int(255*self.g), int(255*self.b) ) )
# 		file.write( struct.pack('<3c', chr(int(255*self.r)), chr(int(255*self.g)), chr(int(255*self.b)) ) )
	
	def __str__(self):
		return '{%f, %f, %f}' % (self.r, self.g, self.b)

class _3ds_face(object):
	'''Class representing a face for a 3ds file.'''
	__slots__ = 'vindex'
	def __init__(self, vindex):
		self.vindex = vindex
	
	def get_size(self):
		return 4*SZ_SHORT
	
	def write(self,file):
		# The last zero is only used by 3d studio
		file.write(struct.pack("<4H", self.vindex[0],self.vindex[1], self.vindex[2], 0))
	
	def __str__(self):
		return '[%d %d %d]' % (self.vindex[0],self.vindex[1], self.vindex[2])

class _3ds_array(object):
	'''Class representing an array of variables for a 3ds file.

	Consists of a _3ds_short to indicate the number of items, followed by the items themselves.
	'''
	__slots__ = 'values', 'size'
	def __init__(self):
		self.values=[]
		self.size=SZ_SHORT
	
	# add an item:
	def add(self,item):
		self.values.append(item)
		self.size+=item.get_size()
	
	def get_size(self):
		return self.size
	
	def write(self,file):
		_3ds_short(len(self.values)).write(file)
		#_3ds_int(len(self.values)).write(file)
		for value in self.values:
			value.write(file)
	
	# To not overwhelm the output in a dump, a _3ds_array only
	# outputs the number of items, not all of the actual items. 
	def __str__(self):
		return '(%d items)' % len(self.values)

class _3ds_named_variable(object):
	'''Convenience class for named variables.'''
	
	__slots__ = 'value', 'name'
	def __init__(self, name, val=None):
		self.name=name
		self.value=val
	
	def get_size(self):
		if (self.value==None): 
			return 0
		else:
			return self.value.get_size()
	
	def write(self, file):
		if (self.value!=None): 
			self.value.write(file)
	
	def dump(self,indent):
		if (self.value!=None):
			spaces=""
			for i in range(indent):
				spaces+="  ";
			if (self.name!=""):
				print(spaces, self.name, " = ", self.value)
			else:
				print(spaces, "[unnamed]", " = ", self.value)


#the chunk class
class _3ds_chunk(object):
	'''Class representing a chunk in a 3ds file.

	Chunks contain zero or more variables, followed by zero or more subchunks.
	'''
	__slots__ = 'ID', 'size', 'variables', 'subchunks'
	def __init__(self, id=0):
		self.ID=_3ds_short(id)
		self.size=_3ds_int(0)
		self.variables=[]
		self.subchunks=[]
	
	def set_ID(id):
		self.ID=_3ds_short(id)
	
	def add_variable(self, name, var):
		'''Add a named variable. 
		
		The name is mostly for debugging purposes.'''
		self.variables.append(_3ds_named_variable(name,var))
	
	def add_subchunk(self, chunk):
		'''Add a subchunk.'''
		self.subchunks.append(chunk)

	def get_size(self):
		'''Calculate the size of the chunk and return it.
		
		The sizes of the variables and subchunks are used to determine this chunk\'s size.'''
		tmpsize=self.ID.get_size()+self.size.get_size()
		for variable in self.variables:
			tmpsize+=variable.get_size()
		for subchunk in self.subchunks:
			tmpsize+=subchunk.get_size()
		self.size.value=tmpsize
		return self.size.value

	def write(self, file):
		'''Write the chunk to a file.
		
		Uses the write function of the variables and the subchunks to do the actual work.'''
		#write header
		self.ID.write(file)
		self.size.write(file)
		for variable in self.variables:
			variable.write(file)
		for subchunk in self.subchunks:
			subchunk.write(file)
		
		
	def dump(self, indent=0):
		'''Write the chunk to a file.
		
		Dump is used for debugging purposes, to dump the contents of a chunk to the standard output. 
		Uses the dump function of the named variables and the subchunks to do the actual work.'''
		spaces=""
		for i in range(indent):
			spaces+="  ";
		print(spaces, "ID=", hex(self.ID.value), "size=", self.get_size())
		for variable in self.variables:
			variable.dump(indent+1)
		for subchunk in self.subchunks:
			subchunk.dump(indent+1)



######################################################
# EXPORT
######################################################

def get_material_images(material):
	# blender utility func.
	if material:
		return [s.texture.image for s in material.textures if s and s.texture.type == 'IMAGE' and s.texture.image]

	return []
# 	images = []
# 	if material:
# 		for mtex in material.getTextures():
# 			if mtex and mtex.tex.type == Blender.Texture.Types.IMAGE:
# 				image = mtex.tex.image
# 				if image:
# 					images.append(image) # maye want to include info like diffuse, spec here.
# 	return images


def make_material_subchunk(id, color):
	'''Make a material subchunk.
	
	Used for color subchunks, such as diffuse color or ambient color subchunks.'''
	mat_sub = _3ds_chunk(id)
	col1 = _3ds_chunk(RGB1)
	col1.add_variable("color1", _3ds_rgb_color(color));
	mat_sub.add_subchunk(col1)
# optional:
#	col2 = _3ds_chunk(RGB1)
#	col2.add_variable("color2", _3ds_rgb_color(color));
#	mat_sub.add_subchunk(col2)
	return mat_sub


def make_material_texture_chunk(id, images):
	"""Make Material Map texture chunk
	"""
	mat_sub = _3ds_chunk(id)
	
	def add_image(img):
		filename = os.path.basename(image.filename)
# 		filename = image.filename.split('\\')[-1].split('/')[-1]
		mat_sub_file = _3ds_chunk(MATMAPFILE)
		mat_sub_file.add_variable("mapfile", _3ds_string(sane_name(filename)))
		mat_sub.add_subchunk(mat_sub_file)
	
	for image in images:
		add_image(image)
	
	return mat_sub

def make_material_chunk(material, image):
	'''Make a material chunk out of a blender material.'''
	material_chunk = _3ds_chunk(MATERIAL)
	name = _3ds_chunk(MATNAME)
	
	if material:	name_str = material.name
	else:			name_str = 'None'
	if image:	name_str += image.name
		
	name.add_variable("name", _3ds_string(sane_name(name_str)))
	material_chunk.add_subchunk(name)
	
	if not material:
		material_chunk.add_subchunk(make_material_subchunk(MATAMBIENT, (0,0,0) ))
		material_chunk.add_subchunk(make_material_subchunk(MATDIFFUSE, (.8, .8, .8) ))
		material_chunk.add_subchunk(make_material_subchunk(MATSPECULAR, (1,1,1) ))
	
	else:
		material_chunk.add_subchunk(make_material_subchunk(MATAMBIENT, [a*material.ambient for a in material.diffuse_color] ))
# 		material_chunk.add_subchunk(make_material_subchunk(MATAMBIENT, [a*material.amb for a in material.rgbCol] ))
		material_chunk.add_subchunk(make_material_subchunk(MATDIFFUSE, material.diffuse_color))
# 		material_chunk.add_subchunk(make_material_subchunk(MATDIFFUSE, material.rgbCol))
		material_chunk.add_subchunk(make_material_subchunk(MATSPECULAR, material.specular_color))
# 		material_chunk.add_subchunk(make_material_subchunk(MATSPECULAR, material.specCol))
		
		images = get_material_images(material) # can be None
		if image: images.append(image)
		
		if images:
			material_chunk.add_subchunk(make_material_texture_chunk(MATMAP, images))
	
	return material_chunk

class tri_wrapper(object):
	'''Class representing a triangle.
	
	Used when converting faces to triangles'''
	
	__slots__ = 'vertex_index', 'mat', 'image', 'faceuvs', 'offset'
	def __init__(self, vindex=(0,0,0), mat=None, image=None, faceuvs=None):
		self.vertex_index= vindex
		self.mat= mat
		self.image= image
		self.faceuvs= faceuvs
		self.offset= [0, 0, 0] # offset indicies


def extract_triangles(mesh):
	'''Extract triangles from a mesh.
	
	If the mesh contains quads, they will be split into triangles.'''
	tri_list = []
	do_uv = len(mesh.uv_textures)
# 	do_uv = mesh.faceUV
	
# 	if not do_uv:
# 		face_uv = None
	
	img = None
	for i, face in enumerate(mesh.faces):
		f_v = face.verts
# 		f_v = face.v

		uf = mesh.active_uv_texture.data[i] if do_uv else None
		
		if do_uv:
			f_uv = uf.uv
			# f_uv =  (uf.uv1, uf.uv2, uf.uv3, uf.uv4) if face.verts[3] else (uf.uv1, uf.uv2, uf.uv3)
# 			f_uv = face.uv
			img = uf.image if uf else None
# 			img = face.image
			if img: img = img.name
		
		# if f_v[3] == 0:
		if len(f_v)==3:
			new_tri = tri_wrapper((f_v[0], f_v[1], f_v[2]), face.material_index, img)
# 			new_tri = tri_wrapper((f_v[0].index, f_v[1].index, f_v[2].index), face.mat, img)
			if (do_uv): new_tri.faceuvs= uv_key(f_uv[0]), uv_key(f_uv[1]), uv_key(f_uv[2])
			tri_list.append(new_tri)
		
		else: #it's a quad
			new_tri = tri_wrapper((f_v[0], f_v[1], f_v[2]), face.material_index, img)
# 			new_tri = tri_wrapper((f_v[0].index, f_v[1].index, f_v[2].index), face.mat, img)
			new_tri_2 = tri_wrapper((f_v[0], f_v[2], f_v[3]), face.material_index, img)
# 			new_tri_2 = tri_wrapper((f_v[0].index, f_v[2].index, f_v[3].index), face.mat, img)
			
			if (do_uv):
				new_tri.faceuvs= uv_key(f_uv[0]), uv_key(f_uv[1]), uv_key(f_uv[2])
				new_tri_2.faceuvs= uv_key(f_uv[0]), uv_key(f_uv[2]), uv_key(f_uv[3])
			
			tri_list.append( new_tri )
			tri_list.append( new_tri_2 )
		
	return tri_list
	
	
def remove_face_uv(verts, tri_list):
	'''Remove face UV coordinates from a list of triangles.
		
	Since 3ds files only support one pair of uv coordinates for each vertex, face uv coordinates
	need to be converted to vertex uv coordinates. That means that vertices need to be duplicated when
	there are multiple uv coordinates per vertex.'''
	
	# initialize a list of UniqueLists, one per vertex:
	#uv_list = [UniqueList() for i in xrange(len(verts))]
	unique_uvs= [{} for i in range(len(verts))]
	
	# for each face uv coordinate, add it to the UniqueList of the vertex
	for tri in tri_list:
		for i in range(3):
			# store the index into the UniqueList for future reference:
			# offset.append(uv_list[tri.vertex_index[i]].add(_3ds_point_uv(tri.faceuvs[i])))
			
			context_uv_vert= unique_uvs[tri.vertex_index[i]]
			uvkey= tri.faceuvs[i]
			
			offset_index__uv_3ds = context_uv_vert.get(uvkey)
			
			if not offset_index__uv_3ds:				
				offset_index__uv_3ds = context_uv_vert[uvkey] = len(context_uv_vert), _3ds_point_uv(uvkey)
			
			tri.offset[i] = offset_index__uv_3ds[0]
			
			
		
	# At this point, each vertex has a UniqueList containing every uv coordinate that is associated with it
	# only once.
	
	# Now we need to duplicate every vertex as many times as it has uv coordinates and make sure the
	# faces refer to the new face indices:
	vert_index = 0
	vert_array = _3ds_array()
	uv_array = _3ds_array()
	index_list = []
	for i,vert in enumerate(verts):
		index_list.append(vert_index)
		
		pt = _3ds_point_3d(vert.co) # reuse, should be ok
		uvmap = [None] * len(unique_uvs[i])
		for ii, uv_3ds in unique_uvs[i].values():
			# add a vertex duplicate to the vertex_array for every uv associated with this vertex:
			vert_array.add(pt)
			# add the uv coordinate to the uv array:
			# This for loop does not give uv's ordered by ii, so we create a new map
			# and add the uv's later
			# uv_array.add(uv_3ds)
			uvmap[ii] = uv_3ds
		
		# Add the uv's in the correct order
		for uv_3ds in uvmap:
			# add the uv coordinate to the uv array:
			uv_array.add(uv_3ds)
		
		vert_index += len(unique_uvs[i])
	
	# Make sure the triangle vertex indices now refer to the new vertex list:
	for tri in tri_list:
		for i in range(3):
			tri.offset[i]+=index_list[tri.vertex_index[i]]
		tri.vertex_index= tri.offset
	
	return vert_array, uv_array, tri_list

def make_faces_chunk(tri_list, mesh, materialDict):
	'''Make a chunk for the faces.
	
	Also adds subchunks assigning materials to all faces.'''
	
	materials = mesh.materials
	if not materials:
		mat = None
	
	face_chunk = _3ds_chunk(OBJECT_FACES)
	face_list = _3ds_array()
	
	
	if len(mesh.uv_textures):
# 	if mesh.faceUV:
		# Gather materials used in this mesh - mat/image pairs
		unique_mats = {}
		for i,tri in enumerate(tri_list):
			
			face_list.add(_3ds_face(tri.vertex_index))
			
			if materials:
				mat = materials[tri.mat]
				if mat: mat = mat.name
			
			img = tri.image
			
			try:
				context_mat_face_array = unique_mats[mat, img][1]
			except:
				
				if mat:	name_str = mat
				else:	name_str = 'None'
				if img: name_str += img
				
				context_mat_face_array = _3ds_array()
				unique_mats[mat, img] = _3ds_string(sane_name(name_str)), context_mat_face_array
				
			
			context_mat_face_array.add(_3ds_short(i))
			# obj_material_faces[tri.mat].add(_3ds_short(i))
		
		face_chunk.add_variable("faces", face_list)
		for mat_name, mat_faces in unique_mats.values():
			obj_material_chunk=_3ds_chunk(OBJECT_MATERIAL)
			obj_material_chunk.add_variable("name", mat_name)
			obj_material_chunk.add_variable("face_list", mat_faces)
			face_chunk.add_subchunk(obj_material_chunk)
			
	else:
		
		obj_material_faces=[]
		obj_material_names=[]
		for m in materials:
			if m:
				obj_material_names.append(_3ds_string(sane_name(m.name)))
				obj_material_faces.append(_3ds_array())
		n_materials = len(obj_material_names)
		
		for i,tri in enumerate(tri_list):
			face_list.add(_3ds_face(tri.vertex_index))
			if (tri.mat < n_materials):
				obj_material_faces[tri.mat].add(_3ds_short(i))
		
		face_chunk.add_variable("faces", face_list)
		for i in range(n_materials):
			obj_material_chunk=_3ds_chunk(OBJECT_MATERIAL)
			obj_material_chunk.add_variable("name", obj_material_names[i])
			obj_material_chunk.add_variable("face_list", obj_material_faces[i])
			face_chunk.add_subchunk(obj_material_chunk)
	
	return face_chunk

def make_vert_chunk(vert_array):
	'''Make a vertex chunk out of an array of vertices.'''
	vert_chunk = _3ds_chunk(OBJECT_VERTICES)
	vert_chunk.add_variable("vertices",vert_array)
	return vert_chunk

def make_uv_chunk(uv_array):
	'''Make a UV chunk out of an array of UVs.'''
	uv_chunk = _3ds_chunk(OBJECT_UV)
	uv_chunk.add_variable("uv coords", uv_array)
	return uv_chunk

def make_mesh_chunk(mesh, materialDict):
	'''Make a chunk out of a Blender mesh.'''
	
	# Extract the triangles from the mesh:
	tri_list = extract_triangles(mesh)
	
	if len(mesh.uv_textures):
# 	if mesh.faceUV:
		# Remove the face UVs and convert it to vertex UV:
		vert_array, uv_array, tri_list = remove_face_uv(mesh.verts, tri_list)
	else:
		# Add the vertices to the vertex array:
		vert_array = _3ds_array()
		for vert in mesh.verts:
			vert_array.add(_3ds_point_3d(vert.co))
		# If the mesh has vertex UVs, create an array of UVs:
		if len(mesh.sticky):
# 		if mesh.vertexUV:
			uv_array = _3ds_array()
			for uv in mesh.sticky:
# 			for vert in mesh.verts:
				uv_array.add(_3ds_point_uv(uv.co))
# 				uv_array.add(_3ds_point_uv(vert.uvco))
		else:
			# no UV at all:
			uv_array = None

	# create the chunk:
	mesh_chunk = _3ds_chunk(OBJECT_MESH)
	
	# add vertex chunk:
	mesh_chunk.add_subchunk(make_vert_chunk(vert_array))
	# add faces chunk:
	
	mesh_chunk.add_subchunk(make_faces_chunk(tri_list, mesh, materialDict))
	
	# if available, add uv chunk:
	if uv_array:
		mesh_chunk.add_subchunk(make_uv_chunk(uv_array))
	
	return mesh_chunk

""" # COMMENTED OUT FOR 2.42 RELEASE!! CRASHES 3DS MAX
def make_kfdata(start=0, stop=0, curtime=0):
	'''Make the basic keyframe data chunk'''
	kfdata = _3ds_chunk(KFDATA)
	
	kfhdr = _3ds_chunk(KFDATA_KFHDR)
	kfhdr.add_variable("revision", _3ds_short(0))
	# Not really sure what filename is used for, but it seems it is usually used
	# to identify the program that generated the .3ds:
	kfhdr.add_variable("filename", _3ds_string("Blender"))
	kfhdr.add_variable("animlen", _3ds_int(stop-start))
	
	kfseg = _3ds_chunk(KFDATA_KFSEG)
	kfseg.add_variable("start", _3ds_int(start))
	kfseg.add_variable("stop", _3ds_int(stop))
	
	kfcurtime = _3ds_chunk(KFDATA_KFCURTIME)
	kfcurtime.add_variable("curtime", _3ds_int(curtime))
	
	kfdata.add_subchunk(kfhdr)
	kfdata.add_subchunk(kfseg)
	kfdata.add_subchunk(kfcurtime)
	return kfdata
"""

"""
def make_track_chunk(ID, obj):
	'''Make a chunk for track data.
	
	Depending on the ID, this will construct a position, rotation or scale track.'''
	track_chunk = _3ds_chunk(ID)
	track_chunk.add_variable("track_flags", _3ds_short())
	track_chunk.add_variable("unknown", _3ds_int())
	track_chunk.add_variable("unknown", _3ds_int())
	track_chunk.add_variable("nkeys", _3ds_int(1))
	# Next section should be repeated for every keyframe, but for now, animation is not actually supported.
	track_chunk.add_variable("tcb_frame", _3ds_int(0))
	track_chunk.add_variable("tcb_flags", _3ds_short())
	if obj.type=='Empty':
		if ID==POS_TRACK_TAG:
			# position vector:
			track_chunk.add_variable("position", _3ds_point_3d(obj.getLocation()))
		elif ID==ROT_TRACK_TAG:
			# rotation (quaternion, angle first, followed by axis):
			q = obj.getEuler().toQuat()
			track_chunk.add_variable("rotation", _3ds_point_4d((q.angle, q.axis[0], q.axis[1], q.axis[2])))
		elif ID==SCL_TRACK_TAG:
			# scale vector:
			track_chunk.add_variable("scale", _3ds_point_3d(obj.getSize()))
	else:
		# meshes have their transformations applied before 
		# exporting, so write identity transforms here:
		if ID==POS_TRACK_TAG:
			# position vector:
			track_chunk.add_variable("position", _3ds_point_3d((0.0,0.0,0.0)))
		elif ID==ROT_TRACK_TAG:
			# rotation (quaternion, angle first, followed by axis):
			track_chunk.add_variable("rotation", _3ds_point_4d((0.0, 1.0, 0.0, 0.0)))
		elif ID==SCL_TRACK_TAG:
			# scale vector:
			track_chunk.add_variable("scale", _3ds_point_3d((1.0, 1.0, 1.0)))
	
	return track_chunk
"""

"""
def make_kf_obj_node(obj, name_to_id):
	'''Make a node chunk for a Blender object.
	
	Takes the Blender object as a parameter. Object id's are taken from the dictionary name_to_id.
	Blender Empty objects are converted to dummy nodes.'''
	
	name = obj.name
	# main object node chunk:
	kf_obj_node = _3ds_chunk(KFDATA_OBJECT_NODE_TAG)
	# chunk for the object id: 
	obj_id_chunk = _3ds_chunk(OBJECT_NODE_ID)
	# object id is from the name_to_id dictionary:
	obj_id_chunk.add_variable("node_id", _3ds_short(name_to_id[name]))
	
	# object node header:
	obj_node_header_chunk = _3ds_chunk(OBJECT_NODE_HDR)
	# object name:
	if obj.type == 'Empty':
		# Empties are called "$$$DUMMY" and use the OBJECT_INSTANCE_NAME chunk 
		# for their name (see below):
		obj_node_header_chunk.add_variable("name", _3ds_string("$$$DUMMY"))
	else:
		# Add the name:
		obj_node_header_chunk.add_variable("name", _3ds_string(sane_name(name)))
	# Add Flag variables (not sure what they do):
	obj_node_header_chunk.add_variable("flags1", _3ds_short(0))
	obj_node_header_chunk.add_variable("flags2", _3ds_short(0))
	
	# Check parent-child relationships:
	parent = obj.parent
	if (parent == None) or (parent.name not in name_to_id):
		# If no parent, or the parents name is not in the name_to_id dictionary,
		# parent id becomes -1:
		obj_node_header_chunk.add_variable("parent", _3ds_short(-1))
	else:
		# Get the parent's id from the name_to_id dictionary:
		obj_node_header_chunk.add_variable("parent", _3ds_short(name_to_id[parent.name]))
	
	# Add pivot chunk:
	obj_pivot_chunk = _3ds_chunk(OBJECT_PIVOT)
	obj_pivot_chunk.add_variable("pivot", _3ds_point_3d(obj.getLocation()))
	kf_obj_node.add_subchunk(obj_pivot_chunk)
	
	# add subchunks for object id and node header:
	kf_obj_node.add_subchunk(obj_id_chunk)
	kf_obj_node.add_subchunk(obj_node_header_chunk)

	# Empty objects need to have an extra chunk for the instance name:
	if obj.type == 'Empty':
		obj_instance_name_chunk = _3ds_chunk(OBJECT_INSTANCE_NAME)
		obj_instance_name_chunk.add_variable("name", _3ds_string(sane_name(name)))
		kf_obj_node.add_subchunk(obj_instance_name_chunk)
	
	# Add track chunks for position, rotation and scale:
	kf_obj_node.add_subchunk(make_track_chunk(POS_TRACK_TAG, obj))
	kf_obj_node.add_subchunk(make_track_chunk(ROT_TRACK_TAG, obj))
	kf_obj_node.add_subchunk(make_track_chunk(SCL_TRACK_TAG, obj))

	return kf_obj_node
"""

# import BPyMessages
def save_3ds(filename, context):
	'''Save the Blender scene to a 3ds file.'''
	# Time the export
	
	if not filename.lower().endswith('.3ds'):
		filename += '.3ds'
	
	# XXX
#	if not BPyMessages.Warning_SaveOver(filename):
#		return
	
	# XXX
	time1 = time.clock()
#	time1= Blender.sys.time()
#	Blender.Window.WaitCursor(1)

	sce = context.scene
#	sce= bpy.data.scenes.active
	
	# Initialize the main chunk (primary):
	primary = _3ds_chunk(PRIMARY)
	# Add version chunk:
	version_chunk = _3ds_chunk(VERSION)
	version_chunk.add_variable("version", _3ds_int(3))
	primary.add_subchunk(version_chunk)
	
	# init main object info chunk:
	object_info = _3ds_chunk(OBJECTINFO)
	
	''' # COMMENTED OUT FOR 2.42 RELEASE!! CRASHES 3DS MAX
	# init main key frame data chunk:
	kfdata = make_kfdata()
	'''
	
	# Get all the supported objects selected in this scene:
	# ob_sel= list(sce.objects.context)
	# mesh_objects = [ (ob, me) for ob in ob_sel   for me in (BPyMesh.getMeshFromObject(ob, None, True, False, sce),) if me ]
	# empty_objects = [ ob for ob in ob_sel if ob.type == 'Empty' ]
	
	# Make a list of all materials used in the selected meshes (use a dictionary,
	# each material is added once):
	materialDict = {}
	mesh_objects = []
	for ob in [ob for ob in context.scene.objects if ob.is_visible()]:
# 	for ob in sce.objects.context:

		# get derived objects
		free, derived = create_derived_objects(ob)

		if derived == None: continue

		for ob_derived, mat in derived:
# 		for ob_derived, mat in getDerivedObjects(ob, False):

			if ob.type not in ('MESH', 'CURVE', 'SURFACE', 'TEXT', 'META'):
				continue

			data = ob_derived.create_mesh(True, 'PREVIEW')
# 			data = getMeshFromObject(ob_derived, None, True, False, sce)
			if data:
				data.transform(mat)
# 				data.transform(mat, recalc_normals=False)
				mesh_objects.append((ob_derived, data))
				mat_ls = data.materials
				mat_ls_len = len(mat_ls)

				# get material/image tuples.
				if len(data.uv_textures):
# 				if data.faceUV:
					if not mat_ls:
						mat = mat_name = None
					
					for f, uf in zip(data.faces, data.active_uv_texture.data):
						if mat_ls:
							mat_index = f.material_index
# 							mat_index = f.mat
							if mat_index >= mat_ls_len:
								mat_index = f.mat = 0
							mat = mat_ls[mat_index]
							if mat:	mat_name = mat.name
							else:	mat_name = None
						# else there alredy set to none
							
						img = uf.image
# 						img = f.image
						if img:	img_name = img.name
						else:	img_name = None
						
						materialDict.setdefault((mat_name, img_name), (mat, img) )
						
					
				else:
					for mat in mat_ls:
						if mat: # material may be None so check its not.
							materialDict.setdefault((mat.name, None), (mat, None) )
					
					# Why 0 Why!
					for f in data.faces:
						if f.material_index >= mat_ls_len:
# 						if f.mat >= mat_ls_len:
							f.material_index = 0
							# f.mat = 0

		if free:
			free_derived_objects(ob)

	
	# Make material chunks for all materials used in the meshes:
	for mat_and_image in materialDict.values():
		object_info.add_subchunk(make_material_chunk(mat_and_image[0], mat_and_image[1]))
	
	# Give all objects a unique ID and build a dictionary from object name to object id:
	"""
	name_to_id = {}
	for ob, data in mesh_objects:
		name_to_id[ob.name]= len(name_to_id)
	#for ob in empty_objects:
	#	name_to_id[ob.name]= len(name_to_id)
	"""
	
	# Create object chunks for all meshes:
	i = 0
	for ob, blender_mesh in mesh_objects:
		# create a new object chunk
		object_chunk = _3ds_chunk(OBJECT)
		
		# set the object name
		object_chunk.add_variable("name", _3ds_string(sane_name(ob.name)))
		
		# make a mesh chunk out of the mesh:
		object_chunk.add_subchunk(make_mesh_chunk(blender_mesh, materialDict))
		object_info.add_subchunk(object_chunk)
		
		''' # COMMENTED OUT FOR 2.42 RELEASE!! CRASHES 3DS MAX
		# make a kf object node for the object:
		kfdata.add_subchunk(make_kf_obj_node(ob, name_to_id))
		'''
# 		if not blender_mesh.users:
		bpy.data.remove_mesh(blender_mesh)
# 		blender_mesh.verts = None

		i+=i

	# Create chunks for all empties:
	''' # COMMENTED OUT FOR 2.42 RELEASE!! CRASHES 3DS MAX
	for ob in empty_objects:
		# Empties only require a kf object node:
		kfdata.add_subchunk(make_kf_obj_node(ob, name_to_id))
		pass
	'''
	
	# Add main object info chunk to primary chunk:
	primary.add_subchunk(object_info)
	
	''' # COMMENTED OUT FOR 2.42 RELEASE!! CRASHES 3DS MAX
	# Add main keyframe data chunk to primary chunk:
	primary.add_subchunk(kfdata)
	'''
	
	# At this point, the chunk hierarchy is completely built.
	
	# Check the size:
	primary.get_size()
	# Open the file for writing:
	file = open( filename, 'wb' )
	
	# Recursively write the chunks to file:
	primary.write(file)
	
	# Close the file:
	file.close()
	
	# Debugging only: report the exporting time:
# 	Blender.Window.WaitCursor(0)
	print("3ds export time: %.2f" % (time.clock() - time1))
# 	print("3ds export time: %.2f" % (Blender.sys.time() - time1))
	
	# Debugging only: dump the chunk hierarchy:
	#primary.dump()


# if __name__=='__main__':
#     if struct:
#         Blender.Window.FileSelector(save_3ds, "Export 3DS", Blender.sys.makename(ext='.3ds'))
#     else:
#         Blender.Draw.PupMenu("Error%t|This script requires a full python installation")
# # save_3ds('/test_b.3ds')
from bpy.props import *
class EXPORT_OT_autodesk_3ds(bpy.types.Operator):
	'''Export to 3DS file format (.3ds).'''
	bl_idname = "export.autodesk_3ds"
	bl_label = 'Export 3DS'
	
	# List of operator properties, the attributes will be assigned
	# to the class instance from the operator settings before calling.

	
	# filename = StringProperty(name="File Name", description="File name used for exporting the 3DS file", maxlen= 1024, default= ""),
	path = StringProperty(name="File Path", description="File path used for exporting the 3DS file", maxlen= 1024, default= "")

	
	def execute(self, context):
		save_3ds(self.path, context)
		return ('FINISHED',)
	
	def invoke(self, context, event):
		wm = context.manager
		wm.add_fileselect(self.__operator__)
		return ('RUNNING_MODAL',)
	
	def poll(self, context): # Poll isnt working yet
		print("Poll")
		return context.active_object != None

bpy.ops.add(EXPORT_OT_autodesk_3ds)

# Add to a menu
import dynamic_menu
menu_func = lambda self, context: self.layout.itemO("export.autodesk_3ds", text="Autodesk 3DS...")
menu_item = dynamic_menu.add(bpy.types.INFO_MT_file_export, menu_func)
