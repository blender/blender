#!BPY

""" Registration info for Blender menus:
Name: 'DirectX'
Blender: 233
Group: 'Export'
Submenu: 'Mesh,armatures,animations' mesh
Tip: 'Export to DirectX8 text file format format.'
"""
# DirectX.py version 2.0
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

# This script export meshes created with Blender in DirectX8 file format
# it exports meshes,armatures,materials,normals,texturecoords and animations

# Grab the latest version here :www.omariben.too.it

import Blender
from Blender import Types, Object, NMesh, Material,Armature
from Blender.Mathutils import *

global bon_list, new_bon,mat_flip
bon_list = []
new_bon = {}
mat_flip = Matrix([1, 0, 0, 0], [0, 1, 0, 0], [0, 0, -1, 0], [0, 0, 0, 1])


#***********************************************
#***********************************************
#                EXPORTER
#***********************************************
#***********************************************

class xExport:
	def __init__(self, filename):
		self.file = open(filename, "w")

#*********************************************************************************************************************************************
	#***********************************************
	#Export Animation
	#***********************************************
	def exportMesh(self,armat,tex):
		global bon_list
		for name in Object.Get():
			obj = name.getData()
			if type(obj) == Types.NMeshType :		
				self.writeMeshcoord(name, obj,armat)
				self.writeMeshMaterialList(name, obj, tex)
				self.writeMeshNormals(name, obj)
				self.writeMeshTextureCoords(name, obj)
				self.writeSkinWeights(bon_list,obj)
				self.file.write(" }\n")
				self.file.write("}\n")
				self.writeAnimation(name, obj, bon_list,armat)
	#***********************************************
	#Export Root Bone
	#***********************************************
	def writeRootBone(self):
		global  bon_list, new_bon,mat_flip
		space = 0
		tex = []
		print "exporting ..."
		self.writeHeader()
		for name in Object.Get():
			obj = name.getData()
			if type(obj) == Types.NMeshType :
				self.writeTextures(name, tex)
			arm = name.getData()
			if type(arm) == Types.ArmatureType :
				Blender.Set('curframe',1)
				am_ob = Object.Get(name.name)
				mat_ob = mat_flip * am_ob.getMatrix()
				self.writeArmFrames(mat_ob, "RootFrame", 0)
				root_bon = arm.getBones()
				bon_list.append(root_bon[0])
				mat_r = self.writeCombineMatrix(root_bon[0])  
				name_r = root_bon[0].getName()
				new_bon[name_r] = len(root_bon[0].getChildren())
				self.writeArmFrames(mat_r, name_r, 1)
				self.writeListOfChildrens(root_bon[0],2)
				self.file.write("}\n")
				self.exportMesh(am_ob, tex)
		self.writeEnd()
	#***********************************************
	#Export Children Bones
	#***********************************************
	def writeListOfChildrens(self,bon,space):
		global bon_list, new_bon
		bon_c = bon.getChildren()
		Blender.Set('curframe',1)
		for n in range(len(bon_c)):
			name_h = bon_c[n].getName()
			chi_h = bon_c[n].getChildren()
			new_bon[name_h] = len(chi_h)

		if bon_c == [] :
			self.CloseBrackets(bon, new_bon, space, bon_list[0])
		
		for nch in range(len(bon_c)):
			bon_list.append(bon_c[nch])
			mat = self.writeCombineMatrix(bon_c[nch])
			name_ch = bon_c[nch].getName()
			self.writeArmFrames(mat, name_ch,space)
			self.findChildrens(bon_c[nch],space)
		
		
	#***********************************************
	#Create Children structure
	#***********************************************
	def CloseBrackets(self, bon, new_bon, space, root_bon):
		tab = "  "
		self.file.write("%s" % (tab * (space -1)))
		self.file.write("}\n")
		while bon.hasParent():
			if new_bon[bon.getName()] == 0:
				pare = bon.getParent()
				name_p = pare.getName()
				if new_bon[name_p] > 0:
					new_bon[name_p] = new_bon[name_p] - 1
				if new_bon[name_p] == 0 and pare != root_bon:
					self.file.write("%s" % (tab * (space-2)))
					self.file.write("}\n")
				space = space - 1
				bon = pare
			else:
				break
		
			
	#***********************************************
	#Create Children structure
	#***********************************************
	def findChildrens(self,bon_c,space):
		bon_cc = bon_c
		space += 1
		self.writeListOfChildrens(bon_cc,space)
	
	
	#***********************************************
	#Offset Matrix
	#***********************************************
	def writeMatrixOffset(self,bon):
		Blender.Set('curframe',1)
		mat_b = bon.getRestMatrix()       
		mat_b.invert() 
		return mat_b


	

	#***********************************************
	#Combine Matrix
	#***********************************************
	def writeCombineMatrix(self,bon):
		Blender.Set('curframe',1)
		mat_b = bon.getRestMatrix()     
		if bon.hasParent():
			pare = bon.getParent()
			mat_p = pare.getRestMatrix()
		else :
			mat_p = Matrix([1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 1, 0], [0, 0, 0, 1])
		mat_p.invert()
		mat_rb = mat_b * mat_p
		return mat_rb

	#***********************************************
	#Combine Matrix
	#***********************************************
	def writeCombineAnimMatrix(self,bon):
		
		mat_b = bon.getRestMatrix()     
		if bon.hasParent():
			pare = bon.getParent()
			mat_p = pare.getRestMatrix()
		else :
			mat_p = Matrix([1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 1, 0], [0, 0, 0, 1])
		mat_p.invert()
		mat_rb = mat_b * mat_p
		return mat_rb


