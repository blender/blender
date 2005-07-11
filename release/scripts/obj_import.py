#!BPY
 
"""
Name: 'Wavefront (.obj)...'
Blender: 237
Group: 'Import'
Tooltip: 'Load a Wavefront OBJ File'
"""

__author__ = "Campbell Barton"
__url__ = ["blender", "elysiun"]
__version__ = "1.0"

__bpydoc__ = """\
This script imports OBJ files to Blender.

Usage:

Run this script from "File->Import" menu and then load the desired OBJ file.
"""

# $Id$
#
# --------------------------------------------------------------------------
# OBJ Import v1.0 by Campbell Barton (AKA Ideasman)
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

ABORT_MENU = 'Failed Reading OBJ%t|File is probably another type|if not send this file to|cbarton@metavr.com|with MTL and image files for further testing.'

NULL_MAT = '(null)' # Name for mesh's that have no mat set.
NULL_IMG = '(null)' # Name for mesh's that have no mat set.

MATLIMIT = 16 # This isnt about to change but probably should not be hard coded.

DIR = ''

#==============================================#
# Return directory, where the file is          #
#==============================================#
def stripFile(path):
	lastSlash = max(path.rfind('\\'), path.rfind('/'))
	if lastSlash != -1:
		path = path[:lastSlash]
	return '%s%s' % (path, sys.sep)

#==============================================#
# Strips the slashes from the back of a string #
#==============================================#
def stripPath(path):
	return path.split('/')[-1].split('\\')[-1]
	
#====================================================#
# Strips the prefix off the name before writing      #
#====================================================#
def stripExt(name): # name is a string
	return name[ : name.rfind('.') ]


from Blender import *


#==================================================================================#
# This function sets textures defined in .mtl file                                 #
#==================================================================================#
def getImg(img_fileName, dir):
	img_fileName_strip = stripPath(img_fileName)
	for i in Image.Get():
		if stripPath(i.filename) == img_fileName_strip:
			return i
	
	try: # Absolute dir
		return Image.Load(img_fileName)
	except IOError:
		pass
	
	# Relative dir
	if img_fileName.startswith('/'):
		img_fileName = img_fileName[1:]
	elif img_fileName.startswith('./'):
		img_fileName = img_fileName[2:]
	elif img_fileName.startswith('\\'):
		img_fileName = img_fileName[1:]		
	elif img_fileName.startswith('.\\'):
		img_fileName = img_fileName[2:]		
	
	# if we are this far it means the image hasnt been loaded.
	try:
		return Image.Load( dir + img_fileName)
	except IOError:
		pass
	
	# Its unlikely but the image might be with the OBJ file, and the path provided not relevent.
	# if the user extracted an archive with no paths this could happen.
	try:
		return Image.Load( dir + img_fileName_strip)
	except IOError:
		pass
	
	print '\tunable to open image file: "%s"' % img_fileName
	return None

#==================================================================================#
# This function sets textures defined in .mtl file                                 #
#==================================================================================#
def loadMaterialImage(mat, img_fileName, type, meshDict, dir):
	TEX_ON_FLAG = NMesh.FaceModes['TEX']
	
	texture = Texture.New(type)
	texture.setType('Image')
	
	# Absolute path - c:\.. etc would work here
	image = getImg(img_fileName, dir)
	
	if image:
		texture.image = image
		
	# adds textures to faces (Textured/Alt-Z mode)
	# Only apply the diffuse texture to the face if the image has not been set with the inline usemat func.
	if image and type == 'Kd':
		for meshPair in meshDict.values():
			for f in meshPair[0].faces:
				#print meshPair[0].materials[f.mat].name, mat.name
				if meshPair[0].materials[f.mat].name == mat.name:
					# the inline usemat command overides the material Image
					if not f.image:
						f.mode |= TEX_ON_FLAG
						f.image = image
	
	# adds textures for materials (rendering)
	elif type == 'Ka':
		mat.setTexture(0, texture, Texture.TexCo.UV, Texture.MapTo.CMIR) # TODO- Add AMB to BPY API
	elif type == 'Kd':
		mat.setTexture(1, texture, Texture.TexCo.UV, Texture.MapTo.COL)
	elif type == 'Ks':
		mat.setTexture(2, texture, Texture.TexCo.UV, Texture.MapTo.SPEC)
	
	elif type == 'Bump': # New Additions
		mat.setTexture(3, texture, Texture.TexCo.UV, Texture.MapTo.NOR)		
	elif type == 'D':
		mat.setTexture(4, texture, Texture.TexCo.UV, Texture.MapTo.ALPHA)				
	elif type == 'refl':
		mat.setTexture(5, texture, Texture.TexCo.UV, Texture.MapTo.REF)				
	

