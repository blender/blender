#!BPY

"""
Name: 'Wavefront (.obj)...'
Blender: 248
Group: 'Export'
Tooltip: 'Save a Wavefront OBJ File'
"""

__author__ = "Campbell Barton, Jiri Hnidek, Paolo Ciccone"
__url__ = ['http://wiki.blender.org/index.php/Scripts/Manual/Export/wavefront_obj', 'www.blender.org', 'blenderartists.org']
__version__ = "1.21"

__bpydoc__ = """\
This script is an exporter to OBJ file format.

Usage:

Select the objects you wish to export and run this script from "File->Export" menu.
Selecting the default options from the popup box will be good in most cases.
All objects that can be represented as a mesh (mesh, curve, metaball, surface, text3d)
will be exported as mesh data.
"""


# --------------------------------------------------------------------------
# OBJ Export v1.1 by Campbell Barton (AKA Ideasman)
# --------------------------------------------------------------------------
# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place - Suite 330, Boston, MA	 02111-1307, USA.
#
# ***** END GPL LICENCE BLOCK *****
# --------------------------------------------------------------------------

# import math and other in functions that use them for the sake of fast Blender startup
# import math
import os
import time

import bpy
import Mathutils


# Returns a tuple - path,extension.
# 'hello.obj' >	 ('hello', '.obj')
def splitExt(path):
	dotidx = path.rfind('.')
	if dotidx == -1:
		return path, ''
	else:
		return path[:dotidx], path[dotidx:] 

def fixName(name):
	if name == None:
		return 'None'
	else:
		return name.replace(' ', '_')


# this used to be in BPySys module
# frankly, I don't understand how it works
def BPySys_cleanName(name):

	v = [0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,46,47,58,59,60,61,62,63,64,91,92,93,94,96,123,124,125,126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,254]

	invalid = ''.join([chr(i) for i in v])

	for ch in invalid:
		name = name.replace(ch, '_')
	return name

# A Dict of Materials
# (material.name, image.name):matname_imagename # matname_imagename has gaps removed.
MTL_DICT = {} 	

def write_mtl(scene, filename, copy_images):

	world = scene.world
	worldAmb = world.ambient_color

	dest_dir = os.path.dirname(filename)

	def copy_image(image):
		rel = image.get_export_path(dest_dir, True)

		if copy_images:
			abspath = image.get_export_path(dest_dir, False)
			if not os.path.exists(abs_path):
				shutil.copy(image.get_abs_filename(), abs_path)

		return rel


	file = open(filename, "w")
	# XXX
#	file.write('# Blender3D MTL File: %s\n' % Blender.Get('filename').split('\\')[-1].split('/')[-1])
	file.write('# Material Count: %i\n' % len(MTL_DICT))
	# Write material/image combinations we have used.
	for key, (mtl_mat_name, mat, img) in MTL_DICT.items():
		
		# Get the Blender data for the material and the image.
		# Having an image named None will make a bug, dont do it :)
		
		file.write('newmtl %s\n' % mtl_mat_name) # Define a new material: matname_imgname
		
		if mat:
			file.write('Ns %.6f\n' % ((mat.specular_hardness-1) * 1.9607843137254901) ) # Hardness, convert blenders 1-511 to MTL's
			file.write('Ka %.6f %.6f %.6f\n' %	tuple([c*mat.ambient for c in worldAmb])  ) # Ambient, uses mirror colour,
			file.write('Kd %.6f %.6f %.6f\n' % tuple([c*mat.diffuse_intensity for c in mat.diffuse_color]) ) # Diffuse
			file.write('Ks %.6f %.6f %.6f\n' % tuple([c*mat.specular_intensity for c in mat.specular_color]) ) # Specular
			if hasattr(mat, "ior"):
				file.write('Ni %.6f\n' % mat.ior) # Refraction index
			else:
				file.write('Ni %.6f\n' % 1.0)
			file.write('d %.6f\n' % mat.alpha) # Alpha (obj uses 'd' for dissolve)

			# 0 to disable lighting, 1 for ambient & diffuse only (specular color set to black), 2 for full lighting.
			if mat.shadeless:
				file.write('illum 0\n') # ignore lighting
			elif mat.specular_intensity == 0:
				file.write('illum 1\n') # no specular.
			else:
				file.write('illum 2\n') # light normaly	
		
		else:
			#write a dummy material here?
			file.write('Ns 0\n')
			file.write('Ka %.6f %.6f %.6f\n' %	tuple([c for c in worldAmb])  ) # Ambient, uses mirror colour,
			file.write('Kd 0.8 0.8 0.8\n')
			file.write('Ks 0.8 0.8 0.8\n')
			file.write('d 1\n') # No alpha
			file.write('illum 2\n') # light normaly
		
		# Write images!
		if img:	 # We have an image on the face!
			# write relative image path
			rel = copy_image(img)
			file.write('map_Kd %s\n' % rel) # Diffuse mapping image
