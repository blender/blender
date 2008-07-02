#!BPY
""" 
Name: '3D Studio (.3ds)...'
Blender: 244
Group: 'Import'
Tooltip: 'Import from 3DS file format (.3ds)'
"""

__author__= ['Bob Holcomb', 'Richard L?rk?ng', 'Damien McGinnes', 'Campbell Barton', 'Mario Lapin']
__url__ = ("blenderartists.org", "www.blender.org", "www.gametutorials.com", "lib3ds.sourceforge.net/")
__version__= '0.996'
__bpydoc__= '''\

3ds Importer

This script imports a 3ds file and the materials into Blender for editing.

Loader is based on 3ds loader from www.gametutorials.com (Thanks DigiBen).

0.996 by Mario Lapin (mario.lapin@gmail.com) 13/04/200 <br>
 - Implemented workaround to correct association between name, geometry and materials of
   imported meshes.
   
   Without this patch, version 0.995 of this importer would associate to each mesh object the
   geometry and the materials of the previously parsed mesh object. By so, the name of the
   first mesh object would be thrown away, and the name of the last mesh object would be
   automatically merged with a '.001' at the end. No object would desappear, however object's
   names and materials would be completely jumbled.

0.995 by Campbell Barton<br>
- workaround for buggy mesh vert delete
- minor tweaks

0.99 by Bob Holcomb<br>
- added support for floating point color values that previously broke on import.

0.98 by Campbell Barton<br>
- import faces and verts to lists instead of a mesh, convert to a mesh later
- use new index mapping feature of mesh to re-map faces that were not added.

0.97 by Campbell Barton<br>
- Strip material names of spaces
- Added import as instance to import the 3ds into its own
  scene and add a group instance to the current scene
- New option to scale down imported objects so they are within a limited bounding area.

0.96 by Campbell Barton<br>
- Added workaround for bug in setting UV's for Zero vert index UV faces.
- Removed unique name function, let blender make the names unique.

0.95 by Campbell Barton<br>
- Removed workarounds for Blender 2.41
- Mesh objects split by material- many 3ds objects used more then 16 per mesh.
- Removed a lot of unneeded variable creation.

0.94 by Campbell Barton<br> 
- Face import tested to be about overall 16x speedup over 0.93.
- Material importing speedup.
- Tested with more models.
- Support some corrupt models.

0.93 by Campbell Barton<br> 
- Tested with 400 3ds files from turbosquid and samples.
- Tactfully ignore faces that used the same verts twice.
- Rollback to 0.83 sloppy un-reorganized code, this broke UV coord loading.
- Converted from NMesh to Mesh.
- Faster and cleaner new names.
- Use external comprehensive image loader.
- Re intergrated 0.92 and 0.9 changes
- Fixes for 2.41 compat.
- Non textured faces do not use a texture flag.

0.92<br>
- Added support for diffuse, alpha, spec, bump maps in a single material

0.9<br>
- Reorganized code into object/material block functions<br>
- Use of Matrix() to copy matrix data<br>
- added support for material transparency<br>

0.83 2005-08-07: Campell Barton
-  Aggressive image finding and case insensitivy for posisx systems.

0.82a 2005-07-22
- image texture loading (both for face uv and renderer)

0.82 - image texture loading (for face uv)

0.81a (fork- not 0.9) Campbell Barton 2005-06-08
- Simplified import code
- Never overwrite data
- Faster list handling
- Leaves import selected

0.81 Damien McGinnes 2005-01-09
- handle missing images better
    
0.8 Damien McGinnes 2005-01-08
- copies sticky UV coords to face ones
- handles images better
- Recommend that you run 'RemoveDoubles' on each imported mesh after using this script

'''

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
import bpy
from Blender import Mesh, Object, Material, Image, Texture, Lamp, Mathutils
from Blender.Mathutils import Vector
import BPyImage

import BPyMessages

import struct
from struct import calcsize, unpack

import os

# If python version is less than 2.4, try to get set stuff from module
try:
	set
except:
	from sets import Set as set

BOUNDS_3DS= []


#this script imports uvcoords as sticky vertex coords
#this parameter enables copying these to face uv coords
#which shold be more useful.

def createBlenderTexture(material, name, image):
	texture= bpy.data.textures.new(name)
	texture.setType('Image')
	texture.image= image
	material.setTexture(0, texture, Texture.TexCo.UV, Texture.MapTo.COL)



######################################################
# Data Structures
######################################################

#Some of the chunks that we will see
#----- Primary Chunk, at the beginning of each file
PRIMARY= long('0x4D4D',16)

#------ Main Chunks
OBJECTINFO   =      long('0x3D3D',16);      #This gives the version of the mesh and is found right before the material and object information
VERSION      =      long('0x0002',16);      #This gives the version of the .3ds file
EDITKEYFRAME=      long('0xB000',16);      #This is the header for all of the key frame info

#------ sub defines of OBJECTINFO
MATERIAL=45055		#0xAFFF				// This stored the texture info
OBJECT=16384		#0x4000				// This stores the faces, vertices, etc...

