#!BPY
 
"""
Name: 'Wavefront (.obj)...'
Blender: 237
Group: 'Import'
Tooltip: 'Load a Wavefront OBJ File, Shift: batch import all dir.'
"""

__author__= "Campbell Barton"
__url__= ["blender", "elysiun"]
__version__= "1.0"

__bpydoc__= """\
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
	lastSlash= max(path.rfind('\\'), path.rfind('/'))
	if lastSlash != -1:
		path= path[:lastSlash]
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
	index= name.rfind('.')
	if index != -1:
		return name[ : index ]
	else:
		return name


from Blender import *
import BPyImage
reload(BPyImage)

# takes a polyline of indicies (fgon)
# and returns a list of face indicie lists.
def ngon(from_mesh, indicies):
	if len(indicies) < 4:
		return [indicies]
	temp_mesh_name= '~NGON_TEMP~'
	is_editmode= Window.EditMode()
	if is_editmode:
		Window.EditMode(0)
	try:
		temp_mesh = Mesh.Get(temp_mesh_name)
		if temp_mesh.users!=0:
			temp_mesh = Mesh.New(temp_mesh_name)
	except:
		temp_mesh = Mesh.New(temp_mesh_name)
		
	
	temp_mesh.verts.extend( [from_mesh.verts[i].co for i in indicies] )
	temp_mesh.edges.extend( [(temp_mesh.verts[i], temp_mesh.verts[i-1]) for i in xrange(len(temp_mesh.verts))] )
	
	oldmode = Mesh.Mode()
	Mesh.Mode(Mesh.SelectModes['VERTEX'])
	for v in temp_mesh.verts:
		v.sel= 1
	
	# Must link to scene
	scn= Scene.GetCurrent()
	temp_ob= Object.New('Mesh')
	temp_ob.link(temp_mesh)
	scn.link(temp_ob)
	temp_mesh.fill()
	scn.unlink(temp_ob)
	Mesh.Mode(oldmode)
	
	new_indicies= [ [v.index for v in f.v]  for f in temp_mesh.faces ]
	
	if not new_indicies: # JUST DO A FAN, Cant Scanfill
		print 'Warning Cannot scanfill!- Fallback on a triangle fan.'
		new_indicies = [ [indicies[0], indicies[i-1], indicies[i]] for i in xrange(2, len(indicies)) ]
	else:
		# Use real scanfill.
		# See if its flipped the wrong way.
		flip= None
		for fi in new_indicies:
			if flip != None:
				break
			for i, vi in enumerate(fi):
				if vi==0 and fi[i-1]==1:
					flip= False
					break
				elif vi==1 and fi[i-1]==0:
					flip= True
					break
		
		if not flip:
			for fi in new_indicies:
				fi.reverse()
	
	if is_editmode:
		Window.EditMode(1)
		
	# Save some memory and forget about the verts.
	# since we cant unlink the mesh.
	temp_mesh.verts= None 
	
	return new_indicies
	


# EG
'''
scn= Scene.GetCurrent()
me = scn.getActiveObject().getData(mesh=1)
ind= [v.index for v in me.verts if v.sel] # Get indicies

indicies = ngon(me, ind) # fill the ngon.

# Extand the faces to show what the scanfill looked like.
print len(indicies)
me.faces.extend([[me.verts[ii] for ii in i] for i in indicies])
'''




try:
	import os
except:
	# So we know if os exists.
	print 'Module "os" not found, install python to enable comprehensive image finding and batch loading.'
	os= None


#==================================================================================#
# This function sets textures defined in .mtl file                                 #
#==================================================================================#
def loadMaterialImage(mat, img_fileName, type, meshDict, dir):
	TEX_ON_FLAG= NMesh.FaceModes['TEX']
	
	texture= Texture.New(type)
	texture.setType('Image')
	
	# Absolute path - c:\.. etc would work here
	image= BPyImage.comprehensiveImageLoad(img_fileName, dir)
	
	if image:
		texture.image= image
		
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
						f.image= image
	
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
			mat= materialDict[matName]= Material.New(matName)
			return mat
		
			
	mtl_file= stripPath(mtl_file)
	mtl_fileName= dir + mtl_file
	
	try:
		fileLines= open(mtl_fileName, 'r').readlines()
	except IOError:
		print '\tunable to open referenced material file: "%s"' % mtl_fileName
		return
	
	try:
		lIdx=0
		while lIdx < len(fileLines):
			l= fileLines[lIdx].split()
			
			# Detect a line that will be ignored
			if len(l) == 0 or l[0].startswith('#'):
				pass
			elif l[0] == 'newmtl':
				currentMat= getMat('_'.join(l[1:]), materialDict) # Material should alredy exist.
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
				img_fileName= ' '.join(l[1:])
				loadMaterialImage(currentMat, img_fileName, 'Ka', meshDict, dir)
			elif l[0] == 'map_Ks':
				img_fileName= ' '.join(l[1:])
				loadMaterialImage(currentMat, img_fileName, 'Ks', meshDict, dir)
			elif l[0] == 'map_Kd':
				img_fileName= ' '.join(l[1:])
				loadMaterialImage(currentMat, img_fileName, 'Kd', meshDict, dir)
			
			# new additions
			elif l[0] == 'map_Bump': # Bumpmap
				img_fileName= ' '.join(l[1:])			
				loadMaterialImage(currentMat, img_fileName, 'Bump', meshDict, dir)
			elif l[0] == 'map_D': # Alpha map - Dissolve
				img_fileName= ' '.join(l[1:])			
				loadMaterialImage(currentMat, img_fileName, 'D', meshDict, dir)

			elif l[0] == 'refl': # Reflectionmap
				img_fileName= ' '.join(l[1:])			
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
	newName= name[:19] # 19 chars is the longest name.
	uniqueInt= 0
	while newName in getUniqueName.uniqueNames:
		newName= '%s.%.3i' % (name[:15], uniqueInt)
		uniqueInt +=1
	getUniqueName.uniqueNames.append(newName)
	return newName
getUniqueName.uniqueNames= []

#==================================================================================#
# This loads data from .obj file                                                   #
#==================================================================================#
def load_obj(file, IMPORT_MTL=1, IMPORT_EDGES=1, IMPORT_SMOOTH_ALL=0, IMPORT_FGON=1, IMPORT_SMOOTH_GROUPS=0, IMPORT_MTL_SPLIT=0, IMPORT_RELATIVE_VERTS=0):
	global currentMesh,\
	currentUsedVertList,\
	currentUsedVertListSmoothGroup,\
	meshDict,\
	contextMeshMatIdx,\
	currentMaterialMeshMapping
	
	print '\nImporting OBJ file: "%s"' % file
	
	time1= sys.time()
	
	getUniqueName.uniqueNames.extend( [ob.name for ob in Object.Get()] )
	getUniqueName.uniqueNames.extend( NMesh.GetNames() )
	
	# Deselect all objects in the scene.
	# do this first so we dont have to bother, with objects we import
	for ob in Scene.GetCurrent().getChildren():
		ob.sel= 0
	
	TEX_OFF_FLAG= ~NMesh.FaceModes['TEX']
	
	# Get the file name with no path or .obj
	fileName= stripExt( stripPath(file) )

	mtl_fileName= [] # Support multiple mtl files if needed.

	DIR= stripFile(file)
	
	tempFile= open(file, 'r')
	fileLines= tempFile.readlines()	
	tempFile.close()
	del tempFile
	uvMapList= [] # store tuple uv pairs here
	
	# This dummy vert makes life a whole lot easier-
	# pythons index system then aligns with objs, remove later
	vertList= [] # Could havea vert but since this is a placeholder theres no Point
	
	
	# Store all imported images in a dict, names are key
	imageDict= {}
	
	# This stores the index that the current mesh has for the current material.
	# if the mesh does not have the material then set -1
	contextMeshMatIdx= -1
	
	# Keep this out of the dict for easy accsess.
	nullMat= Material.New('(null)')
	
	currentMat= nullMat # Use this mat.
	currentImg= None # Null image is a string, otherwise this should be set to an image object.\
	if IMPORT_SMOOTH_ALL:
		currentSmooth= True
	else:
		currentSmooth= False
	
	# Store a list of unnamed names
	currentUnnamedGroupIdx= 1
	currentUnnamedObjectIdx= 1
	
	quadList= (0, 1, 2, 3)
	
	faceQuadVList= [None, None, None, None]
	faceTriVList= [None, None, None]
	
	#==================================================================================#
	# Load all verts first (texture verts too)                                         #
	#==================================================================================#
	print '\tfile length: %d' % len(fileLines)
	# Ignore normals and comments.
	fileLines= [lsplit for l in fileLines if not l.startswith('vn') if not l.startswith('#') for lsplit in (l.split(),) if lsplit]
	Vert= NMesh.Vert
	vertList= [Vert(float(l[1]), float(l[2]), float(l[3]) ) for l in fileLines if l[0] == 'v']
	uvMapList= [(float(l[1]), float(l[2])) for l in fileLines if l[0] == 'vt']
	if IMPORT_SMOOTH_GROUPS:
		smoothingGroups=  dict([('_'.join(l[1:]), None) for l in fileLines if l[0] == 's' ])
	else:
		smoothingGroups= {}
	materialDict=     dict([('_'.join(l[1:]), None) for l in fileLines if l[0] == 'usemtl']) # Store all imported materials as unique dict, names are key
	print '\tvert:%i  texverts:%i  smoothgroups:%i  materials:%s' % (len(vertList), len(uvMapList), len(smoothingGroups), len(materialDict))
	
	# Replace filelines, Excluding v excludes "v ", "vn " and "vt "
	# Remove any variables we may have created.
	try: del _dummy
	except: pass
	try: del _x
	except: pass
	try: del _y
	except: pass
	try: del _z
	except: pass
	try: del lsplit
	except: pass
	del Vert
	
	
	# With negative values this is used a lot. make faster access.
	len_uvMapList= len(uvMapList)
	len_vertList= len(vertList)
	
	#  Only want unique keys anyway
	smoothingGroups['(null)']= None # Make sure we have at least 1.
	smoothingGroups= smoothingGroups.keys()
	print '\tfound %d smoothing groups.' % (len(smoothingGroups) -1)
	
	# Add materials to Blender for later is in teh OBJ
	for k in materialDict.iterkeys():
		materialDict[k]= Material.New(k)
	
	
	# Make a list of all unused vert indicies that we can copy from
	VERT_USED_LIST= [-1]*len_vertList
	
	# Here we store a boolean list of which verts are used or not
	# no we know weather to add them to the current mesh
	# This is an issue with global vertex indicies being translated to per mesh indicies
	# like blenders, we start with a dummy just like the vert.
	# -1 means unused, any other value refers to the local mesh index of the vert.

	# currentObjectName has a char in front of it that determins weather its a group or object.
	# We ignore it when naming the object.
	currentObjectName= 'unnamed_obj_0' # If we cant get one, use this
	
	if IMPORT_MTL_SPLIT:
		currentObjectName_real= currentObjectName
	
	#meshDict= {} # The 3 variables below are stored in a tuple within this dict for each mesh
	currentMesh= NMesh.GetRaw() # The NMesh representation of the OBJ group/Object
	#currentUsedVertList= {} # A Dict of smooth groups, each smooth group has a list of used verts and they are generated on demand so as to save memory.
	currentMaterialMeshMapping= {} # Used to store material indicies so we dont have to search the mesh for materials every time.
	
	# Every mesh has a null smooth group, this is used if there are no smooth groups in the OBJ file.
	# and when for faces where no smooth group is used.
	currentSmoothGroup= '(null)' # The Name of the current smooth group
	
	# For direct accsess to the Current Meshes, Current Smooth Groups- Used verts.
	# This is of course context based and changes on the fly.
	# Set the initial '(null)' Smooth group, every mesh has one.
	currentUsedVertListSmoothGroup= VERT_USED_LIST[:]
	currentUsedVertList= {currentSmoothGroup: currentUsedVertListSmoothGroup }
	
	# 0:NMesh, 1:SmoothGroups[UsedVerts[0,0,0,0]], 2:materialMapping['matname':matIndexForThisNMesh]
	meshDict= {currentObjectName: (currentMesh, currentUsedVertList, currentMaterialMeshMapping) }
	
	# Only show the bad uv error once 
	badObjUvs= 0
	badObjFaceVerts= 0
	badObjFaceTexCo= 0
	
	
	#currentMesh.verts.append(vertList[0]) # So we can sync with OBJ indicies where 1 is the first item.
	if len_uvMapList > 1:
		currentMesh.hasFaceUV(1) # Turn UV's on if we have ANY texture coords in this obj file.
	
	
	
	# Heres the code that gets a mesh, creating a new one if needed.
	# may_exist is used to avoid a dict looup.
	# if the mesh is unnamed then we generate a new name and dont bother looking
	# to see if its alredy there.
	def obj_getmesh(may_exist):
		global currentMesh,\
		currentUsedVertList,\
		currentUsedVertListSmoothGroup,\
		meshDict,\
		contextMeshMatIdx,\
		currentMaterialMeshMapping
		
		#print 'getting mesh,', currentObjectName
		
		# If we havnt written to this mesh before then do so.
		# if we have then we'll just keep appending to it, this is required for soem files.
		
		# If we are new, or we are not yet in the list of added meshes
		# then make us new mesh.
		if (not may_exist) or (not meshDict.has_key(currentObjectName)):
			currentMesh= NMesh.GetRaw()
			
			currentUsedVertList= {}
			
			# SmoothGroup is a string
			########currentSmoothGroup= '(null)' # From examplesm changing the g/o shouldent change the smooth group.
			currentUsedVertList[currentSmoothGroup]= currentUsedVertListSmoothGroup= VERT_USED_LIST[:]						
			
			currentMaterialMeshMapping= {}
			meshDict[currentObjectName]= (currentMesh, currentUsedVertList, currentMaterialMeshMapping)
			currentMesh.hasFaceUV(1)
			contextMeshMatIdx= -1
			
		else: 
			# Since we have this in Blender then we will check if the current Mesh has the material.
			# set the contextMeshMatIdx to the meshs index but only if we have it.
			currentMesh, currentUsedVertList, currentMaterialMeshMapping= meshDict[currentObjectName]
			#getMeshMaterialIndex(currentMesh, currentMat)
			
			try:
				contextMeshMatIdx= currentMaterialMeshMapping[currentMat.name] #getMeshMaterialIndex(currentMesh, currentMat)
			except KeyError:
				contextMeshMatIdx -1
			
			# For new meshes switch smoothing groups to null
			########currentSmoothGroup= '(null)'  # From examplesm changing the g/o shouldent change the smooth group.
			try:
				currentUsedVertListSmoothGroup= currentUsedVertList[currentSmoothGroup]
			except:
				currentUsedVertList[currentSmoothGroup]= currentUsedVertListSmoothGroup= VERT_USED_LIST[:]						
		
	
	
	
	
	
	
	#==================================================================================#
	# Load all faces into objects, main loop                                           #
	#==================================================================================#
	lIdx= 0
	EDGE_FGON_FLAG= NMesh.EdgeFlags['FGON']
	EDGE_DRAW_FLAG= NMesh.EdgeFlags['EDGEDRAW']
	
	if IMPORT_RELATIVE_VERTS:
		VERT_COUNT= TVERT_COUNT=0
		# TOT_VERTS= len(vertList) 
		# len(vertList)  
		# len(uvMapList)
	
	while lIdx < len(fileLines):
		l= fileLines[lIdx]
		#for l in fileLines:
		if len(l) == 0:
			continue
		# FACE
		elif l[0] == 'v':
			if IMPORT_RELATIVE_VERTS:
				VERT_COUNT+=1
		elif l[0] == 'vt':
			if IMPORT_RELATIVE_VERTS:
				TVERT_COUNT+=1
		
		elif l[0] == 'f' or l[0] == 'fo': # fo is not standard.
			# Make a face with the correct material.
			
			# Add material to mesh
			if contextMeshMatIdx == -1:
				tmpMatLs= currentMesh.materials
				
				if len(tmpMatLs) == 16:
					contextMeshMatIdx= 0 # Use first material
					print 'material overflow, attempting to use > 16 materials. defaulting to first.'
				else:
					contextMeshMatIdx= len(tmpMatLs)
					currentMaterialMeshMapping[currentMat.name]= contextMeshMatIdx
					currentMesh.addMaterial(currentMat)
			
			# Set up vIdxLs : Verts
			# Set up vtIdxLs : UV
			# Start with a dummy objects so python accepts OBJs 1 is the first index.
			vIdxLs= []
			vtIdxLs= []
			
			
			fHasUV= len_uvMapList # Assume the face has a UV until it sho it dosent, if there are no UV coords then this will start as 0.
			
			# Support stupid multiline faces
			# not an obj spec but some objs exist that do this.
			# f 1 2 3 \ 
			#   4 5 6 \
			# ..... instead of the more common and sane.
			# f 1 2 3 4 5 6
			#
			# later lines are not modified, just skepped by advancing "lIdx"
			while l[-1] == '\\':
				l.pop()
				lIdx+=1
				l.extend(fileLines[lIdx])
			# Done supporting crappy obj faces over multiple lines.
			
			for v in l:
				if v is not 'f': # Only the first v will be f, any better ways to skip it?
					# OBJ files can have // or / to seperate vert/texVert/normal
					# this is a bit of a pain but we must deal with it.
					objVert= v.split('/')
					
					# Vert Index - OBJ supports negative index assignment (like python)
					index= int(objVert[0])-1
					# Account for negative indicies.
					if index < 0:
						if IMPORT_RELATIVE_VERTS: # non standard
							index= VERT_COUNT+index+1
						else:
							index= len_vertList+index+1
					elif IMPORT_RELATIVE_VERTS:
						index= VERT_COUNT+index+1 # UNTESTED, POSITIVE RELATIVE VERTS.May be out by 1.
					
					vIdxLs.append(index)
					if fHasUV:
						# UV
						index= 0 # Dummy var
						if len(objVert) == 1:
							index= vIdxLs[-1]
						elif objVert[1]: # != '' # Its possible that theres no texture vert just he vert and normal eg 1//2
							index= int(objVert[1])-1
							if index < 0:
								if IMPORT_RELATIVE_VERTS: # non standard
									index= TVERT_COUNT+index+1
								else:
									index= len_uvMapList+index+1
							elif IMPORT_RELATIVE_VERTS: # non standard:
								index= TVERT_COUNT+index+1 # UNTESTED, POSITIVE RELATIVE VERTS. May be out by 1.
							
						if len_uvMapList > index:
							vtIdxLs.append(index) # Seperate UV coords
						else:
							# BAD FILE, I have found this so I account for it.
							# INVALID UV COORD
							# Could ignore this- only happens with 1 in 1000 files.
							badObjFaceTexCo +=1
							vtIdxLs.append(1)
							
							fHasUV= 0
		
						# Dont add a UV to the face if its larger then the UV coord list
						# The OBJ file would have to be corrupt or badly written for thi to happen
						# but account for it anyway.
						if len(vtIdxLs) > 0:
							if vtIdxLs[-1] > len_uvMapList:
								fHasUV= 0
								
								badObjUvs +=1 # ERROR, Cont
			
			# Quads only, we could import quads using the method below but it polite to import a quad as a quad.
			#print lIdx, len(vIdxLs), len(currentUsedVertListSmoothGroup)
			#print fileLines[lIdx]
			
			# Add all the verts we need,
			# dont edd edge verts if were not importing them.
			face_vert_count= len(vIdxLs)
			if (not IMPORT_EDGES) and face_vert_count == 2:
				pass
			else:
				# Add the verts that arnt alredy added.
				for i in vIdxLs:
					if currentUsedVertListSmoothGroup[i] == -1:
						v= vertList[i]
						currentMesh.verts.append(v)
						currentUsedVertListSmoothGroup[i]= len(currentMesh.verts)-1
			
			if face_vert_count == 2:
				if IMPORT_EDGES and vIdxLs[0]!=vIdxLs[1]:
					# Edge
					currentMesh.addEdge(\
					currentMesh.verts[currentUsedVertListSmoothGroup[vIdxLs[0]]],\
					currentMesh.verts[currentUsedVertListSmoothGroup[vIdxLs[0]]]) 
					
			elif face_vert_count == 4:
				
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
						faceQuadVList[i]= currentMesh.verts[currentUsedVertListSmoothGroup[vIdxLs[i]]]
					
					f= NMesh.Face(faceQuadVList)
					# UV MAPPING
					if fHasUV:
						f.uv= [uvMapList[ vtIdxLs[0] ],uvMapList[ vtIdxLs[1] ],uvMapList[ vtIdxLs[2] ],uvMapList[ vtIdxLs[3] ]]
						if currentImg:
							f.image= currentImg
						else:
							f.mode &= TEX_OFF_FLAG
					
					f.mat= contextMeshMatIdx
					f.smooth= currentSmooth
					currentMesh.faces.append(f) # move the face onto the mesh
			
			elif face_vert_count == 3: # This handles tri's and fans, dont use fans anymore.
				for i in range(face_vert_count-2):
					if vIdxLs[0] == vIdxLs[i+1] or\
					vIdxLs[0] == vIdxLs[i+2] or\
					vIdxLs[i+1] == vIdxLs[i+2]:
						badObjFaceVerts+=1
					else:
						for k, j in [(0,0), (1,i+1), (2,i+2)]:
							faceTriVList[k]= currentMesh.verts[currentUsedVertListSmoothGroup[vIdxLs[j]]]	
						
						f= NMesh.Face(faceTriVList)	
						
						# UV MAPPING
						if fHasUV:
							f.uv= [uvMapList[vtIdxLs[0]], uvMapList[vtIdxLs[i+1]], uvMapList[vtIdxLs[i+2]]]
							if currentImg:
								f.image= currentImg
							else:
								f.mode &= TEX_OFF_FLAG
						
						f.mat= contextMeshMatIdx
						f.smooth= currentSmooth
						currentMesh.faces.append(f) # move the face onto the mesh
			
			elif face_vert_count > 4: # NGons.
				# we need to map indicies to uv coords.
				currentMeshRelativeIdxs= [currentUsedVertListSmoothGroup[i] for i in vIdxLs]
				
				if fHasUV:
					vert2UvMapping=dict( [ (currentMeshRelativeIdxs[i],vtIdxLs[i]) for i in xrange(face_vert_count)] )
				
				ngon_face_indicies= ngon(currentMesh, currentMeshRelativeIdxs)
				
				# At the moment scanfill always makes tri's but dont count on it
				for fillFace in ngon_face_indicies:
					f= NMesh.Face([currentMesh.verts[currentMeshRelativeIdxs[i]] for i in fillFace])
					
					if fHasUV:
						f.uv= [uvMapList[vert2UvMapping[currentMeshRelativeIdxs[i]]] for i in fillFace]
						if currentImg:
							f.image= currentImg
						else:
							f.mode &= TEX_OFF_FLAG
					
					f.mat= contextMeshMatIdx
					f.smooth= currentSmooth
					currentMesh.faces.append(f) # move the face onto the mesh
				
				# Set fgon flag.
				if IMPORT_FGON:
					edgeUsers={}
					for fillFace in ngon_face_indicies:
						for i in xrange(len(fillFace)): # Should always be 3
							i1= currentMeshRelativeIdxs[fillFace[i]]
							i2= currentMeshRelativeIdxs[fillFace[i-1]]
							
							# Sort the pair so thet always match.
							if i1>i2: i1,i2=i2,i1
								
							try:
								edgeUsers[i1,i2]+= 1
							except:
								edgeUsers[i1,i2]= 0
					
					for edgeVerts, users in edgeUsers.iteritems():
						if users:
							ed= currentMesh.addEdge(\
							 currentMesh.verts[edgeVerts[0]],\
							 currentMesh.verts[edgeVerts[1]])
							
							ed.flag|= EDGE_FGON_FLAG
			
		# FACE SMOOTHING
		elif l[0] == 's' and IMPORT_SMOOTH_GROUPS:
			# No value? then turn on.
			if len(l) == 1:
				currentSmooth= True
				currentSmoothGroup= '(null)'
			else:
				if l[1] == 'off': # We all have a null group so dont need to try, will try anyway to avoid code duplication.
					if not IMPORT_SMOOTH_ALL:
						currentSmooth= False
					currentSmoothGroup= '(null)'
				else: 
					currentSmooth= True
					currentSmoothGroup= '_'.join(l[1:])
			try:
				currentUsedVertListSmoothGroup= currentUsedVertList[currentSmoothGroup]
			except KeyError:
				currentUsedVertList[currentSmoothGroup]= currentUsedVertListSmoothGroup= VERT_USED_LIST[:]
		
		
		# OBJECT / GROUP
		elif l[0] == 'o' or l[0] == 'g':
			
			# Forget about the current image
			currentImg= None
			
			# This makes sure that if an object and a group have the same name then
			# they are not put into the same object.
			
			# Only make a new group.object name if the verts in the existing object have been used, this is obscure
			# but some files face groups seperating verts and faces which results in silly things. (no groups have names.)
			if len(l) == 1:
				# Make a new empty name
				if l[0] == 'g': # Make a blank group name
					currentObjectName= 'unnamed_grp_%.4d' % currentUnnamedGroupIdx
					currentUnnamedGroupIdx +=1
				else: # is an object.
					currentObjectName= 'unnamed_ob_%.4d' % currentUnnamedObjectIdx
					currentUnnamedObjectIdx +=1
				may_exist= False # we know the model is new.
			else: # No name given
				currentObjectName= '_'.join(l[1:])
				may_exist= True
			
			if IMPORT_MTL_SPLIT:
				currentObjectName_real= currentObjectName
				currentObjectName += '_'+currentMat.name
			
			obj_getmesh(may_exist)
				
		
		# MATERIAL
		elif l[0] == 'usemtl':
			if len(l) == 1 or l[1] == '(null)':
				currentMat= nullMat # We know we have a null mat.
			else:
				currentMat= materialDict['_'.join(l[1:])]
				try:
					contextMeshMatIdx= currentMaterialMeshMapping[currentMat.name]
				except KeyError:
					contextMeshMatIdx= -1 #getMeshMaterialIndex(currentMesh, currentMat)
			
			# Check if we are splitting by material.
			if IMPORT_MTL_SPLIT:
				currentObjectName= currentObjectName_real+'_'+currentMat.name
				obj_getmesh(True)
			
			
		# IMAGE
		elif l[0] == 'usemat' or l[0] == 'usemap':
			if len(l) == 1 or l[1] == '(null)' or l[1] == 'off':
				currentImg= None
			else:
				# Load an image.
				newImgName= stripPath(' '.join(l[1:])) # Use space since its a file name.
				
				try:
					# Assume its alredy set in the dict (may or maynot be loaded)
					currentImg= imageDict[newImgName]
				
				except KeyError: # Not in dict, add for first time.
					# Image has not been added, Try and load the image
					currentImg= BPyImage.comprehensiveImageLoad(newImgName, DIR) # Use join in case of spaces 
					imageDict[newImgName]= currentImg
					# These may be None, thats okay.
		
		# MATERIAL FILE
		elif l[0] == 'mtllib' and IMPORT_MTL and len(l)>1:
			mtl_fileName.append(' '.join(l[1:]) ) # Support for multiple MTL's
		lIdx+=1
		
	# Applies material properties to materials alredy on the mesh as well as Textures.
	if IMPORT_MTL:
		for mtl in mtl_fileName:
			load_mtl(DIR, mtl, meshDict, materialDict)	
	print 'MTLLLL', mtl_fileName
	
	importedObjects= []
	for mk, me in meshDict.iteritems():
		nme= me[0]
		
		# Ignore no vert meshes.
		if not nme.verts: # == []
			continue
		name= getUniqueName(mk)
		ob= NMesh.PutRaw(nme, name)
		ob.name= name
		
		importedObjects.append(ob)
	
	# Select all imported objects.
	for ob in importedObjects:
		ob.sel= 1
	if badObjUvs > 0:
		print '\tERROR: found %d faces with badly formatted UV coords. everything else went okay.' % badObjUvs
	
	if badObjFaceVerts > 0:
		print '\tERROR: found %d faces reusing the same vertex. everything else went okay.' % badObjFaceVerts
	
	if badObjFaceTexCo > 0:
		print '\tERROR: found %d faces with invalit texture coords. everything else went okay.' % badObjFaceTexCo		
	
	
	print "obj import time: ", sys.time() - time1

def load_obj_ui(file):
	
	IMPORT_MTL= Draw.Create(1)
	IMPORT_DIR= Draw.Create(0)
	IMPORT_NEW_SCENE= Draw.Create(0)
	IMPORT_EDGES= Draw.Create(1)
	IMPORT_SMOOTH_ALL= Draw.Create(1)
	IMPORT_FGON= Draw.Create(1)
	IMPORT_SMOOTH_GROUPS= Draw.Create(0)
	IMPORT_MTL_SPLIT= Draw.Create(0)
	IMPORT_RELATIVE_VERTS= Draw.Create(0)
	
	# Get USER Options
	pup_block= [\
	('Material (*.mtl)', IMPORT_MTL, 'Imports material settings and images from the obj\'s .mtl file'),\
	('All *.obj\'s in dir', IMPORT_DIR, 'Import all obj files in this dir (avoid overlapping data with "Create scene")'),\
	('Create scene', IMPORT_NEW_SCENE, 'Imports each obj into its own scene, named from the file'),\
	'Geometry...',\
	('Edges', IMPORT_EDGES, 'Import faces with 2 verts as in edge'),\
	('Smooths all faces', IMPORT_SMOOTH_ALL, 'Smooth all faces even if they are not in a smoothing group'),\
	('Create FGons', IMPORT_FGON, 'Import faces with more then 4 verts as fgons.'),\
	('Smooth Groups', IMPORT_SMOOTH_GROUPS, 'Only Share verts within smooth groups. (Warning, Hogs Memory)'),\
	('Split by Material', IMPORT_MTL_SPLIT, 'Import each material into a seperate mesh (Avoids >16 meterials per mesh problem)'),\
	('Relative Verts', IMPORT_RELATIVE_VERTS, 'Import non standard OBJs with relative vertex indicies, try if your mesh imports with scrambled faces.'),\
	]
	
	if not os:
		pup_block.pop(2) # Make sure this is the IMPORT_DIR option that requires OS
	
	if not Draw.PupBlock('Import...', pup_block):
		return
	
	Window.WaitCursor(1)
	Window.DrawProgressBar(0, '')
	time= sys.time()
	
	IMPORT_MTL= IMPORT_MTL.val
	IMPORT_DIR= IMPORT_DIR.val
	IMPORT_NEW_SCENE= IMPORT_NEW_SCENE.val
	IMPORT_EDGES= IMPORT_EDGES.val
	IMPORT_SMOOTH_ALL= IMPORT_SMOOTH_ALL.val
	IMPORT_FGON= IMPORT_FGON.val
	IMPORT_SMOOTH_GROUPS= IMPORT_SMOOTH_GROUPS.val
	IMPORT_MTL_SPLIT= IMPORT_MTL_SPLIT.val
	IMPORT_RELATIVE_VERTS= IMPORT_RELATIVE_VERTS.val
	#orig_scene= Scene.GetCurrent()
	
	obj_dir= stripFile(file)
	if IMPORT_DIR:
		obj_files= [(obj_dir,f) for f in os.listdir(obj_dir) if f.lower().endswith('obj')]
	else:
		obj_files= [(obj_dir,stripPath(file))]
	
	obj_len= len(obj_files)
	count= 0
	for d, f in obj_files:
		count+= 1
		if not sys.exists(d+f):
			print 'Error: "%s%s" does not exist' % (d,f)
		else:
			if IMPORT_NEW_SCENE:
				scn= Scene.New('.'.join(f.split('.')[0:-1]))
				scn.makeCurrent()
			
			
			Window.DrawProgressBar((float(count)/obj_len) - 0.01, '%s: %i of %i' % (f, count, obj_len))
			load_obj(d+f, IMPORT_MTL, IMPORT_EDGES, IMPORT_SMOOTH_ALL, IMPORT_FGON, IMPORT_SMOOTH_GROUPS, IMPORT_MTL_SPLIT, IMPORT_RELATIVE_VERTS)
			
	
	#orig_scene.makeCurrent() # We can leave them in there new scene.
	Window.DrawProgressBar(1, '')
	Window.WaitCursor(0)
	
	if count > 1:
		print 'Total obj import "%s" dir: %.2f' % (obj_dir, sys.time() - time)


def main():
	Window.FileSelector(load_obj_ui, 'Import a Wavefront OBJ')

if __name__ == '__main__':
	main()
	pass

#load_obj('/1test.obj')