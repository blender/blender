#!BPY
 
"""
Name: 'Wavefront (.obj)...'
Blender: 237
Group: 'Import'
Tooltip: 'Load a Wavefront OBJ File, Shift: batch import all dir.'
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
	index = name.rfind('.')
	if index != -1:
		return name[ : index ]
	else:
		return name


from Blender import *



# Adds a slash to the end of a path if its not there.
def addSlash(path):
	if path.endswith('\\') or path.endswith('/'):
		return path
	return path + sys.sep
	

def getExt(name):
	index = name.rfind('.')
	if index != -1:
		return name[index+1:]
	return name

try:
	import os
except:
	# So we know if os exists.
	print 'Module "os" not found, install python to enable comprehensive image finding and batch loading.'
	os = None

#===========================================================================#
# Comprehansive image loader, will search and find the image                #
# Will return a blender image or none if the image is missing               #
#===========================================================================#
def comprehansiveImageLoad(imagePath, filePath):
	
	# When we have the file load it with this. try/except niceness.
	def imageLoad(path):
		try:
			img = Image.Load(path)
			print '\t\tImage loaded "%s"' % path
			return img
		except:
			print '\t\tImage failed loading "%s", mabe its not a format blender can read.' % (path)
			return None
	
	# Image formats blender can read
	IMAGE_EXT = ['jpg', 'jpeg', 'png', 'tga', 'bmp', 'rgb', 'sgi', 'bw', 'iff', 'lbm', # Blender Internal
	'gif', 'psd', 'tif', 'tiff', 'pct', 'pict', 'pntg', 'qtif'] # Quacktime, worth a try.
	
	
	
	
	print '\tAttempting to load "%s"' % imagePath
	if sys.exists(imagePath):
		print '\t\tFile found where expected.'
		return imageLoad(imagePath)
	
	imageFileName =  stripPath(imagePath) # image path only
	imageFileName_lower =  imageFileName.lower() # image path only
	imageFileName_noext = stripExt(imageFileName) # With no extension.
	imageFileName_noext_lower = stripExt(imageFileName_lower) # With no extension.
	imageFilePath = stripFile(imagePath)
	
	# Remove relative path from image path
	if imageFilePath.startswith('./') or imageFilePath.startswith('.\\'):
		imageFilePath = imageFilePath[2:]
	
	
	# Attempt to load from obj path.
	tmpPath = stripFile(filePath) + stripFile(imageFilePath)
	if sys.exists(tmpPath):
		print '\t\tFile found in obj dir.'
		return imageLoad(imagePath)
	
	# OS NEEDED IF WE GO ANY FURTHER.
	if not os:
		return
	
	
	# We have os.
	# GATHER PATHS.
	paths = {} # Store possible paths we may use, dict for no doubles.
	tmpPath = addSlash(sys.expandpath('//')) # Blenders path
	if sys.exists(tmpPath):
		print '\t\tSearching in %s' % tmpPath
		paths[tmpPath] = [os.listdir(tmpPath)] # Orig name for loading 
		paths[tmpPath].append([f.lower() for f in paths[tmpPath][0]]) # Lower case list.
		paths[tmpPath].append([stripExt(f) for f in paths[tmpPath][1]]) # Lower case no ext
		
	tmpPath = imageFilePath
	if sys.exists(tmpPath):
		print '\t\tSearching in %s' % tmpPath
		paths[tmpPath] = [os.listdir(tmpPath)] # Orig name for loading 
		paths[tmpPath].append([f.lower() for f in paths[tmpPath][0]]) # Lower case list.
		paths[tmpPath].append([stripExt(f) for f in paths[tmpPath][1]]) # Lower case no ext

	tmpPath = stripFile(filePath)
	if sys.exists(tmpPath):
		print '\t\tSearching in %s' % tmpPath
		paths[tmpPath] = [os.listdir(tmpPath)] # Orig name for loading 
		paths[tmpPath].append([f.lower() for f in paths[tmpPath][0]]) # Lower case list.
		paths[tmpPath].append([stripExt(f) for f in paths[tmpPath][1]]) # Lower case no ext
	
	tmpPath = addSlash(Get('texturesdir'))
	if tmpPath and sys.exists(tmpPath):
		print '\t\tSearching in %s' % tmpPath
		paths[tmpPath] = [os.listdir(tmpPath)] # Orig name for loading 
		paths[tmpPath].append([f.lower() for f in paths[tmpPath][0]]) # Lower case list.
		paths[tmpPath].append([stripExt(f) for f in paths[tmpPath][1]]) # Lower case no ext
	
	# Add path if relative image patrh was given.
	for k in paths.iterkeys():
		tmpPath = k + imageFilePath
		if sys.exists(tmpPath):
			paths[tmpPath] = [os.listdir(tmpPath)] # Orig name for loading 
			paths[tmpPath].append([f.lower() for f in paths[tmpPath][0]]) # Lower case list.
			paths[tmpPath].append([stripExt(f) for f in paths[tmpPath][1]]) # Lower case no ext
	# DONE
	
	
	# 
	for path, files in paths.iteritems():
		
		if sys.exists(path + imageFileName):
			return imageLoad(path + imageFileName)
		
		# If the files not there then well do a case insensitive seek.
		filesOrigCase = files[0]
		filesLower = files[1]
		filesLowerNoExt = files[2]
		
		# We are going to try in index the file directly, if its not there just keep on
		index = None
		try:
			# Is it just a case mismatch?
			index = filesLower.index(imageFileName_lower)
		except:
			try:
				# Have the extensions changed?
				index = filesLowerNoExt.index(imageFileName_noext_lower)
				
				ext = getExt( filesLower[index] ) # Get the extension of the file that matches all but ext.
				
				# Check that the ext is useable eg- not a 3ds file :)
				if ext.lower() not in IMAGE_EXT:
					index = None
			
			except:
				index = None
		
		if index != None:
			tmpPath = path + filesOrigCase[index]
			img = imageLoad( tmpPath )
			if img != None:
				print '\t\tImage Found "%s"' % tmpPath
				return img
	
	
	# IMAGE NOT FOUND IN ANY OF THE DIRS!, DO A RECURSIVE SEARCH.
	print '\t\tImage Not Found in any of the dirs, doing a recusrive search'
	for path in paths.iterkeys():
		# Were not going to use files
		
		
		#------------------
		# finds the file starting at the root.
		#	def findImage(findRoot, imagePath):
		#W---------------
		
		# ROOT, DIRS, FILES
		pathWalk = os.walk(path)
		pathList = [True]
		
		matchList = [] # Store a list of (match, size), choose the biggest.
		while True:
			try:
				pathList  = pathWalk.next()
			except:
				break
			
			for file in pathList[2]:
				file_lower = file.lower()
				# FOUND A MATCH
				if (file_lower == imageFileName_lower) or\
				(stripExt(file_lower) == imageFileName_noext_lower and getExt(file_lower) in IMAGE_EXT):
					name = pathList[0] + sys.sep + file
					size = os.path.getsize(name)
					print '\t\t\tfound:', name 
					matchList.append( (name, size) )
		
		if matchList:
			# Sort by file size
			matchList.sort(lambda A, B: cmp(B[1], A[1]) )
			
			print '\t\tFound "%s"' % matchList[0][0]
			
			# Loop through all we have found
			img = None
			for match in matchList:
				img = imageLoad(match[0]) # 0 - first, 0 - pathname
				if img != None:
					break
			return img
	
	
	
	# No go.
	print '\t\tImage Not Found "%s"' % imagePath
	return None





















#==================================================================================#
# This function sets textures defined in .mtl file                                 #
#==================================================================================#
# ___ Replaced by comprehensive imahge get

#==================================================================================#
# This function sets textures defined in .mtl file                                 #
#==================================================================================#
def loadMaterialImage(mat, img_fileName, type, meshDict, dir):
	TEX_ON_FLAG = NMesh.FaceModes['TEX']
	
	texture = Texture.New(type)
	texture.setType('Image')
	
	# Absolute path - c:\.. etc would work here
	image = comprehansiveImageLoad(img_fileName, dir)
	
	if image:
		texture.image = image
		
	# adds textures to faces (Textured/Alt-Z mode)
	# Only apply the diffuse texture to the face if the image has not been set with the inline usemat func.
	if image and type == 'Kd':
		for meshPair in meshDict.itervalues():
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
				currentMat.setMirCol((float(l[1]), float(l[2]), float(l[3])))
			elif l[0] == 'Kd':
				currentMat.setRGBCol((float(l[1]), float(l[2]), float(l[3])))
			elif l[0] == 'Ks':
				currentMat.setSpecCol((float(l[1]), float(l[2]), float(l[3])))
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
		print '\tERROR: Unable to parse MTL file: "%s"' % mtl_file
		return
	print '\tUsing MTL: "%s"' % mtl_file
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
	
	print '\nImporting OBJ file: "%s"' % file
	
	time1 = sys.time()
	
	# Deselect all objects in the scene.
	# do this first so we dont have to bother, with objects we import
	for ob in Scene.GetCurrent().getChildren():
		ob.sel = 0
	
	TEX_OFF_FLAG = ~NMesh.FaceModes['TEX']
	
	# Get the file name with no path or .obj
	fileName = stripExt( stripPath(file) )

	mtl_fileName = [] # Support multiple mtl files if needed.

	DIR = stripFile(file)
	
	tempFile = open(file, 'r')
	fileLines = tempFile.readlines()
	tempFile.close()
	
	uvMapList = [] # store tuple uv pairs here
	
	# This dummy vert makes life a whole lot easier-
	# pythons index system then aligns with objs, remove later
	vertList = [] # Could havea vert but since this is a placeholder theres no Point
	
	
	# Store all imported images in a dict, names are key
	imageDict = {}
	
	# This stores the index that the current mesh has for the current material.
	# if the mesh does not have the material then set -1
	contextMeshMatIdx = -1
	
	# Keep this out of the dict for easy accsess.
	nullMat = Material.New('(null)')
	
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
	
	
	del fileLines
	fileLines = nonVertFileLines
	del nonVertFileLines	
	
	#  Only want unique keys anyway
	smoothingGroups['(null)'] = None # Make sure we have at least 1.
	smoothingGroups = smoothingGroups.keys()
	print '\tfound %d smoothing groups.' % (len(smoothingGroups) -1)
	
	# Add materials to Blender for later is in teh OBJ
	for k in materialDict.iterkeys():
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
	
	# Only show the bad uv error once 
	badObjUvs = 0
	badObjFaceVerts = 0
	badObjFaceTexCo = 0
	
	
	#currentMesh.verts.append(vertList[0]) # So we can sync with OBJ indicies where 1 is the first item.
	if len(uvMapList) > 1:
		currentMesh.hasFaceUV(1) # Turn UV's on if we have ANY texture coords in this obj file.
	
	
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
			
			# Add material to mesh
			if contextMeshMatIdx == -1:
				tmpMatLs = currentMesh.materials
				
				if len(tmpMatLs) == 16:
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
			
			
			fHasUV = len(uvMapList) # Assume the face has a UV until it sho it dosent, if there are no UV coords then this will start as 0.
			for v in l[1:]:
				# OBJ files can have // or / to seperate vert/texVert/normal
				# this is a bit of a pain but we must deal with it.
				objVert = v.split('/')
				
				# Vert Index - OBJ supports negative index assignment (like python)
				
				vIdxLs.append(int(objVert[0])-1)
				if fHasUV:
					# UV
					index = 0 # Dummy var
					if len(objVert) == 1:
						index = vIdxLs[-1]
					elif objVert[1]: # != '' # Its possible that theres no texture vert just he vert and normal eg 1//2
						index = int(objVert[1])-1
					
					if len(uvMapList) > index:
						vtIdxLs.append(index) # Seperate UV coords
					else:
						# BAD FILE, I have found this so I account for it.
						# INVALID UV COORD
						# Could ignore this- only happens with 1 in 1000 files.
						badObjFaceTexCo +=1
						vtIdxLs.append(0)
						
						fHasUV = 0
	
					# Dont add a UV to the face if its larger then the UV coord list
					# The OBJ file would have to be corrupt or badly written for thi to happen
					# but account for it anyway.
					if len(vtIdxLs) > 0:
						if vtIdxLs[-1] > len(uvMapList):
							fHasUV = 0
							
							badObjUvs +=1 # ERROR, Cont
			# Quads only, we could import quads using the method below but it polite to import a quad as a quad.
			if len(vIdxLs) == 4:
				
				# Have found some files where wach face references the same vert
				# - This causes a bug and stopts the import so lets check here
				if vIdxLs[0] == vIdxLs[1] or\
				vIdxLs[0] == vIdxLs[2] or\
				vIdxLs[0] == vIdxLs[3] or\
				vIdxLs[1] == vIdxLs[2] or\
				vIdxLs[1] == vIdxLs[3] or\
				vIdxLs[2] == vIdxLs[3]:
					badObjFaceVerts+=1
				else:
					for i in quadList: #  quadList == [0,1,2,3] 
						if currentUsedVertListSmoothGroup[vIdxLs[i]] == 0:
							faceQuadVList[i] = vertList[vIdxLs[i]]
							currentMesh.verts.append(faceQuadVList[i])
							currentUsedVertListSmoothGroup[vIdxLs[i]] = len(currentMesh.verts)-1
						else:
							faceQuadVList[i] = currentMesh.verts[currentUsedVertListSmoothGroup[vIdxLs[i]]]
					
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
					if vIdxLs[0] == vIdxLs[i+1] or\
					vIdxLs[0] == vIdxLs[i+2] or\
					vIdxLs[i+1] == vIdxLs[i+2]:
						badObjFaceVerts+=1
					else:
						for k, j in [(0,0), (1,i+1), (2,i+2)]:
							if currentUsedVertListSmoothGroup[vIdxLs[j]] == 0:
								faceTriVList[k] = vertList[vIdxLs[j]]
								currentMesh.verts.append(faceTriVList[k])
								currentUsedVertListSmoothGroup[vIdxLs[j]] = len(currentMesh.verts)-1
							else:
								faceTriVList[k] = currentMesh.verts[currentUsedVertListSmoothGroup[vIdxLs[j]]]	
						
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
			if len(l) == 1 or (not meshDict.has_key(currentObjectName)):
				currentMesh = NMesh.GetRaw()
				
				currentUsedVertList = {}
				
				# Sg is a string
				currentSmoothGroup = '(null)'
				currentUsedVertListSmoothGroup = VERT_USED_LIST[:]						
				currentUsedVertList[currentSmoothGroup] = currentUsedVertListSmoothGroup
				currentMaterialMeshMapping = {}
				
				meshDict[currentObjectName] = (currentMesh, currentUsedVertList, currentMaterialMeshMapping)
				currentMesh.hasFaceUV(1)
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
			if len(l) == 1 or l[1] == '(null)':
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
					currentImg = comprehansiveImageLoad(newImgName, DIR) # Use join in case of spaces 
					imageDict[newImgName] = currentImg
					# These may be None, thats okay.
					
					
		
		# MATERIAL FILE
		elif l[0] == 'mtllib':
			mtl_fileName.append(' '.join(l[1:]) ) # SHOULD SUPPORT MULTIPLE MTL?
		lIdx+=1
	
	# Applies material properties to materials alredy on the mesh as well as Textures.
	for mtl in mtl_fileName:
		load_mtl(DIR, mtl, meshDict, materialDict)	
	
	
	importedObjects = []
	for mk, me in meshDict.iteritems():
		nme = me[0]
		
		# Ignore no vert meshes.
		if not nme.verts: # == []
			continue
		
		name = getUniqueName(mk)
		ob = NMesh.PutRaw(nme, name)
		ob.name = name
		
		importedObjects.append(ob)
	
	# Select all imported objects.
	for ob in importedObjects:
		ob.sel = 1
	if badObjUvs > 0:
		print '\tERROR: found %d faces with badly formatted UV coords. everything else went okay.' % badObjUvs
	
	if badObjFaceVerts > 0:
		print '\tERROR: found %d faces reusing the same vertex. everything else went okay.' % badObjFaceVerts
	
	if badObjFaceTexCo > 0:
		print '\tERROR: found %d faces with invalit texture coords. everything else went okay.' % badObjFaceTexCo		
	
	
	print "obj import time: ", sys.time() - time1
	
# Batch directory loading.
def load_obj_dir(obj_dir):
	
	# Strip file
	obj_dir = stripFile(obj_dir)	
	time = sys.time()
	
	objFiles = [f for f in os.listdir(obj_dir) if f.lower().endswith('obj')]
	
	Window.DrawProgressBar(0, '')
	count = 0
	obj_len = len(objFiles)
	for obj in objFiles:
		count+=1
		
		newScn = Scene.New(obj)
		newScn.makeCurrent()
		
		Window.DrawProgressBar((float(count)/obj_len) - 0.01, '%s: %i of %i' % (obj, count, obj_len))
		
		load_obj(obj_dir  + obj)
		
	Window.DrawProgressBar(1, '')
	print 'Total obj import "%s" dir: %.2f' % (obj_dir, sys.time() - time)


def main():
	TEXT_IMPORT = 'Import a Wavefront OBJ'
	TEXT_BATCH_IMPORT = 'Import *.obj to Scenes'
	
	if Window.GetKeyQualifiers() & Window.Qual.SHIFT:
		if not os:
			Draw.PupMenu('Module "os" not found, needed for batch load, using normal selector.')
			Window.FileSelector(load_obj, TEXT_IMPORT)
		else:
			Window.FileSelector(load_obj_dir, TEXT_BATCH_IMPORT)
	else:
		Window.FileSelector(load_obj, TEXT_IMPORT)

if __name__ == '__main__':
	main()