#>------ sub defines of MATERIAL
#------ sub defines of MATERIAL_BLOCK
MAT_NAME		=	long('0xA000',16)	# This holds the material name
MAT_AMBIENT		=	long('0xA010',16)	# Ambient color of the object/material
MAT_DIFFUSE		=	long('0xA020',16)	# This holds the color of the object/material
MAT_SPECULAR	=	long('0xA030',16)	# SPecular color of the object/material
MAT_SHINESS		=	long('0xA040',16)	# ??
MAT_TRANSPARENCY=	long('0xA050',16)	# Transparency value of material
MAT_SELF_ILLUM	=	long('0xA080',16)	# Self Illumination value of material
MAT_WIRE		=	long('0xA085',16)	# Only render's wireframe

MAT_TEXTURE_MAP	=	long('0xA200',16)	# This is a header for a new texture map
MAT_SPECULAR_MAP=	long('0xA204',16)	# This is a header for a new specular map
MAT_OPACITY_MAP	=	long('0xA210',16)	# This is a header for a new opacity map
MAT_REFLECTION_MAP=	long('0xA220',16)	# This is a header for a new reflection map
MAT_BUMP_MAP	=	long('0xA230',16)	# This is a header for a new bump map
MAT_MAP_FILENAME =      long('0xA300',16)      # This holds the file name of the texture

MAT_FLOAT_COLOR = long ('0x0010', 16) #color defined as 3 floats
MAT_24BIT_COLOR	= long ('0x0011', 16) #color defined as 3 bytes

#>------ sub defines of OBJECT
OBJECT_MESH  =      long('0x4100',16);      # This lets us know that we are reading a new object
OBJECT_LAMP =      long('0x4600',16);      # This lets un know we are reading a light object
OBJECT_LAMP_SPOT = long('0x4610',16);		# The light is a spotloght.
OBJECT_LAMP_OFF = long('0x4620',16);		# The light off.
OBJECT_LAMP_ATTENUATE = long('0x4625',16);	
OBJECT_LAMP_RAYSHADE = long('0x4627',16);	
OBJECT_LAMP_SHADOWED = long('0x4630',16);	
OBJECT_LAMP_LOCAL_SHADOW = long('0x4640',16);	
OBJECT_LAMP_LOCAL_SHADOW2 = long('0x4641',16);	
OBJECT_LAMP_SEE_CONE = long('0x4650',16);	
OBJECT_LAMP_SPOT_RECTANGULAR= long('0x4651',16);
OBJECT_LAMP_SPOT_OVERSHOOT= long('0x4652',16);
OBJECT_LAMP_SPOT_PROJECTOR= long('0x4653',16);
OBJECT_LAMP_EXCLUDE= long('0x4654',16);
OBJECT_LAMP_RANGE= long('0x4655',16);
OBJECT_LAMP_ROLL= long('0x4656',16);
OBJECT_LAMP_SPOT_ASPECT= long('0x4657',16);
OBJECT_LAMP_RAY_BIAS= long('0x4658',16);
OBJECT_LAMP_INNER_RANGE= long('0x4659',16);
OBJECT_LAMP_OUTER_RANGE= long('0x465A',16);
OBJECT_LAMP_MULTIPLIER = long('0x465B',16);
OBJECT_LAMP_AMBIENT_LIGHT = long('0x4680',16);



OBJECT_CAMERA=      long('0x4700',16);      # This lets un know we are reading a camera object

#>------ sub defines of CAMERA
OBJECT_CAM_RANGES=   long('0x4720',16);      # The camera range values

#>------ sub defines of OBJECT_MESH
OBJECT_VERTICES =   long('0x4110',16);      # The objects vertices
OBJECT_FACES    =   long('0x4120',16);      # The objects faces
OBJECT_MATERIAL =   long('0x4130',16);      # This is found if the object has a material, either texture map or color
OBJECT_UV       =   long('0x4140',16);      # The UV texture coordinates
OBJECT_TRANS_MATRIX  =   long('0x4160',16); # The Object Matrix

global scn
scn= None

#the chunk class
class chunk:
	ID=0
	length=0
	bytes_read=0

	#we don't read in the bytes_read, we compute that
	binary_format='<HI'

	def __init__(self):
		self.ID=0
		self.length=0
		self.bytes_read=0

	def dump(self):
		print 'ID: ', self.ID
		print 'ID in hex: ', hex(self.ID)
		print 'length: ', self.length
		print 'bytes_read: ', self.bytes_read

def read_chunk(file, chunk):
	temp_data=file.read(calcsize(chunk.binary_format))
	data=unpack(chunk.binary_format, temp_data)
	chunk.ID=data[0]
	chunk.length=data[1]
	#update the bytes read function
	chunk.bytes_read=6

	#if debugging
	#chunk.dump()

def read_string(file):
	#read in the characters till we get a null character
	s=''
	while not s.endswith('\x00'):
		s+=unpack( '<c', file.read(1) )[0]
		#print 'string: ',s
	
	#remove the null character from the string
	return s[:-1]

######################################################
# IMPORT
######################################################
def process_next_object_chunk(file, previous_chunk):
	new_chunk=chunk()
	temp_chunk=chunk()

	while (previous_chunk.bytes_read<previous_chunk.length):
		#read the next chunk
		read_chunk(file, new_chunk)

