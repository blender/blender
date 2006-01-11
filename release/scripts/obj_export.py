#!BPY

"""
Name: 'Wavefront (.obj)...'
Blender: 232
Group: 'Export'
Tooltip: 'Save a Wavefront OBJ File'
"""

__author__ = "Campbell Barton, Jiri Hnidek"
__url__ = ["blender", "elysiun"]
__version__ = "1.0"

__bpydoc__ = """\
This script is an exporter to OBJ file format.

Usage:

Run this script from "File->Export" menu to export all meshes.
"""


# --------------------------------------------------------------------------
# OBJ Export v1.0 by Campbell Barton (AKA Ideasman)
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
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ***** END GPL LICENCE BLOCK *****
# --------------------------------------------------------------------------


import Blender
from Blender import Mesh, Scene, Window, sys, Image, Draw

#==================================================#
# New name based on old with a different extension #
#==================================================#
def newFName(ext):
	return Blender.Get('filename')[: -len(Blender.Get('filename').split('.', -1)[-1]) ] + ext

# Returns a tuple - path,extension.
# 'hello.obj' >  ('hello', '.obj')
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

# Used to add the scene name into the filename without using odd chars
def saneFilechars(name):
	for ch in ' /\\~!@#$%^&*()+=[];\':",./<>?':
		name = name.replace(ch, '_')
	return name

def sortPair(a,b):
	return min(a,b), max(a,b)

def getMeshFromObject(object, name=None, mesh=None):
	if mesh:
		mesh.verts = None # Clear the meshg
	else:
		if not name:
			mesh = Mesh.New()
		else:
			mesh = Mesh.New(name)
	
	
	type = object.getType()
	dataname = object.getData(1)
	
	try:
		mesh.getFromObject(object.name) 
	except:
		return None
	
	if type == 'Mesh':
		tempMe = Mesh.Get( dataname )
		mesh.materials = tempMe.materials
		mesh.degr = tempMe.degr
		mesh.mode = tempMe.mode
	else:
		try:
			# Will only work for curves!!
			# Text- no material access in python interface.
			# Surf- no python interface
			# MBall- no material access in python interface.
			
			data = object.getData()
			materials = data.getMaterials()
			mesh.materials = materials
			print 'assigning materials for non mesh'
		except:
			print 'Cant assign materials to', type
	
	return mesh

global MTL_DICT

# A Dict of Materials
# (material.name, image.name):matname_imagename # matname_imagename has gaps removed.
MTL_DICT = {} 