# 			file.write('map_Kd %s\n' % img.filename.split('\\')[-1].split('/')[-1]) # Diffuse mapping image			
		
		elif mat: # No face image. if we havea material search for MTex image.
			for mtex in mat.textures:
				if mtex and mtex.texture.type == 'IMAGE':
					try:
						filename = copy_image(mtex.texture.image)
# 						filename = mtex.texture.image.filename.split('\\')[-1].split('/')[-1]
						file.write('map_Kd %s\n' % filename) # Diffuse mapping image
						break
					except:
						# Texture has no image though its an image type, best ignore.
						pass
		
		file.write('\n\n')
	
	file.close()

# XXX not used
def copy_file(source, dest):
	file = open(source, 'rb')
	data = file.read()
	file.close()
	
	file = open(dest, 'wb')
	file.write(data)
	file.close()


# XXX not used
def copy_images(dest_dir):
	if dest_dir[-1] != os.sep:
		dest_dir += os.sep
#	if dest_dir[-1] != sys.sep:
#		dest_dir += sys.sep
	
	# Get unique image names
	uniqueImages = {}
	for matname, mat, image in MTL_DICT.values(): # Only use image name
		# Get Texface images
		if image:
			uniqueImages[image] = image # Should use sets here. wait until Python 2.4 is default.
		
		# Get MTex images
		if mat:
			for mtex in mat.textures:
				if mtex and mtex.texture.type == 'IMAGE':
					image_tex = mtex.texture.image
					if image_tex:
						try:
							uniqueImages[image_tex] = image_tex
						except:
							pass
	
	# Now copy images
	copyCount = 0
	
# 	for bImage in uniqueImages.values():
# 		image_path = bpy.sys.expandpath(bImage.filename)
# 		if bpy.sys.exists(image_path):
# 			# Make a name for the target path.
# 			dest_image_path = dest_dir + image_path.split('\\')[-1].split('/')[-1]
# 			if not bpy.sys.exists(dest_image_path): # Image isnt alredy there
# 				print('\tCopying "%s" > "%s"' % (image_path, dest_image_path))
# 				copy_file(image_path, dest_image_path)
# 				copyCount+=1

# 	paths= bpy.util.copy_images(uniqueImages.values(), dest_dir)

	print('\tCopied %d images' % copyCount)
# 	print('\tCopied %d images' % copyCount)

# XXX not converted
def test_nurbs_compat(ob):
	if ob.type != 'Curve':
		return False
	
	for nu in ob.data:
		if (not nu.knotsV) and nu.type != 1: # not a surface and not bezier
			return True
	
	return False


# XXX not converted
def write_nurb(file, ob, ob_mat):
	tot_verts = 0
	cu = ob.data
	
	# use negative indices
	Vector = Blender.Mathutils.Vector
	for nu in cu:
		
		if nu.type==0:		DEG_ORDER_U = 1
		else:				DEG_ORDER_U = nu.orderU-1  # Tested to be correct
		
		if nu.type==1:
			print("\tWarning, bezier curve:", ob.name, "only poly and nurbs curves supported")
			continue
		
		if nu.knotsV:
			print("\tWarning, surface:", ob.name, "only poly and nurbs curves supported")
			continue
		
		if len(nu) <= DEG_ORDER_U:
			print("\tWarning, orderU is lower then vert count, skipping:", ob.name)
			continue
		
		pt_num = 0
		do_closed = (nu.flagU & 1)
		do_endpoints = (do_closed==0) and (nu.flagU & 2)
		
		for pt in nu:
			pt = Vector(pt[0], pt[1], pt[2]) * ob_mat
			file.write('v %.6f %.6f %.6f\n' % (pt[0], pt[1], pt[2]))
			pt_num += 1
		tot_verts += pt_num
		
		file.write('g %s\n' % (fixName(ob.name))) # fixName(ob.getData(1)) could use the data name too
		file.write('cstype bspline\n') # not ideal, hard coded
		file.write('deg %d\n' % DEG_ORDER_U) # not used for curves but most files have it still
		
		curve_ls = [-(i+1) for i in range(pt_num)]
		
		# 'curv' keyword
		if do_closed:
			if DEG_ORDER_U == 1:
				pt_num += 1
				curve_ls.append(-1)
			else:
				pt_num += DEG_ORDER_U
				curve_ls = curve_ls + curve_ls[0:DEG_ORDER_U]
		
		file.write('curv 0.0 1.0 %s\n' % (' '.join( [str(i) for i in curve_ls] ))) # Blender has no U and V values for the curve
		
		# 'parm' keyword
		tot_parm = (DEG_ORDER_U + 1) + pt_num
		tot_parm_div = float(tot_parm-1)
		parm_ls = [(i/tot_parm_div) for i in range(tot_parm)]
		
		if do_endpoints: # end points, force param
			for i in range(DEG_ORDER_U+1):
				parm_ls[i] = 0.0
				parm_ls[-(1+i)] = 1.0
		
		file.write('parm u %s\n' % ' '.join( [str(i) for i in parm_ls] ))

		file.write('end\n')
	
	return tot_verts

