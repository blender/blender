#!BPY

""" Registration info for Blender menus:
Name: 'DirectX8 (.x)...'
Blender: 234
Group: 'Export'
Submenu: 'Export to DX8 file format' export
Submenu: 'How to use this exporter?' help
Tip: 'Export to DirectX8 text file format format.'
"""

# $Id$
#
# DirectX8Exporter.py version 1.0
# Copyright (C) 2003  Arben OMARI -- omariarben@everyday.com 
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

global new_bon,mat_flip,index_list
index_list = []
new_bon = {}
mat_flip = Matrix([1, 0, 0, 0], [0, 1, 0, 0], [0, 0, -1, 0], [0, 0, 0, 1])

def draw():
	
	# clearing screen
	Blender.BGL.glClearColor(0.5, 0.5, 0.5, 1)
	Blender.BGL.glColor3f(1.,1.,1.)
	Blender.BGL.glClear(Blender.BGL.GL_COLOR_BUFFER_BIT)
	
	# Buttons
	Blender.Draw.Button("Exit", 1, 10, 40, 100, 25)

	#Text
	Blender.BGL.glColor3f(1, 1, 1)
	Blender.BGL.glRasterPos2d(10, 310)
	Blender.Draw.Text("1.Only one mesh and one armature in the scene")
	Blender.BGL.glRasterPos2d(10, 290)
	Blender.Draw.Text("2.Before parenting set:")
 
	
	
	Blender.BGL.glRasterPos2d(10, 270)
	Blender.Draw.Text("     a)Armature and mesh must have the same origin location")
	Blender.BGL.glRasterPos2d(10, 255)
	Blender.Draw.Text("       (press N for both and set the same LocX,LocY and LocZ)")
	Blender.BGL.glRasterPos2d(10, 230)
	Blender.Draw.Text("      b)Armature and mesh must have the same to rotation")
	Blender.BGL.glRasterPos2d(10, 215)
	Blender.Draw.Text("        (select them and press Ctrl + A)")
	Blender.BGL.glRasterPos2d(10, 195)
	Blender.Draw.Text("3.Set the number of the animation frames to export ")
	Blender.BGL.glRasterPos2d(10, 175)
	Blender.Draw.Text("5.Read warnings in console(if any)")
	
	

def event(evt, val):
	if evt == Blender.Draw.ESCKEY and not val: Blender.Draw.Exit()