def skip_to_end(file, skip_chunk):
	buffer_size=skip_chunk.length-skip_chunk.bytes_read
	binary_format='%ic' % buffer_size
	temp_data=file.read(calcsize(binary_format))
	skip_chunk.bytes_read+=buffer_size


def add_texture_to_material(image, texture, material, mapto):
	if mapto=='DIFFUSE':
		map=Texture.MapTo.COL
	elif mapto=='SPECULAR':
		map=Texture.MapTo.SPEC
	elif mapto=='OPACITY':
		map=Texture.MapTo.ALPHA
	elif mapto=='BUMP':
		map=Texture.MapTo.NOR
	else:
		print '/tError:  Cannot map to "%s"\n\tassuming diffuse color. modify material "%s" later.' % (mapto, material.name)
		map=Texture.MapTo.COL

	if image: texture.setImage(image) # double check its an image.
	free_tex_slots= [i for i, tex in enumerate( material.getTextures() ) if tex==None]
	if not free_tex_slots:
		print '/tError: Cannot add "%s" map. 10 Texture slots alredy used.' % mapto
	else:
		material.setTexture(free_tex_slots[0],texture,Texture.TexCo.UV,map)


def process_next_chunk(file, previous_chunk, importedObjects, IMAGE_SEARCH):
	#print previous_chunk.bytes_read, 'BYTES READ'
	contextObName= None
	contextLamp= [None, None] # object, Data
	contextMaterial= None
	contextMatrix_rot= None # Blender.Mathutils.Matrix(); contextMatrix.identity()
	#contextMatrix_tx= None # Blender.Mathutils.Matrix(); contextMatrix.identity()
	contextMesh_vertls= None
	contextMesh_facels= None
	contextMeshMaterials= {} # matname:[face_idxs]
	contextMeshUV= None
	
	TEXTURE_DICT={}
	MATDICT={}
	TEXMODE= Mesh.FaceModes['TEX']
	
	# Localspace variable names, faster.
	STRUCT_SIZE_1CHAR= calcsize('c')
	STRUCT_SIZE_2FLOAT= calcsize('2f')
	STRUCT_SIZE_3FLOAT= calcsize('3f')
	STRUCT_SIZE_UNSIGNED_SHORT= calcsize('H')
	STRUCT_SIZE_4UNSIGNED_SHORT= calcsize('4H')
	STRUCT_SIZE_4x3MAT= calcsize('ffffffffffff')
	_STRUCT_SIZE_4x3MAT= calcsize('fffffffffffff')
	# STRUCT_SIZE_4x3MAT= calcsize('ffffffffffff')
	# print STRUCT_SIZE_4x3MAT, ' STRUCT_SIZE_4x3MAT'
	
	def putContextMesh(myContextMesh_vertls, myContextMesh_facels, myContextMeshMaterials):
		
		materialFaces= set() # faces that have a material. Can optimize?
		
		# Now make copies with assigned materils.
		
		def makeMeshMaterialCopy(matName, faces):			
			'''
			Make a new mesh with only face the faces that use this material.
			faces can be any iterable object - containing ints.
			'''
			
			faceVertUsers = [False] * len(myContextMesh_vertls)
			ok=0
			for fIdx in faces:
				for vindex in myContextMesh_facels[fIdx]:
					faceVertUsers[vindex] = True
					if matName != None: # if matName is none then this is a set(), meaning we are using the untextured faces and do not need to store textured faces.
						materialFaces.add(fIdx)
					ok=1
			
			if not ok:
				return
					
			myVertMapping = {}
			vertMappingIndex = 0
			
			vertsToUse = [i for i in xrange(len(myContextMesh_vertls)) if faceVertUsers[i]]
			myVertMapping = dict( [ (ii, i) for i, ii in enumerate(vertsToUse) ] )
			
			tempName= '%s_%s' % (contextObName, matName) # matName may be None.
			bmesh = bpy.data.meshes.new(tempName)
			
			if matName == None:
				img= None
			else:
				bmat = MATDICT[matName][1]
				bmesh.materials= [bmat]
				try:	img= TEXTURE_DICT[bmat.name]
				except:	img= None
				
			bmesh_verts = bmesh.verts
			bmesh_verts.extend( [Vector()] )
			bmesh_verts.extend( [myContextMesh_vertls[i] for i in vertsToUse] )
			# +1 because of DUMMYVERT
			face_mapping= bmesh.faces.extend( [ [ bmesh_verts[ myVertMapping[vindex]+1] for vindex in myContextMesh_facels[fIdx]] for fIdx in faces ], indexList=True )
			
			if bmesh.faces and (contextMeshUV or img):
				bmesh.faceUV= 1
				for ii, i in enumerate(faces):
					
					# Mapped index- faces may have not been added- if so, then map to the correct index
					# BUGGY API - face_mapping is not always the right length
					map_index= face_mapping[ii]
					
					if map_index != None:
						targetFace= bmesh.faces[map_index]
						if contextMeshUV:
							# v.index-1 because of the DUMMYVERT
							targetFace.uv= [contextMeshUV[vindex] for vindex in myContextMesh_facels[i]]
						if img:
							targetFace.image= img
			
			# bmesh.transform(contextMatrix)
			ob = SCN_OBJECTS.new(bmesh, tempName)
			'''
			if contextMatrix_tx:
				ob.setMatrix(contextMatrix_tx)
			'''
			
			if contextMatrix_rot:
				ob.setMatrix(contextMatrix_rot)
			
			importedObjects.append(ob)
			bmesh.calcNormals()
		
		for matName, faces in myContextMeshMaterials.iteritems():
			makeMeshMaterialCopy(matName, faces)
			
		if len(materialFaces)!=len(myContextMesh_facels):
			# Invert material faces.
			makeMeshMaterialCopy(None, set(range(len( myContextMesh_facels ))) - materialFaces)
			#raise 'Some UnMaterialed faces', len(contextMesh.faces)
	
	#a spare chunk
	new_chunk= chunk()
	temp_chunk= chunk()
	
	CreateBlenderObject = False

	#loop through all the data for this chunk (previous chunk) and see what it is
	while (previous_chunk.bytes_read<previous_chunk.length):
		#print '\t', previous_chunk.bytes_read, 'keep going'
		#read the next chunk
		#print 'reading a chunk'
		read_chunk(file, new_chunk)

		#is it a Version chunk?
		if (new_chunk.ID==VERSION):
			#print 'if (new_chunk.ID==VERSION):'
			#print 'found a VERSION chunk'
			#read in the version of the file
			#it's an unsigned short (H)
			temp_data= file.read(calcsize('I'))
			version = unpack('<I', temp_data)[0]
			new_chunk.bytes_read+= 4 #read the 4 bytes for the version number
			#this loader works with version 3 and below, but may not with 4 and above
			if (version>3):
				print '\tNon-Fatal Error:  Version greater than 3, may not load correctly: ', version

		#is it an object info chunk?
		elif (new_chunk.ID==OBJECTINFO):
			#print 'elif (new_chunk.ID==OBJECTINFO):'
			# print 'found an OBJECTINFO chunk'
			process_next_chunk(file, new_chunk, importedObjects, IMAGE_SEARCH)
			
			#keep track of how much we read in the main chunk
			new_chunk.bytes_read+=temp_chunk.bytes_read

		#is it an object chunk?
		elif (new_chunk.ID==OBJECT):
			
			if CreateBlenderObject:
				putContextMesh(contextMesh_vertls, contextMesh_facels, contextMeshMaterials)
				contextMesh_vertls= []; contextMesh_facels= []
			
				## preparando para receber o proximo objeto
				contextMeshMaterials= {} # matname:[face_idxs]
				contextMeshUV= None
				#contextMesh.vertexUV= 1 # Make sticky coords.
				# Reset matrix
				contextMatrix_rot= None
				#contextMatrix_tx= None
				
			CreateBlenderObject= True
			tempName= read_string(file)
			contextObName= tempName
			new_chunk.bytes_read += len(tempName)+1
		
		#is it a material chunk?
		elif (new_chunk.ID==MATERIAL):
			#print 'elif (new_chunk.ID==MATERIAL):'
			contextMaterial= bpy.data.materials.new('Material')
		
		elif (new_chunk.ID==MAT_NAME):
			#print 'elif (new_chunk.ID==MAT_NAME):'
			material_name= read_string(file)
			
			#plus one for the null character that ended the string
			new_chunk.bytes_read+= len(material_name)+1
			
			contextMaterial.name= material_name.rstrip() # remove trailing  whitespace
			MATDICT[material_name]= (contextMaterial.name, contextMaterial)
		
		elif (new_chunk.ID==MAT_AMBIENT):
			#print 'elif (new_chunk.ID==MAT_AMBIENT):'
			read_chunk(file, temp_chunk)
			if (temp_chunk.ID==MAT_FLOAT_COLOR):
				temp_data=file.read(calcsize('3f'))
				temp_chunk.bytes_read+=12
				contextMaterial.mirCol=[float(col) for col in unpack('<3f', temp_data)]
			elif (temp_chunk.ID==MAT_24BIT_COLOR):
				temp_data=file.read(calcsize('3B'))
				temp_chunk.bytes_read+= 3
				contextMaterial.mirCol= [float(col)/255 for col in unpack('<3B', temp_data)] # data [0,1,2] == rgb
			else:
				skip_to_end(file, temp_chunk)
			new_chunk.bytes_read+= temp_chunk.bytes_read

		elif (new_chunk.ID==MAT_DIFFUSE):
			#print 'elif (new_chunk.ID==MAT_DIFFUSE):'
			read_chunk(file, temp_chunk)
			if (temp_chunk.ID==MAT_FLOAT_COLOR):
				temp_data=file.read(calcsize('3f'))
				temp_chunk.bytes_read+=12
				contextMaterial.rgbCol=[float(col) for col in unpack('<3f', temp_data)]
			elif (temp_chunk.ID==MAT_24BIT_COLOR):
				temp_data=file.read(calcsize('3B'))
				temp_chunk.bytes_read+= 3
				contextMaterial.rgbCol= [float(col)/255 for col in unpack('<3B', temp_data)] # data [0,1,2] == rgb
			else:
				skip_to_end(file, temp_chunk)
			new_chunk.bytes_read+= temp_chunk.bytes_read

		elif (new_chunk.ID==MAT_SPECULAR):
			#print 'elif (new_chunk.ID==MAT_SPECULAR):'
			read_chunk(file, temp_chunk)
			if (temp_chunk.ID==MAT_FLOAT_COLOR):
				temp_data=file.read(calcsize('3f'))
				temp_chunk.bytes_read+=12
				contextMaterial.mirCol=[float(col) for col in unpack('<3f', temp_data)]
			elif (temp_chunk.ID==MAT_24BIT_COLOR):
				temp_data=file.read(calcsize('3B'))
				temp_chunk.bytes_read+= 3
				contextMaterial.mirCol= [float(col)/255 for col in unpack('<3B', temp_data)] # data [0,1,2] == rgb
			else:
				skip_to_end(file, temp_chunk)
			new_chunk.bytes_read+= temp_chunk.bytes_read
			
		elif (new_chunk.ID==MAT_TEXTURE_MAP):
			#print 'elif (new_chunk.ID==MAT_TEXTURE_MAP):'
			new_texture= bpy.data.textures.new('Diffuse')
			new_texture.setType('Image')
			img = None
			while (new_chunk.bytes_read<new_chunk.length):
				#print 'MAT_TEXTURE_MAP..while', new_chunk.bytes_read, new_chunk.length
				read_chunk(file, temp_chunk)
				
				if (temp_chunk.ID==MAT_MAP_FILENAME):
					texture_name=read_string(file)
					#img= TEXTURE_DICT[contextMaterial.name]= BPyImage.comprehensiveImageLoad(texture_name, FILENAME)
					img= TEXTURE_DICT[contextMaterial.name]= BPyImage.comprehensiveImageLoad(texture_name, FILENAME, PLACE_HOLDER= False, RECURSIVE= IMAGE_SEARCH)
					new_chunk.bytes_read += (len(texture_name)+1) #plus one for the null character that gets removed
					
				else:
					skip_to_end(file, temp_chunk)
				
				new_chunk.bytes_read+= temp_chunk.bytes_read
			
			#add the map to the material in the right channel
			if img:
				add_texture_to_material(img, new_texture, contextMaterial, 'DIFFUSE')
			
		elif (new_chunk.ID==MAT_SPECULAR_MAP):
			#print 'elif (new_chunk.ID==MAT_SPECULAR_MAP):'
			new_texture= bpy.data.textures.new('Specular')
			new_texture.setType('Image')
			img = None
			while (new_chunk.bytes_read<new_chunk.length):
				read_chunk(file, temp_chunk)
				
				if (temp_chunk.ID==MAT_MAP_FILENAME):
					texture_name= read_string(file)
					#img= BPyImage.comprehensiveImageLoad(texture_name, FILENAME)
					img= BPyImage.comprehensiveImageLoad(texture_name, FILENAME, PLACE_HOLDER= False, RECURSIVE= IMAGE_SEARCH)
					new_chunk.bytes_read+= (len(texture_name)+1) #plus one for the null character that gets removed
				else:
					skip_to_end(file, temp_chunk)
				
				new_chunk.bytes_read+= temp_chunk.bytes_read
				
			#add the map to the material in the right channel
			if img:
				add_texture_to_material(img, new_texture, contextMaterial, 'SPECULAR')
	
		elif (new_chunk.ID==MAT_OPACITY_MAP):
			#print 'new_texture=Blender.Texture.New('Opacity')'
			new_texture= bpy.data.textures.new('Opacity')
			new_texture.setType('Image')
			img = None
			while (new_chunk.bytes_read<new_chunk.length):
				read_chunk(file, temp_chunk)
				
				if (temp_chunk.ID==MAT_MAP_FILENAME):
					texture_name= read_string(file)
					#img= BPyImage.comprehensiveImageLoad(texture_name, FILENAME)
					img= BPyImage.comprehensiveImageLoad(texture_name, FILENAME, PLACE_HOLDER= False, RECURSIVE= IMAGE_SEARCH)
					new_chunk.bytes_read += (len(texture_name)+1) #plus one for the null character that gets removed
				else:
					skip_to_end(file, temp_chunk)
				
				new_chunk.bytes_read+= temp_chunk.bytes_read
			#add the map to the material in the right channel
			if img:
				add_texture_to_material(img, new_texture, contextMaterial, 'OPACITY')

		elif (new_chunk.ID==MAT_BUMP_MAP):
			#print 'elif (new_chunk.ID==MAT_BUMP_MAP):'
			new_texture= bpy.data.textures.new('Bump')
			new_texture.setType('Image')
			img = None
			while (new_chunk.bytes_read<new_chunk.length):
				read_chunk(file, temp_chunk)
				
				if (temp_chunk.ID==MAT_MAP_FILENAME):
					texture_name= read_string(file)
					#img= BPyImage.comprehensiveImageLoad(texture_name, FILENAME)
					img= BPyImage.comprehensiveImageLoad(texture_name, FILENAME, PLACE_HOLDER= False, RECURSIVE= IMAGE_SEARCH)
					new_chunk.bytes_read += (len(texture_name)+1) #plus one for the null character that gets removed
				else:
					skip_to_end(file, temp_chunk)
				
				new_chunk.bytes_read+=temp_chunk.bytes_read
				
			#add the map to the material in the right channel
			if img:
				add_texture_to_material(img, new_texture, contextMaterial, 'BUMP')
			
		elif (new_chunk.ID==MAT_TRANSPARENCY):
			#print 'elif (new_chunk.ID==MAT_TRANSPARENCY):'
			read_chunk(file, temp_chunk)
			temp_data=file.read(STRUCT_SIZE_UNSIGNED_SHORT)
			
			temp_chunk.bytes_read+=2
			contextMaterial.alpha= 1-(float(unpack('<H', temp_data)[0])/100)
			new_chunk.bytes_read+=temp_chunk.bytes_read


		elif (new_chunk.ID==OBJECT_LAMP): # Basic lamp support.
			
			temp_data=file.read(STRUCT_SIZE_3FLOAT)
			
			x,y,z=unpack('<3f', temp_data)
			new_chunk.bytes_read+=STRUCT_SIZE_3FLOAT
			
			contextLamp[1]= bpy.data.lamps.new()
			contextLamp[0]= SCN_OBJECTS.new(contextLamp[1])
			importedObjects.append(contextLamp[0])
			
			#print 'number of faces: ', num_faces
			#print x,y,z
			contextLamp[0].setLocation(x,y,z)
			
			# Reset matrix
			contextMatrix_rot= None
			#contextMatrix_tx= None
			#print contextLamp.name, 
			
		elif (new_chunk.ID==OBJECT_MESH):
			# print 'Found an OBJECT_MESH chunk'
			pass
		elif (new_chunk.ID==OBJECT_VERTICES):
			'''
			Worldspace vertex locations
			'''
			# print 'elif (new_chunk.ID==OBJECT_VERTICES):'
			temp_data=file.read(STRUCT_SIZE_UNSIGNED_SHORT)
			num_verts=unpack('<H', temp_data)[0]
			new_chunk.bytes_read+=2
			
			# print 'number of verts: ', num_verts
			def getvert():
				temp_data= unpack('<3f', file.read(STRUCT_SIZE_3FLOAT))
				new_chunk.bytes_read += STRUCT_SIZE_3FLOAT #12: 3 floats x 4 bytes each
				return temp_data
			
			#contextMesh.verts.extend( [Vector(),] ) # DUMMYVERT! - remove when blenders internals are fixed.
			contextMesh_vertls= [getvert() for i in xrange(num_verts)]
			
			#print 'object verts: bytes read: ', new_chunk.bytes_read

		elif (new_chunk.ID==OBJECT_FACES):
			# print 'elif (new_chunk.ID==OBJECT_FACES):'
			temp_data= file.read(STRUCT_SIZE_UNSIGNED_SHORT)
			num_faces= unpack('<H', temp_data)[0]
			new_chunk.bytes_read+= 2
			#print 'number of faces: ', num_faces
			
			def getface():
				# print '\ngetting a face'
				temp_data= file.read(STRUCT_SIZE_4UNSIGNED_SHORT)
				new_chunk.bytes_read+= STRUCT_SIZE_4UNSIGNED_SHORT #4 short ints x 2 bytes each
				v1,v2,v3,dummy= unpack('<4H', temp_data)
				return v1, v2, v3
			
			contextMesh_facels= [ getface() for i in xrange(num_faces) ]


		elif (new_chunk.ID==OBJECT_MATERIAL):
			# print 'elif (new_chunk.ID==OBJECT_MATERIAL):'
			material_name= read_string(file)
			new_chunk.bytes_read += len(material_name)+1 # remove 1 null character.
			
			temp_data=file.read(STRUCT_SIZE_UNSIGNED_SHORT)
			num_faces_using_mat = unpack('<H', temp_data)[0]
			new_chunk.bytes_read += STRUCT_SIZE_UNSIGNED_SHORT
			
			def getmat():
				temp_data= file.read(STRUCT_SIZE_UNSIGNED_SHORT)
				new_chunk.bytes_read+= STRUCT_SIZE_UNSIGNED_SHORT
				return unpack('<H', temp_data)[0]
			
			contextMeshMaterials[material_name]= [ getmat() for i in xrange(num_faces_using_mat) ]
			
			#look up the material in all the materials

		elif (new_chunk.ID==OBJECT_UV):
			temp_data=file.read(STRUCT_SIZE_UNSIGNED_SHORT)
			num_uv=unpack('<H', temp_data)[0]
			new_chunk.bytes_read+= 2
			
			def getuv():
				temp_data=file.read(STRUCT_SIZE_2FLOAT)
				new_chunk.bytes_read += STRUCT_SIZE_2FLOAT #2 float x 4 bytes each
				return Vector( unpack('<2f', temp_data) )
				
			contextMeshUV= [ getuv() for i in xrange(num_uv) ]
		
		elif (new_chunk.ID== OBJECT_TRANS_MATRIX):
			# How do we know the matrix size? 54 == 4x4 48 == 4x3
			temp_data=file.read(STRUCT_SIZE_4x3MAT)
			data= list( unpack('<ffffffffffff', temp_data)  )
			new_chunk.bytes_read += STRUCT_SIZE_4x3MAT
			
			contextMatrix_rot= Blender.Mathutils.Matrix(\
			 data[:3] + [0],\
			 data[3:6] + [0],\
			 data[6:9] + [0],\
			 data[9:] + [1])
			
			
			'''
			contextMatrix_rot= Blender.Mathutils.Matrix(\
			 data[:3] + [0],\
			 data[3:6] + [0],\
			 data[6:9] + [0],\
			 [0,0,0,1])
			'''
			
			'''
			contextMatrix_rot= Blender.Mathutils.Matrix(\
			 data[:3] ,\
			 data[3:6],\
			 data[6:9])
			'''
			
			'''
			contextMatrix_rot = Blender.Mathutils.Matrix()
			m = 0
			for j in xrange(4):
				for i in xrange(3):
					contextMatrix_rot[j][i] = data[m]
					m+=1
			
			contextMatrix_rot[0][3]=0;
			contextMatrix_rot[1][3]=0;
			contextMatrix_rot[2][3]=0;
			contextMatrix_rot[3][3]=1;
			'''
			
			#contextMatrix_rot.resize4x4()
			#print "MTX"
			#print contextMatrix_rot
			contextMatrix_rot.invert()
			#print contextMatrix_rot
			#contextMatrix_tx = Blender.Mathutils.TranslationMatrix(0.5 * Blender.Mathutils.Vector(data[9:]))
			#contextMatrix_tx.invert()
			
			#tx.invert()
			
			#contextMatrix = contextMatrix * tx
			#contextMatrix = contextMatrix  *tx
			
		elif  (new_chunk.ID==MAT_MAP_FILENAME):
			texture_name=read_string(file)
			try:
				TEXTURE_DICT[contextMaterial.name]
			except:
				#img= TEXTURE_DICT[contextMaterial.name]= BPyImage.comprehensiveImageLoad(texture_name, FILENAME)
				img= TEXTURE_DICT[contextMaterial.name]= BPyImage.comprehensiveImageLoad(texture_name, FILENAME, PLACE_HOLDER= False, RECURSIVE= IMAGE_SEARCH)
			
			new_chunk.bytes_read+= len(texture_name)+1 #plus one for the null character that gets removed
		
		else: #(new_chunk.ID!=VERSION or new_chunk.ID!=OBJECTINFO or new_chunk.ID!=OBJECT or new_chunk.ID!=MATERIAL):
			# print 'skipping to end of this chunk'
			buffer_size=new_chunk.length-new_chunk.bytes_read
			binary_format='%ic' % buffer_size
			temp_data=file.read(calcsize(binary_format))
			new_chunk.bytes_read+=buffer_size


		#update the previous chunk bytes read
		# print 'previous_chunk.bytes_read += new_chunk.bytes_read'
		# print previous_chunk.bytes_read, new_chunk.bytes_read
		previous_chunk.bytes_read += new_chunk.bytes_read
		## print 'Bytes left in this chunk: ', previous_chunk.length-previous_chunk.bytes_read
	
	# FINISHED LOOP
	# There will be a number of objects still not added
	if contextMesh_facels != None:
		putContextMesh(contextMesh_vertls, contextMesh_facels, contextMeshMaterials)