def write(filename, objects, scene,
		  EXPORT_TRI=False,
		  EXPORT_EDGES=False,
		  EXPORT_NORMALS=False,
		  EXPORT_NORMALS_HQ=False,
		  EXPORT_UV=True,
		  EXPORT_MTL=True,
		  EXPORT_COPY_IMAGES=False,
		  EXPORT_APPLY_MODIFIERS=True,
		  EXPORT_ROTX90=True,
		  EXPORT_BLEN_OBS=True,
		  EXPORT_GROUP_BY_OB=False,
		  EXPORT_GROUP_BY_MAT=False,
		  EXPORT_KEEP_VERT_ORDER=False,
		  EXPORT_POLYGROUPS=False,
		  EXPORT_CURVE_AS_NURBS=True):
	'''
	Basic write function. The context and options must be alredy set
	This can be accessed externaly
	eg.
	write( 'c:\\test\\foobar.obj', Blender.Object.GetSelected() ) # Using default options.
	'''

	# XXX
	import math
	
	def veckey3d(v):
		return round(v.x, 6), round(v.y, 6), round(v.z, 6)
		
	def veckey2d(v):
		return round(v[0], 6), round(v[1], 6)
		# return round(v.x, 6), round(v.y, 6)
	
	def findVertexGroupName(face, vWeightMap):
		"""
		Searches the vertexDict to see what groups is assigned to a given face.
		We use a frequency system in order to sort out the name because a given vetex can
		belong to two or more groups at the same time. To find the right name for the face
		we list all the possible vertex group names with their frequency and then sort by
		frequency in descend order. The top element is the one shared by the highest number
		of vertices is the face's group 
		"""
		weightDict = {}
		for vert_index in face.verts:
#		for vert in face:
			vWeights = vWeightMap[vert_index]
#			vWeights = vWeightMap[vert]
			for vGroupName, weight in vWeights:
				weightDict[vGroupName] = weightDict.get(vGroupName, 0) + weight
		
		if weightDict:
			alist = [(weight,vGroupName) for vGroupName, weight in weightDict.items()] # sort least to greatest amount of weight
			alist.sort()
			return(alist[-1][1]) # highest value last
		else:
			return '(null)'

	# TODO: implement this in C? dunno how it should be called...
	def getVertsFromGroup(me, group_index):
		ret = []

		for i, v in enumerate(me.verts):
			for g in v.groups:
				if g.group == group_index:
					ret.append((i, g.weight))

		return ret


	print('OBJ Export path: "%s"' % filename)
	temp_mesh_name = '~tmp-mesh'

	time1 = time.clock()
#	time1 = sys.time()
#	scn = Scene.GetCurrent()

	file = open(filename, "w")

	# Write Header
	version = "2.5"
	file.write('# Blender3D v%s OBJ File: %s\n' % (version, bpy.data.filename.split('/')[-1].split('\\')[-1] ))
	file.write('# www.blender3d.org\n')

	# Tell the obj file what material file to use.
	if EXPORT_MTL:
		mtlfilename = '%s.mtl' % '.'.join(filename.split('.')[:-1])
		file.write('mtllib %s\n' % ( mtlfilename.split('\\')[-1].split('/')[-1] ))
	
	if EXPORT_ROTX90:
		mat_xrot90= Mathutils.RotationMatrix(-math.pi/2, 4, 'x')
		
	# Initialize totals, these are updated each object
	totverts = totuvco = totno = 1
	
	face_vert_index = 1
	
	globalNormals = {}

	# Get all meshes
	for ob_main in objects:

		# ignore dupli children
		if ob_main.parent and ob_main.parent.dupli_type != 'NONE':
			# XXX
			print(ob_main.name, 'is a dupli child - ignoring')
			continue

		obs = []
		if ob_main.dupli_type != 'NONE':
			# XXX
			print('creating dupli_list on', ob_main.name)
			ob_main.create_dupli_list()
			
			obs = [(dob.object, dob.matrix) for dob in ob_main.dupli_list]

			# XXX debug print
			print(ob_main.name, 'has', len(obs), 'dupli children')
		else:
			obs = [(ob_main, ob_main.matrix)]

		for ob, ob_mat in obs:

			# XXX postponed
