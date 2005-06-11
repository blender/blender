#!BPY
 
"""
Name: 'Wavefront (.obj)...'
Blender: 232
Group: 'Import'
Tooltip: 'Load a Wavefront OBJ File'
"""

__author__ = "Campbell Barton"
__url__ = ["blender", "elysiun"]
__version__ = "0.9"

__bpydoc__ = """\
This script imports OBJ files to Blender.

Usage:

Run this script from "File->Import" menu and then load the desired OBJ file.
"""

# $Id$
#
# --------------------------------------------------------------------------
# OBJ Import v0.9 by Campbell Barton (AKA Ideasman)
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

NULL_MAT = '(null)' # Name for mesh's that have no mat set.
NULL_IMG = '(null)' # Name for mesh's that have no mat set.

MATLIMIT = 16 # This isnt about to change but probably should not be hard coded.

DIR = ''

#==============================================#
# Return directory, where is file              #
#==============================================#
def pathName(path,name):
	length=len(path)
	for CH in range(1, length):
		if path[length-CH:] == name:
			path = path[:length-CH]
			break
	return path

#==============================================#
# Strips the slashes from the back of a string #
#==============================================#
def stripPath(path):
	return path.split('/')[-1].split('\\')[-1]
	
#====================================================#
# Strips the prefix off the name before writing      #
#====================================================#
def stripName(name): # name is a string
	prefixDelimiter = '.'
	return name[ : name.find(prefixDelimiter) ]


from Blender import *
import sys as py_sys

#==================================================================================#
# This function sets textures defined in .mtl file                                 #
#==================================================================================#
def getImg(img_fileName):
	for i in Image.Get():
		if i.filename == img_fileName:
			return i
	
	# if we are this far it means the image hasnt been loaded.
	try:
		return Image.Load(img_fileName)
	except IOError:
		print '\tunable to open image file: "%s"' % img_fileName
		return



#==================================================================================#
# This function sets textures defined in .mtl file                                 #
#==================================================================================#
def load_mat_image(mat, img_fileName, type, meshDict):
	
	
	texture = Texture.New(type)
	texture.setType('Image')
	
	image = getImg(img_fileName)
	if image:
		texture.image = image
	
	# adds textures to faces (Textured/Alt-Z mode)
	# Only apply the diffuse texture to the face if the image has not been set with the inline usemat func.
	if type == 'Kd':
		for meshPair in meshDict.values():
			for f in meshPair[0].faces:
				if meshPair[0].materials[f.mat].name == mat.name:
					# the inline usemat command overides the material Image
					if not f.image:
					  f.image = image
		
	# adds textures for materials (rendering)
	elif type == 'Ka':
		mat.setTexture(0, texture, Texture.TexCo.UV, Texture.MapTo.CMIR)
	elif type == 'Kd':
		mat.setTexture(1, texture, Texture.TexCo.UV, Texture.MapTo.COL)
	elif type == 'Ks':
		mat.setTexture(2, texture, Texture.TexCo.UV, Texture.MapTo.SPEC)

