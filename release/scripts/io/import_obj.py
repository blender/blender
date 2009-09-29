#!BPY
 
"""
Name: 'Wavefront (.obj)...'
Blender: 249
Group: 'Import'
Tooltip: 'Load a Wavefront OBJ File, Shift: batch import all dir.'
"""

__author__= "Campbell Barton", "Jiri Hnidek", "Paolo Ciccone"
__url__= ['http://wiki.blender.org/index.php/Scripts/Manual/Import/wavefront_obj', 'blender.org', 'blenderartists.org']
__version__= "2.13"

__bpydoc__= """\
This script imports a Wavefront OBJ files to Blender.

Usage:
Run this script from "File->Import" menu and then load the desired OBJ file.
Note, This loads mesh objects and materials only, nurbs and curves are not supported.
"""

# ***** BEGIN GPL LICENSE BLOCK *****
#
# Script copyright (C) Campbell J Barton 2007-2009
# - V2.12- bspline import/export added (funded by PolyDimensions GmbH)
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

from Blender import Mesh, Draw, Window, Texture, Material, sys
import bpy
import BPyMesh
import BPyImage
import BPyMessages

try:		import os
except:		os= False

# Generic path functions
def stripFile(path):
	'''Return directory, where the file is'''
	lastSlash= max(path.rfind('\\'), path.rfind('/'))
	if lastSlash != -1:
		path= path[:lastSlash]
	return '%s%s' % (path, sys.sep)

def stripPath(path):
	'''Strips the slashes from the back of a string'''
	return path.split('/')[-1].split('\\')[-1]

def stripExt(name): # name is a string
	'''Strips the prefix off the name before writing'''
	index= name.rfind('.')
	if index != -1:
		return name[ : index ]
	else:
		return name
# end path funcs



def line_value(line_split):
	'''
	Returns 1 string represneting the value for this line
	None will be returned if theres only 1 word
	'''
	length= len(line_split)
	if length == 1:
		return None
	
	elif length == 2:
		return line_split[1]
	
	elif length > 2:
		return ' '.join( line_split[1:] )

def obj_image_load(imagepath, DIR, IMAGE_SEARCH):
	'''
	Mainly uses comprehensiveImageLoad
	but tries to replace '_' with ' ' for Max's exporter replaces spaces with underscores.
	'''
	
	if '_' in imagepath:
		image= BPyImage.comprehensiveImageLoad(imagepath, DIR, PLACE_HOLDER= False, RECURSIVE= IMAGE_SEARCH)
		if image: return image
		# Did the exporter rename the image?
		image= BPyImage.comprehensiveImageLoad(imagepath.replace('_', ' '), DIR, PLACE_HOLDER= False, RECURSIVE= IMAGE_SEARCH)
		if image: return image
	
	# Return an image, placeholder if it dosnt exist
	image= BPyImage.comprehensiveImageLoad(imagepath, DIR, PLACE_HOLDER= True, RECURSIVE= IMAGE_SEARCH)
	return image
	

