#!BPY

"""
# Name: 'DirectX (.x)...'
# Blender: 242
# Group: 'Export'
# Tooltip: 'Export to DirectX text file format format for XNA Animation Component Library.'
"""
__author__ = "vertex color exporting feature is added by mnemoto (original:minahito (original:Arben (Ben) Omari))"
__url__ = ("blender", "elysiun", "Adjuster's site http://sunday-lab.blogspot.com/, Author's site http://www.omariben.too.it","Adjuster's site http://ex.homeunix.net/")
__version__ = "3.1"

__bpydoc__ = """\
This script exports a Blender mesh with armature to DirectX 8's text file
format.

Notes:<br>
    Check author's site or the elYsiun forum for a new beta version of the
DX exporter.
"""
# DirectXExporter.py version 3.0
# Copyright (C) 2006  Arben OMARI -- omariarben@everyday.com 
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

# [Notice]
# This script is the custom version of Mr.Arben Omari's great work.
# If you have a question about the adjusted part, visit http://sunday-lab.blogspot.com/.

import Blender
from Blender import Types, Object, NMesh, Material,Armature,Mesh
from Blender.Mathutils import *
from Blender import Draw, BGL
from Blender.BGL import *
import math

global mat_flip,index_list,space,bone_list,mat_dict
global anim,flip_norm,swap_zy,flip_z,speed,ticks,no_light,recalc_norm,Bl_norm
bone_list =[]
index_list = []
mat_dict = {}
space = 0;flip_z = 1;anim=0;swap_yz=0;flip_norm=0;speed=0;ticks= 25
Bl_norm = 1;recalc_norm = 0;no_light = 0

toggle_val = 0
toggle1_val = 0
toggle2_val = 0
toggle3_val = 1
toggle4_val = 0
toggle5_val = 1
toggle6_val = 0
toggle7_val = 0
anim_tick = Draw.Create(25)

#***********************************************
# DirectX file spec only allows letters, digits, and 
# underscore in Names.
#***********************************************
def make_legal_name(starting_name):
    new_name = starting_name.replace('.','_')
    new_name = new_name.replace(' ','_')
    if new_name[0].isdigit():
        new_name = '_' + new_name
    return new_name

#***********************************************
# MAIN
#***********************************************

def my_callback(filename):
	if filename.find('.x', -2) <= 0: filename += '.x' 
	xexport = xExport(filename)
	xexport.SelectObjs()

def my_callback_sel(filename):
	if filename.find('.x', -2) <= 0: filename += '.x' 
	xexport = xExport(filename)
	xexport.exportSelMesh()
def event(evt, val):	
		if evt == Draw.ESCKEY:
			Draw.Exit()                
			return

def button_event(evt): 
	global toggle_val,toggle1_val,toggle2_val,toggle3_val,toggle4_val,toggle5_val,toggle6_val,toggle7_val
	global flip_z,swap_yz,flip_norm,anim,ticks,speed,no_light,Bl_norm,recalc_norm
	arg = __script__['arg']
	if evt == 1:
		toggle_val = 1 - toggle_val
		anim = toggle_val
		Draw.Redraw(1)
	if evt == 2:
		toggle1_val = 1 - toggle1_val
		flip_norm = toggle1_val
		Draw.Redraw(1)
	if evt == 3:
		toggle2_val = 1 - toggle2_val
		swap_yz = toggle2_val
		Draw.Redraw(1)
	if evt == 4:
		toggle3_val = 1 - toggle3_val
		flip_z = toggle3_val
		Draw.Redraw(1)
	if evt == 5:
		toggle4_val = 1 - toggle4_val
		speed = toggle4_val
		Draw.Redraw(1)
	if evt == 10:
		toggle5_val = 1 - toggle5_val
		if toggle5_val==1:
			toggle6_val = 0
			toggle7_val = 0
		else :
			toggle6_val = 1
			toggle7_val = 1
		no_light = toggle7_val
		recalc_norm = toggle6_val
		Bl_norm = toggle5_val
		Draw.Redraw(1)
	if evt == 11:
		toggle6_val = 1 - toggle6_val
		if toggle6_val==1:
			toggle5_val = 0
			toggle7_val = 0
		else :
			toggle5_val = 1
			toggle7_val = 1
		no_light = toggle7_val
		recalc_norm = toggle6_val
		Bl_norm = toggle5_val
		Draw.Redraw(1)
	if evt == 12:
		toggle7_val = 1 - toggle7_val
		if toggle7_val==1:
			toggle6_val = 0
			toggle5_val = 0
		else :
			toggle6_val = 1
			toggle5_val = 1
		no_light = toggle7_val
		recalc_norm = toggle6_val
		Bl_norm = toggle5_val
		Draw.Redraw(1)
	if evt == 6:
		ticks = anim_tick.val
	if evt == 7:
		fname = Blender.sys.makename(ext = ".x")
		Blender.Window.FileSelector(my_callback, "Export DirectX", fname)
	if evt == 8:
		fname = Blender.sys.makename(ext = ".x")
		Blender.Window.FileSelector(my_callback_sel, "Export DirectX", fname)
	if evt == 9:
		Draw.Exit()
		
	
