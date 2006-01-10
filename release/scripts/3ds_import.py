#!BPY

""" 
Name: '3D Studio (.3ds)...'
Blender: 237
Group: 'Import'
Tooltip: 'Import from 3DS file format. (.3ds)'
"""

__author__ = ["Bob Holcomb", "Richard Lärkäng", "Damien McGinnes", "Campbell Barton"]
__url__ = ("blender", "elysiun", "http://www.gametutorials.com")
__version__ = "0.92"
__bpydoc__ = """\

3ds Importer

This script imports a 3ds file and the materials into Blender for editing.

Loader is based on 3ds loader from www.gametutorials.com (Thanks DigiBen).

Changes:

0.92<br>
- Added support for diffuse, alpha, spec, bump maps in a single material

0.9<br>
- Reorganized code into object/material block functions<br>
- Use of Matrix() to copy matrix data<br>
- added support for material transparency<br>

0.81a (fork- not 0.9) Campbell Barton 2005-06-08<br>
- Simplified import code<br>
- Never overwrite data<br>
- Faster list handling<br>
- Leaves import selected<br>

0.81 Damien McGinnes 2005-01-09<br>
- handle missing images better<br>

0.8 Damien McGinnes 2005-01-08<br>
- copies sticky UV coords to face ones<br>
- handles images better<br>
- Recommend that you run 'RemoveDoubles' on each imported mesh after using this script

"""

# $Id$
#
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


# Importing modules

import Blender
from Blender import NMesh, Scene, Object, Material, Image, Texture

import sys, struct, string

import os

######################################################
# Data Structures
######################################################
#----- Primary Chunk, 
PRIMARY			=	long("0x4D4D",16)	# should be aat the beginning of each file
VERSION			=	long("0x0002",16)	#This gives the version of the .3ds file
EDITOR_BLOCK	=	long("0x3D3D",16)	#this is the Editor Data block, contains objects, materials
KEYFRAME_BLOCK	=	long("0xB000",16)	#This is the header for all of the key frame info

#------ sub defines of EDITOR_BLOCK
MATERIAL_BLOCK	=	long("0xAFFF",16)	#This stores the Material info
OBJECT_BLOCK	=	long("0x4000",16)	#This stores the Object,Camera,Light

#------ sub defines of OBJECT_BLOCK
OBJECT_MESH		=	long("0x4100",16)	# This lets us know that we are reading a new object
OBJECT_LIGHT	=	long("0x4600",16)	# This lets un know we are reading a light object
OBJECT_CAMERA	=	long("0x4700",16)	# This lets un know we are reading a camera object

#------ sub defines of OBJECT_MESH
MESH_VERTICES	=	long("0x4110",16)	# The objects vertices
MESH_FACES		=	long("0x4120",16)	# The objects faces
MESH_MATERIAL	=	long("0x4130",16)	# This is found if the object has a material, either texture map or color
MESH_UV			=	long("0x4140",16)	# The UV texture coordinates
MESH_TRANS_MATRIX	=	long("0x4160",16)	# The Object Matrix
MESH_COLOR		=	long("0x4165",16)	# The color of the object
MESH_TEXTURE_INFO	=	long("0x470",16)	# Info about the Object Texture

#------ sub defines of OBJECT_CAMERA
CAMERA_CONE		=	long("0x4710",16)	# The camera see cone
CAMERA_RANGES	=	long("0x4720",16)	# The camera range values

#------ sub defines of OBJECT_LIGHT
LIGHT_SPOTLIGHT	=	long("0x4610",16)	# A spotlight
LIGHT_ATTENUATE	=	long("0x4625",16)	# Light attenuation values


#------ sub defines of MATERIAL_BLOCK
MAT_NAME		=	long("0xA000",16)	# This holds the material name
MAT_AMBIENT		=	long("0xA010",16)	# Ambient color of the object/material
MAT_DIFFUSE		=	long("0xA020",16)	# This holds the color of the object/material
MAT_SPECULAR	=	long("0xA030",16)	# SPecular color of the object/material
MAT_SHINESS		=	long("0xA040",16)	# ??
MAT_TRANSPARENCY=	long("0xA050",16)	# Transparency value of material
MAT_SELF_ILLUM	=	long("0xA080",16)	# Self Illumination value of material
MAT_WIRE		=	long("0xA085",16)	# Only render's wireframe