#			# Nurbs curve support
#			if EXPORT_CURVE_AS_NURBS and test_nurbs_compat(ob):
#				if EXPORT_ROTX90:
#					ob_mat = ob_mat * mat_xrot90
				
#				totverts += write_nurb(file, ob, ob_mat)
				
#				continue
#			end nurbs

			if ob.type != 'MESH':
				continue

			me = ob.create_mesh(EXPORT_APPLY_MODIFIERS, 'PREVIEW')

			if EXPORT_ROTX90:
				me.transform(ob_mat * mat_xrot90)
			else:
				me.transform(ob_mat)

#			# Will work for non meshes now! :)
#			me= BPyMesh.getMeshFromObject(ob, containerMesh, EXPORT_APPLY_MODIFIERS, EXPORT_POLYGROUPS, scn)
#			if not me:
#				continue

			if EXPORT_UV:
				faceuv = len(me.uv_textures) > 0
			else:
				faceuv = False

			# XXX - todo, find a better way to do triangulation
			# ...removed convert_to_triface because it relies on editmesh
			'''
			# We have a valid mesh
			if EXPORT_TRI and me.faces:
				# Add a dummy object to it.
				has_quads = False
				for f in me.faces:
					if f.verts[3] != 0:
						has_quads = True
						break
				
				if has_quads:
					newob = bpy.data.add_object('MESH', 'temp_object')
					newob.data = me
					# if we forget to set Object.data - crash
					scene.add_object(newob)
					newob.convert_to_triface(scene)
					# mesh will still be there
					scene.remove_object(newob)
			'''
			
			# Make our own list so it can be sorted to reduce context switching
			face_index_pairs = [ (face, index) for index, face in enumerate(me.faces)]
			# faces = [ f for f in me.faces ]
			
			if EXPORT_EDGES:
				edges = me.edges
			else:
				edges = []

			if not (len(face_index_pairs)+len(edges)+len(me.verts)): # Make sure there is somthing to write				

				# clean up
				bpy.data.remove_mesh(me)

				continue # dont bother with this mesh.
			
			# XXX
			# High Quality Normals
			if EXPORT_NORMALS and face_index_pairs:
				me.calc_normals()
#				if EXPORT_NORMALS_HQ:
#					BPyMesh.meshCalcNormals(me)
#				else:
#					# transforming normals is incorrect
#					# when the matrix is scaled,
#					# better to recalculate them
#					me.calcNormals()
			
			materials = me.materials
			
			materialNames = []
			materialItems = [m for m in materials]
			if materials:
				for mat in materials:
					if mat: # !=None
						materialNames.append(mat.name)
					else:
						materialNames.append(None)
				# Cant use LC because some materials are None.
				# materialNames = map(lambda mat: mat.name, materials) # Bug Blender, dosent account for null materials, still broken.	
			
			# Possible there null materials, will mess up indicies
			# but at least it will export, wait until Blender gets fixed.
			materialNames.extend((16-len(materialNames)) * [None])
			materialItems.extend((16-len(materialItems)) * [None])
			
			# Sort by Material, then images
			# so we dont over context switch in the obj file.
			if EXPORT_KEEP_VERT_ORDER:
				pass
			elif faceuv:
				# XXX update
				tface = me.active_uv_texture.data

				# exception only raised if Python 2.3 or lower...
				try:
					face_index_pairs.sort(key = lambda a: (a[0].material_index, tface[a[1]].image, a[0].smooth))
				except:
					face_index_pairs.sort(lambda a,b: cmp((a[0].material_index, tface[a[1]].image, a[0].smooth),
															  (b[0].material_index, tface[b[1]].image, b[0].smooth)))
			elif len(materials) > 1:
				try:
					face_index_pairs.sort(key = lambda a: (a[0].material_index, a[0].smooth))
				except:
					face_index_pairs.sort(lambda a,b: cmp((a[0].material_index, a[0].smooth),
															  (b[0].material_index, b[0].smooth)))
			else:
				# no materials
				try:
					face_index_pairs.sort(key = lambda a: a[0].smooth)
				except:
					face_index_pairs.sort(lambda a,b: cmp(a[0].smooth, b[0].smooth))