def draw():
		global animsg,flipmsg,swapmsg,anim_tick
		global flip_z,swap_yz,flip_norm,anim,ticks,speed,recalc_norm,Bl_norm,no_light
		glClearColor(0.55,0.6,0.6,1)
		glClear(BGL.GL_COLOR_BUFFER_BIT)
		#external box
		glColor3f(0.2,0.3,0.3)
		rect(10,402,300,382)
		#--
		#glColor3f(0.3,0.4,0.4)
		#rect(11,399,298,398)
		#--
		glColor3f(0.5,0.75,0.65)
		rect(14,398,292,30)
		#--
		glColor3f(0.5,0.75,0.65)
		rect(14,366,292,160)
		#--
		glColor3f(0.5,0.75,0.65)
		rect(14,202,292,60)
		#--
		glColor3f(0.5,0.75,0.65)
		rect(14,138,292,40)
		#--
		glColor3f(0.5,0.75,0.65)
		rect(14,94,292,70)
		
		glColor3f(0.8,.8,0.6)
		glRasterPos2i(20, 380)
		Draw.Text("DirectX Exporter ",'large')
		Draw.Text("(for Blender 2.41)", 'small')
		#-------Aniamtion toggle---------------------------------------------
		Draw.Toggle("Anim", 1, 20, 330, 55, 20, toggle_val,"export animations")
		if toggle_val :
			anim = 1
			animsg = "animation will be exported"
		else:
			anim = 0
			animsg = "animation will be not exported"
		glRasterPos2i(100,335)
		Draw.Text(animsg)
		#---Flip normals toggle-----------------------------------------------
		Draw.Toggle("Flip norm", 2, 20, 300, 55, 20, toggle1_val,"invert normals")
		if toggle1_val :
			flip_norm = 1
			flipmsg = "flipped normals"
		else:
			flip_norm = 0
			flipmsg = "not flipped normals"
		glRasterPos2i(100,305)
		Draw.Text(flipmsg)
		#------Swap yz toggle----------------------------------------------------------------
		Draw.Toggle("Swap zy", 3, 20, 270, 55, 20, toggle2_val,"swap z,y axis(y up)")
		if toggle2_val :
			swap_yz = 1
			swapmsg = "Y-axis up"
		else:
			swap_yz = 0
			swapmsg = "Z-axis up"
		glRasterPos2i(100,275)
		Draw.Text(swapmsg)
		#------Flip z toggle----------------------------------------------------------------
		Draw.Toggle("Flip z", 4, 20, 240, 55, 20, toggle3_val,"flip z axis")
		if toggle3_val :
			flip_z = 1
			zmsg = "left handed system"
		else:
			flip_z = 0
			zmsg = "right handed system"
		glRasterPos2i(100,245)
		Draw.Text(zmsg)
		#------Speed toggle----------------------------------------------------------------
		Draw.Toggle("Speed", 5, 20, 210, 55, 20, toggle4_val,"Animation speed")
		if toggle4_val :
			speed = 1
			spedmsg = "set speed"
			anim_tick = Draw.Number("", 6,200, 210, 85, 20, anim_tick.val,1,100000,"ticks per second")
		else:
			speed = 0
			spedmsg = ""
		glRasterPos2i(100,215)
		Draw.Text(spedmsg)
		#------Blender Normals toggle----------------------------------------------------------------
		Draw.Toggle("Bl.normals", 10, 20, 105, 75, 25, toggle5_val,"export normals as in Blender")
		if toggle5_val :
			Bl_norm = 1
		#------Recalculute Normals toggle----------------------------------------------------------------
		Draw.Toggle("recalc.no", 11, 120, 105, 75, 25, toggle6_val,"export recalculated normals")
		if toggle6_val :
			recalc_norm = 1
		#------Recalculute Normals toggle----------------------------------------------------------------
		Draw.Toggle("no smooth", 12, 220, 105, 75, 25, toggle7_val,"every vertex has the face normal,no smoothing")
		if toggle7_val :
			no_light = 1
		#------Draw Button export----------------------------------------------------------------
		exp_butt = Draw.Button("Export All",7,20, 155, 75, 30, "export all the scene objects")
		sel_butt = Draw.Button("Export Sel",8,120, 155, 75, 30, "export the selected object")
		exit_butt = Draw.Button("Exit",9,220, 155, 75, 30, "exit")
		glRasterPos2i(20,75)
		Draw.Text("(C) 2006  Arben OMARI ")
		glRasterPos2i(20,55)
		Draw.Text("http://www.omariben.too.it")
		glRasterPos2i(20,35)
		Draw.Text("aromar@tin.it")
		
