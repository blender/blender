#!BPY
""" 
Name: '3D Studio (.3ds)...'
Blender: 241
Group: 'Import'
Tooltip: 'Import from 3DS file format (.3ds)'
"""

__author__= ['Bob Holcomb', 'Richard L?rk?ng', 'Damien McGinnes', 'Campbell Barton']
__url__= ('blender', 'elysiun', 'http://www.gametutorials.com')
__version__= '0.95'
__bpydoc__= '''\

3ds Importer

This script imports a 3ds file and the materials into Blender for editing.

Loader is based on 3ds loader from www.gametutorials.com (Thanks DigiBen).

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

# Importing modules

import Blender
from Blender import Mesh, Scene, Object, Material, Image, Texture, Lamp, Mathutils
from Blender.Mathutils import Vector
import BPyImage
reload( BPyImage )

import struct
from struct import calcsize, unpack

import os

# If python version is less than 2.4, try to get set stuff from module
import sys
if ( (sys.version_info[0] <= 2) and (sys.version_info[1] < 4) ):
	from sets import Set as set

#this script imports uvcoords as sticky vertex coords
#this parameter enables copying these to face uv coords
#which shold be more useful.


#===========================================================================#
# Returns unique name of object/mesh (stops overwriting existing meshes)    #
#===========================================================================#
def getUniqueName(name):
	count= 0
	newname= name[:19]
	while newname in getUniqueName.uniqueObNames:
		newname= '%s.%.3i' % (name[:15], count)
		count+=1
	# Dont use again.
	getUniqueName.uniqueObNames.append(newname)
	return newname
getUniqueName.uniqueObNames= Blender.NMesh.GetNames() + [ob.name for ob in Object.Get()]

def createBlenderTexture(material, name, image):
	texture= Texture.New(name)
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
MAT_MAP_FILENAME =      long('0xA300',16);      # This holds the file name of the texture

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
		s+=unpack( 'c', file.read(1) )[0]
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
		raise '/tError:  Cannot map to ', mapto
		return
	

	texture.setImage(image)
	texture_list=material.getTextures()
	index=0
	for tex in texture_list:
		if tex==None:
			material.setTexture(index,texture,Texture.TexCo.UV,map)
			return
		else:
			index+=1
		if index>10:
			print '/tError: Cannot add diffuse map.  Too many textures'

def process_next_chunk(file, previous_chunk, scn):
	#print previous_chunk.bytes_read, 'BYTES READ'
	contextObName= None
	contextLamp= [None, None] # object, Data
	contextMaterial= None
	contextMatrix= Blender.Mathutils.Matrix(); contextMatrix.identity()
	contextMesh= None
	contextMeshMaterials= {} # matname:[face_idxs]
	
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
	
	
	def putContextMesh(myContextMesh, myContextMeshMaterials):
		
		#print 'prtting myContextMesh', myContextMesh.name
		INV_MAT= Blender.Mathutils.Matrix(contextMatrix)
		
		INV_MAT.invert()
		contextMesh.transform(INV_MAT)
		
		materialFaces= set()
		# Now make copies with assigned materils.
		
		def makeMeshMaterialCopy(matName, faces):			
			# Make a new mesh with only face the faces that use this material.
			faceVertUsers = [False] * len(myContextMesh.verts)
			ok=0
			for fIdx in faces:
				for v in myContextMesh.faces[fIdx].v:
					faceVertUsers[v.index] = True
					if matName != None: # if matName is none then this is a set(), meaning we are using the untextured faces and do not need to store textured faces.
						materialFaces.add(fIdx)
					ok=1
			
			if not ok:
				return
					
			myVertMapping = {}
			vertMappingIndex = 0
			
			vertsToUse = [i for i in xrange(len(myContextMesh.verts)) if faceVertUsers[i]]
			myVertMapping = dict( [ (ii, i) for i, ii in enumerate(vertsToUse) ] )
			
			bmesh = Mesh.New(contextMesh.name)
			
			if matName != None:
				bmat = MATDICT[matName][1]
				try:
					img= TEXTURE_DICT[bmat.name]
				except:
					img= None
				bmesh.materials= [bmat]
			else:
				img= None
				
			bmesh.verts.extend( [myContextMesh.verts[i].co for i in vertsToUse] )
			bmesh.faces.extend( [ [ bmesh.verts[ myVertMapping[v.index]] for v in myContextMesh.faces[fIdx].v] for fIdx in faces ] )
			
			if contextMeshUV or img:
				bmesh.faceUV= 1
				for ii, i in enumerate(faces):
					targetFace= bmesh.faces[ii]
					if contextMeshUV:
						targetFace.uv= [contextMeshUV[v.index] for v in myContextMesh.faces[i].v]
						
					if img:
						targetFace.image= img
			
			ob = Object.New('Mesh', contextMesh.name)
			ob.link(bmesh)
			ob.setMatrix(contextMatrix)
			ob.Layers= scn.Layers
			scn.link(ob)
			ob.sel= 1
		
		for matName, faces in myContextMeshMaterials.iteritems():
			makeMeshMaterialCopy(matName, faces)
			
		if len(materialFaces)!=len(contextMesh.faces):
			# Invert material faces.
			makeMeshMaterialCopy(None, set(range(len( contextMesh.faces ))) - materialFaces)
			#raise 'Some UnMaterialed faces', len(contextMesh.faces)
		
	
	#a spare chunk
	new_chunk= chunk()
	temp_chunk= chunk()

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
			version,= unpack('I', temp_data)
			new_chunk.bytes_read+= 4 #read the 4 bytes for the version number
			#this loader works with version 3 and below, but may not with 4 and above
			if (version>3):
				print '\tNon-Fatal Error:  Version greater than 3, may not load correctly: ', version

		#is it an object info chunk?
		elif (new_chunk.ID==OBJECTINFO):
			#print 'elif (new_chunk.ID==OBJECTINFO):'
			# print 'found an OBJECTINFO chunk'
			process_next_chunk(file, new_chunk, scn)
			
			#keep track of how much we read in the main chunk
			new_chunk.bytes_read+=temp_chunk.bytes_read

		#is it an object chunk?
		elif (new_chunk.ID==OBJECT):
			'elif (new_chunk.ID==OBJECT):'
			tempName= read_string(file)
			contextObName= getUniqueName( tempName )
			new_chunk.bytes_read += len(tempName)+1
		
		#is it a material chunk?
		elif (new_chunk.ID==MATERIAL):
			#print 'elif (new_chunk.ID==MATERIAL):'
			contextMaterial= Material.New()
		
		elif (new_chunk.ID==MAT_NAME):
			#print 'elif (new_chunk.ID==MAT_NAME):'
			material_name= read_string(file)
			
			#plus one for the null character that ended the string
			new_chunk.bytes_read+= len(material_name)+1
			
			contextMaterial.name= material_name
			MATDICT[material_name]= (contextMaterial.name, contextMaterial)
		
		elif (new_chunk.ID==MAT_AMBIENT):
			#print 'elif (new_chunk.ID==MAT_AMBIENT):'
			read_chunk(file, temp_chunk)
			temp_data=file.read(calcsize('3B'))
			temp_chunk.bytes_read+= 3
			contextMaterial.mirCol= [float(col)/255 for col in unpack('3B', temp_data)] # data [0,1,2] == rgb
			new_chunk.bytes_read+= temp_chunk.bytes_read

		elif (new_chunk.ID==MAT_DIFFUSE):
			#print 'elif (new_chunk.ID==MAT_DIFFUSE):'
			read_chunk(file, temp_chunk)
			temp_data=file.read(calcsize('3B'))
			temp_chunk.bytes_read+= 3
			contextMaterial.rgbCol= [float(col)/255 for col in unpack('3B', temp_data)] # data [0,1,2] == rgb
			new_chunk.bytes_read+= temp_chunk.bytes_read

		elif (new_chunk.ID==MAT_SPECULAR):
			#print 'elif (new_chunk.ID==MAT_SPECULAR):'
			read_chunk(file, temp_chunk)
			temp_data= file.read(calcsize('3B'))
			temp_chunk.bytes_read+= 3
			
			contextMaterial.specCol= [float(col)/255 for col in unpack('3B', temp_data)] # data [0,1,2] == rgb
			new_chunk.bytes_read+= temp_chunk.bytes_read

		elif (new_chunk.ID==MAT_TEXTURE_MAP):
			#print 'elif (new_chunk.ID==MAT_TEXTURE_MAP):'
			new_texture= Blender.Texture.New('Diffuse')
			new_texture.setType('Image')
			while (new_chunk.bytes_read<new_chunk.length):
				#print 'MAT_TEXTURE_MAP..while', new_chunk.bytes_read, new_chunk.length
				read_chunk(file, temp_chunk)
				
				if (temp_chunk.ID==MAT_MAP_FILENAME):
					texture_name=read_string(file)
					img= TEXTURE_DICT[contextMaterial.name]= BPyImage.comprehensiveImageLoad(texture_name, FILENAME)
					new_chunk.bytes_read += (len(texture_name)+1) #plus one for the null character that gets removed
					
				else:
					skip_to_end(file, temp_chunk)
				
				new_chunk.bytes_read+= temp_chunk.bytes_read
			
			#add the map to the material in the right channel
			add_texture_to_material(img, new_texture, contextMaterial, 'DIFFUSE')
			
		elif (new_chunk.ID==MAT_SPECULAR_MAP):
			#print 'elif (new_chunk.ID==MAT_SPECULAR_MAP):'
			new_texture= Blender.Texture.New('Specular')
			new_texture.setType('Image')
			while (new_chunk.bytes_read<new_chunk.length):
				read_chunk(file, temp_chunk)
				
				if (temp_chunk.ID==MAT_MAP_FILENAME):
					texture_name= read_string(file)
					img= BPyImage.comprehensiveImageLoad(texture_name, FILENAME)
					new_chunk.bytes_read+= (len(texture_name)+1) #plus one for the null character that gets removed
				else:
					skip_to_end(file, temp_chunk)
				
				new_chunk.bytes_read+= temp_chunk.bytes_read
				
			#add the map to the material in the right channel
			add_texture_to_material(img, new_texture, contextMaterial, 'SPECULAR')
	
		elif (new_chunk.ID==MAT_OPACITY_MAP):
			#print 'new_texture=Blender.Texture.New('Opacity')'
			new_texture= Blender.Texture.New('Opacity')
			new_texture.setType('Image')
			while (new_chunk.bytes_read<new_chunk.length):
				read_chunk(file, temp_chunk)
				
				if (temp_chunk.ID==MAT_MAP_FILENAME):
					texture_name= read_string(file)
					img= BPyImage.comprehensiveImageLoad(texture_name, FILENAME)
					new_chunk.bytes_read += (len(texture_name)+1) #plus one for the null character that gets removed
				else:
					skip_to_end(file, temp_chunk)
				
				new_chunk.bytes_read+= temp_chunk.bytes_read

			#add the map to the material in the right channel
			add_texture_to_material(img, new_texture, contextMaterial, 'OPACITY')

		elif (new_chunk.ID==MAT_BUMP_MAP):
			#print 'elif (new_chunk.ID==MAT_BUMP_MAP):'
			new_texture= Blender.Texture.New('Bump')
			new_texture.setType('Image')
			while (new_chunk.bytes_read<new_chunk.length):
				read_chunk(file, temp_chunk)
				
				if (temp_chunk.ID==MAT_MAP_FILENAME):
					texture_name= read_string(file)
					img= BPyImage.comprehensiveImageLoad(texture_name, FILENAME)
					new_chunk.bytes_read += (len(texture_name)+1) #plus one for the null character that gets removed
				else:
					skip_to_end(file, temp_chunk)
				
				new_chunk.bytes_read+=temp_chunk.bytes_read
				
			#add the map to the material in the right channel
			add_texture_to_material(img, new_texture, contextMaterial, 'BUMP')
			
		elif (new_chunk.ID==MAT_TRANSPARENCY):
			#print 'elif (new_chunk.ID==MAT_TRANSPARENCY):'
			read_chunk(file, temp_chunk)
			temp_data=file.read(STRUCT_SIZE_UNSIGNED_SHORT)
			
			temp_chunk.bytes_read+=2
			contextMaterial.alpha= 1-(float(unpack('H', temp_data)[0])/100)
			new_chunk.bytes_read+=temp_chunk.bytes_read


		elif (new_chunk.ID==OBJECT_LAMP): # Basic lamp support.
			
			#print 'LAMP!!!!!!!!!'
			temp_data=file.read(STRUCT_SIZE_3FLOAT)
			
			x,y,z=unpack('3f', temp_data)
			new_chunk.bytes_read+=STRUCT_SIZE_3FLOAT
			
			contextLamp[0]= Object.New('Lamp')
			contextLamp[1]= Lamp.New()
			contextLamp[0].link(contextLamp[1])
			scn.link(contextLamp[0])
			
			
			
			#print 'number of faces: ', num_faces
			#print x,y,z
			contextLamp[0].setLocation(x,y,z)
			
			# Reset matrix
			contextMatrix= Mathutils.Matrix(); contextMatrix.identity()	
			#print contextLamp.name, 
			
			
		elif (new_chunk.ID==OBJECT_MESH):
			# print 'Found an OBJECT_MESH chunk'
			if contextMesh != None: # Write context mesh if we have one.
				putContextMesh(contextMesh, contextMeshMaterials)
			
			contextMesh= Mesh.New()
			contextMeshMaterials= {} # matname:[face_idxs]
			contextMeshUV= None
			#contextMesh.vertexUV= 1 # Make sticky coords.
			# Reset matrix
			contextMatrix= Blender.Mathutils.Matrix(); contextMatrix.identity()
		
		elif (new_chunk.ID==OBJECT_VERTICES):
			# print 'elif (new_chunk.ID==OBJECT_VERTICES):'
			temp_data=file.read(STRUCT_SIZE_UNSIGNED_SHORT)
			num_verts,=unpack('H', temp_data)
			new_chunk.bytes_read+=2
			
			# print 'number of verts: ', num_verts
			def getvert():
				temp_data=file.read(STRUCT_SIZE_3FLOAT)
				new_chunk.bytes_read += STRUCT_SIZE_3FLOAT #12: 3 floats x 4 bytes each
				return Vector(unpack('3f', temp_data))
			
			contextMesh.verts.extend( [getvert() for i in xrange(num_verts)] )
			#print 'object verts: bytes read: ', new_chunk.bytes_read

		elif (new_chunk.ID==OBJECT_FACES):
			# print 'elif (new_chunk.ID==OBJECT_FACES):'
			temp_data= file.read(STRUCT_SIZE_UNSIGNED_SHORT)
			num_faces,= unpack('H', temp_data)
			new_chunk.bytes_read+= 2
			#print 'number of faces: ', num_faces
			
			def getface():
				# print '\ngetting a face'
				temp_data= file.read(STRUCT_SIZE_4UNSIGNED_SHORT)
				new_chunk.bytes_read+= STRUCT_SIZE_4UNSIGNED_SHORT #4 short ints x 2 bytes each
				v1,v2,v3,dummy= unpack('4H', temp_data)
				if v1==v2 or v1==v3 or v2==v3:
					return None
				return contextMesh.verts[v1], contextMesh.verts[v2], contextMesh.verts[v3]
			
			faces= [ getface() for i in xrange(num_faces) ]
			facesExtend= [ f for f in faces if f ]
			
			if facesExtend:
				contextMesh.faces.extend( facesExtend )
				
				# face mapping so duplicate faces dont mess us up.
				if len(contextMesh.faces)==len(faces):
					contextFaceMapping= None
				else:
					contextFaceMapping= {}
					meshFaceOffset= 0
					for i, f in enumerate(faces):
						if not f: # Face used stupid verts-
							contextFaceMapping[i]= None
							meshFaceOffset+= 1
						else:
							#print 'DOUBLE FACE', '\tfacelen', len(f), i, num_faces, (i-meshFaceOffset)
							#print i-meshFaceOffset, len(contextMesh.faces)q
							if len(contextMesh.faces) <= i-meshFaceOffset: # SHOULD NEVER HAPPEN, CORRUPS 3DS?
								contextFaceMapping[i]= None
								meshFaceOffset-=1
							else:
								meshface= contextMesh.faces[i-meshFaceOffset]
								ok= True
								for vi in xrange(len(f)):
									if meshface.v[vi] != f[vi]:
										ok=False
										break
								if ok:
									meshFaceOffset+=1
									contextFaceMapping[i]= i-meshFaceOffset
								else:
									contextFaceMapping[i]= None
				


		elif (new_chunk.ID==OBJECT_MATERIAL):
			# print 'elif (new_chunk.ID==OBJECT_MATERIAL):'
			material_name= read_string(file)
			new_chunk.bytes_read += len(material_name)+1 # remove 1 null character.
			
			tempMatFaceIndexList = contextMeshMaterials[material_name]= []
			
			temp_data=file.read(STRUCT_SIZE_UNSIGNED_SHORT)
			num_faces_using_mat,= unpack('H', temp_data)
			new_chunk.bytes_read += STRUCT_SIZE_UNSIGNED_SHORT
			
			#list of faces using mat
			for face_counter in xrange(num_faces_using_mat):
				temp_data= file.read(STRUCT_SIZE_UNSIGNED_SHORT)
				new_chunk.bytes_read+= STRUCT_SIZE_UNSIGNED_SHORT
				faceIndex,= unpack('H', temp_data)
				
				# We dont have to use context face mapping.
				if contextFaceMapping:
					meshFaceIndex= contextFaceMapping[faceIndex]
				else:
					meshFaceIndex= faceIndex
				
				if meshFaceIndex != None:
					tempMatFaceIndexList.append(meshFaceIndex)
			
			tempMatFaceIndexList.sort()
			del tempMatFaceIndexList
			#look up the material in all the materials

		elif (new_chunk.ID==OBJECT_UV):
			# print 'elif (new_chunk.ID==OBJECT_UV):'
			temp_data=file.read(STRUCT_SIZE_UNSIGNED_SHORT)
			num_uv,=unpack('H', temp_data)
			new_chunk.bytes_read+= 2
			
			def getuv():
				temp_data=file.read(STRUCT_SIZE_2FLOAT)
				new_chunk.bytes_read += STRUCT_SIZE_2FLOAT #2 float x 4 bytes each
				return Vector( unpack('2f', temp_data) )
				
			contextMeshUV= [ getuv() for i in xrange(num_uv) ]
		
		elif (new_chunk.ID== OBJECT_TRANS_MATRIX):
			# print 'elif (new_chunk.ID== OBJECT_TRANS_MATRIX):'
			temp_data=file.read(STRUCT_SIZE_4x3MAT)
			data= list( unpack('ffffffffffff', temp_data) )
			new_chunk.bytes_read += STRUCT_SIZE_4x3MAT 
			
			contextMatrix= Blender.Mathutils.Matrix(\
			 data[:3] + [0],\
			 data[3:6] + [0],\
			 data[6:9] + [0],\
			 data[9:] + [1])
		
		elif  (new_chunk.ID==MAT_MAP_FILENAME):
			raise 'Hello--'
			texture_name=read_string(file)
			try:
				TEXTURE_DICT[contextMaterial.name]
			except:
				img= TEXTURE_DICT[contextMaterial.name]= BPyImage.comprehensiveImageLoad(texture_name, FILENAME)
			
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
	if contextMesh != None:
		putContextMesh(contextMesh, contextMeshMaterials)

def load_3ds(filename):
	print '\n\nImporting "%s" "%s"' % (filename, Blender.sys.expandpath(filename))
	
	scn= Scene.GetCurrent()
	for ob in scn.getChildren():
		ob.sel= 0
	time1= Blender.sys.time()
	
	global FILENAME
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

	process_next_chunk(file, current_chunk, scn)
	
	# Select all new objects.
	print 'finished importing: "%s" in %.4f sec.' % (filename, (Blender.sys.time()-time1))
	file.close()

if __name__=='__main__':
	Blender.Window.FileSelector(load_3ds, 'Import 3DS', '*.3ds')

# For testing compatibility
'''
TIME= Blender.sys.time()
import os
print 'Searching for files'
os.system('find /metavr/ -iname "*.3ds" > /tmp/temp3ds_list')
# os.system('find /storage/ -iname "*.3ds" > /tmp/temp3ds_list')
print '...Done'
file= open('/tmp/temp3ds_list', 'r')
lines= file.readlines()
file.close()

def between(v,a,b):
	if v <= max(a,b) and v >= min(a,b):
		return True
	return False
	
for i, _3ds in enumerate(lines):
	if between(i, 600, 700):
		_3ds= _3ds[:-1]
		print 'Importing', _3ds, '\nNUMBER', i, 'of', len(lines)
		_3ds_file= _3ds.split('/')[-1].split('\\')[-1]
		newScn= Scene.New(_3ds_file)
		newScn.makeCurrent()
		load_3ds(_3ds)

print 'TOTAL TIME: %.6f' % (Blender.sys.time() - TIME)
'''