MAT_TEXTURE_MAP	=	long("0xA200",16)	# This is a header for a new texture map
MAT_SPECULAR_MAP=	long("0xA204",16)	# This is a header for a new specular map
MAT_OPACITY_MAP	=	long("0xA210",16)	# This is a header for a new opacity map
MAT_REFLECTION_MAP=	long("0xA220",16)	# This is a header for a new reflection map
MAT_BUMP_MAP	=	long("0xA230",16)	# This is a header for a new bump map
MAT_MAP_FILENAME=	long("0xA300",16)	# This holds the file name of the texture
#lots more to add here for maps

######################################################
# Globals
######################################################
TEXTURE_DICT={}
MATERIAL_DICT={}


######################################################
# Chunk Class
######################################################
class chunk:
	ID=0
	length=0
	bytes_read=0

	binary_format="<HI"

	def __init__(self):
		self.ID=0
		self.length=0
		self.bytes_read=0

	def dump(self):
		print "ID in hex: ", hex(self.ID)
		print "length: ", self.length
		print "bytes_read: ", self.bytes_read
		

######################################################
# Helper functions
######################################################
def read_chunk(file, chunk):
	temp_data=file.read(struct.calcsize(chunk.binary_format))
	data=struct.unpack(chunk.binary_format, temp_data)
	chunk.ID=data[0]
	chunk.length=data[1]
	chunk.bytes_read=6

def skip_to_end(file, skip_chunk):
	buffer_size=skip_chunk.length-skip_chunk.bytes_read
	binary_format=str(buffer_size)+"c"
	temp_data=file.read(struct.calcsize(binary_format))
	skip_chunk.bytes_read+=buffer_size

def read_string(file):
	s=""
	index=0
	#read the first character
	temp_data=file.read(1)
	data=struct.unpack("c", temp_data)
	s=s+(data[0])
	#read in the characters till we get a null character
	while(ord(s[index])!=0):
		index+=1
		temp_data=file.read(1)
		data=struct.unpack("c", temp_data)
		s=s+(data[0])
	the_string=s[:-1]  #remove the null character from the string
	return str(the_string)

def getUniqueName(name):
	newName = name
	uniqueInt = 0
	while 1:
		try:
			ob = Object.Get(newName)
			# Okay, this is working, so lets make a new name
			newName = '%s.%d' % (name, uniqueInt)
			uniqueInt +=1
		except AttributeError:
			if newName not in NMesh.GetNames():
				return newName
			else:
				newName = '%s.%d' % (name, uniqueInt)
				uniqueInt +=1

def add_texture_to_material(image, texture, material, mapto):
	if mapto=="DIFFUSE":
		map=Texture.MapTo.COL
	elif mapto=="SPECULAR":
		map=Texture.MapTo.SPEC
	elif mapto=="OPACITY":
		map=Texture.MapTo.ALPHA
	elif mapto=="BUMP":
		map=Texture.MapTo.NOR
	else:
		print "/tError:  Cannot map to ", mapto
		return
	
	texture.setImage(image)
	texture_list=material.getTextures()
	index=0
	for tex in texture_list:
		if tex==None:
			material.setTexture(index,texture,Texture.TexCo.OBJECT,map)
			return
		else:
			index+=1
		if index>10:
			print "/tError: Cannot add diffuse map.  Too many textures"
	
