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
# OBJ Export v0.9b by Campbell Barton (AKA Ideasman)
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

#==================================================#
# New name based on old with a different extension #
#==================================================#
def newFName(ext):
	return Get('filename')[: -len(Get('filename').split('.', -1)[-1]) ] + ext


def fixName(name):
	if name == None:
		return 'None'
	else:
		return name.replace(' ', '_')




from Blender import *

global MTL_DICT

# A Dict of Materials
# (material.name, image.name):matname_imagename # matname_imagename has gaps removed.
MTL_DICT = {} 

def save_mtl(filename):
	global MTL_DICT
	
	world = World.GetCurrent()
	if world:
		worldAmb = world.getAmb()
	else:
		worldAmb = (0,0,0) # Default value
	
	file = open(filename, "w")
	file.write('# Blender MTL File: %s\n' % Get('filename').split('\\')[-1].split('/')[-1])
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
			mat = Material.Get(key[0])
			file.write('Ns %s\n' % round((mat.getHardness()-1) * 1.9607843137254901 ) ) # Hardness, convert blenders 1-511 to MTL's 
			file.write('Ka %s %s %s\n' %  tuple([round(c*mat.getAmb(), 6) for c in worldAmb])  ) # Ambient, uses mirror colour,
			file.write('Kd %s %s %s\n' % tuple([round(c*mat.getRef(), 6) for c in mat.getRGBCol()]) ) # Diffuse
			file.write('Ks %s %s %s\n' % tuple([round(c*mat.getSpec(), 6) for c in mat.getSpecCol()]) ) # Specular
			file.write('Ni %s\n' % round(mat.getIOR(), 6)) # Refraction index
			file.write('d %s\n' % round(mat.getAlpha(), 6)) # Alpha (obj uses 'd' for dissolve)
			
			# 0 to disable lighting, 1 for ambient & diffuse only (specular color set to black), 2 for full lighting.
			if mat.getMode() & Material.Modes['SHADELESS']:
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
				if mtex and mtex.tex.type == Texture.Types.IMAGE:
					try:
						filename = mtex.tex.image.filename.split('\\')[-1].split('/')[-1]
						file.write('map_Kd %s\n' % filename) # Diffuse mapping image
						break
					except:
						# Texture has no image though its an image type, best ignore.
						pass
		
		file.write('\n\n')
	
	file.close()



def save_obj(filename):
	global MTL_DICT
	
	time1 = sys.time()
	scn = Scene.GetCurrent()

	file = open(filename, "w")
	
	# Write Header
	file.write('# Blender OBJ File: %s\n' % (Get('filename').split('/')[-1].split('\\')[-1] ))
	file.write('# www.blender.org\n')

	# Tell the obj file what material file to use.
	mtlfilename = '%s.mtl' % '.'.join(filename.split('.')[:-1])
	file.write('mtllib %s\n' % ( mtlfilename.split('\\')[-1].split('/')[-1] ))

	# Initialize totals, these are updated each object
	totverts = totuvco = totno = 1
	
	globalUVCoords = {}
	globalNormals = {}
	
	# Get all meshs
	for ob in scn.getChildren():
		#for ob in Object.GetSelected():
		try:
			# Will work for non meshes now! :)
			m = NMesh.GetRawFromObject(ob.name)
		except:
			continue
		
		faces = [ f for f in m.faces if len(f) > 2 ]
		
		if not faces: # Make sure there is somthing to write
			continue # dont bother with this mesh.
		
		m.transform(ob.matrix)
		
		# # Crash Blender
		#materials = m.getMaterials(1) # 1 == will return None in the list.
		materials = m.getMaterials()
		
		
		if materials:
			materialNames = map(lambda mat: mat.name, materials) # Bug Blender, dosent account for null materials, still broken.	
		else:
			materialNames = []
		
		# Possible there null materials, will mess up indicies
		# but at least it will export, wait until Blender gets fixed.
		materialNames.extend((16-len(materialNames)) * [None])
		
		
		# Sort by Material, then images
		# so we dont over context switch in the obj file.
		faces.sort(lambda a,b: cmp((a.mat, a.image, a.smooth), (b.mat, b.image, b.smooth)))
		
		
		# Set the default mat to no material and no image.
		contextMat = (0, 0) # Can never be this, so we will label a new material teh first chance we get.
		contextSmooth = None # Will either be true or false,  set bad to force initialization switch.
		
		file.write('o %s_%s\n' % (fixName(ob.name), fixName(m.name))) # Write Object name
		
		# Vert
		for v in m.verts:
			file.write('v %.6f %.6f %.6f\n' % tuple(v.co))
		
		# UV
		if m.hasFaceUV():
			for f in faces:
				for uvKey in f.uv:
					if not globalUVCoords.has_key(uvKey):
						globalUVCoords[uvKey] = totuvco
						totuvco +=1
						file.write('vt %.6f %.6f 0.0\n' % uvKey)
		
		# NORMAL, Smooth/Non smoothed.
		
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
			if f.image: # Object is always true.
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
			if m.hasFaceUV():
				if f.smooth: # Smoothed, use vertex normals
					for vi, v in enumerate(f.v):
						file.write( ' %d/%d/%d' % (\
						  v.index+totverts,\
						  globalUVCoords[ f.uv[vi] ],\
						  globalNormals[ tuple(v.no) ])) # vert, uv, normal
				else: # No smoothing, face normals
					no = globalNormals[ tuple(f.no) ]
					for vi, v in enumerate(f.v):
						file.write( ' %d/%d/%d' % (\
						  v.index+totverts,\
						  globalUVCoords[ f.uv[vi] ],\
						  no)) # vert, uv, normal
			
			else: # No UV's
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
					
			file.write('\n')
		
		# Make the indicies global rather then per mesh
		totverts += len(m.verts)
	file.close()
	
	
	# Now we have all our materials, save them
	save_mtl(mtlfilename)
	
	print "obj export time: %.2f" % (sys.time() - time1)

Window.FileSelector(save_obj, 'Export Wavefront OBJ', newFName('obj'))

'''
TIME = sys.time()
import os
OBJDIR = '/obj_out/'
for scn in Scene.Get():
	scn.makeCurrent()
	obj = OBJDIR + scn.name
	print obj
	save_obj(obj)

print "TOTAL EXPORT TIME: ", sys.time() - TIME
'''