#==================================================================================#
# This function loads materials from .mtl file (have to be defined in obj file)    #
#==================================================================================#
def load_mtl(dir, mtl_file, meshDict):
	
	#===============================================================================#
	# This gets a mat or creates one of the requested name if none exist.           #
	#===============================================================================#
	def getMat(matName):
		# Make a new mat
		try:
			return Material.Get(matName)
		except NameError:
			return Material.New(matName)
			
	mtl_file = stripPath(mtl_file)
	mtl_fileName = dir + mtl_file
	
	try:
		fileLines= open(mtl_fileName, 'r').readlines()
	except IOError:
		print '\tunable to open referenced material file: "%s"' % mtl_fileName
		return
	
	lIdx=0
	while lIdx < len(fileLines):
		l = fileLines[lIdx].split()
	
		# Detect a line that will be ignored
		if len(l) == 0:
			pass
		elif l[0] == '#' or len(l) == 0:
			pass
		elif l[0] == 'newmtl':
			currentMat = getMat('_'.join(l[1:])) # Material should alredy exist.
		elif l[0] == 'Ka':
			currentMat.setMirCol(float(l[1]), float(l[2]), float(l[3]))
		elif l[0] == 'Kd':
			currentMat.setRGBCol(float(l[1]), float(l[2]), float(l[3]))
		elif l[0] == 'Ks':
			currentMat.setSpecCol(float(l[1]), float(l[2]), float(l[3]))
		elif l[0] == 'Ns':
			currentMat.setHardness( int((float(l[1])*0.51)) )
		elif l[0] == 'd':
			currentMat.setAlpha(float(l[1]))
		elif l[0] == 'Tr':
			currentMat.setAlpha(float(l[1]))
		elif l[0] == 'map_Ka':
			img_fileName = dir + l[1]
			load_mat_image(currentMat, img_fileName, 'Ka', meshDict)
		elif l[0] == 'map_Ks':
			img_fileName = dir + l[1]
			load_mat_image(currentMat, img_fileName, 'Ks', meshDict)
		elif l[0] == 'map_Kd':
			img_fileName = dir + l[1]
			load_mat_image(currentMat, img_fileName, 'Kd', meshDict)
		lIdx+=1

#===========================================================================#
# Returns unique name of object/mesh (preserve overwriting existing meshes) #
#===========================================================================#
def getUniqueName(name):
	newName = name
	uniqueInt = 0
	while 1:
		try:
			ob = Object.Get(newName)
			# Okay, this is working, so lets make a new name
			newName = '%s.%d' % (name, uniqueInt)
			uniqueInt +=1
		except AttributeError:
			if newName not in NMesh.GetNames():
				return newName
			else:
				newName = '%s.%d' % (name, uniqueInt)
				uniqueInt +=1


# Gets the meshs index for this material, -1 if its not in the list
def getMeshMaterialIndex(mesh, material):
	meshMatIndex = -1
	matIdx = 0
	meshMatList = mesh.materials
	while matIdx < len(meshMatList):
		if meshMatList[matIdx].name == material.name:
			meshMatIndex = matIdx # The current mat index.
			break
		matIdx+=1
	# -1 if not found
	return meshMatIndex