######################################################
# Process an object (tri-mesh, Camera, or Light)
######################################################
def process_object_block(file, previous_chunk, object_list):
	# Localspace variable names, faster.
	STRUCT_SIZE_2FLOAT = struct.calcsize("2f")
	STRUCT_SIZE_3FLOAT = struct.calcsize("3f")
	STRUCT_SIZE_UNSIGNED_SHORT = struct.calcsize("H")
	STRUCT_SIZE_4UNSIGNED_SHORT = struct.calcsize("4H")
	STRUCT_SIZE_4x3MAT = struct.calcsize("ffffffffffff")
	
	#spare chunks
	new_chunk=chunk()
	temp_chunk=chunk()
	
	global TEXURE_DICT
	global MATERIAL_DICT
	
	#don't know which one we're making, so let's have a place for one of each
	new_mesh=None
	new_light=None
	new_camera=None
	
	#all objects have a name (first thing)
	tempName = str(read_string(file))
	obj_name = getUniqueName( tempName )
	previous_chunk.bytes_read += (len(tempName)+1)
	
	while (previous_chunk.bytes_read<previous_chunk.length):
		read_chunk(file, new_chunk)
		
		if (new_chunk.ID==OBJECT_MESH):
			new_mesh=Blender.NMesh.New(obj_name)
			while (new_chunk.bytes_read<new_chunk.length):
				read_chunk(file, temp_chunk)
				
				if (temp_chunk.ID==MESH_VERTICES):
					temp_data=file.read(STRUCT_SIZE_UNSIGNED_SHORT)
					data=struct.unpack("H", temp_data)
					temp_chunk.bytes_read+=2
					num_verts=data[0]
					for counter in range (num_verts):
						temp_data=file.read(STRUCT_SIZE_3FLOAT)
						temp_chunk.bytes_read += STRUCT_SIZE_3FLOAT
						data=struct.unpack("3f", temp_data)
						v=NMesh.Vert(data[0],data[1],data[2])
						new_mesh.verts.append(v)
		
				elif (temp_chunk.ID==MESH_FACES):
					temp_data=file.read(STRUCT_SIZE_UNSIGNED_SHORT)
					data=struct.unpack("H", temp_data)
					temp_chunk.bytes_read+=2
					num_faces=data[0]
					for counter in range(num_faces):
						temp_data=file.read(STRUCT_SIZE_4UNSIGNED_SHORT)
						temp_chunk.bytes_read += STRUCT_SIZE_4UNSIGNED_SHORT #4 short ints x 2 bytes each
						data=struct.unpack("4H", temp_data)
						#insert the mesh info into the faces, don't worry about data[3] it is a 3D studio thing
						f = NMesh.Face( [new_mesh.verts[data[i]] for i in xrange(3)] )
						f.uv = [ tuple(new_mesh.verts[data[i]].uvco[:2]) for  i in xrange(3) ]
						new_mesh.faces.append(f)
		
				elif (temp_chunk.ID==MESH_MATERIAL):
					material_name=""
					material_name=str(read_string(file))
					temp_chunk.bytes_read += len(material_name)+1 # remove 1 null character.
					material_found=0
					for mat in Material.Get():
						if(mat.name==material_name):
							if len(new_mesh.materials) >= 15:
								print "\tCant assign more than 16 materials per mesh, keep going..."
								break
							else:
								meshHasMat = 0
								for myMat in new_mesh.materials:
									if myMat.name == mat.name:
										meshHasMat = 1
								if meshHasMat == 0:
									new_mesh.addMaterial(mat)
									material_found=1
									#figure out what material index this is for the mesh
									for mat_counter in range(len(new_mesh.materials)):
										if new_mesh.materials[mat_counter].name == material_name:
											mat_index=mat_counter							
								break # get out of this for loop so we don't accidentally set material_found back to 0
						else:
							material_found=0
							
					if material_found == 1:
						#read the number of faces using this material
						temp_data=file.read(STRUCT_SIZE_UNSIGNED_SHORT)
						data=struct.unpack("H", temp_data)
						temp_chunk.bytes_read += STRUCT_SIZE_UNSIGNED_SHORT
						num_faces_using_mat=data[0]
						#list of faces using mat
						for face_counter in range(num_faces_using_mat):
							temp_data=file.read(STRUCT_SIZE_UNSIGNED_SHORT)
							temp_chunk.bytes_read += STRUCT_SIZE_UNSIGNED_SHORT
							data=struct.unpack("H", temp_data)
							new_mesh.faces[data[0]].materialIndex = mat_index	
							try:
								mname = MATERIAL_DICT[mat.name]
								new_mesh.faces[data[0]].image = TEXTURE_DICT[mname]
							except:
								continue
					else:
						#read past the information about the material you couldn't find
						skip_to_end(file,temp_chunk)	
		
				elif (new_chunk.ID == MESH_UV):
					temp_data=file.read(STRUCT_SIZE_UNSIGNED_SHORT)
					data=struct.unpack("H", temp_data)
					temp_chunk.bytes_read+=2
					num_uv=data[0]
		
					for counter in range(num_uv):
						temp_data=file.read(STRUCT_SIZE_2FLOAT)
						temp_chunk.bytes_read += STRUCT_SIZE_2FLOAT #2 float x 4 bytes each
						data=struct.unpack("2f", temp_data)
						#insert the insert the UV coords in the vertex data
						new_mesh.verts[counter].uvco = data
				
				elif (new_chunk.ID == MESH_TRANS_MATRIX):
					temp_data=file.read(STRUCT_SIZE_4x3MAT)
					data = list( struct.unpack("ffffffffffff", temp_data) )
					temp_chunk.bytes_read += STRUCT_SIZE_4x3MAT 
					new_matrix = Blender.Mathutils.Matrix(\
					data[:3] + [0],\
					data[3:6] + [0],\
					data[6:9] + [0],\
					data[9:] + [1])
					new_mesh.setMatrix(new_matrix)
				else:
					skip_to_end(file, temp_chunk)
				
				new_chunk.bytes_read+=temp_chunk.bytes_read
		
		elif (new_chunk.ID==OBJECT_LIGHT):
			skip_to_end(file,new_chunk)

		elif (new_chunk.ID==OBJECT_CAMERA):
			skip_to_end(file,new_chunk)
			
		else: #don't know what kind of object it is
			skip_to_end(file,new_chunk)
		
		if new_mesh!=None:
			object_list.append(NMesh.PutRaw(new_mesh))
		if new_light!=None:
			object_list.append(new_light)
		if new_camera!=None:
			object_list.append(new_camera)
		
		previous_chunk.bytes_read+=new_chunk.bytes_read