def create_materials(filepath, material_libs, unique_materials, unique_material_images, IMAGE_SEARCH):
	'''
	Create all the used materials in this obj,
	assign colors and images to the materials from all referenced material libs
	'''
	DIR= stripFile(filepath)
	
	#==================================================================================#
	# This function sets textures defined in .mtl file                                 #
	#==================================================================================#
	def load_material_image(blender_material, context_material_name, imagepath, type):
		
		texture= bpy.data.textures.new(type)
		texture.setType('Image')
		
		# Absolute path - c:\.. etc would work here
		image= obj_image_load(imagepath, DIR, IMAGE_SEARCH)
		has_data = image.has_data
		texture.image = image

		if not has_data:
			try:
				# first time using this image. We need to load it first
				image.glLoad()
			except:
				# probably the image is crashed
				pass
			else:
				has_data = image.has_data
		
		# Adds textures for materials (rendering)
		if type == 'Kd':
			if has_data and image.depth == 32:
				# Image has alpha
				blender_material.setTexture(0, texture, Texture.TexCo.UV, Texture.MapTo.COL | Texture.MapTo.ALPHA)
				texture.setImageFlags('MipMap', 'InterPol', 'UseAlpha')
				blender_material.mode |= Material.Modes.ZTRANSP
				blender_material.alpha = 0.0
			else:
				blender_material.setTexture(0, texture, Texture.TexCo.UV, Texture.MapTo.COL)
				
			# adds textures to faces (Textured/Alt-Z mode)
			# Only apply the diffuse texture to the face if the image has not been set with the inline usemat func.
			unique_material_images[context_material_name]= image, has_data # set the texface image
		
		elif type == 'Ka':
			blender_material.setTexture(1, texture, Texture.TexCo.UV, Texture.MapTo.CMIR) # TODO- Add AMB to BPY API
			
		elif type == 'Ks':
			blender_material.setTexture(2, texture, Texture.TexCo.UV, Texture.MapTo.SPEC)
		
		elif type == 'Bump':
			blender_material.setTexture(3, texture, Texture.TexCo.UV, Texture.MapTo.NOR)		
		elif type == 'D':
			blender_material.setTexture(4, texture, Texture.TexCo.UV, Texture.MapTo.ALPHA)				
			blender_material.mode |= Material.Modes.ZTRANSP
			blender_material.alpha = 0.0
			# Todo, unset deffuse material alpha if it has an alpha channel
			
		elif type == 'refl':
			blender_material.setTexture(5, texture, Texture.TexCo.UV, Texture.MapTo.REF)		
	
	
	# Add an MTL with the same name as the obj if no MTLs are spesified.
	temp_mtl= stripExt(stripPath(filepath))+ '.mtl'
	
	if sys.exists(DIR + temp_mtl) and temp_mtl not in material_libs:
			material_libs.append( temp_mtl )
	del temp_mtl
	
	#Create new materials
	for name in unique_materials: # .keys()
		if name != None:
			unique_materials[name]= bpy.data.materials.new(name)
			unique_material_images[name]= None, False # assign None to all material images to start with, add to later.
		
	unique_materials[None]= None
	unique_material_images[None]= None, False
	
	for libname in material_libs:
		mtlpath= DIR + libname
		if not sys.exists(mtlpath):
			#print '\tError Missing MTL: "%s"' % mtlpath
			pass
		else:
			#print '\t\tloading mtl: "%s"' % mtlpath
			context_material= None
			mtl= open(mtlpath, 'rU')
			for line in mtl: #.xreadlines():
				if line.startswith('newmtl'):
					context_material_name= line_value(line.split())
					if unique_materials.has_key(context_material_name):
						context_material = unique_materials[ context_material_name ]
					else:
						context_material = None
				
				elif context_material:
					# we need to make a material to assign properties to it.
					line_split= line.split()
					line_lower= line.lower().lstrip()
					if line_lower.startswith('ka'):
						context_material.setMirCol((float(line_split[1]), float(line_split[2]), float(line_split[3])))
					elif line_lower.startswith('kd'):
						context_material.setRGBCol((float(line_split[1]), float(line_split[2]), float(line_split[3])))
					elif line_lower.startswith('ks'):
						context_material.setSpecCol((float(line_split[1]), float(line_split[2]), float(line_split[3])))
					elif line_lower.startswith('ns'):
						context_material.setHardness( int((float(line_split[1])*0.51)) )
					elif line_lower.startswith('ni'): # Refraction index
						context_material.setIOR( max(1, min(float(line_split[1]), 3))) # Between 1 and 3
					elif line_lower.startswith('d') or line_lower.startswith('tr'):
						context_material.setAlpha(float(line_split[1]))
						context_material.mode |= Material.Modes.ZTRANSP
					elif line_lower.startswith('map_ka'):
						img_filepath= line_value(line.split())
						if img_filepath:
							load_material_image(context_material, context_material_name, img_filepath, 'Ka')
					elif line_lower.startswith('map_ks'):
						img_filepath= line_value(line.split())
						if img_filepath:
							load_material_image(context_material, context_material_name, img_filepath, 'Ks')
					elif line_lower.startswith('map_kd'):
						img_filepath= line_value(line.split())
						if img_filepath:
							load_material_image(context_material, context_material_name, img_filepath, 'Kd')
					elif line_lower.startswith('map_bump'):
						img_filepath= line_value(line.split())
						if img_filepath:
							load_material_image(context_material, context_material_name, img_filepath, 'Bump')
					elif line_lower.startswith('map_d') or line_lower.startswith('map_tr'): # Alpha map - Dissolve
						img_filepath= line_value(line.split())
						if img_filepath:
							load_material_image(context_material, context_material_name, img_filepath, 'D')
					
					elif line_lower.startswith('refl'): # Reflectionmap
						img_filepath= line_value(line.split())
						if img_filepath:
							load_material_image(context_material, context_material_name, img_filepath, 'refl')
			mtl.close()



	
def split_mesh(verts_loc, faces, unique_materials, filepath, SPLIT_OB_OR_GROUP, SPLIT_MATERIALS):
	'''
	Takes vert_loc and faces, and seperates into multiple sets of 
	(verts_loc, faces, unique_materials, dataname)
	This is done so objects do not overload the 16 material limit.
	'''
	
	filename = stripExt(stripPath(filepath))
	
	if not SPLIT_OB_OR_GROUP and not SPLIT_MATERIALS:
		# use the filename for the object name since we arnt chopping up the mesh.
		return [(verts_loc, faces, unique_materials, filename)]
	
	
	def key_to_name(key):
		# if the key is a tuple, join it to make a string
		if type(key) == tuple:
			return '%s_%s' % key
		elif not key:
			return filename # assume its a string. make sure this is true if the splitting code is changed
		else:
			return key
	
	# Return a key that makes the faces unique.
	if SPLIT_OB_OR_GROUP and not SPLIT_MATERIALS:
		def face_key(face):
			return face[4] # object
	
	elif not SPLIT_OB_OR_GROUP and SPLIT_MATERIALS:
		def face_key(face):
			return face[2] # material
	
	else: # Both
		def face_key(face):
			return face[4], face[2] # object,material		
	
	
	face_split_dict= {}
	
	oldkey= -1 # initialize to a value that will never match the key
	
	for face in faces:
		
		key= face_key(face)
		
		if oldkey != key:
			# Check the key has changed.
			try:
				verts_split, faces_split, unique_materials_split, vert_remap= face_split_dict[key]
			except KeyError:
				faces_split= []
				verts_split= []
				unique_materials_split= {}
				vert_remap= [-1]*len(verts_loc)
				
				face_split_dict[key]= (verts_split, faces_split, unique_materials_split, vert_remap)
			
			oldkey= key
			
		face_vert_loc_indicies= face[0]
		
		# Remap verts to new vert list and add where needed
		for enum, i in enumerate(face_vert_loc_indicies):
			if vert_remap[i] == -1:
				new_index= len(verts_split)
				vert_remap[i]= new_index # set the new remapped index so we only add once and can reference next time.
				face_vert_loc_indicies[enum] = new_index # remap to the local index
				verts_split.append( verts_loc[i] ) # add the vert to the local verts 
				
			else:
				face_vert_loc_indicies[enum] = vert_remap[i] # remap to the local index
			
			matname= face[2]
			if matname and not unique_materials_split.has_key(matname):
				unique_materials_split[matname] = unique_materials[matname]
		
		faces_split.append(face)
	
	
	# remove one of the itemas and reorder
	return [(value[0], value[1], value[2], key_to_name(key)) for key, value in face_split_dict.iteritems()]


