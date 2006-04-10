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
import BPyMesh

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
	for ch in ' /\\~!@#$%^&*()+=[];\':",./<>?\t\r\n':
		name = name.replace(ch, '_')
	return name

def sortPair(a,b):
	return min(a,b), max(a,b)

global MTL_DICT

# A Dict of Materials
# (material.name, image.name):matname_imagename # matname_imagename has gaps removed.
MTL_DICT = {} 

def write_mtl(filename):
	global MTL_DICT
	
	world = Blender.World.GetCurrent()
	if world:
		worldAmb = world.getAmb()
	else:
		worldAmb = (0,0,0) # Default value
	
	file = open(filename, "w")
	file.write('# Blender3D MTL File: %s\n' % Blender.Get('filename').split('\\')[-1].split('/')[-1])
	file.write('# Material Count: %i\n' % len(MTL_DICT))
	# Write material/image combinations we have used.
	for key, mtl_mat_name in MTL_DICT.iteritems():
		
		# Get the Blender data for the material and the image.
		# Having an image named None will make a bug, dont do it :)
		
		file.write('newmtl %s\n' % mtl_mat_name) # Define a new material: matname_imgname
		
		if key[0] == None:
			#write a dummy material here?
			file.write('Ns 0\n')
			file.write('Ka %.6f %.6f %.6f\n' %  tuple([c for c in worldAmb])  ) # Ambient, uses mirror colour,
			file.write('Kd 0.8 0.8 0.8\n')
			file.write('Ks 0.8 0.8 0.8\n')
			file.write('d 1\n') # No alpha
			file.write('illum 2\n') # light normaly	
		else:
			mat = Blender.Material.Get(key[0])
			file.write('Ns %.6f\n' % ((mat.getHardness()-1) * 1.9607843137254901) ) # Hardness, convert blenders 1-511 to MTL's 
			file.write('Ka %.6f %.6f %.6f\n' %  tuple([c*mat.getAmb() for c in worldAmb])  ) # Ambient, uses mirror colour,
			file.write('Kd %.6f %.6f %.6f\n' % tuple([c*mat.getRef() for c in mat.getRGBCol()]) ) # Diffuse
			file.write('Ks %.6f %.6f %.6f\n' % tuple([c*mat.getSpec() for c in mat.getSpecCol()]) ) # Specular
			file.write('Ni %.6f\n' % mat.getIOR()) # Refraction index
			file.write('d %.6f\n' % mat.getAlpha()) # Alpha (obj uses 'd' for dissolve)
			
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
		# Get Texface images
		if imagename != None:
			uniqueImages[imagename] = None # Should use sets here. wait until Python 2.4 is default.
		
		# Get MTex images
		if matname != None:
			mat= Material.Get(matname)
			for mtex in mat.getTextures():
				if mtex and mtex.tex.type == Blender.Texture.Types.IMAGE:
					try:
						uniqueImages[mtex.tex.image.name] = None
					except:
						pass
	
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
	