def rect(x,y,width,height):
		glBegin(GL_LINE_LOOP)
		glVertex2i(x,y)
		glVertex2i(x+width,y)
		glVertex2i(x+width,y-height)
		glVertex2i(x,y-height)
		glEnd()
		
def rectFill(x,y,width,height):
		glBegin(GL_POLYGON)
		glVertex2i(x,y)
		glVertex2i(x+width,y)
		glVertex2i(x+width,y-height)
		glVertex2i(x,y-height)
		glEnd()
		
		
		
Draw.Register(draw, event, button_event)


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
	#Select Scene objects
	#***********************************************
	def analyzeScene(self):
			parent_list = []
			for obj in Blender.Scene.GetCurrent().objects:
				if obj.type in ('Mesh', 'Armature', 'Empty'):
					if obj.parent == None :
						parent_list.append(obj)
						
			return parent_list
		
	def getChildren(self,obj):	
		obs = Blender.Scene.GetCurrent().objects
		return [ ob for ob in obs if ob.parent == obj ]
	
	def getArmChildren(self,obj):		
		for ob in Blender.Scene.GetCurrent().objects: #Object.Get():
			if ob.parent == obj :
				return ob
	
	def getLocMat(self, obj):
		pare = obj.parent
		mat = obj.matrixWorld
		mat_id = Matrix([1,0,0,0],[0,1,0,0],[0,0,1,0],[0,0,0,1])
		if pare:
			mat_p = pare.matrixWorld
			mat_c = Matrix(mat_p)
			mat_c.invert()
			mat_f = mat * mat_c
		else :
			mat_id.invert()
			mat_f = mat * mat_id
		return mat_f
	
	def writeObjFrames(self,obj):
		global space,chld_obj,ch_list
		mesh = obj.getData()
		if obj.type == "Empty" :
			mat = self.getLocMat(obj)
			mat_c = Matrix(mat)
			self.writeArmFrames(mat_c, make_legal_name(obj.name))
		if type(mesh) == Types.ArmatureType :
			Child_obj = self.getArmChildren(obj)
			chld_obj = obj
			ch_list.append(Child_obj)
			self.writeRootBone(obj, Child_obj)	
		if obj.type == 'Mesh' and obj not in ch_list:
			self.exportMesh(obj)
			
			
	def writeChildObj(self,obj):
		global space,ch_list
		space += 1
		if obj :
			for ob in obj:
				if ob not in ch_list:
					self.writeObjFrames(ob)
					ch_list.append(ob)
					ch_ob = self.getChildren(ob)
					self.writeChildObj(ch_ob)
					self.closeBrackets()
					self.file.write(" // End of the Object %s \n" % (ob.name))
					
					
	def writeRootFrame(self):
		global flip_z,swap_yz,speed
		if speed:
			self.writeAnimTicks()
		if flip_z:
			mat_flip = Matrix([1, 0, 0, 0], [0, 1, 0, 0], [0, 0, -1, 0], [0, 0, 0, 1])
		else :
			mat_flip = Matrix([1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 1, 0], [0, 0, 0, 1])
		if swap_yz :
			mat_rot = RotationMatrix(-90, 4, 'x')
			mat_flip = mat_rot * mat_flip
		self.writeArmFrames(mat_flip, "RootFrame")		
			
	##################################################################	
	def SelectObjs(self):
		global space,chld_obj,ch_list,flip_z,swap_yz,speed
		print "exporting..."
		self.writeHeader()
		self.writeRootFrame()
		obj_list = self.analyzeScene()
		space += 1
		ch_list = []
		for obj in obj_list:
			self.writeObjFrames(obj)
			ch_l = self.getChildren(obj)
			for ch in ch_l:
			
			
				if ch and ch.type == "Armature":
					ch_list.append(ch)
					self.writeObjFrames(ch)
				else :	
					self.writeChildObj(ch_l)
			if obj.type != "Armature":
				self.file.write("  }  // SI End of the Object %s \n" % (obj.name))	
				
				
				
		self.file.write("}  // End of the Root Frame\n")		
		if anim :
			self.file.write("AnimationSet AnimationSet0 {\n")
			for obj in Blender.Scene.GetCurrent().objects:
				if obj.type in ('Mesh', 'Empty'):
					ip_list = obj.ipo
					if ip_list != None :
						self.writeAnimationObj(obj)
				elif obj.type == 'Armature':
					act_list = obj.getAction()
					if act_list != None :
						self.writeAnimation(obj)
					#ip_list = obj.ipo
					#if ip_list != None :
					#	self.writeAnimationObj(obj)

			self.file.write("} // End of Animation Set\n")
		self.writeEnd()
		#######################################################
		
				
	def writeAnimTicks(self):
		global ticks
		self.file.write("AnimTicksPerSecond {\n")
		self.file.write("%d; \n" % (ticks))
		self.file.write("}\n")
	
	#***********************************************
	#Export Mesh without Armature
	#***********************************************
	def exportMesh(self, obj):
		tex = []
		mesh = obj.getData()
		self.writeTextures(obj, tex)		
		self.writeMeshcoordArm(obj, arm_ob = None)
		self.writeMeshMaterialList(obj, mesh, tex)
		self.writeMeshNormals(obj, mesh)
		self.writeMeshTextureCoords(obj, mesh)
		self.writeMeshVertexColors(obj, mesh)
		self.file.write("  }  // End of the Mesh %s \n" % (obj.name))
		
					
	#***********************************************
	#Export the Selected Mesh
	#***********************************************
	def exportSelMesh(self):
		print "exporting ..."
		self.writeHeader()
		self.writeRootFrame()
		tex = []
		objs = Object.GetSelected()
		for obj in objs:
			if obj.type == 'Mesh':
				mesh = obj.data
				self.writeTextures(obj, tex)		
				self.writeMeshcoordArm(obj, arm_ob = None)
				self.writeMeshMaterialList(obj, mesh, tex)
				self.writeMeshNormals(obj, mesh)
				self.writeMeshTextureCoords(obj, mesh)
				self.writeMeshVertexColors(obj, mesh)
				self.file.write(" }\n")
				self.file.write("}\n")
				ind = objs.index(obj)
				if ind == len(objs)-1:
					self.file.write("}\n")
				ip_list = obj.ipo
				if ip_list != None :
					self.file.write("AnimationSet AnimationSet0 {\n")
					self.writeAnimationObj(obj)
					self.file.write("}\n")
			else :
				print "The selected object is not a mesh"
		print "...finished"
	#***********************************************
	#Export Mesh with Armature
	#***********************************************
	def exportMeshArm(self,arm,arm_ob,ch_obj):
		tex = []
		mesh = ch_obj.getData()
		self.writeTextures(ch_obj, tex)		
		self.writeMeshcoordArm(ch_obj ,arm_ob)
		self.writeMeshMaterialList(ch_obj, mesh, tex)
		self.writeMeshNormals(ch_obj, mesh)
		self.writeMeshTextureCoords(ch_obj, mesh)
		self.writeSkinWeights(arm,mesh)
		#self.file.write("  }  // End of the Frame %s \n" % (ch_obj.name))
		self.file.write("  }  // End of the Object %s \n" % (ch_obj.name))
				
	#***********************************************
	#Export Root Bone
	#***********************************************
	def writeRootBone(self, chld_obj,  child_obj):
		global space,root_bon
		arms = chld_obj.getData()
		mat_arm = self.getLocMat(chld_obj)
		for bon in arms.bones.values():
			if bon.hasParent():
				pass
			else:
				root_bon = bon
		space += 1
		mat_r = self.writeAnimCombineMatrix(root_bon,1)
		self.writeArmFrames(mat_r, make_legal_name(root_bon.name))
		
		bon_c = root_bon.children
		self.writeChildren(bon_c)
		self.file.write("  }  // End of the Bone %s \n" % (root_bon.name))	
		self.exportMeshArm(arms, chld_obj ,child_obj)
			
	#***********************************************
	#Create Children structure
	#***********************************************
	def writeBon(self,bon):
		global space
		mat_r = self.writeAnimCombineMatrix(bon,1)
		self.writeArmFrames(mat_r, make_legal_name(bon.name))
		
	
	def writeChildren(self,bon_c):
		global space,bone_list
		space += 1
		if bon_c:
			for bo in bon_c:
				if bo.name not in bone_list:
					self.writeBon(bo)
					bone_list.append(bo.name)
					bo_c = bo.children 
					self.writeChildren(bo_c)
					self.closeBrackets()
				
				
					
	def closeBrackets(self):
		global space
		space = space-1
		tab = "  "
		self.file.write("%s" % (tab * space))
		self.file.write("}\n")
		
		
			
	#***********************************************
	#Offset Matrix
	#***********************************************
	def writeMatrixOffset(self,bon):
		global  chld_obj
		Blender.Set('curframe', 1)
		pose = chld_obj.getPose()
		pos_b = pose.bones[bon.name]
		mat_b = pos_b.poseMatrix
		mat_c = Matrix(mat_b)
		mat_c.invert()
		return mat_c

	
	#***********************************************
	#Combine Matrix
	#***********************************************
	def writeCombineMatrix(self,bon):
		global  chld_obj
		
		Blender.Set('curframe', 1)
		pose = chld_obj.getPose()
		pos_b = pose.bones[bon.name]
		mat_b = pos_b.poseMatrix
		if bon.hasParent():
			pare = bon.parent
			pos_p = pose.bones[pare.name]
			mat_p = pos_p.poseMatrix
			
		else:
			mat_p = Matrix([1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 1, 0], [0, 0, 0, 1])
		mat_c = Matrix(mat_p)
		mat_c.invert()
		mat_f = mat_b * mat_c
		
		return mat_f
	#***********************************************
	#Combine Matrix
	#***********************************************
	def writeAnimCombineMatrix(self,bon,fre):
		global  chld_obj
		Blender.Set('curframe', fre)
		pose = chld_obj.getPose()
		pos_b = pose.bones[bon.name]
		mat_b = pos_b.poseMatrix
		if bon.hasParent():
			pare = bon.parent
			pos_p = pose.bones[pare.name]
			mat_p = pos_p.poseMatrix
			
		else:
			mat_p = Matrix([1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 1, 0], [0, 0, 0, 1])
		mat_c = Matrix(mat_p)
		mat_c.invert()
		mat_f = mat_b * mat_c
		
		return mat_f