#			if EXPORT_KEEP_VERT_ORDER:
#				pass
#			elif faceuv:
#				try:	faces.sort(key = lambda a: (a.mat, a.image, a.smooth))
#				except:	faces.sort(lambda a,b: cmp((a.mat, a.image, a.smooth), (b.mat, b.image, b.smooth)))
#			elif len(materials) > 1:
#				try:	faces.sort(key = lambda a: (a.mat, a.smooth))
#				except:	faces.sort(lambda a,b: cmp((a.mat, a.smooth), (b.mat, b.smooth)))
#			else:
#				# no materials
#				try:	faces.sort(key = lambda a: a.smooth)
#				except:	faces.sort(lambda a,b: cmp(a.smooth, b.smooth))

			faces = [pair[0] for pair in face_index_pairs]
			
			# Set the default mat to no material and no image.
			contextMat = (0, 0) # Can never be this, so we will label a new material teh first chance we get.
			contextSmooth = None # Will either be true or false,  set bad to force initialization switch.
			
			if EXPORT_BLEN_OBS or EXPORT_GROUP_BY_OB:
				name1 = ob.name
				name2 = ob.data.name
				if name1 == name2:
					obnamestring = fixName(name1)
				else:
					obnamestring = '%s_%s' % (fixName(name1), fixName(name2))
				
				if EXPORT_BLEN_OBS:
					file.write('o %s\n' % obnamestring) # Write Object name
				else: # if EXPORT_GROUP_BY_OB:
					file.write('g %s\n' % obnamestring)
			
			
			# Vert
			for v in me.verts:
				file.write('v %.6f %.6f %.6f\n' % tuple(v.co))
			
			# UV
			if faceuv:
				uv_face_mapping = [[0,0,0,0] for f in faces] # a bit of a waste for tri's :/

				uv_dict = {} # could use a set() here
				uv_layer = me.active_uv_texture
				for f, f_index in face_index_pairs:

					tface = uv_layer.data[f_index]

					uvs = tface.uv
					# uvs = [tface.uv1, tface.uv2, tface.uv3]

					# # add another UV if it's a quad
					# if len(f.verts) == 4:
					# 	uvs.append(tface.uv4)

					for uv_index, uv in enumerate(uvs):
						uvkey = veckey2d(uv)
						try:
							uv_face_mapping[f_index][uv_index] = uv_dict[uvkey]
						except:
							uv_face_mapping[f_index][uv_index] = uv_dict[uvkey] = len(uv_dict)
							file.write('vt %.6f %.6f\n' % tuple(uv))

#				uv_dict = {} # could use a set() here
#				for f_index, f in enumerate(faces):
					
#					for uv_index, uv in enumerate(f.uv):
#						uvkey = veckey2d(uv)
#						try:
#							uv_face_mapping[f_index][uv_index] = uv_dict[uvkey]
#						except:
#							uv_face_mapping[f_index][uv_index] = uv_dict[uvkey] = len(uv_dict)
#							file.write('vt %.6f %.6f\n' % tuple(uv))
				
				uv_unique_count = len(uv_dict)
# 				del uv, uvkey, uv_dict, f_index, uv_index
				# Only need uv_unique_count and uv_face_mapping
			
			# NORMAL, Smooth/Non smoothed.
			if EXPORT_NORMALS:
				for f in faces:
					if f.smooth:
						for v in f:
							noKey = veckey3d(v.normal)
							if noKey not in globalNormals:
								globalNormals[noKey] = totno
								totno +=1
								file.write('vn %.6f %.6f %.6f\n' % noKey)
					else:
						# Hard, 1 normal from the face.
						noKey = veckey3d(f.normal)
						if noKey not in globalNormals:
							globalNormals[noKey] = totno
							totno +=1
							file.write('vn %.6f %.6f %.6f\n' % noKey)
			
			if not faceuv:
				f_image = None

			# XXX
			if EXPORT_POLYGROUPS:
				# Retrieve the list of vertex groups
#				vertGroupNames = me.getVertGroupNames()

				currentVGroup = ''
				# Create a dictionary keyed by face id and listing, for each vertex, the vertex groups it belongs to
				vgroupsMap = [[] for _i in range(len(me.verts))]
#				vgroupsMap = [[] for _i in xrange(len(me.verts))]
				for g in ob.vertex_groups:
#				for vertexGroupName in vertGroupNames:
					for vIdx, vWeight in getVertsFromGroup(me, g.index):
#					for vIdx, vWeight in me.getVertsFromGroup(vertexGroupName, 1):
						vgroupsMap[vIdx].append((g.name, vWeight))

			for f_index, f in enumerate(faces):
				f_v = [{"index": index, "vertex": me.verts[index]} for index in f.verts]

				# if f.verts[3] == 0:
				# 	f_v.pop()

#				f_v= f.v
				f_smooth= f.smooth
				f_mat = min(f.material_index, len(materialNames)-1)
