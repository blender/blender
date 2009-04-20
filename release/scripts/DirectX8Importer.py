#!BPY

""" Registration info for Blender menus:
Name: 'DirectX(.x)...'
Blender: 244
Group: 'Import'

Tip: 'Import from DirectX text file format format.'
"""
# DirectXImporter.py version 1.2
# Copyright (C) 2005  Arben OMARI -- omariarben@everyday.com 
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

# This script import meshes from DirectX text file format

# Grab the latest version here :www.omariben.too.it
import bpy
import Blender
from Blender import Mesh,Object,Material,Texture,Image,Draw


class xImport:
	def __init__(self, filename):
		global my_path
		self.file = open(filename, "r")
		my_path = Blender.sys.dirname(filename)

		# 
		self.lines = [l_split for l in self.file.readlines() for l_split in (' '.join(l.split()),) if l_split]

	def Import(self):
		lines = self.lines
		print "importing into Blender ..."
		scene  = bpy.data.scenes.active
		
		mesh_indicies = {} # the index of each 'Mesh' is used as the key for those meshes indicies
		context_indicies = None # will raise an error if used!
		
		
		#Get the line of Texture Coords
		nr_uv_ind = 0

		#Get Materials
		nr_fac_mat = 0
		i = -1
		mat_list = []
		tex_list = []
		mesh_line_indicies = []
		for j, line in enumerate(lines):
			l = line.strip()
			words = line.split()
			if words[0] == "Material" :
				#context_indicies["Material"] = j
				self.loadMaterials(j, mat_list, tex_list)
			elif words[0] == "MeshTextureCoords" :
				context_indicies["MeshTextureCoords"] = j
				#nr_uv_ind = j
			elif words[0] == "MeshMaterialList" :
				context_indicies["MeshMaterialList"] = j+2
				#nr_fac_mat = j + 2
			elif words[0] == "Mesh": # Avoid a second loop
				context_indicies = mesh_indicies[j] = {'MeshTextureCoords':0, 'MeshMaterialList':0}
		
		for mesh_index, value in mesh_indicies.iteritems():
			mesh = Mesh.New()
			self.loadVertices(mesh_index, mesh, value['MeshTextureCoords'], value['MeshMaterialList'], tex_list)
			
			mesh.materials = mat_list[:16]
			if value['MeshMaterialList']:
				self.loadMeshMaterials(value['MeshMaterialList'], mesh)
			scene.objects.new(mesh)
			
		self.file.close()
		print "... finished"

	#------------------------------------------------------------------------------
	#        CREATE THE MESH
	#------------------------------------------------------------------------------
	def loadVertices(self, nr_vr_ind, mesh, nr_uv, nr_fac_mat, tex_list):
		v_ind = nr_vr_ind + 1
		lin = self.lines[v_ind]
		if lin :
			lin_c = self.CleanLine(lin)
			nr_vert = int((lin_c.split()[0]))
		else :
			v_ind = nr_vr_ind + 2
			lin = self.lines[v_ind]
			lin_c = self.CleanLine(lin)
			nr_vert = int((lin_c.split()[0]))

		#--------------------------------------------------
		nr_fac_li = v_ind + nr_vert +1
		lin_f = self.lines[nr_fac_li]
		if lin_f :
			lin_fc = self.CleanLine(lin_f)
			nr_face = int((lin_fc.split()[0]))
		else :
			nr_fac_li = v_ind + nr_vert +1
			lin_f = self.lines[nr_fac_li]
			lin_fc = self.CleanLine(lin_f)
			nr_face = int((lin_fc.split()[0]))

		#Get Coordinates
		verts_list = [(0,0,0)] # WARNING - DUMMY VERT - solves EEKADOODLE ERROR
		for l in xrange(v_ind + 1, (v_ind + nr_vert +1)):
			line_v = self.lines[l]
			lin_v = self.CleanLine(line_v)
			words = lin_v.split()
			if len(words)==3:
				verts_list.append((float(words[0]),float(words[1]),float(words[2])))
		
		mesh.verts.extend(verts_list)
		del verts_list
		
		face_list = []
		#Make Faces
		i = 0
		mesh_verts = mesh.verts
		for f in xrange(nr_fac_li + 1, (nr_fac_li + nr_face + 1)):
			i += 1
			line_f = self.lines[f]
			lin_f = self.CleanLine(line_f)
			
			# +1 for dummy vert only!
			words = lin_f.split()
			if len(words) == 5:
				face_list.append((1+int(words[1]), 1+int(words[2]), 1+int(words[3]), 1+int(words[4])))
			elif len(words) == 4:
				face_list.append((1+int(words[1]), 1+int(words[2]), 1+int(words[3])))
		
		mesh.faces.extend(face_list)
		del face_list
		
		if nr_uv :
			mesh.faceUV = True
			for f in mesh.faces:
				fuv = f.uv
				for ii, v in enumerate(f):
					# _u, _v = self.CleanLine(self.lines[nr_uv + 2 + v.index]).split()
					
					# Use a dummy vert
					_u, _v = self.CleanLine(self.lines[nr_uv + 1 + v.index]).split()
					
					fuv[ii].x = float(_u)
					fuv[ii].y = float(_v)
			
				if nr_fac_mat :
					fac_line = self.lines[nr_fac_mat + i]
					fixed_fac = self.CleanLine(fac_line)
					w_tex = int(fixed_fac.split()[0])
					f.image = tex_list[w_tex]
					
		# remove dummy vert
		mesh.verts.delete([0,])
		
	def CleanLine(self,line):
		return line.replace(\
			";", " ").replace(\
			'"', ' ').replace(\
			"{", " ").replace(\
			"}", " ").replace(\
			",", " ").replace(\
			"'", " ")

	#------------------------------------------------------------------
	# CREATE MATERIALS
	#------------------------------------------------------------------
	def loadMaterials(self, nr_mat, mat_list, tex_list):
		
		def load_image(name):
			try:
				return Image.Load(Blender.sys.join(my_path,name))
			except:
				return None
		
		mat = bpy.data.materials.new()
		line = self.lines[nr_mat + 1]
		fixed_line = self.CleanLine(line)
		words = fixed_line.split()
		mat.rgbCol = [float(words[0]),float(words[1]),float(words[2])]
		mat.setAlpha(float(words[3]))
		mat_list.append(mat)
		l = self.lines[nr_mat + 5]
		fix_3_line = self.CleanLine(l)
		tex_n = fix_3_line.split()
		
		if tex_n and tex_n[0] == "TextureFilename" :

			if len(tex_n) > 1:
				tex_list.append(load_image(tex_n[1]))

			if len(tex_n) <= 1 :

				l_succ = self.lines[nr_mat + 6]
				fix_3_succ = self.CleanLine(l_succ)
				tex_n_succ = fix_3_succ.split()
				tex_list.append(load_image(tex_n_succ[0]))
		else :
			tex_list.append(None) # no texture for this index

		return mat_list, tex_list
	#------------------------------------------------------------------
	# SET MATERIALS
	#------------------------------------------------------------------
	def loadMeshMaterials(self, nr_fc_mat, mesh):
		for face in mesh.faces:
			nr_fc_mat += 1
			line = self.lines[nr_fc_mat]
			fixed_line = self.CleanLine(line)
			wrd = fixed_line.split()
			mat_idx = int(wrd[0])
			face.mat = mat_idx

#------------------------------------------------------------------
#  MAIN
#------------------------------------------------------------------
def my_callback(filename):
	if not filename.lower().endswith('.x'): print "Not an .x file" 
	ximport = xImport(filename)
	ximport.Import()

arg = __script__['arg']

if __name__ == '__main__':
	Blender.Window.FileSelector(my_callback, "Import DirectX", "*.x")

#my_callback('/fe/x/directxterrain.x')
#my_callback('/fe/x/Male_Normal_MAX.X')
#my_callback('/fe/x/male_ms3d.x')
