#!BPY

""" Registration info for Blender menus:
Name: 'DirectX'
Blender: 232
Group: 'Export'
Submenu: 'Only mesh data...' mesh
Submenu: 'Animation(not armature yet)...' anim
Tip: 'Export to DirectX text file format format.'
"""
# DirectX.py version 1.0
# Copyright (C) 2003  Arben OMARI -- aromari@tin.it 
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

# This script export meshes created with Blender in DirectX file format
# it exports meshes,materials,normals,texturecoords and and animations

# Grab the latest version here :www.omariben.too.it

import Blender
from Blender import Types, Object, NMesh, Material
#import string
from math import *



	
#***********************************************
#***********************************************
#                EXPORTER
#***********************************************
#***********************************************

class xExport:
	def __init__(self, filename):
		self.file = open(filename, "w")

	#***********************************************
	#  Export animations
	#***********************************************
	def exportAnim(self):
		tex = []
		print "exporting ..."
		self.writeHeader()
		for name in Object.Get():
			obj = name.getData()
			if type(obj) == Types.NMeshType :
				self.writeMaterials(name,tex)	
				self.writeFrames(name, obj)	
				self.writeMeshcoord(name, obj )
				self.writeMeshMaterialList(name, obj, tex)
				self.writeMeshNormals(name, obj)
				self.writeMeshTextureCoords(name, obj)
				self.file.write("}\n")
				self.file.write("}\n")
				self.writeAnimation(name, obj)
			
		self.writeEnd()

	#***********************************************
	#  Export geometry
	#***********************************************
	def exportTex(self):
		tex = []
		print "exporting ..."
		self.writeHeader()
		for name in Object.Get():
			obj = name.getData()
			if type(obj) == Types.NMeshType :
				self.writeMaterials(name,tex)		
				self.writeMeshcoord(name, obj )
				self.writeMeshMaterialList(name, obj, tex)
				self.writeMeshNormals(name, obj)
				self.writeMeshTextureCoords(name, obj)
				self.file.write("}\n")

		self.writeEnd()
    
	#***********************************************
	#HEADER
	#***********************************************  
	def writeHeader(self):
		self.file.write("xof 0302txt 0064\n")
		self.file.write("\n")
		self.file.write("Header{\n")
		self.file.write("1;0;1;\n")
		self.file.write("}\n")

	#***********************************************
	#CLOSE FILE
	#***********************************************
	def writeEnd(self):
		self.file.close()
		print "... finished"
	#***********************************************
	#EXPORT MATERIALS
	#***********************************************
	def writeMaterials(self,name,tex):
		for mat in Material.Get():
			self.file.write("Material")
			self.file.write(" %s "% (mat.name))
			self.file.write("{\n")
			self.file.write("%s; %s; %s;" % (mat.R, mat.G, mat.B))
			self.file.write("%s;;\n" % (mat.alpha))
			self.file.write("%s;\n" % (mat.spec))
			self.file.write("%s; %s; %s;;\n" % (mat.specR, mat.specG, mat.specB))
			self.file.write("0.0; 0.0; 0.0;;\n")
			self.file.write("TextureFilename {\n")
			self.file.write('none ;')
			self.file.write("}\n")
			self.file.write("}\n") 
		self.writeTextures(name, tex)
		

	#***********************************************
	#EXPORT TEXTURES
	#***********************************************
	def writeTextures(self,name, tex):
		mesh = name.data
		for face in mesh.faces:
			if face.image and face.image.name not in tex:
				tex.append(face.image.name)
				self.file.write("Material Mat")
				self.file.write("%s "% (len(tex)))
				self.file.write("{\n")
				self.file.write("1.0; 1.0; 1.0; 1.0;;\n")
				self.file.write("1.0;\n")
				self.file.write("1.0; 1.0; 1.0;;\n")
				self.file.write("0.0; 0.0; 0.0;;\n")
				self.file.write("TextureFilename {\n")
				self.file.write('"%s" ;'% (face.image.name))
				self.file.write("}\n")
				self.file.write("}\n") 
		

	#***********************************************
	#EXPORT MESH DATA
	#***********************************************
	def writeMeshcoord(self, name, obj ):
		
		self.file.write("Mesh Mesh_%s {\n" % (name.name))    
		numfaces=len(obj.faces)
		#POSITION
		loc = name.getMatrix()
		x = loc[3][0]
		y = loc[3][1]
		z = loc[3][2]
		#VERTICES NUMBER
		mesh = name.data
		numvert = 0
		for face in mesh.faces:
			numvert = numvert + len(face.v)
		self.file.write("%s;\n" % (numvert))
		#VERTICES COORDINATES
		counter = 0
		for face in mesh.faces:
			counter += 1
			if counter == numfaces:
				if len(face.v) == 4:
					self.file.write("%s; %s; %s;,\n" % ((face.v[0].co[0] + x), face.v[0].co[1] + y, face.v[0].co[2] + z))
					self.file.write("%s; %s; %s;,\n" % ((face.v[1].co[0] + x), face.v[1].co[1] + y, face.v[1].co[2] + z))		
					self.file.write("%s; %s; %s;,\n" % ((face.v[2].co[0] + x), face.v[2].co[1] + y, face.v[2].co[2] + z))
					self.file.write("%s; %s; %s;;\n" % ((face.v[3].co[0] + x), face.v[3].co[1] + y, face.v[3].co[2] + z))
				elif len(face.v) == 3 :
					self.file.write("%s; %s; %s;,\n" % ((face.v[0].co[0] + x), face.v[0].co[1] + y, face.v[0].co[2] + z))
					self.file.write("%s; %s; %s;,\n" % ((face.v[1].co[0] + x), face.v[1].co[1] + y, face.v[1].co[2] + z))		
					self.file.write("%s; %s; %s;;\n" % ((face.v[2].co[0] + x), face.v[2].co[1] + y, face.v[2].co[2] + z))
					
			else :
				if len(face.v) == 4:
					self.file.write("%s; %s; %s;,\n" % ((face.v[0].co[0] + x), face.v[0].co[1] + y, face.v[0].co[2] + z))
					self.file.write("%s; %s; %s;,\n" % ((face.v[1].co[0] + x), face.v[1].co[1] + y, face.v[1].co[2] + z))		
					self.file.write("%s; %s; %s;,\n" % ((face.v[2].co[0] + x), face.v[2].co[1] + y, face.v[2].co[2] + z))
					self.file.write("%s; %s; %s;,\n" % ((face.v[3].co[0] + x), face.v[3].co[1] + y, face.v[3].co[2] + z))
				elif len(face.v) == 3:
					self.file.write("%s; %s; %s;,\n" % ((face.v[0].co[0] + x), face.v[0].co[1] + y, face.v[0].co[2] + z))
					self.file.write("%s; %s; %s;,\n" % ((face.v[1].co[0] + x), face.v[1].co[1] + y, face.v[1].co[2] + z))		
					self.file.write("%s; %s; %s;,\n" % ((face.v[2].co[0] + x), face.v[2].co[1] + y, face.v[2].co[2] + z))
					


		#FACES NUMBER 
		
		self.file.write("%s;\n" % (numfaces))  
		#FACES INDEX
		numface=len(obj.faces)
		coun,counter = 0, 0
		for face in mesh.faces :
			coun += 1
			if coun == numface:
				if len(face.v) == 3:
					self.file.write("3; %s; %s; %s;;\n" % (counter, counter + 1, counter + 2))
					counter += 3
				else :
					self.file.write("4; %s; %s; %s; %s;;\n" % (counter, counter + 1, counter + 2, counter + 3))
					counter += 4
			else:
				
				if len(face.v) == 3:
					self.file.write("3; %s; %s; %s;,\n" % (counter, counter + 1, counter + 2))
					counter += 3
				else :
					self.file.write("4; %s; %s; %s; %s;,\n" % (counter, counter + 1, counter + 2, counter + 3))
					counter += 4
		

		
		
		
		
	#***********************************************
	#MESH MATERIAL LIST
	#***********************************************
	def writeMeshMaterialList(self, name, obj, tex):
		self.file.write("//LET'S BEGIN WITH OPTIONAL DATA\n")
		self.file.write(" MeshMaterialList {\n")
		#HOW MANY MATERIALS ARE USED
		count = 0
		for mat in Material.Get():
			count+=1
		self.file.write("%s;\n" % (len(tex) + count))
		#HOW MANY FACES IT HAS
		numfaces=len(obj.faces)
		self.file.write("%s;\n" % (numfaces))
		##MATERIALS INDEX FOR EVERY FACE
		counter = 0
		for face in obj.faces :
			counter += 1
			mater = face.materialIndex
			if counter == numfaces:
				if face.image and face.image.name in tex :
					self.file.write("%s;;\n" % (tex.index(face.image.name) + count))
				else :
					self.file.write("%s;;\n" % (mater))
			else :
				if face.image and face.image.name in tex :
					self.file.write("%s,\n" % (tex.index(face.image.name) + count))
				else :
					self.file.write("%s,\n" % (mater))
			
		##MATERIAL NAME
		for mat in Material.Get():
			self.file.write("{%s}\n"% (mat.name))
		
		for mat in tex:
			self.file.write("{Mat")
			self.file.write("%s}\n"% (tex.index(mat) + 1))
		self.file.write("}\n")
	#***********************************************
	#MESH NORMALS
	#***********************************************
	def writeMeshNormals(self,name,obj):
		self.file.write(" MeshNormals {\n")
		#VERTICES NUMBER
		numvert=len(obj.verts)
		self.file.write("%s;\n" % (numvert))
		#VERTICES NORMAL
		counter = 0
		for vert in obj.verts:
			counter += 1  
			if counter == numvert:
				self.file.write("%s; %s; %s;;\n" % (vert.no[0], vert.no[1], vert.no[2]))
			else :
				self.file.write("%s; %s; %s;,\n" % (vert.no[0], vert.no[1], vert.no[2]))
		#FACES NUMBER 
		numfaces=len(obj.faces)
		self.file.write("%s;\n" % (numfaces))  
		#FACES INDEX
		counter = 0
		for face in obj.faces :
			counter += 1
			if counter == numfaces:
				if len(face.v) == 3:
					self.file.write("3; %s; %s; %s;;\n" % (face[0].index, face[1].index, face[2].index))
				elif len(face.v) == 4:
					self.file.write("4; %s; %s; %s; %s;;\n" % (face[0].index, face[1].index, face[2].index, face[3].index))
			else:
				if len(face.v) == 3:
					self.file.write("3; %s; %s; %s;,\n" % (face[0].index, face[1].index, face[2].index))
				elif len(face.v) == 4 :
					self.file.write("4; %s; %s; %s; %s;,\n" % (face[0].index, face[1].index, face[2].index, face[3].index))
		self.file.write("}\n")
	#***********************************************
	#MESH TEXTURE COORDS
	#***********************************************
	def writeMeshTextureCoords(self, name, obj):
			if obj.hasFaceUV():
				self.file.write("MeshTextureCoords {\n")
				#VERTICES NUMBER
				mesh = name.data
				numvert = 0
				for face in mesh.faces:
					numvert = numvert + len(face.v)
				self.file.write("%s;\n" % (numvert))
				#UV COORDS
				counter = -1
				for face in mesh.faces:
					counter += 1
					if len(face.v) == 4:
						self.file.write("%s;%s;,\n" % (mesh.faces[counter].uv[0][0], -mesh.faces[counter].uv[0][1]))
						self.file.write("%s;%s;,\n" % (mesh.faces[counter].uv[1][0], -mesh.faces[counter].uv[1][1]))
						self.file.write("%s;%s;,\n" % (mesh.faces[counter].uv[2][0], -mesh.faces[counter].uv[2][1]))
						self.file.write("%s;%s;,\n" % (mesh.faces[counter].uv[3][0], -mesh.faces[counter].uv[3][1]))
					elif len(face.v) == 3:
						self.file.write("%s;%s;,\n" % (mesh.faces[counter].uv[0][0], -mesh.faces[counter].uv[0][1]))
						self.file.write("%s;%s;,\n" % (mesh.faces[counter].uv[1][0], -mesh.faces[counter].uv[1][1]))
						self.file.write("%s;%s;,\n" % (mesh.faces[counter].uv[2][0], -mesh.faces[counter].uv[2][1]))

				self.file.write("}\n")

	#***********************************************
	#FRAMES
	#***********************************************
	def writeFrames(self, name, obj):
		matx = name.getMatrix()
		self.file.write("Frame Fr_")  
		self.file.write("%s {\n" % (obj.name))
		self.file.write(" FrameTransformMatrix {\n")
		self.file.write(" %s,%s,%s,%s,\n" %
							(round(matx[0][0],6),round(matx[0][1],6),round(matx[0][2],6),round(matx[0][3],6)))
		self.file.write(" %s,%s,%s,%s,\n" %
							(round(matx[1][0],6),round(matx[1][1],6),round(matx[1][2],6),round(matx[1][3],6)))
		self.file.write(" %s,%s,%s,%s,\n" %
							(round(matx[2][0],6),round(matx[2][1],6),round(matx[2][2],6),round(matx[2][3],6)))
		self.file.write(" %s,%s,%s,%s;;\n" %
							(round(matx[3][0],6),round(matx[3][1],6),round(matx[3][2],6),round(matx[3][3],6)))
		self.file.write(" }\n")
	#***********************************************
	#WRITE ANIMATION KEYS
	#***********************************************
	def writeAnimation(self, name, obj):
		startFr = Blender.Get('staframe')
		endFr = Blender.Get('endframe')
		self.file.write("AnimationSet animset_")
		self.file.write("%s {\n" % (obj.name))
		self.file.write(" Animation anim_")
		self.file.write("%s { \n" % (obj.name))
		self.file.write("  {Fr_")
		self.file.write("%s }\n" % (obj.name))
		self.file.write("   AnimationKey { \n")
		self.file.write("   0;\n")
		self.file.write("   %s; \n" % (endFr))
		for fr in range(startFr,endFr + 1) :
			self.file.write("   %s; " % (fr))
			self.file.write("4; ")
			Blender.Set('curframe',fr)
			rot = name.rot
			rot_x = rot[0]
			rot_y = rot[1]
			rot_z = rot[2]
			quat = self.euler2quat(rot_x,rot_y,rot_z)
			self.file.write("%s, %s, %s,%s;;" % 
							(quat[0],quat[1],quat[2],quat[3]))
			if fr == endFr:
				self.file.write(";\n")
			else:
				self.file.write(",\n")
		self.file.write("   }\n")
		self.file.write("   AnimationKey { \n")
		self.file.write("   2;\n")
		self.file.write("   %s; \n" % (endFr))
		for fr in range(startFr,endFr + 1) :
			self.file.write("   %s; " % (fr))
			self.file.write("3; ")
			Blender.Set('curframe',fr)
			loc = name.loc
			self.file.write("%s, %s, %s;;" %
							(loc[0],loc[1],loc[2]))
			if fr == endFr:
				self.file.write(";\n")
			else:
				self.file.write(",\n")
		self.file.write("   }\n")
		self.file.write("   AnimationKey { \n")
		self.file.write("   1;\n")
		self.file.write("   %s; \n" % (endFr))
		for fr in range(startFr,endFr + 1) :
			self.file.write("   %s; " % (fr))
			self.file.write("3; ")
			Blender.Set('curframe',fr)
			size = name.size
			self.file.write("%s, %s, %s;;" %
							(size[0],size[1],size[2]))
			if fr == endFr:
				self.file.write(";\n")
			else:
				self.file.write(",\n")
		self.file.write("   }\n")
		self.file.write("  }\n")
		self.file.write(" }\n")

	def euler2quat(self,rot_x,rot_y,rot_z):
		c_x = cos(rot_x / 2)
		c_y = cos(rot_y / 2)
		c_z = cos(rot_z / 2)

		s_x = sin(rot_x / 2)
		s_y = sin(rot_y / 2)
		s_z = sin(rot_z / 2)

		cy_cz = c_y * c_z
		sy_sz = s_y * s_z
 
		quat_w = c_x * cy_cz - s_x * sy_sz
		quat_x = s_x * cy_cz + c_x * sy_sz
		quat_y = c_x * s_y * c_z - s_x * c_y * s_z
		quat_z = c_x * c_y * s_z + s_x * s_y * c_z

		return(quat_w,quat_x,quat_y,quat_z)

#***********************************************
# MAIN
#***********************************************

	
def my_callback(filename):
	if filename.find('.x', -2) <= 0: filename += '.x' # add '.x' if the user didn't
	xexport = xExport(filename)
	arg = __script__['arg']
	if arg == 'anim':
		xexport.exportAnim()
	else:
		xexport.exportTex()

Blender.Window.FileSelector(my_callback, "Export DirectX")