def create_mesh(scn, new_objects, has_ngons, CREATE_FGONS, CREATE_EDGES, verts_loc, verts_tex, faces, unique_materials, unique_material_images, unique_smooth_groups, vertex_groups, dataname):
	'''
	Takes all the data gathered and generates a mesh, adding the new object to new_objects
	deals with fgons, sharp edges and assigning materials
	'''
	if not has_ngons:
		CREATE_FGONS= False
	
	if unique_smooth_groups:
		sharp_edges= {}
		smooth_group_users= dict([ (context_smooth_group, {}) for context_smooth_group in unique_smooth_groups.iterkeys() ])
		context_smooth_group_old= -1
	
	# Split fgons into tri's
	fgon_edges= {} # Used for storing fgon keys
	if CREATE_EDGES:
		edges= []
	
	context_object= None
	
	# reverse loop through face indicies
	for f_idx in xrange(len(faces)-1, -1, -1):
		
		face_vert_loc_indicies,\
		face_vert_tex_indicies,\
		context_material,\
		context_smooth_group,\
		context_object= faces[f_idx]
		
		len_face_vert_loc_indicies = len(face_vert_loc_indicies)
		
		if len_face_vert_loc_indicies==1:
			faces.pop(f_idx)# cant add single vert faces
		
		elif not face_vert_tex_indicies or len_face_vert_loc_indicies == 2: # faces that have no texture coords are lines
			if CREATE_EDGES:
				# generators are better in python 2.4+ but can't be used in 2.3
				# edges.extend( (face_vert_loc_indicies[i], face_vert_loc_indicies[i+1]) for i in xrange(len_face_vert_loc_indicies-1) )
				edges.extend( [(face_vert_loc_indicies[i], face_vert_loc_indicies[i+1]) for i in xrange(len_face_vert_loc_indicies-1)] )

			faces.pop(f_idx)
		else:
			
			# Smooth Group
			if unique_smooth_groups and context_smooth_group:
				# Is a part of of a smooth group and is a face
				if context_smooth_group_old is not context_smooth_group:
					edge_dict= smooth_group_users[context_smooth_group]
					context_smooth_group_old= context_smooth_group
				
				for i in xrange(len_face_vert_loc_indicies):
					i1= face_vert_loc_indicies[i]
					i2= face_vert_loc_indicies[i-1]
					if i1>i2: i1,i2= i2,i1
					
					try:
						edge_dict[i1,i2]+= 1
					except KeyError:
						edge_dict[i1,i2]=  1
			
			# FGons into triangles
			if has_ngons and len_face_vert_loc_indicies > 4:
				
				ngon_face_indices= BPyMesh.ngon(verts_loc, face_vert_loc_indicies)
				faces.extend(\
				[(\
				[face_vert_loc_indicies[ngon[0]], face_vert_loc_indicies[ngon[1]], face_vert_loc_indicies[ngon[2]] ],\
				[face_vert_tex_indicies[ngon[0]], face_vert_tex_indicies[ngon[1]], face_vert_tex_indicies[ngon[2]] ],\
				context_material,\
				context_smooth_group,\
				context_object)\
				for ngon in ngon_face_indices]\
				)
				
				# edges to make fgons
				if CREATE_FGONS:
					edge_users= {}
					for ngon in ngon_face_indices:
						for i in (0,1,2):
							i1= face_vert_loc_indicies[ngon[i  ]]
							i2= face_vert_loc_indicies[ngon[i-1]]
							if i1>i2: i1,i2= i2,i1
							
							try:
								edge_users[i1,i2]+=1
							except KeyError:
								edge_users[i1,i2]= 1
					
					for key, users in edge_users.iteritems():
						if users>1:
							fgon_edges[key]= None
				
				# remove all after 3, means we dont have to pop this one.
				faces.pop(f_idx)
		
		
	# Build sharp edges
	if unique_smooth_groups:
		for edge_dict in smooth_group_users.itervalues():
			for key, users in edge_dict.iteritems():
				if users==1: # This edge is on the boundry of a group
					sharp_edges[key]= None
	
	
	# map the material names to an index
	material_mapping= dict([(name, i) for i, name in enumerate(unique_materials)]) # enumerate over unique_materials keys()
	
	materials= [None] * len(unique_materials)
	
	for name, index in material_mapping.iteritems():
		materials[index]= unique_materials[name]
	
	me= bpy.data.meshes.new(dataname)
	
	me.materials= materials[0:16] # make sure the list isnt too big.
	#me.verts.extend([(0,0,0)]) # dummy vert
	me.verts.extend(verts_loc)
	
	face_mapping= me.faces.extend([f[0] for f in faces], indexList=True)
	
	if verts_tex and me.faces:
		me.faceUV= 1
		# TEXMODE= Mesh.FaceModes['TEX']
	
	context_material_old= -1 # avoid a dict lookup
	mat= 0 # rare case it may be un-initialized.
	me_faces= me.faces
	ALPHA= Mesh.FaceTranspModes.ALPHA
	
	for i, face in enumerate(faces):
		if len(face[0]) < 2:
			pass #raise "bad face"
		elif len(face[0])==2:
			if CREATE_EDGES:
				edges.append(face[0])
		else:
			face_index_map= face_mapping[i]
			if face_index_map!=None: # None means the face wasnt added
				blender_face= me_faces[face_index_map]
				
				face_vert_loc_indicies,\
				face_vert_tex_indicies,\
				context_material,\
				context_smooth_group,\
				context_object= face
				
				
				
				if context_smooth_group:
					blender_face.smooth= True
				
				if context_material:
					if context_material_old is not context_material:
						mat= material_mapping[context_material]
						if mat>15:
							mat= 15
						context_material_old= context_material
					
					blender_face.mat= mat
				
				
				if verts_tex:	
					if context_material:
						image, has_data= unique_material_images[context_material]
						if image: # Can be none if the material dosnt have an image.
							blender_face.image= image
							if has_data and image.depth == 32:
								blender_face.transp |= ALPHA
					
					# BUG - Evil eekadoodle problem where faces that have vert index 0 location at 3 or 4 are shuffled.
					if len(face_vert_loc_indicies)==4:
						if face_vert_loc_indicies[2]==0 or face_vert_loc_indicies[3]==0:
							face_vert_tex_indicies= face_vert_tex_indicies[2], face_vert_tex_indicies[3], face_vert_tex_indicies[0], face_vert_tex_indicies[1]
					else: # length of 3
						if face_vert_loc_indicies[2]==0:
							face_vert_tex_indicies= face_vert_tex_indicies[1], face_vert_tex_indicies[2], face_vert_tex_indicies[0]
					# END EEEKADOODLE FIX
					
					# assign material, uv's and image
					for ii, uv in enumerate(blender_face.uv):
						uv.x, uv.y=  verts_tex[face_vert_tex_indicies[ii]]
	del me_faces
	del ALPHA
	
	# Add edge faces.
	me_edges= me.edges
	if CREATE_FGONS and fgon_edges:
		FGON= Mesh.EdgeFlags.FGON
		for ed in me.findEdges( fgon_edges.keys() ):
			if ed!=None:
				me_edges[ed].flag |= FGON
		del FGON
	
	if unique_smooth_groups and sharp_edges:
		SHARP= Mesh.EdgeFlags.SHARP
		for ed in me.findEdges( sharp_edges.keys() ):
			if ed!=None:
				me_edges[ed].flag |= SHARP
		del SHARP
	
	if CREATE_EDGES:
		me_edges.extend( edges )
	
	del me_edges
	
	me.calcNormals()
	
	ob= scn.objects.new(me)
	new_objects.append(ob)

	# Create the vertex groups. No need to have the flag passed here since we test for the 
	# content of the vertex_groups. If the user selects to NOT have vertex groups saved then
	# the following test will never run
	for group_name, group_indicies in vertex_groups.iteritems():
		me.addVertGroup(group_name)
		me.assignVertsToGroup(group_name, group_indicies,1.00, Mesh.AssignModes.REPLACE)