def load_3ds(filename, PREF_UI= True):
	global FILENAME, SCN_OBJECTS
	
	if BPyMessages.Error_NoFile(filename):
		return
	
	print '\n\nImporting 3DS: "%s"' % (Blender.sys.expandpath(filename))
	
	time1= Blender.sys.time()
	
	FILENAME=filename
	current_chunk=chunk()
	
	file=open(filename,'rb')
	
	#here we go!
	# print 'reading the first chunk'
	read_chunk(file, current_chunk)
	if (current_chunk.ID!=PRIMARY):
		print '\tFatal Error:  Not a valid 3ds file: ', filename
		file.close()
		return
	
	
	# IMPORT_AS_INSTANCE= Blender.Draw.Create(0)
	IMPORT_CONSTRAIN_BOUNDS= Blender.Draw.Create(10.0)
	IMAGE_SEARCH= Blender.Draw.Create(1)
	
	# Get USER Options
	pup_block= [\
	('Size Constraint:', IMPORT_CONSTRAIN_BOUNDS, 0.0, 1000.0, 'Scale the model by 10 until it reacehs the size constraint. Zero Disables.'),\
	('Image Search', IMAGE_SEARCH, 'Search subdirs for any assosiated images (Warning, may be slow)'),\
	#('Group Instance', IMPORT_AS_INSTANCE, 'Import objects into a new scene and group, creating an instance in the current scene.'),\
	]
	
	if PREF_UI:
		if not Blender.Draw.PupBlock('Import 3DS...', pup_block):
			return
	
	Blender.Window.WaitCursor(1)
	
	IMPORT_CONSTRAIN_BOUNDS= IMPORT_CONSTRAIN_BOUNDS.val
	# IMPORT_AS_INSTANCE= IMPORT_AS_INSTANCE.val
	IMAGE_SEARCH = IMAGE_SEARCH.val
	
	if IMPORT_CONSTRAIN_BOUNDS:
		BOUNDS_3DS[:]= [1<<30, 1<<30, 1<<30, -1<<30, -1<<30, -1<<30]
	else:
		BOUNDS_3DS[:]= []
	
	##IMAGE_SEARCH
	
	scn= bpy.data.scenes.active
	SCN_OBJECTS = scn.objects
	SCN_OBJECTS.selected = [] # de select all
	
	importedObjects= [] # Fill this list with objects
	process_next_chunk(file, current_chunk, importedObjects, IMAGE_SEARCH)
	
	
	# Link the objects into this scene.
	# Layers= scn.Layers
	
	# REMOVE DUMMYVERT, - remove this in the next release when blenders internal are fixed.
	
	
	for ob in importedObjects:
		if ob.type=='Mesh':
			me= ob.getData(mesh=1)
			me.verts.delete([me.verts[0],])
	
	# Done DUMMYVERT
	"""
	if IMPORT_AS_INSTANCE:
		name= filename.split('\\')[-1].split('/')[-1]
		# Create a group for this import.
		group_scn= Scene.New(name)
		for ob in importedObjects:
			group_scn.link(ob) # dont worry about the layers
		
		grp= Blender.Group.New(name)
		grp.objects= importedObjects
		
		grp_ob= Object.New('Empty', name)
		grp_ob.enableDupGroup= True
		grp_ob.DupGroup= grp
		scn.link(grp_ob)
		grp_ob.Layers= Layers
		grp_ob.sel= 1
	else:
		# Select all imported objects.
		for ob in importedObjects:
			scn.link(ob)
			ob.Layers= Layers
			ob.sel= 1
	"""
	
	if IMPORT_CONSTRAIN_BOUNDS!=0.0:
		# Set bounds from objecyt bounding box
		for ob in importedObjects:
			if ob.type=='Mesh':
				ob.makeDisplayList() # Why dosnt this update the bounds?
				for v in ob.getBoundBox():
					for i in (0,1,2):
						if v[i] < BOUNDS_3DS[i]:
							BOUNDS_3DS[i]= v[i] # min
						
						if v[i] > BOUNDS_3DS[i+3]:
							BOUNDS_3DS[i+3]= v[i] # min
		
		# Get the max axis x/y/z
		max_axis= max(BOUNDS_3DS[3]-BOUNDS_3DS[0], BOUNDS_3DS[4]-BOUNDS_3DS[1], BOUNDS_3DS[5]-BOUNDS_3DS[2])
		# print max_axis
		if max_axis < 1<<30: # Should never be false but just make sure.
			
			# Get a new scale factor if set as an option
			SCALE=1.0
			while (max_axis*SCALE) > IMPORT_CONSTRAIN_BOUNDS:
				SCALE/=10
			
			# SCALE Matrix
			SCALE_MAT= Blender.Mathutils.Matrix([SCALE,0,0,0],[0,SCALE,0,0],[0,0,SCALE,0],[0,0,0,1])
			
			for ob in importedObjects:
				ob.setMatrix(ob.matrixWorld*SCALE_MAT)
				
		# Done constraining to bounds.
	
	# Select all new objects.
	print 'finished importing: "%s" in %.4f sec.' % (filename, (Blender.sys.time()-time1))
	file.close()
	Blender.Window.WaitCursor(0)
	