def save_mtl(filename):
	global MTL_DICT
	
	world = Blender.World.GetCurrent()
	if world:
		worldAmb = world.getAmb()
	else:
		worldAmb = (0,0,0) # Default value
	
	file = open(filename, "w")
	file.write('# Blender MTL File: %s\n' % Blender.Get('filename').split('\\')[-1].split('/')[-1])
	file.write('# Material Count: %i\n' % len(MTL_DICT))
	# Write material/image combinations we have used.
	for key, mtl_mat_name in MTL_DICT.iteritems():
		
		# Get the Blender data for the material and the image.
		# Having an image named None will make a bug, dont do it :)
		
		file.write('newmtl %s\n' % mtl_mat_name) # Define a new material: matname_imgname
		
		if key[0] == None:
			#write a dummy material here?
			file.write('Ns 0\n')
			file.write('Ka %s %s %s\n' %  tuple([round(c, 6) for c in worldAmb])  ) # Ambient, uses mirror colour,
			file.write('Kd 0.8 0.8 0.8\n')
			file.write('Ks 0.8 0.8 0.8\n')
			file.write('d 1\n') # No alpha
			file.write('illum 2\n') # light normaly	
			
		else:
			mat = Blender.Material.Get(key[0])
			file.write('Ns %s\n' % round((mat.getHardness()-1) * 1.9607843137254901 ) ) # Hardness, convert blenders 1-511 to MTL's 
			file.write('Ka %s %s %s\n' %  tuple([round(c*mat.getAmb(), 6) for c in worldAmb])  ) # Ambient, uses mirror colour,
			file.write('Kd %s %s %s\n' % tuple([round(c*mat.getRef(), 6) for c in mat.getRGBCol()]) ) # Diffuse
			file.write('Ks %s %s %s\n' % tuple([round(c*mat.getSpec(), 6) for c in mat.getSpecCol()]) ) # Specular
			file.write('Ni %s\n' % round(mat.getIOR(), 6)) # Refraction index
			file.write('d %s\n' % round(mat.getAlpha(), 6)) # Alpha (obj uses 'd' for dissolve)
			
			# 0 to disable lighting, 1 for ambient & diffuse only (specular color set to black), 2 for full lighting.
			if mat.getMode() & Blender.Material.Modes['SHADELESS']:
				file.write('illum 0\n') # ignore lighting
			elif mat.getSpec() == 0:
				file.write('illum 1\n') # no specular.
			else:
				file.write('illum 2\n') # light normaly	
		
		
		# Write images!
		if key[1] != None:  # We have an image on the face!
			img = Image.Get(key[1])
			file.write('map_Kd %s\n' % img.filename.split('\\')[-1].split('/')[-1]) # Diffuse mapping image			
		
		elif key[0] != None: # No face image. if we havea material search for MTex image.
			for mtex in mat.getTextures():
				if mtex and mtex.tex.type == Blender.Texture.Types.IMAGE:
					try:
						filename = mtex.tex.image.filename.split('\\')[-1].split('/')[-1]
						file.write('map_Kd %s\n' % filename) # Diffuse mapping image
						break
					except:
						# Texture has no image though its an image type, best ignore.
						pass
		
		file.write('\n\n')
	
	file.close()

def copy_file(source, dest):
	file = open(source, 'rb')
	data = file.read()
	file.close()
	
	file = open(dest, 'wb')
	file.write(data)
	file.close()


def copy_images(dest_dir):
	if dest_dir[-1] != sys.sep:
		dest_dir += sys.sep
	
	# Get unique image names
	uniqueImages = {}
	for matname, imagename in MTL_DICT.iterkeys(): # Only use image name
		if imagename != None:
			uniqueImages[imagename] = None # Should use sets here. wait until Python 2.4 is default.
	
	# Now copy images
	copyCount = 0
	
	for imageName in uniqueImages.iterkeys():
		print imageName
		bImage = Image.Get(imageName)
		image_path = sys.expandpath(bImage.filename)
		if sys.exists(image_path):
			# Make a name for the target path.
			dest_image_path = dest_dir + image_path.split('\\')[-1].split('/')[-1]
			if not sys.exists(dest_image_path): # Image isnt alredy there
				print '\tCopying "%s" > "%s"' % (image_path, dest_image_path)
				copy_file(image_path, dest_image_path)
				copyCount+=1
	print '\tCopied %d images' % copyCount
	