######################################################
# Process a Material
######################################################
def process_material_block(file, previous_chunk):
	# Localspace variable names, faster.
	STRUCT_SIZE_3BYTE = struct.calcsize("3B")
	STRUCT_SIZE_UNSIGNED_SHORT = struct.calcsize("H")
	
	#spare chunks
	new_chunk=chunk()
	temp_chunk=chunk()
	
	global TEXURE_DICT
	global MATERIAL_DICT
	
	new_material=Blender.Material.New()
	
	while (previous_chunk.bytes_read<previous_chunk.length):
		#read the next chunk
		read_chunk(file, new_chunk)
		
		if (new_chunk.ID==MAT_NAME):
			material_name=""
			material_name=str(read_string(file))
			new_chunk.bytes_read+=(len(material_name)+1) #plus one for the null character that ended the string
			new_material.setName(material_name)
			MATERIAL_DICT[material_name] = new_material.name
		
		elif (new_chunk.ID==MAT_AMBIENT):
			read_chunk(file, temp_chunk)
			temp_data=file.read(STRUCT_SIZE_3BYTE)
			data=struct.unpack("3B", temp_data)
			temp_chunk.bytes_read+=3
			new_material.mirCol = [float(col)/255 for col in data] # data [0,1,2] == rgb
			new_chunk.bytes_read+=temp_chunk.bytes_read

		elif (new_chunk.ID==MAT_DIFFUSE):
			read_chunk(file, temp_chunk)
			temp_data=file.read(STRUCT_SIZE_3BYTE)
			data=struct.unpack("3B", temp_data)
			temp_chunk.bytes_read+=3
			new_material.rgbCol = [float(col)/255 for col in data] # data [0,1,2] == rgb
			new_chunk.bytes_read+=temp_chunk.bytes_read

		elif (new_chunk.ID==MAT_SPECULAR):
			read_chunk(file, temp_chunk)
			temp_data=file.read(STRUCT_SIZE_3BYTE)
			data=struct.unpack("3B", temp_data)
			temp_chunk.bytes_read+=3
			new_material.specCol = [float(col)/255 for col in data] # data [0,1,2] == rgb
			new_chunk.bytes_read+=temp_chunk.bytes_read

		elif (new_chunk.ID==MAT_TEXTURE_MAP):
			new_texture=Blender.Texture.New('Diffuse')
			new_texture.setType('Image')
			while (new_chunk.bytes_read<new_chunk.length):
				read_chunk(file, temp_chunk)
				
				if (temp_chunk.ID==MAT_MAP_FILENAME):
					texture_name=""
					texture_name=str(read_string(file))
					try:
						img = Image.Load(texture_name)
						TEXTURE_DICT[new_material.name]=img
					except IOError:
						fname = os.path.join( os.path.dirname(FILENAME), texture_name)
						try:
							img = Image.Load(fname)
							TEXTURE_DICT[new_material.name]=img
						except IOError:
							print "\tERROR: failed to load image ",texture_name
							TEXTURE_DICT[new_material.name] = None # Dummy
							img=Blender.Image.New(fname,1,1,24) #blank image
					new_chunk.bytes_read += (len(texture_name)+1) #plus one for the null character that gets removed
					
				else:
					skip_to_end(file, temp_chunk)
				
				new_chunk.bytes_read+=temp_chunk.bytes_read
			
			#add the map to the material in the right channel
			add_texture_to_material(img, new_texture, new_material, "DIFFUSE")
				
		elif (new_chunk.ID==MAT_SPECULAR_MAP):
			new_texture=Blender.Texture.New('Specular')
			new_texture.setType('Image')
			while (new_chunk.bytes_read<new_chunk.length):
				read_chunk(file, temp_chunk)
				
				if (temp_chunk.ID==MAT_MAP_FILENAME):
					texture_name=""
					texture_name=str(read_string(file))
					try:
						img = Image.Load(texture_name)
						TEXTURE_DICT[new_material.name]=img
					except IOError:
						fname = os.path.join( os.path.dirname(FILENAME), texture_name)
						try:
							img = Image.Load(fname)
							TEXTURE_DICT[new_material.name]=img
						except IOError:
							print "\tERROR: failed to load image ",texture_name
							TEXTURE_DICT[new_material.name] = None # Dummy
							img=Blender.Image.New(fname,1,1,24) #blank image
					new_chunk.bytes_read += (len(texture_name)+1) #plus one for the null character that gets removed
				else:
					skip_to_end(file, temp_chunk)
				
				new_chunk.bytes_read+=temp_chunk.bytes_read
				
			#add the map to the material in the right channel
			add_texture_to_material(img, new_texture, new_material, "SPECULAR")
	
		elif (new_chunk.ID==MAT_OPACITY_MAP):
			new_texture=Blender.Texture.New('Opacity')
			new_texture.setType('Image')
			while (new_chunk.bytes_read<new_chunk.length):
				read_chunk(file, temp_chunk)
				
				if (temp_chunk.ID==MAT_MAP_FILENAME):
					texture_name=""
					texture_name=str(read_string(file))
					try:
						img = Image.Load(texture_name)
						TEXTURE_DICT[new_material.name]=img
					except IOError:
						fname = os.path.join( os.path.dirname(FILENAME), texture_name)
						try:
							img = Image.Load(fname)
							TEXTURE_DICT[new_material.name]=img
						except IOError:
							print "\tERROR: failed to load image ",texture_name
							TEXTURE_DICT[new_material.name] = None # Dummy
							img=Blender.Image.New(fname,1,1,24) #blank image
					new_chunk.bytes_read += (len(texture_name)+1) #plus one for the null character that gets removed
				else:
					skip_to_end(file, temp_chunk)
				
				new_chunk.bytes_read+=temp_chunk.bytes_read

			#add the map to the material in the right channel
			add_texture_to_material(img, new_texture, new_material, "OPACITY")

		elif (new_chunk.ID==MAT_BUMP_MAP):
			new_texture=Blender.Texture.New('Bump')
			new_texture.setType('Image')
			while (new_chunk.bytes_read<new_chunk.length):
				read_chunk(file, temp_chunk)
				
				if (temp_chunk.ID==MAT_MAP_FILENAME):
					texture_name=""
					texture_name=str(read_string(file))
					try:
						img = Image.Load(texture_name)
						TEXTURE_DICT[new_material.name]=img
					except IOError:
						fname = os.path.join( os.path.dirname(FILENAME), texture_name)
						try:
							img = Image.Load(fname)
							TEXTURE_DICT[new_material.name]=img
						except IOError:
							print "\tERROR: failed to load image ",texture_name
							TEXTURE_DICT[new_material.name] = None # Dummy
							img=Blender.Image.New(fname,1,1,24) #blank image 	
					new_chunk.bytes_read += (len(texture_name)+1) #plus one for the null character that gets removed
				else:
					skip_to_end(file, temp_chunk)
				
				new_chunk.bytes_read+=temp_chunk.bytes_read
				
			#add the map to the material in the right channel
			add_texture_to_material(img, new_texture, new_material, "BUMP")
			
		elif (new_chunk.ID==MAT_TRANSPARENCY):
			read_chunk(file, temp_chunk)
			temp_data=file.read(STRUCT_SIZE_UNSIGNED_SHORT)
			data=struct.unpack("H", temp_data)
			temp_chunk.bytes_read+=2
			new_material.setAlpha(1-(float(data[0])/100))
			new_chunk.bytes_read+=temp_chunk.bytes_read
			
		else:
			skip_to_end(file,new_chunk)
		
		previous_chunk.bytes_read+=new_chunk.bytes_read

