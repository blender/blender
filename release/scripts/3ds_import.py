#!BPY

""" 
Name: '3D Studio (.3ds)...'
Blender: 237
Group: 'Import'
Tooltip: 'Import from 3DS file format (.3ds).'
"""

__author__ = ["Bob Holcomb", "Richard Lärkäng", "Damien McGinnes", "Campbell Barton"]
__url__ = ("blender", "elysiun", "http://www.gametutorials.com")
__version__ = "0.82"
__bpydoc__ = """\

3ds Importer

This script imports a 3ds file and the materials into blender for editing.

Loader is based on 3ds loader from www.gametutorials.com(Thanks DigiBen).

Changes:<br>
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
from Blender import NMesh, Scene, Object, Material, Image

import sys, struct, string

import os

#this script imports uvcoords as sticky vertex coords
#this parameter enables copying these to face uv coords
#which shold be more useful.


#===========================================================================#
# Returns unique name of object/mesh (stops overwriting existing meshes)    #
#===========================================================================#
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


######################################################
# Data Structures
######################################################

#Some of the chunks that we will see
#----- Primary Chunk, at the beginning of each file
PRIMARY= long("0x4D4D",16)

#------ Main Chunks
OBJECTINFO   =      long("0x3D3D",16);      #This gives the version of the mesh and is found right before the material and object information
VERSION      =      long("0x0002",16);      #This gives the version of the .3ds file
EDITKEYFRAME=      long("0xB000",16);      #This is the header for all of the key frame info

#------ sub defines of OBJECTINFO
MATERIAL=45055		#0xAFFF				// This stored the texture info
OBJECT=16384		#0x4000				// This stores the faces, vertices, etc...

#>------ sub defines of MATERIAL
MATNAME    =      long("0xA000",16);      # This holds the material name
MATAMBIENT   =      long("0xA010",16);      # Ambient color of the object/material
MATDIFFUSE   =      long("0xA020",16);      # This holds the color of the object/material
MATSPECULAR   =      long("0xA030",16);      # SPecular color of the object/material
MATSHINESS   =      long("0xA040",16);      # ??
MATMAP       =      long("0xA200",16);      # This is a header for a new material
MATMAPFILE    =      long("0xA300",16);      # This holds the file name of the texture

#>------ sub defines of OBJECT
OBJECT_MESH  =      long("0x4100",16);      # This lets us know that we are reading a new object
OBJECT_LIGHT =      long("0x4600",16);      # This lets un know we are reading a light object
OBJECT_CAMERA=      long("0x4700",16);      # This lets un know we are reading a camera object

#>------ sub defines of CAMERA
OBJECT_CAM_RANGES=   long("0x4720",16);      # The camera range values

#>------ sub defines of OBJECT_MESH
OBJECT_VERTICES =   long("0x4110",16);      # The objects vertices
OBJECT_FACES    =   long("0x4120",16);      # The objects faces
OBJECT_MATERIAL =   long("0x4130",16);      # This is found if the object has a material, either texture map or color
OBJECT_UV       =   long("0x4140",16);      # The UV texture coordinates
OBJECT_TRANS_MATRIX  =   long("0x4160",16); # The Object Matrix

#the chunk class
class chunk:
	ID=0
	length=0
	bytes_read=0

	#we don't read in the bytes_read, we compute that
	binary_format="<HI"

	def __init__(self):
		self.ID=0
		self.length=0
		self.bytes_read=0

	def dump(self):
		print "ID: ", self.ID
		print "ID in hex: ", hex(self.ID)
		print "length: ", self.length
		print "bytes_read: ", self.bytes_read
		

def read_chunk(file, chunk):
		temp_data=file.read(struct.calcsize(chunk.binary_format))
		data=struct.unpack(chunk.binary_format, temp_data)
		chunk.ID=data[0]
		chunk.length=data[1]
		#update the bytes read function
		chunk.bytes_read=6

		#if debugging
		#chunk.dump()

def read_string(file):
	s=""
	index=0
	#print "reading a string"
	#read in the characters till we get a null character
	temp_data=file.read(1)
	data=struct.unpack("c", temp_data)
	s=s+(data[0])
	#print "string: ",s
	while(ord(s[index])!=0):
		index+=1
		temp_data=file.read(1)
		data=struct.unpack("c", temp_data)
		s=s+(data[0])
		#print "string: ",s
	
	#remove the null character from the string
	the_string=s[:-1]
	return str(the_string)

######################################################
# IMPORT
######################################################
def process_next_object_chunk(file, previous_chunk):
	new_chunk=chunk()
	temp_chunk=chunk()

	while (previous_chunk.bytes_read<previous_chunk.length):
		#read the next chunk
		read_chunk(file, new_chunk)


def process_next_chunk(file, previous_chunk, new_object_list):
	contextObName = None
	#contextLamp = None
	contextMaterial = None
	contextMatrix = Blender.Mathutils.Matrix(); contextMatrix.identity()
	contextMesh = None
	
	TEXDICT={}
	MATDICT={}
	
	objectList = [] # Keep a list of imported objects.
	
	# Localspace variable names, faster.
	STRUCT_SIZE_1CHAR = struct.calcsize("c")
	STRUCT_SIZE_2FLOAT = struct.calcsize("2f")
	STRUCT_SIZE_3FLOAT = struct.calcsize("3f")
	STRUCT_SIZE_UNSIGNED_SHORT = struct.calcsize("H")
	STRUCT_SIZE_4UNSIGNED_SHORT = struct.calcsize("4H")
	STRUCT_SIZE_4x3MAT = struct.calcsize("ffffffffffff")
	
	
	def putContextMesh(myContextMesh):
		INV_MAT = Blender.Mathutils.CopyMat(contextMatrix)
		INV_MAT.invert()
		contextMesh.transform(INV_MAT)
		objectList.append(NMesh.PutRaw(contextMesh))
		objectList[-1].name = contextObName
		objectList[-1].setMatrix(contextMatrix)
	
	
	#a spare chunk
	new_chunk=chunk()
	temp_chunk=chunk()

	#loop through all the data for this chunk (previous chunk) and see what it is
	while (previous_chunk.bytes_read<previous_chunk.length):
		#read the next chunk
		#print "reading a chunk"
		read_chunk(file, new_chunk)

		#is it a Version chunk?
		if (new_chunk.ID==VERSION):
			#print "found a VERSION chunk"
			#read in the version of the file
			#it's an unsigned short (H)
			temp_data=file.read(struct.calcsize("I"))
			data=struct.unpack("I", temp_data)
			version=data[0]
			new_chunk.bytes_read+=4 #read the 4 bytes for the version number
			#this loader works with version 3 and below, but may not with 4 and above
			if (version>3):
				print "\tNon-Fatal Error:  Version greater than 3, may not load correctly: ", version

		#is it an object info chunk?
		elif (new_chunk.ID==OBJECTINFO):
			# print "found an OBJECTINFO chunk"
			process_next_chunk(file, new_chunk, new_object_list)
			
			#keep track of how much we read in the main chunk
			new_chunk.bytes_read+=temp_chunk.bytes_read

		#is it an object chunk?
		elif (new_chunk.ID==OBJECT):
			# print "found an OBJECT chunk"
			tempName = str(read_string(file))
			contextObName = getUniqueName( tempName )
			new_chunk.bytes_read += (len(tempName)+1)
		
		#is it a material chunk?
		elif (new_chunk.ID==MATERIAL):
			# print "found a MATERIAL chunk"
			contextMaterial = Material.New()
		
		elif (new_chunk.ID==MATNAME):
			# print "Found a MATNAME chunk"
			material_name=""
			material_name=str(read_string(file))
			
			#plus one for the null character that ended the string
			new_chunk.bytes_read+=(len(material_name)+1)
			
			contextMaterial.setName(material_name)
			MATDICT[material_name] = contextMaterial.name
		
		elif (new_chunk.ID==MATAMBIENT):
			# print "Found a MATAMBIENT chunk"

			read_chunk(file, temp_chunk)
			temp_data=file.read(struct.calcsize("3B"))
			data=struct.unpack("3B", temp_data)
			temp_chunk.bytes_read+=3
			contextMaterial.mirCol = [float(col)/255 for col in data] # data [0,1,2] == rgb
			new_chunk.bytes_read+=temp_chunk.bytes_read

		elif (new_chunk.ID==MATDIFFUSE):
			# print "Found a MATDIFFUSE chunk"

			read_chunk(file, temp_chunk)
			temp_data=file.read(struct.calcsize("3B"))
			data=struct.unpack("3B", temp_data)
			temp_chunk.bytes_read+=3
			contextMaterial.rgbCol = [float(col)/255 for col in data] # data [0,1,2] == rgb
			new_chunk.bytes_read+=temp_chunk.bytes_read

		elif (new_chunk.ID==MATSPECULAR):
			# print "Found a MATSPECULAR chunk"

			read_chunk(file, temp_chunk)
			temp_data=file.read(struct.calcsize("3B"))
			data=struct.unpack("3B", temp_data)
			temp_chunk.bytes_read+=3
			
			contextMaterial.specCol = [float(col)/255 for col in data] # data [0,1,2] == rgb
			new_chunk.bytes_read+=temp_chunk.bytes_read

		elif (new_chunk.ID==MATMAP):
			# print "Found a MATMAP chunk"
			pass # This chunk has no data

		elif (new_chunk.ID==MATMAPFILE):
			# print "Found a MATMAPFILE chunk"
			texture_name=""
			texture_name=str(read_string(file))
			try:
				img = Image.Load(texture_name)
				TEXDICT[contextMaterial.name]=img
			except IOError:
				fname = os.path.join( os.path.dirname(FILENAME), texture_name)
				try:
					img = Image.Load(fname)
					TEXDICT[contextMaterial.name]=img
				except IOError:
					print "\tERROR: failed to load image ",texture_name
					TEXDICT[contextMaterial.name] = None # Dummy
					
			#plus one for the null character that gets removed
			new_chunk.bytes_read += (len(texture_name)+1)


		elif (new_chunk.ID==OBJECT_MESH):
			# print "Found an OBJECT_MESH chunk"
			if contextMesh != None: # Write context mesh if we have one.
				putContextMesh(contextMesh)
			
			contextMesh = NMesh.New()
			
			# Reset matrix
			contextMatrix = Blender.Mathutils.Matrix(); contextMatrix.identity()
			
		elif (new_chunk.ID==OBJECT_VERTICES):
			# print "Found an OBJECT_VERTICES chunk"
			#print "object_verts: length: ", new_chunk.length
			temp_data=file.read(STRUCT_SIZE_UNSIGNED_SHORT)
			data=struct.unpack("H", temp_data)
			new_chunk.bytes_read+=2
			num_verts=data[0]
			# print "number of verts: ", num_verts
			for counter in range (num_verts):
				temp_data=file.read(STRUCT_SIZE_3FLOAT)
				new_chunk.bytes_read += STRUCT_SIZE_3FLOAT #12: 3 floats x 4 bytes each
				data=struct.unpack("3f", temp_data)
				v=NMesh.Vert(data[0],data[1],data[2])
				contextMesh.verts.append(v)
			#print "object verts: bytes read: ", new_chunk.bytes_read

		elif (new_chunk.ID==OBJECT_FACES):
			# print "Found an OBJECT_FACES chunk"
			#print "object faces: length: ", new_chunk.length
			temp_data=file.read(STRUCT_SIZE_UNSIGNED_SHORT)
			data=struct.unpack("H", temp_data)
			new_chunk.bytes_read+=2
			num_faces=data[0]
			#print "number of faces: ", num_faces

			for counter in range(num_faces):
				temp_data=file.read(STRUCT_SIZE_4UNSIGNED_SHORT)
				new_chunk.bytes_read += STRUCT_SIZE_4UNSIGNED_SHORT #4 short ints x 2 bytes each
				data=struct.unpack("4H", temp_data)
				
				#insert the mesh info into the faces, don't worry about data[3] it is a 3D studio thing
				f = NMesh.Face( [contextMesh.verts[data[i]] for i in xrange(3) ] )
				f.uv = [ tuple(contextMesh.verts[data[i]].uvco[:2]) for  i in xrange(3) ]
				contextMesh.faces.append(f)
			#print "object faces: bytes read: ", new_chunk.bytes_read

		elif (new_chunk.ID==OBJECT_MATERIAL):
			# print "Found an OBJECT_MATERIAL chunk"
			material_name=""
			material_name=str(read_string(file))
			new_chunk.bytes_read += len(material_name)+1 # remove 1 null character.

			#look up the material in all the materials
			material_found=0
			for mat in Material.Get():
				
				#found it, add it to the mesh
				if(mat.name==material_name):
					if len(contextMesh.materials) >= 15:
						print "\tCant assign more than 16 materials per mesh, keep going..."
						break
					else:
						meshHasMat = 0
						for myMat in contextMesh.materials:
							if myMat.name == mat.name:
								meshHasMat = 1
						
						if meshHasMat == 0:
							contextMesh.addMaterial(mat)
							material_found=1
							
							#figure out what material index this is for the mesh
							for mat_counter in range(len(contextMesh.materials)):
								if contextMesh.materials[mat_counter].name == material_name:
									mat_index=mat_counter
									#print "material index: ",mat_index
							
						
						break # get out of this for loop so we don't accidentally set material_found back to 0
				else:
					material_found=0
					# print "Not matching: ", mat.name, " and ", material_name

			if material_found == 1:
				contextMaterial = mat
				#read the number of faces using this material
				temp_data=file.read(STRUCT_SIZE_UNSIGNED_SHORT)
				data=struct.unpack("H", temp_data)
				new_chunk.bytes_read += STRUCT_SIZE_UNSIGNED_SHORT
				num_faces_using_mat=data[0]

				#list of faces using mat
				for face_counter in range(num_faces_using_mat):
					temp_data=file.read(STRUCT_SIZE_UNSIGNED_SHORT)
					new_chunk.bytes_read += STRUCT_SIZE_UNSIGNED_SHORT
					data=struct.unpack("H", temp_data)
					contextMesh.faces[data[0]].materialIndex = mat_index
					
					try:
						mname = MATDICT[contextMaterial.name]
						contextMesh.faces[data[0]].image = TEXDICT[mname]
					except:
						continue
			else:
				#read past the information about the material you couldn't find
				#print "Couldn't find material.  Reading past face material info"
				buffer_size=new_chunk.length-new_chunk.bytes_read
				binary_format=str(buffer_size)+"c"
				temp_data=file.read(struct.calcsize(binary_format))
				new_chunk.bytes_read+=buffer_size
			
			#print "object mat: bytes read: ", new_chunk.bytes_read

		elif (new_chunk.ID == OBJECT_UV):
			# print "Found an OBJECT_UV chunk"
			temp_data=file.read(STRUCT_SIZE_UNSIGNED_SHORT)
			data=struct.unpack("H", temp_data)
			new_chunk.bytes_read+=2
			num_uv=data[0]

			for counter in range(num_uv):
				temp_data=file.read(STRUCT_SIZE_2FLOAT)
				new_chunk.bytes_read += STRUCT_SIZE_2FLOAT #2 float x 4 bytes each
				data=struct.unpack("2f", temp_data)
				
				#insert the insert the UV coords in the vertex data
				contextMesh.verts[counter].uvco = data
		
		elif (new_chunk.ID == OBJECT_TRANS_MATRIX):
			# print "Found an OBJECT_TRANS_MATRIX chunk"
			
			temp_data=file.read(STRUCT_SIZE_4x3MAT)
			data = list( struct.unpack("ffffffffffff", temp_data) )
			new_chunk.bytes_read += STRUCT_SIZE_4x3MAT 
			
			contextMatrix = Blender.Mathutils.Matrix(\
			 data[:3] + [0],\
			 data[3:6] + [0],\
			 data[6:9] + [0],\
			 data[9:] + [1])
		
		
		
		else: #(new_chunk.ID!=VERSION or new_chunk.ID!=OBJECTINFO or new_chunk.ID!=OBJECT or new_chunk.ID!=MATERIAL):
			# print "skipping to end of this chunk"
			buffer_size=new_chunk.length-new_chunk.bytes_read
			binary_format=str(buffer_size)+"c"
			temp_data=file.read(struct.calcsize(binary_format))
			new_chunk.bytes_read+=buffer_size


		#update the previous chunk bytes read
		previous_chunk.bytes_read += new_chunk.bytes_read
		#print "Bytes left in this chunk: ", previous_chunk.length-previous_chunk.bytes_read
	
	# FINISHED LOOP
	# There will be a number of objects still not added
	if contextMesh != None:
		putContextMesh(contextMesh)
	
	for ob in objectList:
		ob.sel = 1

def load_3ds (filename):
	print 'Importing "%s"' % filename
	
	time1 = Blender.sys.time()
	
	global FILENAME
	FILENAME=filename
	current_chunk=chunk()
	
	file=open(filename,"rb")
	
	#here we go!
	# print "reading the first chunk"
	new_object_list = []
	read_chunk(file, current_chunk)
	if (current_chunk.ID!=PRIMARY):
		print "\tFatal Error:  Not a valid 3ds file: ", filename
		file.close()
		return

	process_next_chunk(file, current_chunk, new_object_list)
	
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