def save_obj(filename, objects, EXPORT_EDGES=False, EXPORT_NORMALS=False, EXPORT_MTL=True, EXPORT_COPY_IMAGES=False, EXPORT_APPLY_MODIFIERS=True):
	'''
	Basic save function. The context and options must be alredy set
	This can be accessed externaly
	eg.
	save_obj( 'c:\\test\\foobar.obj', Blender.Object.GetSelected() ) # Using default options.
	'''
	print 'OBJ Export path: "%s"' % filename
	global MTL_DICT
	temp_mesh_name = '~tmp-mesh'
	time1 = sys.time()
	scn = Scene.GetCurrent()

	file = open(filename, "w")
	
	# Write Header
	file.write('# Blender OBJ File: %s\n' % (Blender.Get('filename').split('/')[-1].split('\\')[-1] ))
	file.write('# www.blender.org\n')

	# Tell the obj file what material file to use.
	mtlfilename = '%s.mtl' % '.'.join(filename.split('.')[:-1])
	file.write('mtllib %s\n' % ( mtlfilename.split('\\')[-1].split('/')[-1] ))
	
	# Get the container mesh.
	if EXPORT_APPLY_MODIFIERS:
		containerMesh = meshName = tempMesh = None
		for meshName in Blender.NMesh.GetNames():
			if meshName.startswith(temp_mesh_name):
				tempMesh = Mesh.Get(meshName)
				if not tempMesh.users:
					containerMesh = tempMesh
		if not containerMesh:
			containerMesh = Mesh.New(temp_mesh_name)
		del meshName
		del tempMesh
	
	
	
	# Initialize totals, these are updated each object
	totverts = totuvco = totno = 1
	
	globalUVCoords = {}
	globalNormals = {}
	
	# Get all meshs
	for ob in objects:
		
		# Will work for non meshes now! :)
		if EXPORT_APPLY_MODIFIERS or ob.getType() != 'Mesh':
			m = getMeshFromObject(ob, temp_mesh_name, containerMesh)
			if not m:
				continue
		else: # We are a mesh. get the data.
			m = ob.getData(mesh=1)
		
		faces = [ f for f in m.faces ]
		if EXPORT_EDGES:
			edges = [ ed for ed in m.edges ]
		else:
			edges = []
			
		if not (len(faces)+len(edges)): # Make sure there is somthing to write
			continue # dont bother with this mesh.
		
		m.transform(ob.matrix)
		
		# # Crash Blender
		#materials = m.getMaterials(1) # 1 == will return None in the list.
		materials = m.materials
		
		
		if materials:
			materialNames = map(lambda mat: mat.name, materials) # Bug Blender, dosent account for null materials, still broken.	
		else:
			materialNames = []
		
		# Possible there null materials, will mess up indicies
		# but at least it will export, wait until Blender gets fixed.
		materialNames.extend((16-len(materialNames)) * [None])
		
		
		# Sort by Material, then images
		# so we dont over context switch in the obj file.
		if m.faceUV:
			faces.sort(lambda a,b: cmp((a.mat, a.image, a.smooth), (b.mat, b.image, b.smooth)))
		else:
			faces.sort(lambda a,b: cmp((a.mat, a.smooth), (b.mat, b.smooth)))
		
		
		# Set the default mat to no material and no image.
		contextMat = (0, 0) # Can never be this, so we will label a new material teh first chance we get.
		contextSmooth = None # Will either be true or false,  set bad to force initialization switch.
		
		file.write('o %s_%s\n' % (fixName(ob.name), fixName(m.name))) # Write Object name
		
		# Vert
		for v in m.verts:
			file.write('v %.6f %.6f %.6f\n' % tuple(v.co))
		
		# UV
		if m.faceUV:
			for f in faces:
				for uvKey in f.uv:
					uvKey = tuple(uvKey)
					if not globalUVCoords.has_key(uvKey):
						globalUVCoords[uvKey] = totuvco
						totuvco +=1
						file.write('vt %.6f %.6f 0.0\n' % uvKey)
		
		# NORMAL, Smooth/Non smoothed.
		if EXPORT_NORMALS:
			for f in faces:
				if f.smooth:
					for v in f.v:
						noKey = tuple(v.no)
						if not globalNormals.has_key( noKey ):
							globalNormals[noKey] = totno
							totno +=1
							file.write('vn %.6f %.6f %.6f\n' % noKey)
				else:
					# Hard, 1 normal from the face.
					noKey = tuple(f.no)
					if not globalNormals.has_key( noKey ):
						globalNormals[noKey] = totno
						totno +=1
						file.write('vn %.6f %.6f %.6f\n' % noKey)
		
		
		uvIdx = 0
		for f in faces:
			
			# MAKE KEY
			if m.faceUV and f.image: # Object is always true.
				key = materialNames[f.mat],  f.image.name
			else:
				key = materialNames[f.mat],  None # No image, use None instead.
			
			# CHECK FOR CONTEXT SWITCH
			if key == contextMat:
				pass # Context alredy switched, dont do anythoing
			elif key[0] == None and key[1] == None:
				# Write a null material, since we know the context has changed.
				file.write('usemtl (null)\n') # mat, image
				
			else:
				try: # Faster to try then 2x dict lookups.
					
					# We have the material, just need to write the context switch,
					file.write('usemtl %s\n' % MTL_DICT[key]) # mat, image
					
				except KeyError:
					# First add to global dict so we can export to mtl
					# Then write mtl
					
					# Make a new names from the mat and image name,
					# converting any spaces to underscores with fixName.
					
					# If none image dont bother adding it to the name
					if key[1] == None:
						tmp_matname = MTL_DICT[key] ='%s' % fixName(key[0])
						file.write('usemtl %s\n' % tmp_matname) # mat, image
						
					else:
						tmp_matname = MTL_DICT[key] = '%s_%s' % (fixName(key[0]), fixName(key[1]))
						file.write('usemtl %s\n' % tmp_matname) # mat, image
				
			contextMat = key
			
			if f.smooth != contextSmooth:
				if f.smooth:
					file.write('s 1\n')
				else:
					file.write('s off\n')
				contextSmooth = f.smooth
			
			file.write('f')
			if m.faceUV:
				if EXPORT_NORMALS:
					if f.smooth: # Smoothed, use vertex normals
						for vi, v in enumerate(f.v):
							file.write( ' %d/%d/%d' % (\
							  v.index+totverts,\
							  globalUVCoords[ tuple(f.uv[vi]) ],\
							  globalNormals[ tuple(v.no) ])) # vert, uv, normal
					else: # No smoothing, face normals
						no = globalNormals[ tuple(f.no) ]
						for vi, v in enumerate(f.v):
							file.write( ' %d/%d/%d' % (\
							  v.index+totverts,\
							  globalUVCoords[ tuple(f.uv[vi]) ],\
							  no)) # vert, uv, normal
				
				else: # No Normals
					for vi, v in enumerate(f.v):
						file.write( ' %d/%d' % (\
						  v.index+totverts,\
						  globalUVCoords[ tuple(f.uv[vi])])) # vert, uv
					
					
			else: # No UV's
				if EXPORT_NORMALS:
					if f.smooth: # Smoothed, use vertex normals
						for v in f.v:
							file.write( ' %d//%d' % (\
							  v.index+totverts,\
							  globalNormals[ tuple(v.no) ]))
					else: # No smoothing, face normals
						no = globalNormals[ tuple(f.no) ]
						for v in f.v:
							file.write( ' %d//%d' % (\
							  v.index+totverts,\
							  no))
				else: # No Normals
					for v in f.v:
						file.write( ' %d' % (\
						  v.index+totverts))
					
			file.write('\n')
		
		# Write edges.
		if EXPORT_EDGES:
			edgeUsers = {}
			for f in faces:
				for i in xrange(len(f.v)):
					faceEdgeVKey = sortPair(f.v[i].index, f.v[i-1].index)
					
					# We dont realy need to keep count. Just that a face uses it 
					# so dont export.
					edgeUsers[faceEdgeVKey] = 1 
				
			for ed in edges:
				edgeVKey = sortPair(ed.v1.index, ed.v2.index)
				if not edgeUsers.has_key(edgeVKey): # No users? Write the edge.
					file.write('f %d %d\n' % (edgeVKey[0]+totverts, edgeVKey[1]+totverts))
		
		# Make the indicies global rather then per mesh
		totverts += len(m.verts)
	file.close()
	
	
	# Now we have all our materials, save them
	if EXPORT_MTL:
		save_mtl(mtlfilename)
	if EXPORT_COPY_IMAGES:
		dest_dir = filename
		# Remove chars until we are just the path.
		while dest_dir and dest_dir[-1] not in '\\/':
			dest_dir = dest_dir[:-1]
		if dest_dir:
			copy_images(dest_dir)
		else:
			print '\tError: "%s" could not be used as a base for an image path.' % filename
	
	print "OBJ Export time: %.2f" % (sys.time() - time1)
	
	
	
	
	
	