def write(filename, objects,\
EXPORT_TRI=False,  EXPORT_EDGES=False,  EXPORT_NORMALS=False,\
EXPORT_UV=True,  EXPORT_MTL=True,  EXPORT_COPY_IMAGES=False,\
EXPORT_APPLY_MODIFIERS=True,  EXPORT_BLEN_OBS=True,\
EXPORT_GROUP_BY_OB=False,  EXPORT_GROUP_BY_MAT=False):
	'''
	Basic write function. The context and options must be alredy set
	This can be accessed externaly
	eg.
	write( 'c:\\test\\foobar.obj', Blender.Object.GetSelected() ) # Using default options.
	'''
	print 'OBJ Export path: "%s"' % filename
	global MTL_DICT
	temp_mesh_name = '~tmp-mesh'

	time1 = sys.time()
	scn = Scene.GetCurrent()

	file = open(filename, "w")
	
	# Write Header
	file.write('# Blender v%s OBJ File: %s\n' % (Blender.Get('version'), Blender.Get('filename').split('/')[-1].split('\\')[-1] ))
	file.write('# www.blender3d.org\n')

	# Tell the obj file what material file to use.
	mtlfilename = '%s.mtl' % '.'.join(filename.split('.')[:-1])
	file.write('mtllib %s\n' % ( mtlfilename.split('\\')[-1].split('/')[-1] ))
	
	# Get the container mesh. - used for applying modifiers and non mesh objects.
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
		# getMeshFromObject(ob, container_mesh=None, apply_modifiers=True, vgroups=True, scn=None)
		m= BPyMesh.getMeshFromObject(ob, containerMesh, True, False, scn)
		
		if not m:
			continue
		
		# We have a valid mesh
		if EXPORT_TRI:
			# Add a dummy object to it.
			oldmode = Mesh.Mode()
			Mesh.Mode(Mesh.SelectModes['FACE'])
			quadcount = 0
			for f in m.faces:
				if len(f.v) == 4:
					f.sel = 1
					quadcount +=1
			
			if quadcount:
				tempob = Blender.Object.New('Mesh')
				tempob.link(m)
				scn.link(tempob)
				m.quadToTriangle(0) # more=0 shortest length
				oldmode = Mesh.Mode(oldmode)
				scn.unlink(tempob)
			Mesh.Mode(oldmode)
		
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
		
		materialNames = []
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
		
		
		# Sort by Material, then images
		# so we dont over context switch in the obj file.
		if m.faceUV and EXPORT_UV:
			faces.sort(lambda a,b: cmp((a.mat, a.image, a.smooth), (b.mat, b.image, b.smooth)))
		else:
			faces.sort(lambda a,b: cmp((a.mat, a.smooth), (b.mat, b.smooth)))
		
		
		# Set the default mat to no material and no image.
		contextMat = (0, 0) # Can never be this, so we will label a new material teh first chance we get.
		contextSmooth = None # Will either be true or false,  set bad to force initialization switch.
		
		if EXPORT_BLEN_OBS or EXPORT_GROUP_BY_OB:
			obnamestring = '%s_%s' % (fixName(ob.name), fixName(ob.getData(1)))
			if EXPORT_BLEN_OBS:
				file.write('o %s\n' % obnamestring) # Write Object name
			else: # if EXPORT_GROUP_BY_OB:
				file.write('g %s\n' % obnamestring)
			
		# Vert
		for v in m.verts:
			file.write('v %.6f %.6f %.6f\n' % tuple(v.co))
		
		# UV
		if m.faceUV and EXPORT_UV:
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
			if EXPORT_UV and m.faceUV and f.image: # Object is always true.
				key = materialNames[min(f.mat,len(materialNames)-1)],  f.image.name
				#key = materialNames[f.mat],  f.image.name
			else:
				key = materialNames[min(f.mat,len(materialNames)-1)],  None # No image, use None instead.
				#key = materialNames[f.mat],  None # No image, use None instead.
				
			
			# CHECK FOR CONTEXT SWITCH
			if key == contextMat:
				pass # Context alredy switched, dont do anythoing
			else:
				if key[0] == None and key[1] == None:
					# Write a null material, since we know the context has changed.
					matstring = '(null)'
					file.write('usemtl (null)\n') # mat, image
					
				else:
					try: # Faster to try then 2x dict lookups.
						# We have the material, just need to write the context switch,
						matstring = MTL_DICT[key]
						
						
					except KeyError:
						# First add to global dict so we can export to mtl
						# Then write mtl
						
						# Make a new names from the mat and image name,
						# converting any spaces to underscores with fixName.
						
						# If none image dont bother adding it to the name
						if key[1] == None:
							matstring = MTL_DICT[key] ='%s' % fixName(key[0])
						else:
							matstring = MTL_DICT[key] = '%s_%s' % (fixName(key[0]), fixName(key[1]))
				
				if EXPORT_GROUP_BY_MAT:
					file.write('g %s_%s_%s\n' % (fixName(ob.name), fixName(ob.getData(1)), matstring) ) # can be mat_image or (null)
				file.write('usemtl %s\n' % matstring) # can be mat_image or (null)
				
			contextMat = key
			
			if f.smooth != contextSmooth:
				if f.smooth:
					file.write('s 1\n')
				else:
					file.write('s off\n')
				contextSmooth = f.smooth
			
			file.write('f')
			if m.faceUV and EXPORT_UV:
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
		m.verts= None
	file.close()
	
	
	# Now we have all our materials, save them
	if EXPORT_MTL:
		write_mtl(mtlfilename)
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
	
	