#				f_mat = min(f.mat, len(materialNames)-1)
				if faceuv:

					tface = me.active_uv_texture.data[face_index_pairs[f_index][1]]

					f_image = tface.image
					f_uv = tface.uv
					# f_uv= [tface.uv1, tface.uv2, tface.uv3]
					# if len(f.verts) == 4:
					# 	f_uv.append(tface.uv4)
#					f_image = f.image
#					f_uv= f.uv
				
				# MAKE KEY
				if faceuv and f_image: # Object is always true.
					key = materialNames[f_mat],	 f_image.name
				else:
					key = materialNames[f_mat],	 None # No image, use None instead.

				# Write the vertex group
				if EXPORT_POLYGROUPS:
					if len(ob.vertex_groups):
						# find what vertext group the face belongs to
						theVGroup = findVertexGroupName(f,vgroupsMap)
						if	theVGroup != currentVGroup:
							currentVGroup = theVGroup
							file.write('g %s\n' % theVGroup)
#				# Write the vertex group
#				if EXPORT_POLYGROUPS:
#					if vertGroupNames:
#						# find what vertext group the face belongs to
#						theVGroup = findVertexGroupName(f,vgroupsMap)
#						if	theVGroup != currentVGroup:
#							currentVGroup = theVGroup
#							file.write('g %s\n' % theVGroup)

				# CHECK FOR CONTEXT SWITCH
				if key == contextMat:
					pass # Context alredy switched, dont do anything
				else:
					if key[0] == None and key[1] == None:
						# Write a null material, since we know the context has changed.
						if EXPORT_GROUP_BY_MAT:
							# can be mat_image or (null)
							file.write('g %s_%s\n' % (fixName(ob.name), fixName(ob.data.name)) ) # can be mat_image or (null)
						file.write('usemtl (null)\n') # mat, image
						
					else:
						mat_data= MTL_DICT.get(key)
						if not mat_data:
							# First add to global dict so we can export to mtl
							# Then write mtl
							
							# Make a new names from the mat and image name,
							# converting any spaces to underscores with fixName.
							
							# If none image dont bother adding it to the name
							if key[1] == None:
								mat_data = MTL_DICT[key] = ('%s'%fixName(key[0])), materialItems[f_mat], f_image
							else:
								mat_data = MTL_DICT[key] = ('%s_%s' % (fixName(key[0]), fixName(key[1]))), materialItems[f_mat], f_image
						
						if EXPORT_GROUP_BY_MAT:
							file.write('g %s_%s_%s\n' % (fixName(ob.name), fixName(ob.data.name), mat_data[0]) ) # can be mat_image or (null)

						file.write('usemtl %s\n' % mat_data[0]) # can be mat_image or (null)
					
				contextMat = key
				if f_smooth != contextSmooth:
					if f_smooth: # on now off
						file.write('s 1\n')
						contextSmooth = f_smooth
					else: # was off now on
						file.write('s off\n')
						contextSmooth = f_smooth
				
				file.write('f')
				if faceuv:
					if EXPORT_NORMALS:
						if f_smooth: # Smoothed, use vertex normals
							for vi, v in enumerate(f_v):
								file.write( ' %d/%d/%d' % \
												(v["index"] + totverts,
												 totuvco + uv_face_mapping[f_index][vi],
												 globalNormals[ veckey3d(v["vertex"].normal) ]) ) # vert, uv, normal
							
						else: # No smoothing, face normals
							no = globalNormals[ veckey3d(f.normal) ]
							for vi, v in enumerate(f_v):
								file.write( ' %d/%d/%d' % \
												(v["index"] + totverts,
												 totuvco + uv_face_mapping[f_index][vi],
												 no) ) # vert, uv, normal
					else: # No Normals
						for vi, v in enumerate(f_v):
							file.write( ' %d/%d' % (\
							  v["index"] + totverts,\
							  totuvco + uv_face_mapping[f_index][vi])) # vert, uv
					
					face_vert_index += len(f_v)
				
				else: # No UV's
					if EXPORT_NORMALS:
						if f_smooth: # Smoothed, use vertex normals
							for v in f_v:
								file.write( ' %d//%d' %
											(v["index"] + totverts, globalNormals[ veckey3d(v["vertex"].normal) ]) )
						else: # No smoothing, face normals
							no = globalNormals[ veckey3d(f.normal) ]
							for v in f_v:
								file.write( ' %d//%d' % (v["index"] + totverts, no) )
					else: # No Normals
						for v in f_v:
							file.write( ' %d' % (v["index"] + totverts) )
						
				file.write('\n')
			
			# Write edges.
			if EXPORT_EDGES:
				for ed in edges:
					if ed.loose:
						file.write('f %d %d\n' % (ed.verts[0] + totverts, ed.verts[1] + totverts))
				
			# Make the indicies global rather then per mesh
			totverts += len(me.verts)
			if faceuv:
				totuvco += uv_unique_count

			# clean up
			bpy.data.remove_mesh(me)

		if ob_main.dupli_type != 'NONE':
			ob_main.free_dupli_list()

	file.close()
	
	
	# Now we have all our materials, save them
	if EXPORT_MTL:
		write_mtl(scene, mtlfilename, EXPORT_COPY_IMAGES)