def save_obj_ui(filename):
	
	for s in Window.GetScreenInfo():
		Window.QHandle(s['id'])
	
	EXPORT_APPLY_MODIFIERS = Draw.Create(1)
	EXPORT_SEL_ONLY = Draw.Create(0)
	EXPORT_EDGES = Draw.Create(0)
	EXPORT_NORMALS = Draw.Create(0)
	EXPORT_MTL = Draw.Create(1)
	EXPORT_ALL_SCENES = Draw.Create(0)
	EXPORT_ANIMATION = Draw.Create(0)
	EXPORT_COPY_IMAGES = Draw.Create(0)
	
	
	# Get USER Options
	pup_block = [\
	('Apply Modifiers', EXPORT_APPLY_MODIFIERS, 'Use transformed mesh data from each object. May break vert order for morph targets.'),\
	('Selection Only', EXPORT_SEL_ONLY, 'Only export objects in visible selection.'),\
	('Edges', EXPORT_EDGES, 'Edges not connected to faces.'),\
	('Normals', EXPORT_NORMALS, 'Export vertex normal data (Ignored on import).'),\
	('Materials', EXPORT_MTL, 'Write a seperate MTL file with the OBJ.'),\
	('All Scenes', EXPORT_ALL_SCENES, 'Each scene as a seperate OBJ file.'),\
	('Animation', EXPORT_ANIMATION, 'Each frame as a seperate OBJ file.'),\
	('Copy Images', EXPORT_COPY_IMAGES, 'Copy image files to the export directory, never everwrite.'),\
	]
	
	if not Draw.PupBlock('Export...', pup_block):
		return
	
	Window.WaitCursor(1)
	
	EXPORT_APPLY_MODIFIERS = EXPORT_APPLY_MODIFIERS.val
	EXPORT_SEL_ONLY = EXPORT_SEL_ONLY.val
	EXPORT_EDGES = EXPORT_EDGES.val
	EXPORT_NORMALS = EXPORT_NORMALS.val
	EXPORT_MTL = EXPORT_MTL.val
	EXPORT_ALL_SCENES = EXPORT_ALL_SCENES.val
	EXPORT_ANIMATION = EXPORT_ANIMATION.val
	EXPORT_COPY_IMAGES = EXPORT_COPY_IMAGES.val
	
	
	
	base_name, ext = splitExt(filename)
	context_name = [base_name, '', '', ext] # basename, scene_name, framenumber, extension
	
	# Use the options to export the data using save_obj()
	# def save_obj(filename, objects, EXPORT_EDGES=False, EXPORT_NORMALS=False, EXPORT_MTL=True, EXPORT_COPY_IMAGES=False, EXPORT_APPLY_MODIFIERS=True):
	orig_scene = Scene.GetCurrent()
	if EXPORT_ALL_SCENES:
		export_scenes = Scene.Get()
	else:
		export_scenes = [orig_scene]
	
	# Export all scenes.
	for scn in export_scenes:
		scn.makeCurrent() # If alredy current, this is not slow.
		context = scn.getRenderingContext()
		orig_frame = Blender.Get('curframe')
		
		if EXPORT_ALL_SCENES: # Add scene name into the context_name
			context_name[1] = '_%s' % saneFilechars(scn.name) # WARNING, its possible that this could cause a collision. we could fix if were feeling parranoied.
		
		# Export an animation?
		if EXPORT_ANIMATION:
			scene_frames = range(context.startFrame(), context.endFrame()+1) # up to and including the end frame.
		else:
			scene_frames = [orig_frame] # Dont export an animation.
		
		# Loop through all frames in the scene and export.
		for frame in scene_frames:
			if EXPORT_ANIMATION: # Add frame to the filename.
				context_name[2] = '_%.6d' % frame
			
			Blender.Set('curframe', frame)
			if EXPORT_SEL_ONLY:
				export_objects = Object.GetSelected() # Export Context
			else:
				export_objects = scn.getChildren()
			
			# EXPORTTHE FILE.
			save_obj(''.join(context_name), export_objects, EXPORT_EDGES, EXPORT_NORMALS, EXPORT_MTL, EXPORT_COPY_IMAGES, EXPORT_APPLY_MODIFIERS)
		
		Blender.Set('curframe', orig_frame)
	
	# Restore old active scene.
	orig_scene.makeCurrent()
	Window.WaitCursor(0)
	
	

Window.FileSelector(save_obj_ui, 'Export Wavefront OBJ', newFName('obj'))