def create_nurbs(scn, context_nurbs, vert_loc, new_objects):
	'''
	Add nurbs object to blender, only support one type at the moment
	'''
	deg = context_nurbs.get('deg', (3,))
	curv_range = context_nurbs.get('curv_range', None)
	curv_idx = context_nurbs.get('curv_idx', [])
	parm_u = context_nurbs.get('parm_u', [])
	parm_v = context_nurbs.get('parm_v', [])
	name = context_nurbs.get('name', 'ObjNurb')
	cstype = context_nurbs.get('cstype', None)
	
	if cstype == None:
		print '\tWarning, cstype not found'
		return
	if cstype != 'bspline':
		print '\tWarning, cstype is not supported (only bspline)'
		return
	if not curv_idx:
		print '\tWarning, curv argument empty or not set'
		return
	if len(deg) > 1 or parm_v:
		print '\tWarning, surfaces not supported'
		return
	
	cu = bpy.data.curves.new(name, 'Curve')
	cu.flag |= 1 # 3D curve
	
	nu = None
	for pt in curv_idx:
		
		pt = vert_loc[pt]
		pt = (pt[0], pt[1], pt[2], 1.0)
		
		if nu == None:
			nu = cu.appendNurb(pt)
		else:
			nu.append(pt)
	
	nu.orderU = deg[0]+1
	
	# get for endpoint flag from the weighting
	if curv_range and len(parm_u) > deg[0]+1:
		do_endpoints = True
		for i in xrange(deg[0]+1):
			
			if abs(parm_u[i]-curv_range[0]) > 0.0001:
				do_endpoints = False
				break
			
			if abs(parm_u[-(i+1)]-curv_range[1]) > 0.0001:
				do_endpoints = False
				break
			
	else:
		do_endpoints = False
	
	if do_endpoints:
		nu.flagU |= 2
	
	
	# close
	'''
	do_closed = False
	if len(parm_u) > deg[0]+1:
		for i in xrange(deg[0]+1):
			#print curv_idx[i], curv_idx[-(i+1)]
			
			if curv_idx[i]==curv_idx[-(i+1)]:
				do_closed = True
				break
	
	if do_closed:
		nu.flagU |= 1
	'''
	
	ob = scn.objects.new(cu)
	new_objects.append(ob)


def strip_slash(line_split):
	if line_split[-1][-1]== '\\':
		if len(line_split[-1])==1:
			line_split.pop() # remove the \ item
		else:
			line_split[-1]= line_split[-1][:-1] # remove the \ from the end last number
		return True
	return False



def get_float_func(filepath):
	'''
	find the float function for this obj file
	- weather to replace commas or not
	'''
	file= open(filepath, 'rU')
	for line in file: #.xreadlines():
		line = line.lstrip()
		if line.startswith('v'): # vn vt v 
			if ',' in line:
				return lambda f: float(f.replace(',', '.'))
			elif '.' in line:
				return float
	
	# incase all vert values were ints 
	return float