######################################################
# process a main chunk
######################################################
def process_main_chunk(file,previous_chunk,new_object_list):
	
	#spare chunks
	new_chunk=chunk()
	temp_chunk=chunk()

	#Go through the main chunk
	while (previous_chunk.bytes_read<previous_chunk.length):
		read_chunk(file, new_chunk)
		
		if (new_chunk.ID==VERSION):
			temp_data=file.read(struct.calcsize("I"))
			data=struct.unpack("I", temp_data)
			version=data[0]
			new_chunk.bytes_read+=4 #read the 4 bytes for the version number
			if (version>3): #this loader works with version 3 and below, but may not with 4 and above
				print "\tNon-Fatal Error:  Version greater than 3, may not load correctly: ", version

		elif (new_chunk.ID==EDITOR_BLOCK):
			while(new_chunk.bytes_read<new_chunk.length):
				read_chunk(file, temp_chunk)
				if (temp_chunk.ID==MATERIAL_BLOCK):
					process_material_block(file, temp_chunk)
				elif (temp_chunk.ID==OBJECT_BLOCK):
					process_object_block(file, temp_chunk,new_object_list)
				else:
					skip_to_end(file,temp_chunk)
					
				new_chunk.bytes_read+=temp_chunk.bytes_read
		else: 
			skip_to_end(file,new_chunk)
	
		previous_chunk.bytes_read+=new_chunk.bytes_read