#*********************************************************************************************************************************************
	#***********************************************
	#Write SkinWeights
	#***********************************************
	def writeSkinWeights(self, bon_list, mesh):
		global mat_dict
		Blender.Set('curframe',1)
		self.file.write("  XSkinMeshHeader {\n")
		max_infl = 0
		for bo in bon_list :
			name = bo.getName() 
			try :
				vertx_list = mesh.getVertsFromGroup(name,1)
				for inde in vertx_list :
					vert_infl = mesh.getVertexInfluences(inde[0])
					ln_infl = len(vert_infl)
					if ln_infl > max_infl :
						max_infl = ln_infl
				
			except:
				pass
		
		self.file.write("    %s; \n" % (max_infl))
		self.file.write("    %s; \n" % (max_infl * 3))
		self.file.write("    %s; \n" % (len(bon_list)))
		self.file.write("  }\n")
		
		for bo in bon_list :
			name = bo.getName() 
			try :
				vert_list = mesh.getVertsFromGroup(name,1)
				self.file.write("  SkinWeights {\n")
				self.file.write('    "%s"; \n' % (name))
				self.file.write('     %s; \n' % (len(vert_list)))
				count = 0
				for ind in vert_list :
					count += 1
					if count == len(vert_list):
						self.file.write("    %s; \n" % (ind[0]))
					else :
						self.file.write("    %s, \n" % (ind[0]))
				cou = 0
				for ind in vert_list :
					cou += 1
					ver_infl = mesh.getVertexInfluences(ind[0])
				
					len_infl = float(len(ver_infl))
					infl = 1 / len_infl
				
					if cou == len(vert_list):
						self.file.write("    %s; \n" % (round(infl,6)))
					else :
						self.file.write("    %s, \n" % (round(infl,6)))

			
				matx = self.writeMatrixOffset(bo)
			
				self.writeOffsFrames(matx, name, 1)
			except :
				pass
		self.file.write("  }\n")
		

	#***********************************************
	# Write Matrices
	#***********************************************
	def writeArmFrames(self, matx, name, space):
		tab = "  "
		self.file.write("%s" % (tab * space))
		self.file.write("Frame ")  
		self.file.write("%s {\n\n" % (name))
		self.file.write("%s" % (tab * space))
		self.file.write("  FrameTransformMatrix {\n")
		self.file.write("%s" % (tab * space))
		self.file.write("    %s,%s,%s,%s," %
							(round(matx[0][0],4),round(matx[0][1],4),round(matx[0][2],4),round(matx[0][3],4)))
		self.file.write("%s,%s,%s,%s," %
							(round(matx[1][0],4),round(matx[1][1],4),round(matx[1][2],4),round(matx[1][3],4)))	
		self.file.write("%s,%s,%s,%s," %
							(round(matx[2][0],4),round(matx[2][1],4),round(matx[2][2],4),round(matx[2][3],4)))
		self.file.write("%s,%s,%s,%s;;\n" %
							(round(matx[3][0],4),round(matx[3][1],4),round(matx[3][2],4),round(matx[3][3],6)))
		self.file.write("%s" % (tab * space))
		self.file.write("  }\n")
	
	#***********************************************
	# Write Matrices
	#***********************************************
	def writeOffsFrames(self, matx, name, space):
		tab = "  "
		self.file.write("%s" % (tab * space))
		self.file.write("    %s,%s,%s,%s," %
							(round(matx[0][0],4),round(matx[0][1],4),round(matx[0][2],4),round(matx[0][3],4)))
		self.file.write("%s,%s,%s,%s," %
							(round(matx[1][0],4),round(matx[1][1],4),round(matx[1][2],4),round(matx[1][3],4)))	
		self.file.write("%s,%s,%s,%s," %
							(round(matx[2][0],4),round(matx[2][1],4),round(matx[2][2],4),round(matx[2][3],4)))
		self.file.write("%s,%s,%s,%s;;\n" %
							(round(matx[3][0],4),round(matx[3][1],4),round(matx[3][2],4),round(matx[3][3],6)))
		self.file.write("%s" % (tab * space))
		self.file.write("  }\n")
	
	 