DEBUG= False
if __name__=='__main__' and not DEBUG:
	Blender.Window.FileSelector(load_3ds, 'Import 3DS', '*.3ds')

# For testing compatibility
#load_3ds('/metavr/convert/vehicle/truck_002/TruckTanker1.3DS', False)
#load_3ds('/metavr/archive/convert/old/arranged_3ds_to_hpx-2/only-need-engine-trains/Engine2.3DS', False)
'''

else:
	# DEBUG ONLY
	TIME= Blender.sys.time()
	import os
	print 'Searching for files'
	os.system('find /metavr/ -iname "*.3ds" > /tmp/temp3ds_list')
	# os.system('find /storage/ -iname "*.3ds" > /tmp/temp3ds_list')
	print '...Done'
	file= open('/tmp/temp3ds_list', 'r')
	lines= file.readlines()
	file.close()
	# sort by filesize for faster testing
	lines_size = [(os.path.getsize(f[:-1]), f[:-1]) for f in lines]
	lines_size.sort()
	lines = [f[1] for f in lines_size]
	

	def between(v,a,b):
		if v <= max(a,b) and v >= min(a,b):
			return True		
		return False
		
	for i, _3ds in enumerate(lines):
		if between(i, 650,800):
			#_3ds= _3ds[:-1]
			print 'Importing', _3ds, '\nNUMBER', i, 'of', len(lines)
			_3ds_file= _3ds.split('/')[-1].split('\\')[-1]
			newScn= Blender.Scene.New(_3ds_file)
			newScn.makeCurrent()
			load_3ds(_3ds, False)

	print 'TOTAL TIME: %.6f' % (Blender.sys.time() - TIME)

'''