#***********************************************
# main entry point for loading 3ds files
#***********************************************
def load_3ds (filename):
	current_chunk=chunk()
	print "--------------------------------"
	print 'Importing "%s"' % filename
	time1 = Blender.sys.time()  #for timing purposes
	file=open(filename,"rb")
	new_object_list = []
	
	global FILENAME
	FILENAME=filename
	
	read_chunk(file, current_chunk)
	
	if (current_chunk.ID!=PRIMARY):
		print "\tFatal Error:  Not a valid 3ds file: ", filename
		file.close()
		return
	
	process_main_chunk(file, current_chunk, new_object_list)
	
	# Select all new objects.
	for ob in new_object_list: ob.sel = 1
	
	print 'finished importing: "%s" in %.4f sec.' % (filename, (Blender.sys.time()-time1))
	file.close()

#***********************************************
# MAIN
#***********************************************
def my_callback(filename):
	load_3ds(filename)

Blender.Window.FileSelector(my_callback, "Import 3DS", '*.3ds')

# For testing compatibility
'''
TIME = Blender.sys.time()
import os
for _3ds in os.listdir('/3ds/'):
	if _3ds.lower().endswith('3ds'):
		print _3ds
		newScn = Scene.New(_3ds)
		newScn.makeCurrent()
		my_callback('/3ds/' + _3ds)

print "TOTAL TIME: ", Blender.sys.time() - TIME
'''