#*********************************************************************************************************************************************
	
	#***********************************************
	#HEADER
	#***********************************************  
	def writeHeader(self):
		self.file.write("xof 0303txt 0032\n\n\n")
		self.file.write("template VertexDuplicationIndices { \n\
 <b8d65549-d7c9-4995-89cf-53a9a8b031e3>\n\
 DWORD nIndices;\n\
 DWORD nOriginalVertices;\n\
 array DWORD indices[nIndices];\n\
}\n\
template XSkinMeshHeader {\n\
 <3cf169ce-ff7c-44ab-93c0-f78f62d172e2>\n\
 WORD nMaxSkinWeightsPerVertex;\n\
 WORD nMaxSkinWeightsPerFace;\n\
 WORD nBones;\n\
}\n\
template SkinWeights {\n\
 <6f0d123b-bad2-4167-a0d0-80224f25fabb>\n\
 STRING transformNodeName;\n\
 DWORD nWeights;\n\
 array DWORD vertexIndices[nWeights];\n\
 array float weights[nWeights];\n\
 Matrix4x4 matrixOffset;\n\
}\n\n")
		
	#***********************************************
	#CLOSE FILE
	#***********************************************
	def writeEnd(self):
		self.file.close()
		print "... finished"


	#***********************************************
	#EXPORT TEXTURES
	#***********************************************
	def writeTextures(self,name, tex):
		mesh = name.data
		for face in mesh.faces:
			if face.image and face.image.name not in tex:
				tex.append(face.image.name)
				


	#***********************************************
	#EXPORT MESH DATA
	#***********************************************
	def writeMeshcoord(self, name, mesh,armat):
		global mat_flip
	
		#ROTATION
		mat_ob = name.getMatrix() 
		mat_ar = armat.getInverseMatrix()
		mat_f = mat_ob * mat_ar
		self.writeArmFrames(mat_f, "body", 1)

		self.file.write("  Mesh object {\n")     
		numfaces=0
		#VERTICES NUMBER
		self.file.write("    %s;\n" % (len(mesh.verts)))
		#VERTICES COORDINATES
		numvert=0     
		for vertex in mesh.verts:
			numvert=numvert+1
			self.file.write("    %s; %s; %s;" % (round(vertex[0],6), round(vertex[1],6), round((vertex[2]),6)))
			if numvert == len(mesh.verts):
				self.file.write(";\n")
			else: 
				self.file.write(",\n")
		
		#FACES NUMBER
		numfaces=len(mesh.faces)
		self.file.write("    %s;\n" % (numfaces))   
		#FACES INDEX
		counter = 0
		for face in mesh.faces :
			counter += 1
			if counter == numfaces:
				if len(face.v) == 3:
					self.file.write("    3; %s, %s, %s;;\n\n" % (face[0].index, face[1].index, face[2].index))
				elif len(face.v) == 4:
					self.file.write("    4; %s, %s, %s, %s;;\n\n" % (face[0].index, face[1].index, face[2].index, face[3].index))
				elif len(face.v) == 2:
					print "WARNING:the mesh has faces with less then 3 vertices"
			else:
				if len(face.v) == 3:
					self.file.write("    3; %s, %s, %s;,\n" % (face[0].index, face[1].index, face[2].index))
				elif len(face.v) == 4 :
					self.file.write("    4; %s, %s, %s, %s;,\n" % (face[0].index, face[1].index, face[2].index, face[3].index))
				elif len(face.v) == 2:
					print "WARNING:the mesh has faces with less then 3 vertices"

		
		
	
		
	#***********************************************
	#MESH MATERIAL LIST
	#***********************************************
	def writeMeshMaterialList(self, name, obj, tex):
		self.file.write("  MeshMaterialList {\n")
		#HOW MANY MATERIALS ARE USED
		count = 0
		for mat in Material.Get():
			count+=1
		self.file.write("    %s;\n" % (len(tex) + count))
		#HOW MANY FACES IT HAS
		numfaces=len(obj.faces)
		self.file.write("    %s;\n" % (numfaces))
		##MATERIALS INDEX FOR EVERY FACE
		counter = 0
		for face in obj.faces :
			counter += 1
			mater = face.materialIndex
			if counter == numfaces:
				if face.image and face.image.name in tex :
					self.file.write("    %s;;\n" % (tex.index(face.image.name) + count))
				else :
					self.file.write("    %s;;\n" % (mater))
			else :
				if face.image and face.image.name in tex :
					self.file.write("    %s,\n" % (tex.index(face.image.name) + count))
				else :
					self.file.write("    %s,\n" % (mater))
			
		##MATERIAL NAME
		for mat in Material.Get():
			self.file.write("  Material")
			for a in range(0,len(mat.name)):
				if mat.name[a] == ".":
					print "WARNING:the material " + mat.name + " contains '.' within.Many viewers may refuse to read the exported file"
			self.file.write(" %s "% (mat.name))
			self.file.write("{\n")
			self.file.write("    %s; %s; %s;" % (mat.R, mat.G, mat.B))
			self.file.write("%s;;\n" % (mat.alpha))
			self.file.write("    %s;\n" % (mat.spec))
			self.file.write("    %s; %s; %s;;\n" % (mat.specR, mat.specG, mat.specB))
			self.file.write("    0.0; 0.0; 0.0;;\n")
			self.file.write("  TextureFilename {\n")
			self.file.write('    "none" ;')
			self.file.write("  }\n")
			self.file.write("  }\n") 
		
		for mat in tex:
			self.file.write("  Material Mat")
			self.file.write("%s "% (len(tex)))
			self.file.write("{\n")
			self.file.write("    1.0; 1.0; 1.0; 1.0;;\n")
			self.file.write("    1.0;\n")
			self.file.write("    1.0; 1.0; 1.0;;\n")
			self.file.write("    0.0; 0.0; 0.0;;\n")
			self.file.write("  TextureFilename {\n")
			self.file.write('    "%s" ;'% (face.image.name))
			self.file.write("  }\n")
			self.file.write("  }\n") 
		self.file.write("    }\n")

	#***********************************************
	#MESH NORMALS
	#***********************************************
	def writeMeshNormals(self,name,obj):
		self.file.write("  MeshNormals {\n")
		#VERTICES NUMBER
		numvert=len(obj.verts)
		self.file.write("    %s;\n" % (numvert))
		#VERTICES NORMAL
		counter = 0
		for vert in obj.verts:
			counter += 1  
			if counter == numvert:
				self.file.write("    %s; %s; %s;;\n" % ((round(vert.no[0],6)),(round(vert.no[1],6)),(round(vert.no[2],6))))
			else :
				self.file.write("    %s; %s; %s;,\n" % ((round(vert.no[0],6)), (round(vert.no[1],6)),(round(vert.no[2],6))))
		#FACES NUMBER 
		numfaces=len(obj.faces)
		self.file.write("    %s;\n" % (numfaces))  
		#FACES INDEX
		counter = 0
		for face in obj.faces :
			counter += 1
			if counter == numfaces:
				if len(face.v) == 3:
					self.file.write("    3; %s, %s, %s;;\n" % (face[0].index, face[1].index, face[2].index))
				elif len(face.v) == 4:
					self.file.write("    4; %s, %s, %s, %s;;\n" % (face[0].index, face[1].index, face[2].index, face[3].index))
			else:
				if len(face.v) == 3:
					self.file.write("    3; %s, %s, %s;,\n" % (face[0].index, face[1].index, face[2].index))
				elif len(face.v) == 4 :
					self.file.write("    4; %s, %s, %s, %s;,\n" % (face[0].index, face[1].index, face[2].index, face[3].index))
		self.file.write("}\n")
	#***********************************************
	#MESH TEXTURE COORDS
	#***********************************************
	def writeMeshTextureCoords(self, name, obj):
			uv_list = {}
			if obj.hasFaceUV():
				self.file.write("  MeshTextureCoords {\n")
				#VERTICES NUMBER
				mesh = name.data
				numvert = 0
				for face in mesh.faces:
					numvert = numvert + len(face.v)
				self.file.write("    %s;\n" % (len(mesh.verts)))
				#UV COORDS
				counter = -1
				for face in mesh.faces:
					counter += 1
					nrvx = len(face.uv)
					for n in range(nrvx) :
						uv_list[face.v[n].index] = face.uv[n][0], face.uv[n][1]
				for vert in mesh.verts:
					self.file.write("   %s;  %s;,\n" % (uv_list[vert.index][0],1 - (uv_list[vert.index][1])))
					
				self.file.write("}\n")