def load_obj(filepath,
						 CLAMP_SIZE= 0.0, 
						 CREATE_FGONS= True, 
						 CREATE_SMOOTH_GROUPS= True, 
						 CREATE_EDGES= True, 
						 SPLIT_OBJECTS= True, 
						 SPLIT_GROUPS= True, 
						 SPLIT_MATERIALS= True, 
						 ROTATE_X90= True, 
						 IMAGE_SEARCH=True,
						 POLYGROUPS=False):
	'''
	Called by the user interface or another script.
	load_obj(path) - should give acceptable results.
	This function passes the file and sends the data off
		to be split into objects and then converted into mesh objects
	'''
	print '\nimporting obj "%s"' % filepath
	
	if SPLIT_OBJECTS or SPLIT_GROUPS or SPLIT_MATERIALS:
		POLYGROUPS = False
	
	time_main= sys.time()
	
	verts_loc= []
	verts_tex= []
	faces= [] # tuples of the faces
	material_libs= [] # filanems to material libs this uses
	vertex_groups = {} # when POLYGROUPS is true
	
	# Get the string to float conversion func for this file- is 'float' for almost all files.
	float_func= get_float_func(filepath)
	
	# Context variables
	context_material= None
	context_smooth_group= None
	context_object= None
	context_vgroup = None
	
	# Nurbs
	context_nurbs = {}
	nurbs = []
	context_parm = '' # used by nurbs too but could be used elsewhere

	has_ngons= False
	# has_smoothgroups= False - is explicit with len(unique_smooth_groups) being > 0
	
	# Until we can use sets
	unique_materials= {}
	unique_material_images= {}
	unique_smooth_groups= {}
	# unique_obects= {} - no use for this variable since the objects are stored in the face.
	
	# when there are faces that end with \
	# it means they are multiline- 
	# since we use xreadline we cant skip to the next line
	# so we need to know weather 
	context_multi_line= ''
	
	print '\tparsing obj file "%s"...' % filepath,
	time_sub= sys.time()

	file= open(filepath, 'rU')
	for line in file: #.xreadlines():
		line = line.lstrip() # rare cases there is white space at the start of the line
		
		if line.startswith('v '):
			line_split= line.split()
			# rotate X90: (x,-z,y)
			verts_loc.append( (float_func(line_split[1]), -float_func(line_split[3]), float_func(line_split[2])) )
				
		elif line.startswith('vn '):
			pass
		
		elif line.startswith('vt '):
			line_split= line.split()
			verts_tex.append( (float_func(line_split[1]), float_func(line_split[2])) ) 
		
		# Handel faces lines (as faces) and the second+ lines of fa multiline face here
		# use 'f' not 'f ' because some objs (very rare have 'fo ' for faces)
		elif line.startswith('f') or context_multi_line == 'f':
			
			if context_multi_line:
				# use face_vert_loc_indicies and face_vert_tex_indicies previously defined and used the obj_face
				line_split= line.split()
				
			else:
				line_split= line[2:].split()
				face_vert_loc_indicies= []
				face_vert_tex_indicies= []
				
				# Instance a face
				faces.append((\
				face_vert_loc_indicies,\
				face_vert_tex_indicies,\
				context_material,\
				context_smooth_group,\
				context_object\
				))
			
			if strip_slash(line_split):
				context_multi_line = 'f'
			else:
				context_multi_line = ''
			
			for v in line_split:
				obj_vert= v.split('/')
				
				vert_loc_index= int(obj_vert[0])-1
				# Add the vertex to the current group
				# *warning*, this wont work for files that have groups defined around verts
				if	POLYGROUPS and context_vgroup:
					vertex_groups[context_vgroup].append(vert_loc_index)
				
				# Make relative negative vert indicies absolute
				if vert_loc_index < 0:
					vert_loc_index= len(verts_loc) + vert_loc_index + 1
				
				face_vert_loc_indicies.append(vert_loc_index)
				
				if len(obj_vert)>1 and obj_vert[1]:
					# formatting for faces with normals and textures us 
					# loc_index/tex_index/nor_index
					
					vert_tex_index= int(obj_vert[1])-1
					# Make relative negative vert indicies absolute
					if vert_tex_index < 0:
						vert_tex_index= len(verts_tex) + vert_tex_index + 1
					
					face_vert_tex_indicies.append(vert_tex_index)
				else:
					# dummy
					face_vert_tex_indicies.append(0)
			
			if len(face_vert_loc_indicies) > 4:
				has_ngons= True
		
		elif CREATE_EDGES and (line.startswith('l ') or context_multi_line == 'l'):
			# very similar to the face load function above with some parts removed
			
			if context_multi_line:
				# use face_vert_loc_indicies and face_vert_tex_indicies previously defined and used the obj_face
				line_split= line.split()
				
			else:
				line_split= line[2:].split()
				face_vert_loc_indicies= []
				face_vert_tex_indicies= []
				
				# Instance a face
				faces.append((\
				face_vert_loc_indicies,\
				face_vert_tex_indicies,\
				context_material,\
				context_smooth_group,\
				context_object\
				))
			
			if strip_slash(line_split):
				context_multi_line = 'l'
			else:
				context_multi_line = ''
			
			isline= line.startswith('l')
			
			for v in line_split:
				vert_loc_index= int(v)-1
				
				# Make relative negative vert indicies absolute
				if vert_loc_index < 0:
					vert_loc_index= len(verts_loc) + vert_loc_index + 1
				
				face_vert_loc_indicies.append(vert_loc_index)
		
		elif line.startswith('s'):
			if CREATE_SMOOTH_GROUPS:
				context_smooth_group= line_value(line.split())
				if context_smooth_group=='off':
					context_smooth_group= None
				elif context_smooth_group: # is not None
					unique_smooth_groups[context_smooth_group]= None
		
		elif line.startswith('o'):
			if SPLIT_OBJECTS:
				context_object= line_value(line.split())
				# unique_obects[context_object]= None
			
		elif line.startswith('g'):
			if SPLIT_GROUPS:
				context_object= line_value(line.split())
				# print 'context_object', context_object
				# unique_obects[context_object]= None
			elif POLYGROUPS:
				context_vgroup = line_value(line.split())
				if context_vgroup and context_vgroup != '(null)':
					vertex_groups.setdefault(context_vgroup, [])
				else:
					context_vgroup = None # dont assign a vgroup
		
		elif line.startswith('usemtl'):
			context_material= line_value(line.split())
			unique_materials[context_material]= None
		elif line.startswith('mtllib'): # usemap or usemat
			material_libs.extend( line.split()[1:] ) # can have multiple mtllib filenames per line
			
			
			# Nurbs support
		elif line.startswith('cstype '):
			context_nurbs['cstype']= line_value(line.split()) # 'rat bspline' / 'bspline'
		elif line.startswith('curv ') or context_multi_line == 'curv':
			line_split= line.split()
			
			curv_idx = context_nurbs['curv_idx'] = context_nurbs.get('curv_idx', []) # incase were multiline
			
			if not context_multi_line:
				context_nurbs['curv_range'] = float_func(line_split[1]), float_func(line_split[2])
				line_split[0:3] = [] # remove first 3 items
			
			if strip_slash(line_split):
				context_multi_line = 'curv'
			else:
				context_multi_line = ''
				
			
			for i in line_split:
				vert_loc_index = int(i)-1
				
				if vert_loc_index < 0:
					vert_loc_index= len(verts_loc) + vert_loc_index + 1
				
				curv_idx.append(vert_loc_index)
			
		elif line.startswith('parm') or context_multi_line == 'parm':
			line_split= line.split()
			
			if context_multi_line:
				context_multi_line = ''
			else:
				context_parm = line_split[1]
				line_split[0:2] = [] # remove first 2
			
			if strip_slash(line_split):
				context_multi_line = 'parm'
			else:
				context_multi_line = ''
			
			if context_parm.lower() == 'u':
				context_nurbs.setdefault('parm_u', []).extend( [float_func(f) for f in line_split] )
			elif context_parm.lower() == 'v': # surfaces not suported yet
				context_nurbs.setdefault('parm_v', []).extend( [float_func(f) for f in line_split] )
			# else: # may want to support other parm's ?
		
		elif line.startswith('deg '):
			context_nurbs['deg']= [int(i) for i in line.split()[1:]]
		elif line.startswith('end'):
			# Add the nurbs curve
			if context_object:
				context_nurbs['name'] = context_object
			nurbs.append(context_nurbs)
			context_nurbs = {}
			context_parm = ''
		
		''' # How to use usemap? depricated?
		elif line.startswith('usema'): # usemap or usemat
			context_image= line_value(line.split())
		'''
	
	file.close()
	time_new= sys.time()
	print '%.4f sec' % (time_new-time_sub)
	time_sub= time_new
	
	
	print '\tloading materials and images...',
	create_materials(filepath, material_libs, unique_materials, unique_material_images, IMAGE_SEARCH)
	
	time_new= sys.time()
	print '%.4f sec' % (time_new-time_sub)
	time_sub= time_new
	
	if not ROTATE_X90:
		verts_loc[:] = [(v[0], v[2], -v[1]) for v in verts_loc]
	
	# deselect all
	scn = bpy.data.scenes.active
	scn.objects.selected = []
	new_objects= [] # put new objects here
	
	print '\tbuilding geometry...\n\tverts:%i faces:%i materials: %i smoothgroups:%i ...' % ( len(verts_loc), len(faces), len(unique_materials), len(unique_smooth_groups) ),
	# Split the mesh by objects/materials, may 
	if SPLIT_OBJECTS or SPLIT_GROUPS:	SPLIT_OB_OR_GROUP = True
	else:								SPLIT_OB_OR_GROUP = False
	
	for verts_loc_split, faces_split, unique_materials_split, dataname in split_mesh(verts_loc, faces, unique_materials, filepath, SPLIT_OB_OR_GROUP, SPLIT_MATERIALS):
		# Create meshes from the data, warning 'vertex_groups' wont support splitting
		create_mesh(scn, new_objects, has_ngons, CREATE_FGONS, CREATE_EDGES, verts_loc_split, verts_tex, faces_split, unique_materials_split, unique_material_images, unique_smooth_groups, vertex_groups, dataname)
	
	# nurbs support
	for context_nurbs in nurbs:
		create_nurbs(scn, context_nurbs, verts_loc, new_objects)
	
	
	axis_min= [ 1000000000]*3
	axis_max= [-1000000000]*3
	
	if CLAMP_SIZE:
		# Get all object bounds
		for ob in new_objects:
			for v in ob.getBoundBox():
				for axis, value in enumerate(v):
					if axis_min[axis] > value:	axis_min[axis]= value
					if axis_max[axis] < value:	axis_max[axis]= value
		
		# Scale objects
		max_axis= max(axis_max[0]-axis_min[0], axis_max[1]-axis_min[1], axis_max[2]-axis_min[2])
		scale= 1.0
		
		while CLAMP_SIZE < max_axis * scale:
			scale= scale/10.0
		
		for ob in new_objects:
			ob.setSize(scale, scale, scale)
	
	# Better rotate the vert locations
	#if not ROTATE_X90:
	#	for ob in new_objects:
	#		ob.RotX = -1.570796326794896558
	
	time_new= sys.time()
	
	print '%.4f sec' % (time_new-time_sub)
	print 'finished importing: "%s" in %.4f sec.' % (filepath, (time_new-time_main))