def bevent(evt):
	
	if evt == 1: Blender.Draw.Exit()
	
		

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
	def exportMesh(self,arm,arm_ob,tex):
		
		for name in Object.Get():
			obj = name.getData()
			if type(obj) == Types.NMeshType :		
				self.writeMeshcoord(name, obj,arm_ob)
				self.writeMeshMaterialList(name, obj, tex)
				self.writeMeshNormals(name, obj)
				self.writeMeshTextureCoords(name, obj)
				self.writeSkinWeights(arm,obj)
				self.file.write(" }\n")
				self.file.write("}\n")
				self.writeAnimation(name, obj,arm)
	#***********************************************
	#Export Root Bone
	#***********************************************
	def writeRootBone(self):
		global new_bon,mat_flip
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
				mat_r = self.writeCombineMatrix(root_bon[0])  
				name_r = root_bon[0].getName()
				new_bon[name_r] = len(root_bon[0].getChildren())
				self.writeArmFrames(mat_r, name_r, 1)
				self.writeListOfChildrens(root_bon[0],2,arm)
				self.file.write("}\n")
				self.exportMesh(arm,am_ob, tex)
		self.writeEnd()
	#***********************************************
	#Export Children Bones
	#***********************************************
	def writeListOfChildrens(self,bon,space,arm):
		global  new_bon
		bon_c = bon.getChildren()
		Blender.Set('curframe',1)
		for n in range(len(bon_c)):
			name_h = bon_c[n].getName()
			chi_h = bon_c[n].getChildren()
			new_bon[name_h] = len(chi_h)

		if bon_c == [] :
			self.CloseBrackets(bon, new_bon, space, arm.getBones()[0])
		
		for nch in range(len(bon_c)):
			mat = self.writeCombineMatrix(bon_c[nch])
			name_ch = bon_c[nch].getName()
			self.writeArmFrames(mat, name_ch,space)
			self.findChildrens(bon_c[nch],space,arm)
		
		
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
	def findChildrens(self,bon_c,space,arm):
		bon_cc = bon_c
		space += 1
		self.writeListOfChildrens(bon_cc,space,arm)
	
	
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
	def writeSkinWeights(self, arm, mesh):
		global index_list
		
		Blender.Set('curframe',1)
		self.file.write("  XSkinMeshHeader {\n")
		max_infl = 0
		for bo in arm.getBones() :
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
		self.file.write("    %s; \n" % (len(arm.getBones())))
		self.file.write("  }\n")
		
		for bo in arm.getBones() :
			bo_list = []
			weight_list = []
			name = bo.getName() 
			try :
				vert_list = mesh.getVertsFromGroup(name,1)
				le = 0
				for indx in vert_list:
					ver_infl = mesh.getVertexInfluences(indx[0])
					len_infl = float(len(ver_infl))
					infl = 1 / len_infl
					i = -1
					for el in index_list :
						i += 1
						if el == indx[0] :
							le +=1
							bo_list.append(i)
							weight_list.append(infl)


				self.file.write("  SkinWeights {\n")
				self.file.write('    "%s"; \n' % (name))
				self.file.write('     %s; \n' % (le))
				count = 0
				for ind in bo_list :
					count += 1
					if count == len(bo_list):
						self.file.write("    %s; \n" % (ind))
					else :
						self.file.write("    %s, \n" % (ind))
				cou = 0
				for wegh in weight_list :
					cou += 1
					
					if cou == len(weight_list):
						self.file.write("    %s; \n" % (round(wegh,6)))
					else :
						self.file.write("    %s, \n" % (round(wegh,6)))

			
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
		global index_list
		#ROTATION
		mat_ob = name.getMatrix() 
		mat_ar = armat.getInverseMatrix()
		mat_f = mat_ob * mat_ar
		self.writeArmFrames(mat_f, "body", 1)

		self.file.write("Mesh {\n")    
		numfaces=len(mesh.faces)
		#VERTICES NUMBER
		numvert = 0
		for face in mesh.faces:
			numvert = numvert + len(face.v)
		self.file.write("%s;\n" % (numvert))
		#VERTICES COORDINATES
		counter = 0
		for face in mesh.faces:
			counter += 1
			for n in range(len(face.v)):
				index_list.append(face.v[n].index)
				self.file.write("%s; %s; %s;" % (face.v[n].co[0], face.v[n].co[1], face.v[n].co[2]))
				if counter == numfaces :
					if n == len(face.v)-1 :
						self.file.write(";\n")
					else :
						self.file.write(",\n")
				else :
					self.file.write(",\n")

		#FACES NUMBER 
		self.file.write("%s;\n" % (numfaces))  
		#FACES INDEX
		numface=len(mesh.faces)
		coun,counter = 0, 0
		for face in mesh.faces :
			coun += 1
			if coun == numface:
				if len(face.v) == 3:
					self.file.write("3; %s, %s, %s;;\n" % (counter, counter + 2, counter + 1))
					counter += 3
				else :
					self.file.write("4; %s, %s, %s, %s;;\n" % (counter, counter + 3, counter + 2, counter + 1))
					counter += 4
			else:
				
				if len(face.v) == 3:
					self.file.write("3; %s, %s, %s;,\n" % (counter, counter + 2, counter + 1))
					counter += 3
				else :
					self.file.write("4; %s, %s, %s, %s;,\n" % (counter, counter + 3, counter + 2, counter + 1))
					counter += 4
		

		
	#***********************************************
	#VERTEX DUPLICATION INDEX
	#***********************************************
	def writeVertDupInd(self, mesh):
		self.file.write("  VertexDuplicationIndices {\n")
		numvert = 0
		numfaces=len(mesh.faces)
		for face in mesh.faces:
			numvert = numvert + len(face.v)
		self.file.write("   %s;\n" % (numvert+len(mesh.verts)))
		self.file.write("   %s;\n" % (len(mesh.verts)))
		#VERTICES INDEX
		cou = 0
		for vert in mesh.verts:
			cou += 1
			self.file.write("   %s" % ((vert.index)))
			if cou == len(mesh.verts):
				self.file.write(";\n")
			else:
				self.file.write(",\n")

		counter = 0
		for face in mesh.faces:
			counter += 1
			if counter == numfaces:
				if len(face.v) == 4:
					self.file.write("   %s,\n" % ((face.v[0].index)))
					self.file.write("   %s,\n" % ((face.v[1].index)))		
					self.file.write("   %s,\n" % ((face.v[2].index)))
					self.file.write("   %s;\n" % ((face.v[3].index)))
				elif len(face.v) == 3 :
					self.file.write("   %s,\n" % ((face.v[0].index)))
					self.file.write("   %s,\n" % ((face.v[1].index)))		
					self.file.write("   %s;\n" % ((face.v[2].index)))

			else :
				if len(face.v) == 4:
					self.file.write("   %s,\n" % ((face.v[0].index)))
					self.file.write("   %s,\n" % ((face.v[1].index)))		
					self.file.write("   %s,\n" % ((face.v[2].index)))
					self.file.write("   %s,\n" % ((face.v[3].index)))
				elif len(face.v) == 3 :
					self.file.write("   %s,\n" % ((face.v[0].index)))
					self.file.write("   %s,\n" % ((face.v[1].index)))		
					self.file.write("   %s,\n" % ((face.v[2].index)))
					
		self.file.write("    }\n")
		
		
		
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
	def writeMeshNormals(self,name,mesh):
		self.file.write("  MeshNormals {\n")
		#VERTICES NUMBER
		numvert = 0
		for face in mesh.faces:
			numvert = numvert + len(face.v)
		self.file.write("%s;\n" % (numvert))
		numfaces=len(mesh.faces)
		
		#VERTICES NORMAL
		counter = 0
		for face in mesh.faces:
			counter += 1  
			for n in range(len(face.v)):
				self.file.write("    %s; %s; %s;" % ((round(face.v[n].no[0],6)),(round(face.v[n].no[1],6)),(round(face.v[n].no[2],6))))
				if counter == numfaces :
					if n == len(face.v)-1 :
						self.file.write(";\n")
					else :
						self.file.write(",\n")
				else :
					self.file.write(",\n")
		


		#FACES NUMBER 
		self.file.write("%s;\n" % (numfaces))  
		coun,counter = 0, 0
		for face in mesh.faces :
			coun += 1
			if coun == numfaces:
				if len(face.v) == 3:
					self.file.write("3; %s, %s, %s;;\n" % (counter, counter + 2, counter + 1))
					counter += 3
				else :
					self.file.write("4; %s, %s, %s, %s;;\n" % (counter, counter + 3, counter + 2, counter + 1))
					counter += 4
			else:
				
				if len(face.v) == 3:
					self.file.write("3; %s, %s, %s;,\n" % (counter, counter + 2, counter + 1))
					counter += 3
				else :
					self.file.write("4; %s, %s, %s, %s;,\n" % (counter, counter + 3, counter + 2, counter + 1))
					counter += 4
		self.file.write("}\n")

	#***********************************************
	#MESH TEXTURE COORDS
	#***********************************************
	def writeMeshTextureCoords(self, name, mesh):
		if mesh.hasFaceUV():
			self.file.write("MeshTextureCoords {\n")
			#VERTICES NUMBER
			numvert = 0
			for face in mesh.faces:
				numvert += len(face.v)
			self.file.write("%s;\n" % (numvert))
			#UV COORDS
			numfaces = len(mesh.faces)
			counter = -1
			co = 0
			for face in mesh.faces:
				counter += 1
				co += 1
				for n in range(len(face.v)):
					self.file.write("%s;%s;" % (mesh.faces[counter].uv[n][0], -mesh.faces[counter].uv[n][1]))
					if co == numfaces :
						if n == len(face.v) - 1 :
							self.file.write(";\n")
						else :
							self.file.write(",\n")
					else :
						self.file.write(",\n")

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
	def writeAnimation(self, name, obj, arm):
		self.file.write("AnimationSet {\n")
		startFr = Blender.Get('staframe')
		endFr = Blender.Get('endframe')
		for bon in arm.getBones() :
			
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
	if filename.find('.x', -2) <= 0: filename += '.x' 
	xexport = xExport(filename)
	xexport.writeRootBone()


arg = __script__['arg']
if arg == 'help':
	Blender.Draw.Register(draw,event,bevent)
else:
	fname = Blender.sys.makename(ext = ".x")
	Blender.Window.FileSelector(my_callback, "Export DirectX8", fname)	