#***********************************************#***********************************************#***********************************************
	#***********************************************
	#FRAMES
	#***********************************************
	def writeFrames(self, matx):
		self.file.write("%s,%s,%s,%s," %
							(round(matx[0][0],4),round(matx[0][1],4),round(matx[0][2],4),round(matx[0][3],6)))
		self.file.write("%s,%s,%s,%s," %
							(round(matx[1][0],4),round(matx[1][1],4),round(matx[1][2],4),round(matx[1][3],6)))	
		self.file.write("%s,%s,%s,%s," %
							(round(matx[2][0],4),round(matx[2][1],4),round(matx[2][2],4),round(matx[2][3],6)))
		self.file.write("%s,%s,%s,%s;;" %
							(round(matx[3][0],4),round(matx[3][1],4),round(matx[3][2],4),round(matx[3][3],6)))
		
		
		
		
	#***********************************************
	#WRITE ANIMATION KEYS
	#***********************************************
	def writeAnimation(self, name, obj, bone_list, arm):
		self.file.write("AnimationSet {\n")
		startFr = Blender.Get('staframe')
		endFr = Blender.Get('endframe')
		for bon in bon_list :
			
			self.file.write(" Animation { \n")
			self.file.write("  {%s}\n" %(bon.getName()))
			self.file.write("  AnimationKey { \n")
			self.file.write("   4;\n")
			self.file.write("   %s; \n" % (endFr))

			self.file.write("   %s;" % (1))
			self.file.write("16;")
			mat = self.writeCombineMatrix(bon)
			self.writeFrames(mat)
			self.file.write(",\n")

			for fr in range(startFr+1,endFr + 1) :
				self.file.write("   %s;" % (fr))
				self.file.write("16;")
				Blender.Set('curframe',fr)
				
				mat_new = self.writeCombineAnimMatrix(bon)
				self.writeFrames(mat_new)
				
				if fr == endFr:
					self.file.write(";\n")
				else:
					self.file.write(",\n")
			self.file.write("   }\n")
			self.file.write(" }\n")
			self.file.write("\n")
		self.file.write("}\n")
		
#***********************************************#***********************************************#***********************************************
#***********************************************
# MAIN
#***********************************************

	
def my_callback(filename):
	if filename.find('.x', -2) <= 0: filename += '.x' # add '.x' if the user didn't
	xexport = xExport(filename)
	xexport.writeRootBone()
	

Blender.Window.FileSelector(my_callback, "Export DirectX8")	
