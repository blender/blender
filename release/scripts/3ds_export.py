#!BPY

""" 
Name: '3D Studio (.3ds)...'
Blender: 237
Group: 'Export'
Tooltip: 'Export to 3DS file format (.3ds).'
"""

__author__ = ["Campbell Barton", "Bob Holcomb", "Richard Lärkäng", "Damien McGinnes"]
__url__ = ("blender", "elysiun", "http://www.gametutorials.com")
__version__ = "0.82"
__bpydoc__ = """\

3ds Exporter

This script Exports a 3ds file and the materials into blender for editing.

Exporting is based on 3ds loader from www.gametutorials.com(Thanks DigiBen).
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

import Blender
from Blender import NMesh, Scene, Object, Material
import struct


######################################################
# Data Structures
######################################################

#Some of the chunks that we will export
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

RGB1=	long("0x0011",16)
RGB2=	long("0x0012",16)

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

#==============================================#
# Strips the slashes from the back of a string #
#==============================================#
def stripPath(path):
	return path.split('/')[-1].split('\\')[-1]

#==================================================#
# New name based on old with a different extension #
#==================================================#
def newFName(ext):
	return Blender.Get('filename')[: -len(Blender.Get('filename').split('.', -1)[-1]) ] + ext


#the chunk class
class chunk:
	ID=0
	size=0

	def __init__(self):
		self.ID=0
		self.size=0

	def get_size(self):
		self.size=6

	def write(self, file):
		#write header
		data=struct.pack(\
		"<HI",\
		self.ID,\
		self.size)
		file.write(data)

	def dump(self):
		print "ID: ", self.ID
		print "ID in hex: ", hex(self.ID)
		print "size: ", self.size



#may want to add light, camera, keyframe chunks.
class vert_chunk(chunk):
	verts=[]

	def __init__(self):
		self.verts=[]
		self.ID=OBJECT_VERTICES

	def get_size(self):
		chunk.get_size(self)
		temp_size=2 #for the number of verts short
		temp_size += 12 * len(self.verts)  #3 floats x 4 bytes each
		self.size+=temp_size
		#~ print "vert_chunk size: ", self.size
		return self.size
	
	def write(self, file):
		chunk.write(self, file)
		#write header
		data=struct.pack("<H", len(self.verts))
		file.write(data)
		#write verts
		for vert in self.verts:
			data=struct.pack("<3f",vert[0],vert[1], vert[2])
			file.write(data)

class obj_material_chunk(chunk):
	name=""
	faces=[]

	def __init__(self):
		self.name=""
		self.faces=[]
		self.ID=OBJECT_MATERIAL

	def get_size(self):
		chunk.get_size(self)
		temp_size=(len(self.name)+1)
		temp_size+=2
		for face in self.faces:
			temp_size+=2
		self.size+=temp_size
		#~ print "obj material chunk size: ", self.size
		return self.size

	def write(self, file):
		chunk.write(self, file)
		#write name
		name_length=len(self.name)+1
		binary_format="<"+str(name_length)+"s"
		data=struct.pack(binary_format, self.name)
		file.write(data)
		binary_format="<H"
		#~ print "Nr of faces: ", len(self.faces)
		data=struct.pack(binary_format, len(self.faces))
		file.write(data)
		for face in self.faces:
			data=struct.pack(binary_format, face)
			file.write(data)

class face_chunk(chunk):
	faces=[]
	num_faces=0
	m_chunks=[]

	def __init__(self):
		self.faces=[]
		self.ID=OBJECT_FACES
		self.num_faces=0
		self.m_chunks=[]

	def get_size(self):
		chunk.get_size(self)
		temp_size = 2 #num faces info
		temp_size += 8 * len(self.faces)  #4 short ints x 2 bytes each
		for m in self.m_chunks:
			temp_size+=m.get_size()
		self.size += temp_size
		#~ print "face_chunk size: ", self.size
		return self.size

	def write(self, file):
		chunk.write(self, file)
		data=struct.pack("<H", len(self.faces))
		file.write(data)
		#write faces
		for face in self.faces:
			data=struct.pack("<4H", face[0],face[1], face[2], 0) # The last zero is only used by 3d studio
			file.write(data)
		#write materials
		for m in self.m_chunks:
			m.write(file)

class uv_chunk(chunk):
	uv=[]
	num_uv=0

	def __init__(self):
		self.uv=[]
		self.ID=OBJECT_UV
		self.num_uv=0

	def get_size(self):
		chunk.get_size(self)
		temp_size=2 #for num UV
		for this_uv in self.uv:
			temp_size+=8  #2 floats at 4 bytes each
		self.size+=temp_size
		#~ print "uv chunk size: ", self.size
		return self.size

	def write(self, file):
		chunk.write(self, file)
		
		#write header
		data=struct.pack("<H", len(self.uv))
		file.write(data)
		
		#write verts
		for this_uv in self.uv:
			data=struct.pack("<2f", this_uv[0], this_uv[1])
			file.write(data)

class mesh_chunk(chunk):
	v_chunk=vert_chunk()
	f_chunk=face_chunk()
	uv_chunk=uv_chunk()

	def __init__(self):
		self.v_chunk=vert_chunk()
		self.f_chunk=face_chunk()
		self.uv_chunk=uv_chunk()
		self.ID=OBJECT_MESH

	def get_size(self):
		chunk.get_size(self)
		temp_size=self.v_chunk.get_size()
		temp_size+=self.f_chunk.get_size()
		temp_size+=self.uv_chunk.get_size()
		self.size+=temp_size
		#~ print "object mesh chunk size: ", self.size
		return self.size

	def write(self, file):
		chunk.write(self, file)
		#write stuff
		self.v_chunk.write(file)
		self.f_chunk.write(file)
		self.uv_chunk.write(file)

class object_chunk(chunk):
	name=""
	mesh_chunks=[]

	def __init__(self):
		self.name=""
		self.mesh_chunks=[]
		self.ID=OBJECT

	def get_size(self):
		chunk.get_size(self)
		temp_size=len(self.name)+1 #+1 for null character
		for mesh in self.mesh_chunks:
			temp_size+=mesh.get_size()
		self.size+=temp_size
		#~ print "object chunk size: ", self.size
		return self.size

	def write(self, file):
		chunk.write(self, file)
		#write name
		
		binary_format = "<%ds" % (len(self.name)+1)
		data=struct.pack(binary_format, self.name)
		file.write(data)
		#write stuff
		for mesh in self.mesh_chunks:
			mesh.write(file)

class object_info_chunk(chunk):
	obj_chunks=[]
	mat_chunks=[]

	def __init__(self):
		self.obj_chunks=[]
		self.mat_chunks=[]
		self.ID=OBJECTINFO

	def get_size(self):
		chunk.get_size(self)
		temp_size=0
		for mat in self.mat_chunks:
			temp_size+=mat.get_size()
		for obj in self.obj_chunks:
			temp_size+=obj.get_size()
		self.size+=temp_size
		#~ print "object info size: ", self.size
		return self.size

	def write(self, file):
		chunk.write(self, file)
		#write all the materials
		for mat in self.mat_chunks:
			mat.write(file)
		#write all the objects
		for obj in self.obj_chunks:
			obj.write(file)



class version_chunk(chunk):
	version=3

	def __init__(self):
		self.ID=VERSION
		self.version=3 #that the document that I'm using

	def get_size(self):
		chunk.get_size(self)
		self.size += 4 #bytes for the version info
		#~ print "version chunk size: ", self.size
		return self.size

	def write(self, file):
		chunk.write(self, file)
		#write header and version
		data=struct.pack("<I", self.version)
		file.write(data)

class rgb_chunk(chunk):
	col=[]

	def __init__(self):
		self.col=[]

	def get_size(self):
		chunk.get_size(self)
		self.size+=3 #color size
		#~ print "rgb chunk size: ", self.size
		return self.size

	def write(self, file):
		chunk.write(self, file)
		#write colors
		for c in self.col:
			file.write( struct.pack("<c", chr(int(255*c))) )


class rgb1_chunk(rgb_chunk):

	def __init__(self):
		self.ID=RGB1

class rgb2_chunk(rgb_chunk):

	def __init__(self):
		self.ID=RGB2

class material_ambient_chunk(chunk):
	col1=None
	col2=None

	def __init__(self):
		self.ID=MATAMBIENT
		self.col1=rgb1_chunk()
		self.col2=rgb2_chunk()

	def get_size(self):
		chunk.get_size(self)
		temp_size=self.col1.get_size()
		temp_size+=self.col2.get_size()
		self.size+=temp_size
		#~ print "material ambient size: ", self.size
		return self.size

	def write(self, file):
		chunk.write(self, file)
		#write colors
		self.col1.write(file)
		self.col2.write(file)

class material_diffuse_chunk(chunk):
	col1=None
	col2=None

	def __init__(self):
		self.ID=MATDIFFUSE
		self.col1=rgb1_chunk()
		self.col2=rgb2_chunk()

	def get_size(self):
		chunk.get_size(self)
		temp_size=self.col1.get_size()
		temp_size+=self.col2.get_size()
		self.size+=temp_size
		#~ print "material diffuse size: ", self.size
		return self.size

	def write(self, file):
		chunk.write(self, file)
		#write colors
		self.col1.write(file)
		self.col2.write(file)

class material_specular_chunk(chunk):
	col1=None
	col2=None

	def __init__(self):
		self.ID=MATSPECULAR
		self.col1=rgb1_chunk()
		self.col2=rgb2_chunk()

	def get_size(self):
		chunk.get_size(self)
		temp_size=self.col1.get_size()
		temp_size+=self.col2.get_size()
		self.size+=temp_size
		#~ print "material specular size: ", self.size
		return self.size

	def write(self, file):
		chunk.write(self, file)
		#write colors
		self.col1.write(file)
		self.col2.write(file)

class material_name_chunk(chunk):
	name=""

	def __init__(self):
		self.ID=MATNAME
		self.name=""

	def get_size(self):
		chunk.get_size(self)
		temp_size=(len(self.name)+1)
		self.size+=temp_size
		#~ print "material name size: ", self.size
		return self.size

	def write(self, file):
		chunk.write(self, file)
		#write name
		name_length=len(self.name)+1
		binary_format="<"+str(name_length)+"s"
		data=struct.pack(binary_format, self.name)
		file.write(data)

class material_chunk(chunk):
	matname_chunk=None
	matambient_chunk=None
	matdiffuse_chunk=None
	matspecular_chunk=None

	def __init__(self):
		self.ID=MATERIAL
		self.matname_chunk=material_name_chunk()
		self.matambient_chunk=material_ambient_chunk()
		self.matdiffuse_chunk=material_diffuse_chunk()
		self.matspecular_chunk=material_specular_chunk()

	def get_size(self):
		chunk.get_size(self)
		temp_size=self.matname_chunk.get_size()
		temp_size+=self.matambient_chunk.get_size()
		temp_size+=self.matdiffuse_chunk.get_size()
		temp_size+=self.matspecular_chunk.get_size()
		self.size+=temp_size
		#~ print "material chunk size: ", self.size
		return self.size

	def write(self, file):
		chunk.write(self, file)
		#write name chunk
		self.matname_chunk.write(file)
		#write material colors
		self.matambient_chunk.write(file)
		self.matdiffuse_chunk.write(file)
		self.matspecular_chunk.write(file)

class primary_chunk(chunk):
	version=None
	obj_info=None

	def __init__(self):
		self.version=version_chunk()
		self.obj_info=object_info_chunk()
		self.ID=PRIMARY

	def get_size(self):
		chunk.get_size(self)
		temp_size=self.version.get_size()
		temp_size+=self.obj_info.get_size()
		self.size+=temp_size
		#~ print "primary chunk size: ", self.size
		return self.size

	def write(self, file):
		chunk.write(self, file)
		#write version chunk
		self.version.write(file)
		#write object_info chunk
		self.obj_info.write(file)

def read_chunk(file, chunk):
		chunk.ID, chunk.size = \
		struct.unpack(\
		chunk.binary_format, \
		file.read(struct.calcsize(chunk.binary_format))  )
	
def read_string(file):
	s=""
	index=0
	
	#read in the characters till we get a null character
	data=struct.unpack("c", file.read(struct.calcsize("c")))
	s=s+(data[0])
	#print "string: ",s
	while(ord(s[index])!=0):
		index+=1
		data=struct.unpack("c", file.read(struct.calcsize("c")))
		s=s+(data[0])
		#print "string: ",s
	return str(s)

######################################################
# EXPORT
######################################################
def save_3ds(filename):
	# Time the export
	time1 = Blender.sys.time()

	exported_materials = {}

	#fill the chunks full of data
	primary=primary_chunk()
	#get all the objects in this scene
	object_list = [ ob for ob in Blender.Object.GetSelected() if ob.getType() == 'Mesh' ]
	#fill up the data structures with objects
	for obj in object_list:
		#create a new object chunk
		primary.obj_info.obj_chunks.append(object_chunk())
		#get the mesh data
		blender_mesh = obj.getData()
		blender_mesh.transform(obj.getMatrix())
		#set the object name
		primary.obj_info.obj_chunks[len(primary.obj_info.obj_chunks)-1].name=obj.getName()

		matrix = obj.getMatrix()

		#make a new mesh chunk object
		mesh=mesh_chunk()
		
		mesh.v_chunk.verts = blender_mesh.verts
		
		dummy = None # just incase...
		
		for m in blender_mesh.materials:
			mesh.f_chunk.m_chunks.append(obj_material_chunk())
			mesh.f_chunk.m_chunks[len(mesh.f_chunk.m_chunks)-1].name = m.name

			# materials should only be exported once
			try:
				dummy = exported_materials[m.name]
				
				
			except KeyError:
				material = material_chunk()
				material.matname_chunk.name=m.name
				material.matambient_chunk.col1.col = m.mirCol
				material.matambient_chunk.col2.col = m.mirCol
				material.matdiffuse_chunk.col1.col = m.rgbCol
				material.matdiffuse_chunk.col2.col = m.rgbCol
				material.matspecular_chunk.col1.col = m.specCol
				material.matspecular_chunk.col2.col = m.specCol
				
				primary.obj_info.mat_chunks.append(material)
				
				exported_materials[m.name] = None
		
		del dummy # unpolute the namespace
		
		valid_faces = [f for f in blender_mesh.faces if len(f) > 2]
		facenr=0
		#fill in faces
		for face in valid_faces:
			
			#is this a tri or a quad
			num_fv=len(face.v)
			
			
			#it's a tri
			if num_fv==3:
				mesh.f_chunk.faces.append((face[0].index, face[1].index, face[2].index))
				if (face.materialIndex < len(mesh.f_chunk.m_chunks)):
					mesh.f_chunk.m_chunks[face.materialIndex].faces.append(facenr)
				facenr+=1
			
			else: #it's a quad					
				mesh.f_chunk.faces.append((face[0].index, face[1].index, face[2].index))  # 0,1,2
				mesh.f_chunk.faces.append((face[2].index, face[3].index, face[0].index))  # 2,3,0
				#first tri
				if (face.materialIndex < len(mesh.f_chunk.m_chunks)):
					mesh.f_chunk.m_chunks[face.materialIndex].faces.append(facenr)
				facenr+=1
				#other tri
				if (face.materialIndex < len(mesh.f_chunk.m_chunks)):
					mesh.f_chunk.m_chunks[face.materialIndex].faces.append(facenr)
				facenr+=1
			

		#fill in the UV info
		if blender_mesh.hasVertexUV():
			for vert in blender_mesh.verts:
				mesh.uv_chunk.uv.append((vert.uvco[0], vert.uvco[1]))

		elif blender_mesh.hasFaceUV():
			for face in valid_faces:
				# Tri or quad.
				for uv_coord in face.uv:
					mesh.uv_chunk.uv.append((uv_coord[0], uv_coord[1]))

		#filled in our mesh, lets add it to the file
		primary.obj_info.obj_chunks[len(primary.obj_info.obj_chunks)-1].mesh_chunks.append(mesh)

	#check the size
	primary.get_size()
	#open the files up for writing
	file = open( filename, "wb" )
	#recursively write the stuff to file
	primary.write(file)
	file.close()
	print "3ds export time: %.2f" % (Blender.sys.time() - time1)
	

Blender.Window.FileSelector(save_3ds, "Export 3DS", newFName('3ds'))