#==================================================================================#
# This function loads materials from .mtl file (have to be defined in obj file)    #
#==================================================================================#
def load_mtl(dir, mtl_file, meshDict, materialDict):
	
	#===============================================================================#
	# This gets a mat or creates one of the requested name if none exist.           #
	#===============================================================================#
	def getMat(matName, materialDict):
		# Make a new mat
		try:
			return materialDict[matName]
		#except NameError or KeyError:
		except: # Better do any exception
			# Do we realy need to keep the dict up to date?, not realy but keeps consuistant.
			materialDict[matName] = Material.New(matName)
			return materialDict[matName]
		
			
	mtl_file = stripPath(mtl_file)
	mtl_fileName = dir + mtl_file
	
	try:
		fileLines= open(mtl_fileName, 'r').readlines()
	except IOError:
		print '\tunable to open referenced material file: "%s"' % mtl_fileName
		return
	
	try:
		lIdx=0
		while lIdx < len(fileLines):
			l = fileLines[lIdx].split()
			
			# Detect a line that will be ignored
			if len(l) == 0:
				pass
			elif l[0] == '#' or len(l) == 0:
				pass
			elif l[0] == 'newmtl':
				currentMat = getMat('_'.join(l[1:]), materialDict) # Material should alredy exist.
			elif l[0] == 'Ka':
				currentMat.setMirCol(float(l[1]), float(l[2]), float(l[3]))
			elif l[0] == 'Kd':
				currentMat.setRGBCol(float(l[1]), float(l[2]), float(l[3]))
			elif l[0] == 'Ks':
				currentMat.setSpecCol(float(l[1]), float(l[2]), float(l[3]))
			elif l[0] == 'Ns':
				currentMat.setHardness( int((float(l[1])*0.51)) )
			elif l[0] == 'Ni': # Refraction index
				currentMat.setIOR( max(1, min(float(l[1]), 3))) # Between 1 and 3
			elif l[0] == 'd':
				currentMat.setAlpha(float(l[1]))
			elif l[0] == 'Tr':
				currentMat.setAlpha(float(l[1]))
			elif l[0] == 'map_Ka':
				img_fileName = ' '.join(l[1:])
				loadMaterialImage(currentMat, img_fileName, 'Ka', meshDict, dir)
			elif l[0] == 'map_Ks':
				img_fileName = ' '.join(l[1:])
				loadMaterialImage(currentMat, img_fileName, 'Ks', meshDict, dir)
			elif l[0] == 'map_Kd':
				img_fileName = ' '.join(l[1:])
				loadMaterialImage(currentMat, img_fileName, 'Kd', meshDict, dir)
			
			# new additions
			elif l[0] == 'map_Bump': # Bumpmap
				img_fileName = ' '.join(l[1:])			
				loadMaterialImage(currentMat, img_fileName, 'Bump', meshDict, dir)
			elif l[0] == 'map_D': # Alpha map - Dissolve
				img_fileName = ' '.join(l[1:])			
				loadMaterialImage(currentMat, img_fileName, 'D', meshDict, dir)

			elif l[0] == 'refl': # Reflectionmap
				img_fileName = ' '.join(l[1:])			
				loadMaterialImage(currentMat, img_fileName, 'refl', meshDict, dir)
			
			lIdx+=1
	except:
		print '\tERROR: Unable to parse MTL file.'
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