#*********************************************************************************************************************************************
	#***********************************************
	#Write SkinWeights
	#***********************************************
	def writeSkinWeights(self, arm, mesh):
		global index_list
		v_dict = {}
		Blender.Set('curframe',1)
		self.file.write("  XSkinMeshHeader {\n")
		max_infl = 0
		for bo in arm.bones.values() :
			name = bo.name
			try :
				vertx_list = mesh.getVertsFromGroup(name,1)
				for inde in vertx_list :
					vert_infl = mesh.getVertexInfluences(inde[0])
					ln_infl = len(vert_infl)
					if ln_infl > max_infl :
						max_infl = ln_infl
				
			except:
				pass
		
		self.file.write("    %d; \n" % (max_infl))
		self.file.write("    %d; \n" % (max_infl * 3))
		self.file.write("    %d; \n" % (len(arm.bones.values())))
		self.file.write("  }\n")
		
		for bo in arm.bones.values() :
			bo_list = []
			weight_list = []
			name = bo.name 
			f_name = make_legal_name(name)
			try :
				vert_list = mesh.getVertsFromGroup(name,1)
				le = 0
				for indx in vert_list:
					ver_infl = mesh.getVertexInfluences(indx[0])
					infl = 0.0
					if len(ver_infl) != 0:
						sum = 0.0
						for bone_n, weight in ver_infl:
							if bone_n == name:
								infl = weight
							sum += weight
						infl /= sum
					
					i = -1
					for el in index_list :
						i += 1
						if el == indx[0] :
							le +=1
							bo_list.append(i)
							weight_list.append(infl)


				self.file.write("  SkinWeights {\n")
				self.file.write('    "%s"; \n' % (f_name))
				self.file.write('     %d; \n' % (le))
				count = 0
				for ind in bo_list :
					count += 1
					if count == len(bo_list):
						self.file.write("    %d; \n" % (ind))
					else :
						self.file.write("    %d, \n" % (ind))
				cou = 0
				for wegh in weight_list :
					cou += 1
					
					if cou == len(weight_list):
						self.file.write("    %f; \n" % (round(wegh,6)))
					else :
						self.file.write("    %f, \n" % (round(wegh,6)))

			
				matx = self.writeMatrixOffset(bo)
				self.writeOffsFrames(matx, name)
			except :
				pass
		self.file.write("  } // End of XSkinMeshHeader\n")
		

	#***********************************************
	# Write Matrices
	#***********************************************
	def writeArmFrames(self, matx, name):
		global space
		tab = "  "
		self.file.write("%s" % (tab * space))
		self.file.write("Frame ")  
		self.file.write("%s {\n\n" % (name))
		self.file.write("%s" % (tab * space))
		self.file.write("  FrameTransformMatrix {\n")
		self.writeFrame(matx)
	
	#***********************************************
	# Write Frames
	#***********************************************
	def writeOffsFrames(self, matx, name):
		space = 1
		self.writeFrame(matx)
	
	 #***********************************************
	# Write Frames
	#***********************************************
	def writeFrame(self, matx):
		tab = "  "
		self.file.write("%s" % (tab * space))
		self.file.write("    %f,%f,%f,%f,\n" %
							(round(matx[0][0],4),round(matx[0][1],4),round(matx[0][2],4),round(matx[0][3],4)))
		self.file.write("%s" % (tab * space))
		self.file.write("    %f,%f,%f,%f,\n" %
							(round(matx[1][0],4),round(matx[1][1],4),round(matx[1][2],4),round(matx[1][3],4)))
		self.file.write("%s" % (tab * space))	
		self.file.write("    %f,%f,%f,%f,\n" %
							(round(matx[2][0],4),round(matx[2][1],4),round(matx[2][2],4),round(matx[2][3],4)))
		self.file.write("%s" % (tab * space))
		self.file.write("    %f,%f,%f,%f;;\n" %
							(round(matx[3][0],4),round(matx[3][1],4),round(matx[3][2],4),round(matx[3][3],4)))
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
	#EXPORT MESH DATA with Armature
	#***********************************************
	def writeMeshcoordArm(self, obj ,arm_ob):
		global index_list,flip_z
		#TransformMatrix
		mat = self.getLocMat(obj)
		self.writeArmFrames(mat, make_legal_name(obj.name))
		mesh = NMesh.GetRawFromObject(obj.name)
		self.file.write("Mesh {\n")    
		numface=len(mesh.faces)
		#VERTICES NUMBER
		numvert = 0
		for face in mesh.faces:
			numvert = numvert + len(face.v)
		self.file.write("%d;\n" % (numvert))
		if numvert == 0:
			print "Mesh named",mesh.name,"has no vertices.Problems may occur using the .x file"
		#VERTICES COORDINATES
		counter = 0
		for face in mesh.faces:
			counter += 1
			for n in range(len(face.v)):
				index_list.append(face.v[n].index)
				vec_vert = Vector([(face.v[n].co[0]), face.v[n].co[1], face.v[n].co[2], 1])
				if arm_ob :
					f_vec_vert = vec_vert * mat
				else :
					f_vec_vert = vec_vert
				self.file.write("%f; %f; %f;" % (round(f_vec_vert[0],4), round(f_vec_vert[1],4), round(f_vec_vert[2],4)))
				if counter == numface :
					if n == len(face.v)-1 :
						self.file.write(";\n")
					else :
						self.file.write(",\n")
				else :
					self.file.write(",\n")
		if flip_z:
			a3 = 0;b3 = 2;c3 = 1
			a4 = 0;b4 = 3;c4 = 2;d4 = 1
		else:
			a3 = 0;b3 = 1;c3 = 2
			a4 = 0;b4 = 1;c4 = 2;d4 = 3

		#FACES NUMBER 
		self.file.write("%s;\n" % (numface))  
		coun,counter = 0, 0
		for face in mesh.faces :
			coun += 1
			separator = ','
			if coun == numface:
			    separator = ';'
			if len(face.v) == 3:
				self.file.write("3; %d, %d, %d;%c\n" % (counter + a3, counter + b3, counter + c3, separator))
				counter += 3
			elif len(face.v) == 4:
				self.file.write("4; %d, %d, %d, %d;%c\n" % (counter + a4, counter + b4, counter + c4, counter + d4, separator))
				counter += 4
			elif len(face.v) < 3:
				print "WARNING:the mesh has faces with less then 3 vertices"
				print "        It my be not exported correctly."

	
	#***********************************************
	#MESH MATERIAL LIST
	#***********************************************
	def writeMeshMaterialList(self, obj, mesh, tex):
		self.file.write("  MeshMaterialList {\n")
		#HOW MANY MATERIALS ARE USED
		count = 0
		for mat in mesh.getMaterials():
			count+=1
		self.file.write("    %d;\n" % (len(tex) + count))
		#HOW MANY FACES IT HAS
		numfaces=len(mesh.faces)
		self.file.write("    %d;\n" % (numfaces))
		##MATERIALS INDEX FOR EVERY FACE
		counter = 0
		for face in mesh.faces :
			counter += 1
			mater = face.materialIndex
			if counter == numfaces:
				if face.image and face.image.name in tex :
					self.file.write("    %d;;\n" % (tex.index(face.image.name) + count))
				else :
					self.file.write("    %d;;\n" % (mater))
			else :
				if face.image and face.image.name in tex :
					self.file.write("    %d,\n" % (tex.index(face.image.name) + count))
				else :
					self.file.write("    %d,\n" % (mater))
			
		##MATERIAL NAME
		for mat in mesh.getMaterials():
			self.file.write("  Material")
			self.file.write(" %s "% (make_legal_name(mat.name)))
			self.file.write("{\n")
			self.file.write("    %f; %f; %f;" % (mat.R, mat.G, mat.B))
			self.file.write("%s;;\n" % (mat.alpha))
			self.file.write("    %f;\n" % (mat.spec))
			self.file.write("    %f; %f; %f;;\n" % (mat.specR, mat.specG, mat.specB))
			self.file.write("    0.0; 0.0; 0.0;;\n")
			self.file.write("  }  //End of Material\n") 
		
		for mat in tex:
			self.file.write("  Material Mat")
			self.file.write("%s "% (len(tex)))
			self.file.write("{\n")
			self.file.write("    1.0; 1.0; 1.0; 1.0;;\n")
			self.file.write("    1.0;\n")
			self.file.write("    1.0; 1.0; 1.0;;\n")
			self.file.write("    0.0; 0.0; 0.0;;\n")
			self.file.write("  TextureFilename {")
			self.file.write('    "%s";'% (mat))
			self.file.write("  }\n")
			self.file.write("  }  // End of Material\n")
		self.file.write("    }  //End of MeshMaterialList\n")

	#***********************************************
	#MESH NORMALS
	#***********************************************
	def writeMeshNormals(self,name,mesh):
		global flip_norm,flip_z,no_light,recalc_norm,Bl_norm
		
		self.file.write("  MeshNormals {\n")
		#VERTICES NUMBER
		numvert = 0
		for face in mesh.faces:
			numvert = numvert + len(face.v)
		self.file.write("%d;\n" % (numvert))
		numfaces=len(mesh.faces)
		if flip_norm :
			fl = -1
		else :
			fl = 1
		#VERTICES NORMAL
		if Bl_norm:
			self.writeBlenderNormals(mesh,fl)
		if recalc_norm:
			self.writeRecalcNormals(mesh,fl)	
		if no_light:
			self.writeNoSmothing(mesh,fl)
						
		
		
		if flip_z:
			a3 = 0;b3 = 2;c3 = 1
			a4 = 0;b4 = 3;c4 = 2;d4 = 1
		else:
			a3 = 0;b3 = 1;c3 = 2
			a4 = 0;b4 = 1;c4 = 2;d4 = 3

		#FACES NUMBER 
		self.file.write("%s;\n" % (numfaces))  
		coun,counter = 0, 0
		for face in mesh.faces :
			coun += 1
			if coun == numfaces:
				if len(face.v) == 3:
					self.file.write("3; %d, %d, %d;;\n" % (counter + a3, counter + b3, counter + c3))
					counter += 3
				else :
					self.file.write("4; %d, %d, %d, %d;;\n" % (counter + a4, counter + b4, counter + c4, counter + d4))
					counter += 4
			else:
				
				if len(face.v) == 3:
					self.file.write("3; %d, %d, %d;,\n" % (counter + a3, counter + b3, counter + c3))
					counter += 3
				else :
					self.file.write("4; %d, %d, %d, %d;,\n" % (counter + a4, counter + b4, counter + c4, counter + d4))
					counter += 4
		self.file.write("}  //End of MeshNormals\n")
		
	def writeBlenderNormals(self,mesh,fl):
			numfaces=len(mesh.faces)
			#VERTICES NORMAL
			counter = 0
			for face in mesh.faces:
				counter += 1  
				for n in range(len(face.v)):
					self.file.write("    %f; %f; %f;" % (
									(round(face.v[n].no[0],6)*fl),(round(face.v[n].no[1],6)*fl),(round(face.v[n].no[2],6)*fl)))
					if counter == numfaces :
						if n == len(face.v)-1 :
							self.file.write(";\n")
						else :
							self.file.write(",\n")
					else :
						self.file.write(",\n")
						
	def writeRecalcNormals(self,mesh,fl):
		numfaces=len(mesh.faces)
		normal_list = {}
		idx = 0
		for vertex in mesh.verts:
			v_norm = Vector([0, 0, 0])
			normal_list[idx] = v_norm
			idx += 1
			for face in mesh.faces:
				for verts in face.v:
					if verts.index == vertex.index :
							v_norm[0] += face.no[0]
							v_norm[1] += face.no[1]
							v_norm[2] += face.no[2]
			
			v_norm.normalize()
						
		counter = 0
		for face in mesh.faces:
				counter += 1 
				n = 0 
				for vert in face.v:
					n += 1
					norm = normal_list[vert.index]
					
					self.file.write("    %f; %f; %f;" % (
									(round(norm[0],6)*fl),(round(norm[1],6)*fl),(round(norm[2],6)*fl)))		
					if counter == numfaces :
						if n == len(face.v) :
							self.file.write(";\n")
						else :
							self.file.write(",\n")
					else :
						self.file.write(",\n")
						
	def writeNoSmothing(self,mesh,fl):
		numfaces=len(mesh.faces)
		counter = 0
		for face in mesh.faces:
				counter += 1 
				n = 0 
				for n in range(len(face.v)):
					n += 1
					self.file.write("    %f; %f; %f;" % (
									(round(face.no[0],6)*fl),(round(face.no[1],6)*fl),(round(face.no[2],6)*fl)))
					
							
					if counter == numfaces :
						if n == len(face.v) :
							self.file.write(";\n")
						else :
							self.file.write(",\n")
					else :
						self.file.write(",\n")
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
			self.file.write("%d;\n" % (numvert))
			#UV COORDS
			numfaces = len(mesh.faces)
			counter = -1
			co = 0
			for face in mesh.faces:
				counter += 1
				co += 1
				for n in range(len(face.v)):
					self.file.write("%f;%f;" % (mesh.faces[counter].uv[n][0], -mesh.faces[counter].uv[n][1]))
					if co == numfaces :
						if n == len(face.v) - 1 :
							self.file.write(";\n")
						else :
							self.file.write(",\n")
					else :
						self.file.write(",\n")

			self.file.write("}  //End of MeshTextureCoords\n")

	#***********************************************
	#MESH VORTEX COLORS
	#***********************************************
	def writeMeshVertexColors(self, name, mesh):
		if mesh.hasVertexColours():
			self.file.write("MeshVertexColors {\n")
			#VERTICES NUMBER
			numvert = reduce( lambda i,f: len(f)+i, mesh.faces, 0)
			self.file.write("%d;\n" % (numvert))
			#VERTEX COLORS
			
			vcounter =0
			for f in mesh.faces:
				col = f.col
				for i,c in enumerate(col):
					# Note vcol alpha has no meaning
					self.file.write("%d;%f;%f;%f;%f;" % (vcounter,c.r/255.0, c.g/255.0, c.b/255.0, 1.0)) # c.a/255.0))
					vcounter+=1
					if vcounter == numvert :
						self.file.write(";\n")
					else :
						self.file.write(",\n")

			self.file.write("}  //End of MeshVertexColors\n")