# 	if EXPORT_COPY_IMAGES:
# 		dest_dir = os.path.basename(filename)
# # 		dest_dir = filename
# # 		# Remove chars until we are just the path.
# # 		while dest_dir and dest_dir[-1] not in '\\/':
# # 			dest_dir = dest_dir[:-1]
# 		if dest_dir:
# 			copy_images(dest_dir)
# 		else:
# 			print('\tError: "%s" could not be used as a base for an image path.' % filename)

	print("OBJ Export time: %.2f" % (time.clock() - time1))
#	print "OBJ Export time: %.2f" % (sys.time() - time1)

def do_export(filename, context, 
			  EXPORT_APPLY_MODIFIERS = True, # not used
			  EXPORT_ROTX90 = True, # wrong
			  EXPORT_TRI = False, # ok
			  EXPORT_EDGES = False,
			  EXPORT_NORMALS = False, # not yet
			  EXPORT_NORMALS_HQ = False, # not yet
			  EXPORT_UV = True, # ok
			  EXPORT_MTL = True,
			  EXPORT_SEL_ONLY = True, # ok
			  EXPORT_ALL_SCENES = False, # XXX not working atm
			  EXPORT_ANIMATION = False,
			  EXPORT_COPY_IMAGES = False,
			  EXPORT_BLEN_OBS = True,
			  EXPORT_GROUP_BY_OB = False,
			  EXPORT_GROUP_BY_MAT = False,
			  EXPORT_KEEP_VERT_ORDER = False,
			  EXPORT_POLYGROUPS = False,
			  EXPORT_CURVE_AS_NURBS = True):
	#	Window.EditMode(0)
	#	Window.WaitCursor(1)

	base_name, ext = splitExt(filename)
	context_name = [base_name, '', '', ext] # Base name, scene name, frame number, extension
	
	orig_scene = context.scene

#	if EXPORT_ALL_SCENES:
#		export_scenes = bpy.data.scenes
#	else:
#		export_scenes = [orig_scene]

	# XXX only exporting one scene atm since changing 
	# current scene is not possible.
	# Brecht says that ideally in 2.5 we won't need such a function,
	# allowing multiple scenes open at once.
	export_scenes = [orig_scene]

	# Export all scenes.
	for scn in export_scenes:
		#		scn.makeCurrent() # If already current, this is not slow.
		#		context = scn.getRenderingContext()
		orig_frame = scn.current_frame
		
		if EXPORT_ALL_SCENES: # Add scene name into the context_name
			context_name[1] = '_%s' % BPySys_cleanName(scn.name) # WARNING, its possible that this could cause a collision. we could fix if were feeling parranoied.
		
		# Export an animation?
		if EXPORT_ANIMATION:
			scene_frames = range(scn.start_frame, context.end_frame+1) # Up to and including the end frame.
		else:
			scene_frames = [orig_frame] # Dont export an animation.
		
		# Loop through all frames in the scene and export.
		for frame in scene_frames:
			if EXPORT_ANIMATION: # Add frame to the filename.
				context_name[2] = '_%.6d' % frame
			
			scn.current_frame = frame
			if EXPORT_SEL_ONLY:
				export_objects = context.selected_objects
			else:	
				export_objects = scn.objects
			
			full_path= ''.join(context_name)
			
			# erm... bit of a problem here, this can overwrite files when exporting frames. not too bad.
			# EXPORT THE FILE.
			write(full_path, export_objects, scn,
				  EXPORT_TRI, EXPORT_EDGES, EXPORT_NORMALS,
				  EXPORT_NORMALS_HQ, EXPORT_UV, EXPORT_MTL,
				  EXPORT_COPY_IMAGES, EXPORT_APPLY_MODIFIERS,
				  EXPORT_ROTX90, EXPORT_BLEN_OBS,
				  EXPORT_GROUP_BY_OB, EXPORT_GROUP_BY_MAT, EXPORT_KEEP_VERT_ORDER,
				  EXPORT_POLYGROUPS, EXPORT_CURVE_AS_NURBS)

		
		scn.current_frame = orig_frame
	
	# Restore old active scene.