#==================================================================================#
# This loads data from .obj file                                                   #
#==================================================================================#
def load_obj(file):
	time1 = sys.time()
	
	# Deselect all objects in the scene.
	# do this first so we dont have to bother, with objects we import
	for ob in Scene.GetCurrent().getChildren():
		ob.sel = 0
	
	TEX_OFF_FLAG = ~NMesh.FaceModes['TEX']
	
	# Get the file name with no path or .obj
	fileName = stripExt( stripPath(file) )

	mtl_fileName = None

	DIR = stripFile(file)
	
	tempFile = open(file, 'r')
	fileLines = tempFile.readlines()
	tempFile.close()
	
	uvMapList = [(0,0)] # store tuple uv pairs here

	# This dummy vert makes life a whole lot easier-
	# pythons index system then aligns with objs, remove later
	vertList = [None] # Could havea vert but since this is a placeholder theres no Point
	
	
	# Store all imported images in a dict, names are key
	imageDict = {}
	
	# This stores the index that the current mesh has for the current material.
	# if the mesh does not have the material then set -1
	contextMeshMatIdx = -1
	
	# Keep this out of the dict for easy accsess.
	nullMat = Material.New(NULL_MAT)
	
	currentMat = nullMat # Use this mat.
	currentImg = None # Null image is a string, otherwise this should be set to an image object.\
	currentSmooth = False
	
	# Store a list of unnamed names
	currentUnnamedGroupIdx = 1
	currentUnnamedObjectIdx = 1
	
	quadList = (0, 1, 2, 3)
	
	faceQuadVList = [None, None, None, None]
	faceTriVList = [None, None, None]
	
	#==================================================================================#
	# Load all verts first (texture verts too)                                         #
	#==================================================================================#
	nonVertFileLines = []
	smoothingGroups = {}
	materialDict = {} # Store all imported materials as unique dict, names are key
	lIdx = 0
	print '\tfile length: %d' % len(fileLines)
	try:
		while lIdx < len(fileLines):
			# Ignore vert normals
			if fileLines[lIdx].startswith('vn'):
				lIdx+=1
				continue
			
			# Dont Bother splitting empty or comment lines.
			if len(fileLines[lIdx]) == 0 or\
			fileLines[lIdx][0] == '\n' or\
			fileLines[lIdx][0] == '#':
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
				
				# Smoothing groups, make a list of unique.
				elif l[0] == 's':
					if len(l) > 1:
						smoothingGroups['_'.join(l[1:])] = None # Can we assign something more usefull? cant use sets yet
							
					# Keep Smoothing group line
					nonVertFileLines.append(l)
				
				# Smoothing groups, make a list of unique.
				elif l[0] == 'usemtl':
					if len(l) > 1:
						materialDict['_'.join(l[1:])] = None # Can we assign something more usefull? cant use sets yet
							
					# Keep Smoothing group line
					nonVertFileLines.append(l)
				
				else:
					nonVertFileLines.append(l)
			lIdx+=1
		
	except:
		print Draw.PupMenu(ABORT_MENU)
		return
	
	del fileLines
	fileLines = nonVertFileLines
	del nonVertFileLines	
	
	#  Only want unique keys anyway
	smoothingGroups['(null)'] = None # Make sure we have at least 1.
	smoothingGroups = smoothingGroups.keys()
	print '\tfound %d smoothing groups.' % (len(smoothingGroups) -1)
	
	# Add materials to Blender for later is in teh OBJ
	for k in materialDict.keys():
		materialDict[k] = Material.New(k)
	
	
	# Make a list of all unused vert indicies that we can copy from
	VERT_USED_LIST = [0]*len(vertList)
	
	# Here we store a boolean list of which verts are used or not
	# no we know weather to add them to the current mesh
	# This is an issue with global vertex indicies being translated to per mesh indicies
	# like blenders, we start with a dummy just like the vert.
	# -1 means unused, any other value refers to the local mesh index of the vert.

	# currentObjectName has a char in front of it that determins weather its a group or object.
	# We ignore it when naming the object.
	currentObjectName = 'unnamed_obj_0' # If we cant get one, use this
	
	meshDict = {} # The 3 variables below are stored in a tuple within this dict for each mesh
	currentMesh = NMesh.GetRaw() # The NMesh representation of the OBJ group/Object
	currentUsedVertList = {} # A Dict of smooth groups, each smooth group has a list of used verts and they are generated on demand so as to save memory.
	currentMaterialMeshMapping = {} # Used to store material indicies so we dont have to search the mesh for materials every time.
	
	# Every mesh has a null smooth group, this is used if there are no smooth groups in the OBJ file.
	# and when for faces where no smooth group is used.
	currentSmoothGroup = '(null)' # The Name of the current smooth group
	
	# For direct accsess to the Current Meshes, Current Smooth Groups- Used verts.
	# This is of course context based and changes on the fly.
	currentUsedVertListSmoothGroup = VERT_USED_LIST[:]
	
	# Set the initial '(null)' Smooth group, every mesh has one.
	currentUsedVertList[currentSmoothGroup] = currentUsedVertListSmoothGroup
	
	
	# 0:NMesh, 1:SmoothGroups[UsedVerts[0,0,0,0]], 2:materialMapping['matname':matIndexForThisNMesh]
	meshDict[currentObjectName] = (currentMesh, currentUsedVertList, currentMaterialMeshMapping) 
	

	
		
	
	
	currentMesh.verts.append(vertList[0]) # So we can sync with OBJ indicies where 1 is the first item.
	if len(uvMapList) > 1:
		currentMesh.hasFaceUV(1) # Turn UV's on if we have ANY texture coords in this obj file.
	

	#==================================================================================#
	# Load all faces into objects, main loop                                           #
	#==================================================================================#
	try:
		lIdx = 0
		# Face and Object loading LOOP
		while lIdx < len(fileLines):
			l = fileLines[lIdx]
			
			# FACE
			if l[0] == 'f': 
				# Make a face with the correct material.
				
				# Add material to mesh
				if contextMeshMatIdx == -1:
					tmpMatLs = currentMesh.materials
					
					if len(tmpMatLs) == MATLIMIT:
						contextMeshMatIdx = 0 # Use first material
						print 'material overflow, attempting to use > 16 materials. defaulting to first.'
					else:
						contextMeshMatIdx = len(tmpMatLs)
						currentMaterialMeshMapping[currentMat.name] = contextMeshMatIdx
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
					'''
					f = NMesh.Face()
					for i in quadList: #  quadList == [0,1,2,3] 
						if currentUsedVertListSmoothGroup[vIdxLs[i]] == 0:
							v = vertList[vIdxLs[i]]
							currentMesh.verts.append(v)
							f.append(v)
							currentUsedVertListSmoothGroup[vIdxLs[i]] = len(currentMesh.verts)-1
						else:
							f.v.append(currentMesh.verts[currentUsedVertListSmoothGroup[vIdxLs[i]]])
					'''
					if currentUsedVertListSmoothGroup[vIdxLs[0]] == 0:
						faceQuadVList[0] = vertList[vIdxLs[0]]
						currentUsedVertListSmoothGroup[vIdxLs[0]] = len(currentMesh.verts)
					else:
						faceQuadVList[0] = currentMesh.verts[currentUsedVertListSmoothGroup[vIdxLs[0]]]
					
					if currentUsedVertListSmoothGroup[vIdxLs[1]] == 0:
						faceQuadVList[1] = vertList[vIdxLs[1]]
						currentUsedVertListSmoothGroup[vIdxLs[1]] = len(currentMesh.verts)+1
					else:
						faceQuadVList[1] = currentMesh.verts[currentUsedVertListSmoothGroup[vIdxLs[1]]]
						
					if currentUsedVertListSmoothGroup[vIdxLs[2]] == 0:
						faceQuadVList[2] = vertList[vIdxLs[2]]
						currentUsedVertListSmoothGroup[vIdxLs[2]] = len(currentMesh.verts)+2
					else:
						faceQuadVList[2] = currentMesh.verts[currentUsedVertListSmoothGroup[vIdxLs[2]]]
	
					if currentUsedVertListSmoothGroup[vIdxLs[3]] == 0:
						faceQuadVList[3] = vertList[vIdxLs[3]]
						currentUsedVertListSmoothGroup[vIdxLs[3]] = len(currentMesh.verts)+3
					else:
						faceQuadVList[3] = currentMesh.verts[currentUsedVertListSmoothGroup[vIdxLs[3]]]
					
					currentMesh.verts.extend(faceQuadVList)
					f = NMesh.Face(faceQuadVList)
					
					# UV MAPPING
					if fHasUV:
						f.uv = [uvMapList[ vtIdxLs[0] ],uvMapList[ vtIdxLs[1] ],uvMapList[ vtIdxLs[2] ],uvMapList[ vtIdxLs[3] ]]
						if currentImg:
							f.image = currentImg
						else:
							f.mode &= TEX_OFF_FLAG
					
					f.mat = contextMeshMatIdx
					f.smooth = currentSmooth
					currentMesh.faces.append(f) # move the face onto the mesh
				
				elif len(vIdxLs) >= 3: # This handles tri's and fans
					for i in range(len(vIdxLs)-2):
						'''
						f = NMesh.Face()
						for ii in [0, i+1, i+2]:
							if currentUsedVertListSmoothGroup[vIdxLs[ii]] == 0:
								v = vertList[vIdxLs[ii]]
								currentMesh.verts.append(v)
								f.append(v)
								currentUsedVertListSmoothGroup[vIdxLs[ii]] = len(currentMesh.verts)-1
							else:
								f.v.append(currentMesh.verts[currentUsedVertListSmoothGroup[vIdxLs[ii]]])
						'''
						
							
						if currentUsedVertListSmoothGroup[vIdxLs[0]] == 0:
							faceTriVList[0] = vertList[vIdxLs[0]]
							currentUsedVertListSmoothGroup[vIdxLs[0]] = len(currentMesh.verts)
						else:
							faceTriVList[0] = currentMesh.verts[currentUsedVertListSmoothGroup[vIdxLs[0]]]
						
						if currentUsedVertListSmoothGroup[vIdxLs[i+1]] == 0:
							faceTriVList[1] = vertList[vIdxLs[i+1]]
							currentUsedVertListSmoothGroup[vIdxLs[i+1]] = len(currentMesh.verts)+1
						else:
							faceTriVList[1] = currentMesh.verts[currentUsedVertListSmoothGroup[vIdxLs[i+1]]]
							
						if currentUsedVertListSmoothGroup[vIdxLs[i+2]] == 0:
							faceTriVList[2] = vertList[vIdxLs[i+2]]
							currentUsedVertListSmoothGroup[vIdxLs[i+2]] = len(currentMesh.verts)+2
						else:
							faceTriVList[2] = currentMesh.verts[currentUsedVertListSmoothGroup[vIdxLs[i+2]]]
						
						currentMesh.verts.extend(faceTriVList)
						f = NMesh.Face(faceTriVList)					
						
						
						# UV MAPPING
						if fHasUV:
							f.uv = [uvMapList[vtIdxLs[0]], uvMapList[vtIdxLs[i+1]], uvMapList[vtIdxLs[i+2]]]
							if currentImg:
								f.image = currentImg
							else:
								f.mode &= TEX_OFF_FLAG
						
						f.mat = contextMeshMatIdx
						f.smooth = currentSmooth
						currentMesh.faces.append(f) # move the face onto the mesh
			
			# FACE SMOOTHING
			elif l[0] == 's':
				# No value? then turn on.
				if len(l) == 1:
					currentSmooth = True
					currentSmoothGroup = '(null)'
					try:
						currentUsedVertListSmoothGroup = currentUsedVertList[currentSmoothGroup]
					except KeyError:
						currentUsedVertListSmoothGroup = VERT_USED_LIST[:]
						currentUsedVertList[currentSmoothGroup] = currentUsedVertListSmoothGroup
						
				else:
					if l[1] == 'off':
						currentSmooth = False
						currentSmoothGroup = '(null)'
						# We all have a null group so dont need to try
						currentUsedVertListSmoothGroup = currentUsedVertList['(null)']
					else: 
						currentSmooth = True
						currentSmoothGroup = '_'.join(l[1:])
	
			# OBJECT / GROUP
			elif l[0] == 'o' or l[0] == 'g':
				
				# Forget about the current image
				currentImg = None
				
				# This makes sure that if an object and a group have the same name then
				# they are not put into the same object.
				
				# Only make a new group.object name if the verts in the existing object have been used, this is obscure
				# but some files face groups seperating verts and faces which results in silly things. (no groups have names.)
				if len(l) > 1:
					currentObjectName = '_'.join(l[1:])
				else: # No name given
					# Make a new empty name
					if l[0] == 'g': # Make a blank group name
						currentObjectName = 'unnamed_grp_%d' % currentUnnamedGroupIdx
						currentUnnamedGroupIdx +=1
					else: # is an object.
						currentObjectName = 'unnamed_ob_%d' % currentUnnamedObjectIdx
						currentUnnamedObjectIdx +=1
				
				
				# If we havnt written to this mesh before then do so.
				# if we have then we'll just keep appending to it, this is required for soem files.
				
				# If we are new, or we are not yet in the list of added meshes
				# then make us new mesh.
				if len(l) == 1 or currentObjectName not in meshDict.keys():
					currentMesh = NMesh.GetRaw()
					
					currentUsedVertList = {}
					
					# Sg is a string
					currentSmoothGroup = '(null)'
					currentUsedVertListSmoothGroup = VERT_USED_LIST[:]						
					currentUsedVertList[currentSmoothGroup] = currentUsedVertListSmoothGroup
					currentMaterialMeshMapping = {}
					
					meshDict[currentObjectName] = (currentMesh, currentUsedVertList, currentMaterialMeshMapping)
					currentMesh.hasFaceUV(1)
					currentMesh.verts.append( vertList[0] )
					contextMeshMatIdx = -1
					
				else: 
					# Since we have this in Blender then we will check if the current Mesh has the material.
					# set the contextMeshMatIdx to the meshs index but only if we have it.
					currentMesh, currentUsedVertList, currentMaterialMeshMapping = meshDict[currentObjectName]
					#getMeshMaterialIndex(currentMesh, currentMat)
					
					try:
						contextMeshMatIdx = currentMaterialMeshMapping[currentMat.name] #getMeshMaterialIndex(currentMesh, currentMat)
					except KeyError:
						contextMeshMatIdx -1
					
					# For new meshes switch smoothing groups to null
					currentSmoothGroup = '(null)'
					currentUsedVertListSmoothGroup = currentUsedVertList[currentSmoothGroup]
			
			# MATERIAL
			elif l[0] == 'usemtl':
				if len(l) == 1 or l[1] == NULL_MAT:
					currentMat = nullMat # We know we have a null mat.
				else:
					currentMat = materialDict['_'.join(l[1:])]
					try:
						contextMeshMatIdx = currentMaterialMeshMapping[currentMat.name]
					except KeyError:
						contextMeshMatIdx = -1 #getMeshMaterialIndex(currentMesh, currentMat)
				
			# IMAGE
			elif l[0] == 'usemat' or l[0] == 'usemap':
				if len(l) == 1 or l[1] == '(null)' or l[1] == 'off':
					currentImg = None
				else:
					# Load an image.
					newImgName = stripPath(' '.join(l[1:])) # Use space since its a file name.
					
					try:
						# Assume its alredy set in the dict (may or maynot be loaded)
						currentImg = imageDict[newImgName]
					
					except KeyError: # Not in dict, add for first time.
						# Image has not been added, Try and load the image
						currentImg = getImg(newImgName, DIR) # Use join in case of spaces 
						imageDict[newImgName] = currentImg
						# These may be None, thats okay.
						
						
			
			# MATERIAL FILE
			elif l[0] == 'mtllib':
				mtl_fileName = ' '.join(l[1:]) # SHOULD SUPPORT MULTIPLE MTL?
			lIdx+=1
		
		# Applies material properties to materials alredy on the mesh as well as Textures.
		if mtl_fileName:
			load_mtl(DIR, mtl_fileName, meshDict, materialDict)	
		
		
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
	
	except:
		print Draw.PupMenu(ABORT_MENU)
		return


def load_obj_callback(file):
	# Try/Fails should realy account for these, but if somthing realy bad happens then Popup error.
	try:
		load_obj(file)
	except:
		print Draw.PupMenu(ABORT_MENU)

Window.FileSelector(load_obj_callback, 'Import Wavefront OBJ')

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

print "TOTAL IMPORT TIME: ", sys.time() - TIME
'''
#load_obj('/obj/foot_bones.obj')
#load_obj('/obj/mba1.obj')
#load_obj('/obj/PixZSphere50.OBJ')
#load_obj('/obj/obj_test/LHand.obj')