def write_ui(filename):
	
	for s in Window.GetScreenInfo():
		Window.QHandle(s['id'])
	
	EXPORT_APPLY_MODIFIERS = Draw.Create(1)
	EXPORT_TRI = Draw.Create(0)
	EXPORT_EDGES = Draw.Create(0)
	EXPORT_NORMALS = Draw.Create(0)
	EXPORT_UV = Draw.Create(1)
	EXPORT_MTL = Draw.Create(1)
	EXPORT_SEL_ONLY = Draw.Create(1)
	EXPORT_ALL_SCENES = Draw.Create(0)
	EXPORT_ANIMATION = Draw.Create(0)
	EXPORT_COPY_IMAGES = Draw.Create(0)
	EXPORT_BLEN_OBS = Draw.Create(1)
	EXPORT_GROUP_BY_OB = Draw.Create(0)
	EXPORT_GROUP_BY_MAT = Draw.Create(0)
	
	
	# Get USER Options
	pup_block = [\
	('Mesh Options...'),\
	('Apply Modifiers', EXPORT_APPLY_MODIFIERS, 'Use transformed mesh data from each object. May break vert order for morph targets.'),\
	('Triangulate', EXPORT_TRI, 'Triangulate quadsModifiers.'),\
	('Edges', EXPORT_EDGES, 'Edges not connected to faces.'),\
	('Normals', EXPORT_NORMALS, 'Export vertex normal data (Ignored on import).'),\
	('UVs', EXPORT_UV, 'Export texface UV coords.'),\
	('Materials', EXPORT_MTL, 'Write a separate MTL file with the OBJ.'),\
	('Context...'),\
	('Selection Only', EXPORT_SEL_ONLY, 'Only export objects in visible selection. Else export whole scene.'),\
	('All Scenes', EXPORT_ALL_SCENES, 'Each scene as a seperate OBJ file.'),\
	('Animation', EXPORT_ANIMATION, 'Each frame as a numbered OBJ file.'),\
	('Copy Images', EXPORT_COPY_IMAGES, 'Copy image files to the export directory, never overwrite.'),\
	('Grouping...'),\
	('Objects', EXPORT_BLEN_OBS, 'Export blender objects as OBJ objects.'),\
	('Object Groups', EXPORT_GROUP_BY_OB, 'Export blender objects as OBJ groups.'),\
	('Material Groups', EXPORT_GROUP_BY_MAT, 'Group by materials.'),\
	]
	
	if not Draw.PupBlock('Export...', pup_block):
		return
	
	Window.WaitCursor(1)
	
	EXPORT_APPLY_MODIFIERS = EXPORT_APPLY_MODIFIERS.val
	EXPORT_TRI = EXPORT_TRI.val
	EXPORT_EDGES = EXPORT_EDGES.val
	EXPORT_NORMALS = EXPORT_NORMALS.val
	EXPORT_UV = EXPORT_UV.val
	EXPORT_MTL = EXPORT_MTL.val
	EXPORT_SEL_ONLY = EXPORT_SEL_ONLY.val
	EXPORT_ALL_SCENES = EXPORT_ALL_SCENES.val
	EXPORT_ANIMATION = EXPORT_ANIMATION.val
	EXPORT_COPY_IMAGES = EXPORT_COPY_IMAGES.val
	EXPORT_BLEN_OBS = EXPORT_BLEN_OBS.val
	EXPORT_GROUP_BY_OB = EXPORT_GROUP_BY_OB.val
	EXPORT_GROUP_BY_MAT = EXPORT_GROUP_BY_MAT.val
	
	
	
	base_name, ext = splitExt(filename)
	context_name = [base_name, '', '', ext] # basename, scene_name, framenumber, extension
	
	# Use the options to export the data using write()
	# def write(filename, objects, EXPORT_EDGES=False, EXPORT_NORMALS=False, EXPORT_MTL=True, EXPORT_COPY_IMAGES=False, EXPORT_APPLY_MODIFIERS=True):
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
				export_objects = Blender.Object.GetSelected() # Export Context
			else:	
				export_objects = scn.getChildren()
			
			# EXPORT THE FILE.
			write(''.join(context_name), export_objects,\
			EXPORT_TRI, EXPORT_EDGES, EXPORT_NORMALS,\
			EXPORT_UV, EXPORT_MTL, EXPORT_COPY_IMAGES,\
			EXPORT_APPLY_MODIFIERS,\
			EXPORT_BLEN_OBS, EXPORT_GROUP_BY_OB, EXPORT_GROUP_BY_MAT)
		
		Blender.Set('curframe', orig_frame)
	
	# Restore old active scene.
	orig_scene.makeCurrent()
	Window.WaitCursor(0)


if __name__ == '__main__':
	Window.FileSelector(write_ui, 'Export Wavefront OBJ', sys.makename(ext='.obj'))