#	orig_scene.makeCurrent()
#	Window.WaitCursor(0)

	
'''
Currently the exporter lacks these features:
* nurbs
* multiple scene export (only active scene is written)
* particles
'''
class EXPORT_OT_obj(bpy.types.Operator):
	'''Save a Wavefront OBJ File'''
	
	__idname__ = "export.obj"
	__label__ = 'Export OBJ'
	
	# List of operator properties, the attributes will be assigned
	# to the class instance from the operator settings before calling.

	__props__ = [
		bpy.props.StringProperty(attr="path", name="File Path", description="File path used for exporting the OBJ file", maxlen= 1024, default= ""),

		# context group
		bpy.props.BoolProperty(attr="use_selection", name="Selection Only", description="", default= False),
		bpy.props.BoolProperty(attr="use_all_scenes", name="All Scenes", description="", default= False),
		bpy.props.BoolProperty(attr="use_animation", name="All Animation", description="", default= False),

		# object group
		bpy.props.BoolProperty(attr="use_modifiers", name="Apply Modifiers", description="", default= True),
		bpy.props.BoolProperty(attr="use_rotate90", name="Rotate X90", description="", default= True),

		# extra data group
		bpy.props.BoolProperty(attr="use_edges", name="Edges", description="", default= True),
		bpy.props.BoolProperty(attr="use_normals", name="Normals", description="", default= False),
		bpy.props.BoolProperty(attr="use_hq_normals", name="High Quality Normals", description="", default= True),
		bpy.props.BoolProperty(attr="use_uvs", name="UVs", description="", default= True),
		bpy.props.BoolProperty(attr="use_materials", name="Materials", description="", default= True),
		bpy.props.BoolProperty(attr="copy_images", name="Copy Images", description="", default= False),
		bpy.props.BoolProperty(attr="use_triangles", name="Triangulate", description="", default= False),
		bpy.props.BoolProperty(attr="use_vertex_groups", name="Polygroups", description="", default= False),
		bpy.props.BoolProperty(attr="use_nurbs", name="Nurbs", description="", default= False),

		# grouping group
		bpy.props.BoolProperty(attr="use_blen_objects", name="Objects as OBJ Objects", description="", default= True),
		bpy.props.BoolProperty(attr="group_by_object", name="Objects as OBJ Groups ", description="", default= False),
		bpy.props.BoolProperty(attr="group_by_material", name="Material Groups", description="", default= False),
		bpy.props.BoolProperty(attr="keep_vertex_order", name="Keep Vertex Order", description="", default= False)
	]
	
	def execute(self, context):

		do_export(self.path, context,
				  EXPORT_TRI=self.use_triangles,
				  EXPORT_EDGES=self.use_edges,
				  EXPORT_NORMALS=self.use_normals,
				  EXPORT_NORMALS_HQ=self.use_hq_normals,
				  EXPORT_UV=self.use_uvs,
				  EXPORT_MTL=self.use_materials,
				  EXPORT_COPY_IMAGES=self.copy_images,
				  EXPORT_APPLY_MODIFIERS=self.use_modifiers,
				  EXPORT_ROTX90=self.use_rotate90,
				  EXPORT_BLEN_OBS=self.use_blen_objects,
				  EXPORT_GROUP_BY_OB=self.group_by_object,
				  EXPORT_GROUP_BY_MAT=self.group_by_material,
				  EXPORT_KEEP_VERT_ORDER=self.keep_vertex_order,
				  EXPORT_POLYGROUPS=self.use_vertex_groups,
				  EXPORT_CURVE_AS_NURBS=self.use_nurbs,
				  EXPORT_SEL_ONLY=self.use_selection,
				  EXPORT_ALL_SCENES=self.use_all_scenes)

		return ('FINISHED',)
	
	def invoke(self, context, event):
		wm = context.manager
		wm.add_fileselect(self.__operator__)
		return ('RUNNING_MODAL',)
	
	def poll(self, context): # Poll isnt working yet
		print("Poll")
		return context.active_object != None

bpy.ops.add(EXPORT_OT_obj)

import dynamic_menu
menu_func = lambda self, context: self.layout.itemO("export.obj", text="Wavefront (.obj)...")
menu_item = dynamic_menu.add(bpy.types.INFO_MT_file_export, menu_func)

if __name__ == "__main__":
	bpy.ops.EXPORT_OT_obj(filename="/tmp/test.obj")

# CONVERSION ISSUES
# - matrix problem
# - duplis - only tested dupliverts
# - NURBS - needs API additions
# - all scenes export
# + normals calculation
# - get rid of cleanName somehow