#***********************************************#***********************************************#***********************************************
	#***********************************************
	#FRAMES
	#***********************************************
	def writeFrames(self, matx):
		
		self.file.write("%f,%f,%f,%f," %
							(round(matx[0][0],4),round(matx[0][1],4),round(matx[0][2],4),round(matx[0][3],4)))
		self.file.write("%f,%f,%f,%f," %
							(round(matx[1][0],4),round(matx[1][1],4),round(matx[1][2],4),round(matx[1][3],4)))	
		self.file.write("%f,%f,%f,%f," %
							(round(matx[2][0],4),round(matx[2][1],4),round(matx[2][2],4),round(matx[2][3],4)))
		self.file.write("%f,%f,%f,%f;;" %
							(round(matx[3][0],4),round(matx[3][1],4),round(matx[3][2],4),round(matx[3][3],4)))
		
		
		
	
	
	#***********************************************
	#WRITE ANIMATION KEYS
	#***********************************************
	def writeAnimation(self,arm_ob):
		global mat_dict, root_bon
		arm = arm_ob.getData()
		act_list = arm_ob.getAction()
		ip = act_list.getAllChannelIpos()
		for bon in arm.bones.values() :
			point_list = []
			name = bon.name
			name_f = make_legal_name(name)
			try :
				ip_bon_channel = ip[bon.name]
				ip_bon_name = ip_bon_channel.getName()
				
				ip_bon = Blender.Ipo.Get(ip_bon_name)
				poi = ip_bon.getCurves()
				
				for po in poi[3].getPoints():
					a = po.getPoints()
					point_list.append(int(a[0]))
				#point_list.pop(0) 
				
				self.file.write(" Animation { \n")
				self.file.write("  { %s }\n" %(name_f))
				self.file.write("  AnimationKey { \n")
				self.file.write("   4;\n")
				self.file.write("   %d; \n" % (len(point_list)))

				for fr in point_list:
					
					if name == root_bon.name :
						
						
						mat_b = self.writeAnimCombineMatrix(bon,fr)
						mat_arm = self.getLocMat(arm_ob)
						mat = mat_b * mat_arm
					else:	
						mat = self.writeAnimCombineMatrix(bon,fr)
						
					self.file.write("   %d;" % (fr))
					self.file.write("16;")
				
					self.writeFrames(mat)
				
					if fr == point_list[len(point_list)-1]:
						self.file.write(";\n")
					else:
						self.file.write(",\n")
				self.file.write("   }\n")
				self.file.write(" }\n")
				self.file.write("\n")
			except:
				pass
		
		

	#***********************************************
	#WRITE ANIMATION KEYS
	#***********************************************
	def writeAnimationObj(self, obj):
		point_list = []
		ip = obj.ipo
		poi = ip.getCurves()
		for po in poi[0].getPoints():
			a = po.getPoints()
			point_list.append(int(a[0]))
		
		self.file.write(" Animation {\n")
		self.file.write("  { ")
		self.file.write("%s }\n" % (make_legal_name(obj.name)))
		self.file.write("   AnimationKey { \n")
		self.file.write("   4;\n")
		self.file.write("   %d; \n" % (len(point_list)))
		for fr in point_list:
			self.file.write("   %d;" % (fr))
			self.file.write("16;")
			Blender.Set('curframe',fr)
				
			#mat_new = self.getLocMat(obj) 
			mat_new = obj.matrixLocal 
			self.writeFrames(mat_new)

			if fr == point_list[len(point_list)-1]:
				self.file.write(";\n")
			else:
				self.file.write(",\n")
		self.file.write("   }\n")
		self.file.write("  }\n")

	
		
#***********************************************#***********************************************#***********************************************



	
	