#==================================================================================#
# This loads data from .obj file                                                   #
#==================================================================================#
def load_obj(file):
	time1 = sys.time()
	
	TEX_OFF_FLAG = ~NMesh.FaceModes['TEX']
	
	# Get the file name with no path or .obj
	fileName = stripName( stripPath(file) )

	mtl_fileName = ''

	DIR = pathName(file, stripPath(file))

	fileLines = open(file, 'r').readlines()
	
	uvMapList = [(0,0)] # store tuple uv pairs here

	# This dummy vert makes life a whole lot easier-
	# pythons index system then aligns with objs, remove later
	vertList = [NMesh.Vert(0, 0, 0)]
	
	# Store all imported materials in a dict, names are key
	materiaDict = {}
	
	# Store all imported images in a dict, names are key
	imageDict = {}
	
	# This stores the index that the current mesh has for the current material.
	# if the mesh does not have the material then set -1
	contextMeshMatIdx = -1
	
	# Keep this out of the dict for easy accsess.
	nullMat = Material.New(NULL_MAT)
	
	currentMat = nullMat # Use this mat.
	currentImg = NULL_IMG # Null image is a string, otherwise this should be set to an image object.\
	currentSmooth = 1
	
	# Store a list of unnamed names
	currentUnnamedGroupIdx = 0
	currentUnnamedObjectIdx = 0
	
	quadList = (0, 1, 2, 3)
	
	#==================================================================================#
	# Load all verts first (texture verts too)                                         #
	#==================================================================================#
	nonVertFileLines = []
	lIdx = 0
	print '\tfile length: %d' % len(fileLines)
	while lIdx < len(fileLines):
		
		# Dont Bother splitting empty or comment lines.
		if len(fileLines[lIdx]) == 0:
			pass
		elif fileLines[lIdx][0] == '\n':
			pass
		elif fileLines[lIdx][0] == '#':
			pass
		
		else:
			fileLines[lIdx] = fileLines[lIdx].split()		
			l = fileLines[lIdx]
			
			# Splitting may 
			if len(l) == 0:
				pass
			# Verts
			elif l[0] == 'v':
				vertList.append( NMesh.Vert(float(l[1]), float(l[2]), float(l[3]) ) )
			
			# UV COORDINATE
			elif l[0] == 'vt':
				uvMapList.append( (float(l[1]), float(l[2])) )
			else:
				nonVertFileLines.append(l)
		lIdx+=1
	
	del fileLines
	fileLines = nonVertFileLines
	del nonVertFileLines	
	
	# Make a list of all unused vert indicies that we can copy from
	VERT_USED_LIST = [-1]*len(vertList)
	
	# Here we store a boolean list of which verts are used or not
	# no we know weather to add them to the current mesh
	# This is an issue with global vertex indicies being translated to per mesh indicies
	# like blenders, we start with a dummy just like the vert.
	# -1 means unused, any other value refers to the local mesh index of the vert.

	# objectName has a char in front of it that determins weather its a group or object.
	# We ignore it when naming the object.
	objectName = 'omesh' # If we cant get one, use this
	
	meshDict = {}
	currentMesh = NMesh.GetRaw()
	meshDict[objectName] = (currentMesh, VERT_USED_LIST[:]) # Mesh/meshDict[objectName][1]
	currentMesh.verts.append(vertList[0])
	currentMesh.hasFaceUV(1)
	

	#==================================================================================#
	# Load all faces into objects, main loop                                           #
	#==================================================================================#
	lIdx = 0
	# Face and Object loading LOOP
	while lIdx < len(fileLines):
		l = fileLines[lIdx]
		
		# FACE
		if l[0] == 'f': 
			# Make a face with the correct material.
			f = NMesh.Face()
			
			# Add material to mesh
			if contextMeshMatIdx == -1:
				tmpMatLs = currentMesh.materials
				
				if len(tmpMatLs) == MATLIMIT:
					contextMeshMatIdx = 0 # Use first material
					print 'material overflow, attempting to use > 16 materials. defaulting to first.'
				else:
					contextMeshMatIdx = len(tmpMatLs)
					currentMesh.addMaterial(currentMat)

			# Set up vIdxLs : Verts
			# Set up vtIdxLs : UV
			# Start with a dummy objects so python accepts OBJs 1 is the first index.
			vIdxLs = []
			vtIdxLs = []
			fHasUV = len(uvMapList)-1 # Assume the face has a UV until it sho it dosent, if there are no UV coords then this will start as 0.
			for v in l[1:]:
				# OBJ files can have // or / to seperate vert/texVert/normal
				# this is a bit of a pain but we must deal with it.
				objVert = v.split('/')
				
				# Vert Index - OBJ supports negative index assignment (like python)
				
				vIdxLs.append(int(objVert[0]))
				if fHasUV:
					# UV
					if len(objVert) == 1:
						#vtIdxLs.append(int(objVert[0])) # replace with below.
						vtIdxLs.append(vIdxLs[-1]) # Sticky UV coords
					elif objVert[1]: # != '' # Its possible that theres no texture vert just he vert and normal eg 1//2
						vtIdxLs.append(int(objVert[1])) # Seperate UV coords
					else:
						fHasUV = 0

					# Dont add a UV to the face if its larger then the UV coord list
					# The OBJ file would have to be corrupt or badly written for thi to happen
					# but account for it anyway.
					if len(vtIdxLs) > 0:
						if vtIdxLs[-1] > len(uvMapList):
							fHasUV = 0
							print 'badly written OBJ file, invalid references to UV Texture coordinates.'
			
			# Quads only, we could import quads using the method below but it polite to import a quad as a quad.
			if len(vIdxLs) == 4:
				for i in quadList: #  quadList == [0,1,2,3] 
					if meshDict[objectName][1][vIdxLs[i]] == -1:
						currentMesh.verts.append(vertList[vIdxLs[i]])
						f.v.append(currentMesh.verts[-1])
						meshDict[objectName][1][vIdxLs[i]] = len(currentMesh.verts)-1
					else:
						f.v.append(currentMesh.verts[meshDict[objectName][1][vIdxLs[i]]])
				
				# UV MAPPING
				if fHasUV:
					f.uv.extend([uvMapList[ vtIdxLs[0] ],uvMapList[ vtIdxLs[1] ],uvMapList[ vtIdxLs[2] ],uvMapList[ vtIdxLs[3] ]])
					#for i in [0,1,2,3]:
					#	f.uv.append( uvMapList[ vtIdxLs[i] ] )

				if f.v > 0:
					f.mat = contextMeshMatIdx
					if currentImg != NULL_IMG:
						f.image = currentImg
					else:
						f.mode &= TEX_OFF_FLAG
					currentMesh.faces.append(f) # move the face onto the mesh
					if len(f) > 0:
						f.smooth = currentSmooth
			
			elif len(vIdxLs) >= 3: # This handles tri's and fans
				for i in range(len(vIdxLs)-2):
					f = NMesh.Face()
					
					for ii in [0, i+1, i+2]:
						
						if meshDict[objectName][1][vIdxLs[ii]] == -1:
							currentMesh.verts.append(vertList[vIdxLs[ii]])
							f.v.append(currentMesh.verts[-1])
							meshDict[objectName][1][vIdxLs[ii]] = len(currentMesh.verts)-1
						else:
							f.v.append(currentMesh.verts[meshDict[objectName][1][vIdxLs[ii]]])

					# UV MAPPING
					if fHasUV:
						f.uv.extend([uvMapList[ vtIdxLs[0] ], uvMapList[ vtIdxLs[i+1] ], uvMapList[ vtIdxLs[i+2] ]])
					
					if f.v > 0:
						f.mat = contextMeshMatIdx
						if currentImg != NULL_IMG:
							f.image = currentImg
						else:
							f.mode |= TEX_OFF_FLAG
						currentMesh.faces.append(f) # move the face onto the mesh
						if len(f) > 0:
							f.smooth = currentSmooth
		
		
		# FACE SMOOTHING
		elif l[0] == 's':
			# No value? then turn on.
			if len(l) == 1:
				currentSmooth = 1
			else:
				if l[1] == 'off':
					currentSmooth = 0
				else: 
					currentSmooth = 1

		# OBJECT / GROUP
		elif l[0] == 'o' or l[0] == 'g':
			# This makes sure that if an object and a group have the same name then
			# they are not put into the same object.
			
			# Only make a new group.object name if the verts in the existing object have been used, this is obscure
			# but some files face groups seperating verts and faces which results in silly things. (no groups have names.)
			if len(l) > 1:
				objectName = '_'.join(l[1:])
			else: # No name given
				# Make a new empty name
				if l[0] == 'g': # Make a blank group name
					objectName = 'unnamed_grp_%d' % currentUnnamedGroupIdx
					currentUnnamedGroupIdx +=1
				else: # is an object.
					objectName = 'unnamed_ob_%d' % currentUnnamedObjectIdx
					currentUnnamedObjectIdx +=1
			
			
			# If we havnt written to this mesh before then do so.
			# if we have then we'll just keep appending to it, this is required for soem files.
			
			# If we are new, or we are not yet in the list of added meshes
			# then make us new mesh.
			if len(l) == 1 or objectName not in meshDict.keys():
				currentMesh = NMesh.GetRaw()
				meshDict[objectName] = (currentMesh, VERT_USED_LIST[:])
				currentMesh.hasFaceUV(1)
				currentMesh.verts.append( vertList[0] )
				contextMeshMatIdx = -1
				
			else: 
				# Since we have this in Blender then we will check if the current Mesh has the material.
				# set the contextMeshMatIdx to the meshs index but only if we have it.
				currentMesh = meshDict[objectName]
				contextMeshMatIdx = getMeshMaterialIndex(currentMesh, currentMat)



		# MATERIAL
		elif l[0] == 'usemtl':
			if len(l) == 1 or l[1] == NULL_MAT:
				#~ currentMat = getMat(NULL_MAT)
				newMatName = NULL_MAT
				currentMat = nullMat
			else:
				#~ currentMat = getMat(' '.join(l[1:])) # Use join in case of spaces
				newMatName = '_'.join(l[1:])
				
				
				try: # Add to material list if not there
					currentMat = materiaDict[newMatName]
					newMatName = currentMat.name # Make sure we are up to date, Blender might have incremented the name.
					
					# Since we have this in Blender then we will check if the current Mesh has the material.
					matIdx = 0
					tmpMeshMaterials = currentMesh.materials
					while matIdx < len(tmpMeshMaterials):
						if tmpMeshMaterials[matIdx].name == newMatName:
							contextMeshMatIdx = matIdx # The current mat index.
							break
						matIdx+=1
					
					
					
				except KeyError: # Not added yet, add now.
					currentMat = Material.New(newMatName)
					materiaDict[newMatName] = currentMat
					contextMeshMatIdx = -1 # Mesh cant possibly have the material.
			
		# IMAGE
		elif l[0] == 'usemat' or l[0] == 'usemap':
			if len(l) == 1 or l[1] == '(null)' or l[1] == 'off':
				currentImg = NULL_IMG
			else:
				# Load an image.
				newImgName = stripPath(' '.join(l[1:]))
				
				try:
					# Assume its alredy set in the dict (may or maynot be loaded)
					currentImg = imageDict[newImgName]
				
				except KeyError: # Not in dict, add for first time.
					try: # Image has not been added, Try and load the image
						currentImg = Image.Load( '%s%s' % (DIR, newImgName) ) # Use join in case of spaces 
						imageDict[newImgName] = currentImg
						
					except IOError: # Cant load, just set blank.
						imageDict[newImgName] = NULL_IMG
						currentImg = NULL_IMG
		
		# MATERIAL FILE
		elif l[0] == 'mtllib':
			mtl_fileName = ' '.join(l[1:]) # SHOULD SUPPORT MULTIPLE MTL?
		
		lIdx+=1

	
	#==============================================#
	# Write all meshs in the dictionary            #
	#==============================================# 
	for ob in Scene.GetCurrent().getChildren(): # Deselect all
		ob.sel = 0	
	
	
	# Applies material properties to materials alredy on the mesh as well as Textures.
	if mtl_fileName != '':
		load_mtl(DIR, mtl_fileName, meshDict)	
	
	
	importedObjects = []
	for mk in meshDict.keys():
		meshDict[mk][0].verts.pop(0)
		
		# Ignore no vert meshes.
		if not meshDict[mk][0].verts:
			continue
		
		name = getUniqueName(mk)
		ob = NMesh.PutRaw(meshDict[mk][0], name)
		ob.name = name
		
		importedObjects.append(ob)
	
	# Select all imported objects.
	for ob in importedObjects:
		ob.sel = 1

	print "obj import time: ", sys.time() - time1

Window.FileSelector(load_obj, 'Import Wavefront OBJ')


# For testing compatibility
'''
TIME = sys.time()
import os
for obj in os.listdir('/obj/'):
	if obj.lower().endswith('obj'):
		print obj
		newScn = Scene.New(obj)
		newScn.makeCurrent()
		load_obj('/obj/' + obj)
'''
#print "TOTAL IMPORT TIME: ", sys.time() - TIME
#load_obj('/obj/her.obj')