DEBUG= True


def load_obj_ui(filepath, BATCH_LOAD= False):
	if BPyMessages.Error_NoFile(filepath):
		return
	
	global CREATE_SMOOTH_GROUPS, CREATE_FGONS, CREATE_EDGES, SPLIT_OBJECTS, SPLIT_GROUPS, SPLIT_MATERIALS, CLAMP_SIZE, IMAGE_SEARCH, POLYGROUPS, KEEP_VERT_ORDER, ROTATE_X90
	
	CREATE_SMOOTH_GROUPS= Draw.Create(0)
	CREATE_FGONS= Draw.Create(1)
	CREATE_EDGES= Draw.Create(1)
	SPLIT_OBJECTS= Draw.Create(0)
	SPLIT_GROUPS= Draw.Create(0)
	SPLIT_MATERIALS= Draw.Create(0)
	CLAMP_SIZE= Draw.Create(10.0)
	IMAGE_SEARCH= Draw.Create(1)
	POLYGROUPS= Draw.Create(0)
	KEEP_VERT_ORDER= Draw.Create(1)
	ROTATE_X90= Draw.Create(1)
	
	
	# Get USER Options
	# Note, Works but not pretty, instead use a more complicated GUI
	'''
	pup_block= [\
	'Import...',\
	('Smooth Groups', CREATE_SMOOTH_GROUPS, 'Surround smooth groups by sharp edges'),\
	('Create FGons', CREATE_FGONS, 'Import faces with more then 4 verts as fgons.'),\
	('Lines', CREATE_EDGES, 'Import lines and faces with 2 verts as edges'),\
	'Separate objects from obj...',\
	('Object', SPLIT_OBJECTS, 'Import OBJ Objects into Blender Objects'),\
	('Group', SPLIT_GROUPS, 'Import OBJ Groups into Blender Objects'),\
	('Material', SPLIT_MATERIALS, 'Import each material into a seperate mesh (Avoids > 16 per mesh error)'),\
	'Options...',\
	('Keep Vert Order', KEEP_VERT_ORDER, 'Keep vert and face order, disables some other options.'),\
	('Clamp Scale:', CLAMP_SIZE, 0.0, 1000.0, 'Clamp the size to this maximum (Zero to Disable)'),\
	('Image Search', IMAGE_SEARCH, 'Search subdirs for any assosiated images (Warning, may be slow)'),\
	]
	
	if not Draw.PupBlock('Import OBJ...', pup_block):
		return
	
	if KEEP_VERT_ORDER.val:
		SPLIT_OBJECTS.val = False
		SPLIT_GROUPS.val = False
		SPLIT_MATERIALS.val = False
	'''
	
	
	
	# BEGIN ALTERNATIVE UI *******************
	if True: 
		
		EVENT_NONE = 0
		EVENT_EXIT = 1
		EVENT_REDRAW = 2
		EVENT_IMPORT = 3
		
		GLOBALS = {}
		GLOBALS['EVENT'] = EVENT_REDRAW
		#GLOBALS['MOUSE'] = Window.GetMouseCoords()
		GLOBALS['MOUSE'] = [i/2 for i in Window.GetScreenSize()]
		
		def obj_ui_set_event(e,v):
			GLOBALS['EVENT'] = e
		
		def do_split(e,v):
			global SPLIT_OBJECTS, SPLIT_GROUPS, SPLIT_MATERIALS, KEEP_VERT_ORDER, POLYGROUPS
			if SPLIT_OBJECTS.val or SPLIT_GROUPS.val or SPLIT_MATERIALS.val:
				KEEP_VERT_ORDER.val = 0
				POLYGROUPS.val = 0
			else:
				KEEP_VERT_ORDER.val = 1
			
		def do_vertorder(e,v):
			global SPLIT_OBJECTS, SPLIT_GROUPS, SPLIT_MATERIALS, KEEP_VERT_ORDER
			if KEEP_VERT_ORDER.val:
				SPLIT_OBJECTS.val = SPLIT_GROUPS.val = SPLIT_MATERIALS.val = 0
			else:
				if not (SPLIT_OBJECTS.val or SPLIT_GROUPS.val or SPLIT_MATERIALS.val):
					KEEP_VERT_ORDER.val = 1
			
		def do_polygroups(e,v):
			global SPLIT_OBJECTS, SPLIT_GROUPS, SPLIT_MATERIALS, KEEP_VERT_ORDER, POLYGROUPS
			if POLYGROUPS.val:
				SPLIT_OBJECTS.val = SPLIT_GROUPS.val = SPLIT_MATERIALS.val = 0
			
		def do_help(e,v):
			url = __url__[0]
			print 'Trying to open web browser with documentation at this address...'
			print '\t' + url
			
			try:
				import webbrowser
				webbrowser.open(url)
			except:
				print '...could not open a browser window.'
		
		def obj_ui():
			ui_x, ui_y = GLOBALS['MOUSE']
			
			# Center based on overall pup size
			ui_x -= 165
			ui_y -= 90
			
			global CREATE_SMOOTH_GROUPS, CREATE_FGONS, CREATE_EDGES, SPLIT_OBJECTS, SPLIT_GROUPS, SPLIT_MATERIALS, CLAMP_SIZE, IMAGE_SEARCH, POLYGROUPS, KEEP_VERT_ORDER, ROTATE_X90
			
			Draw.Label('Import...', ui_x+9, ui_y+159, 220, 21)
			Draw.BeginAlign()
			CREATE_SMOOTH_GROUPS = Draw.Toggle('Smooth Groups', EVENT_NONE, ui_x+9, ui_y+139, 110, 20, CREATE_SMOOTH_GROUPS.val, 'Surround smooth groups by sharp edges')
			CREATE_FGONS = Draw.Toggle('NGons as FGons', EVENT_NONE, ui_x+119, ui_y+139, 110, 20, CREATE_FGONS.val, 'Import faces with more then 4 verts as fgons')
			CREATE_EDGES = Draw.Toggle('Lines as Edges', EVENT_NONE, ui_x+229, ui_y+139, 110, 20, CREATE_EDGES.val, 'Import lines and faces with 2 verts as edges')
			Draw.EndAlign()
			
			Draw.Label('Separate objects by OBJ...', ui_x+9, ui_y+110, 220, 20)
			Draw.BeginAlign()
			SPLIT_OBJECTS = Draw.Toggle('Object', EVENT_REDRAW, ui_x+9, ui_y+89, 55, 21, SPLIT_OBJECTS.val, 'Import OBJ Objects into Blender Objects', do_split)
			SPLIT_GROUPS = Draw.Toggle('Group', EVENT_REDRAW, ui_x+64, ui_y+89, 55, 21, SPLIT_GROUPS.val, 'Import OBJ Groups into Blender Objects', do_split)
			SPLIT_MATERIALS = Draw.Toggle('Material', EVENT_REDRAW, ui_x+119, ui_y+89, 60, 21, SPLIT_MATERIALS.val, 'Import each material into a seperate mesh (Avoids > 16 per mesh error)', do_split)
			Draw.EndAlign()
			
			# Only used for user feedback
			KEEP_VERT_ORDER = Draw.Toggle('Keep Vert Order', EVENT_REDRAW, ui_x+184, ui_y+89, 113, 21, KEEP_VERT_ORDER.val, 'Keep vert and face order, disables split options, enable for morph targets', do_vertorder)
			
			ROTATE_X90 = Draw.Toggle('-X90', EVENT_REDRAW, ui_x+302, ui_y+89, 38, 21, ROTATE_X90.val, 'Rotate X 90.')
			
			Draw.Label('Options...', ui_x+9, ui_y+60, 211, 20)
			CLAMP_SIZE = Draw.Number('Clamp Scale: ', EVENT_NONE, ui_x+9, ui_y+39, 130, 21, CLAMP_SIZE.val, 0.0, 1000.0, 'Clamp the size to this maximum (Zero to Disable)')
			POLYGROUPS = Draw.Toggle('Poly Groups', EVENT_REDRAW, ui_x+144, ui_y+39, 90, 21, POLYGROUPS.val, 'Import OBJ groups as vertex groups.', do_polygroups)
			IMAGE_SEARCH = Draw.Toggle('Image Search', EVENT_NONE, ui_x+239, ui_y+39, 100, 21, IMAGE_SEARCH.val, 'Search subdirs for any assosiated images (Warning, may be slow)')
			Draw.BeginAlign()
			Draw.PushButton('Online Help', EVENT_REDRAW, ui_x+9, ui_y+9, 110, 21, 'Load the wiki page for this script', do_help)
			Draw.PushButton('Cancel', EVENT_EXIT, ui_x+119, ui_y+9, 110, 21, '', obj_ui_set_event)
			Draw.PushButton('Import', EVENT_IMPORT, ui_x+229, ui_y+9, 110, 21, 'Import with these settings', obj_ui_set_event)
			Draw.EndAlign()
			
		
		# hack so the toggle buttons redraw. this is not nice at all
		while GLOBALS['EVENT'] not in (EVENT_EXIT, EVENT_IMPORT):
			Draw.UIBlock(obj_ui, 0)
		
		if GLOBALS['EVENT'] != EVENT_IMPORT:
			return
		
	# END ALTERNATIVE UI *********************
	
	
	
	
	
	
	
	Window.WaitCursor(1)
	
	if BATCH_LOAD: # load the dir
		try:
			files= [ f for f in os.listdir(filepath) if f.lower().endswith('.obj') ]
		except:
			Window.WaitCursor(0)
			Draw.PupMenu('Error%t|Could not open path ' + filepath)
			return
		
		if not files:
			Window.WaitCursor(0)
			Draw.PupMenu('Error%t|No files at path ' + filepath)
			return
		
		for f in files:
			scn= bpy.data.scenes.new( stripExt(f) )
			scn.makeCurrent()
			
			load_obj(sys.join(filepath, f),\
			  CLAMP_SIZE.val,\
			  CREATE_FGONS.val,\
			  CREATE_SMOOTH_GROUPS.val,\
			  CREATE_EDGES.val,\
			  SPLIT_OBJECTS.val,\
			  SPLIT_GROUPS.val,\
			  SPLIT_MATERIALS.val,\
			  ROTATE_X90.val,\
			  IMAGE_SEARCH.val,\
			  POLYGROUPS.val
			)
	
	else: # Normal load
		load_obj(filepath,\
		  CLAMP_SIZE.val,\
		  CREATE_FGONS.val,\
		  CREATE_SMOOTH_GROUPS.val,\
		  CREATE_EDGES.val,\
		  SPLIT_OBJECTS.val,\
		  SPLIT_GROUPS.val,\
		  SPLIT_MATERIALS.val,\
		  ROTATE_X90.val,\
		  IMAGE_SEARCH.val,\
		  POLYGROUPS.val
		)
	
	Window.WaitCursor(0)


def load_obj_ui_batch(file):
	load_obj_ui(file, True)

DEBUG= False

if __name__=='__main__' and not DEBUG:
	if os and Window.GetKeyQualifiers() & Window.Qual.SHIFT:
		Window.FileSelector(load_obj_ui_batch, 'Import OBJ Dir', '')
	else:
		Window.FileSelector(load_obj_ui, 'Import a Wavefront OBJ', '*.obj')

	# For testing compatibility
'''
else:
	# DEBUG ONLY
	TIME= sys.time()
	DIR = '/fe/obj'
	import os
	print 'Searching for files'
	def fileList(path):
		for dirpath, dirnames, filenames in os.walk(path):
			for filename in filenames:
				yield os.path.join(dirpath, filename)
	
	files = [f for f in fileList(DIR) if f.lower().endswith('.obj')]
	files.sort()
	
	for i, obj_file in enumerate(files):
		if 0 < i < 20:
			print 'Importing', obj_file, '\nNUMBER', i, 'of', len(files)
			newScn= bpy.data.scenes.new(os.path.basename(obj_file))
			newScn.makeCurrent()
			load_obj(obj_file, False, IMAGE_SEARCH=0)

	print 'TOTAL TIME: %.6f' % (sys.time() - TIME)
'''
#load_obj('/test.obj')
#load_obj('/fe/obj/mba1.obj')
