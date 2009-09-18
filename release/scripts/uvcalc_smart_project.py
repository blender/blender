#!BPY

""" Registration info for Blender menus: <- these words are ignored
Name: 'Unwrap (smart projections)'
Blender: 240
Group: 'UVCalculation'
Tooltip: 'UV Unwrap mesh faces for all select mesh objects'
"""


__author__ = "Campbell Barton"
__url__ = ("blender", "blenderartists.org")
__version__ = "1.1 12/18/05"

__bpydoc__ = """\
This script projection unwraps the selected faces of a mesh.

it operates on all selected mesh objects, and can be used unwrap
selected faces, or all faces.
"""

# -------------------------------------------------------------------------- 
# Smart Projection UV Projection Unwrapper v1.1 by Campbell Barton (AKA Ideasman) 
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


from Blender import Object, Draw, Window, sys, Mesh, Geometry
from Blender.Mathutils import Matrix, Vector, RotationMatrix
import bpy
from math import cos

DEG_TO_RAD = 0.017453292519943295 # pi/180.0
SMALL_NUM = 0.000000001
BIG_NUM = 1e15

global USER_FILL_HOLES
global USER_FILL_HOLES_QUALITY
USER_FILL_HOLES = None
USER_FILL_HOLES_QUALITY = None

dict_matrix = {}

def pointInTri2D(v, v1, v2, v3):
	global dict_matrix
	
	key = v1.x, v1.y, v2.x, v2.y, v3.x, v3.y
	
	# Commented because its slower to do teh bounds check, we should realy cache the bounds info for each face.
	'''
	# BOUNDS CHECK
	xmin= 1000000
	ymin= 1000000
	
	xmax= -1000000
	ymax= -1000000
	
	for i in (0,2,4):
		x= key[i]
		y= key[i+1]
		
		if xmax<x:	xmax= x
		if ymax<y:	ymax= y
		if xmin>x:	xmin= x
		if ymin>y:	ymin= y	
	
	x= v.x
	y= v.y
	
	if x<xmin or x>xmax or y < ymin or y > ymax:
		return False
	# Done with bounds check
	'''
	try:
		mtx = dict_matrix[key]
		if not mtx:
			return False
	except:
		side1 = v2 - v1
		side2 = v3 - v1
		
		nor = side1.cross(side2)
		
		l1 = [side1[0], side1[1], side1[2]]
		l2 = [side2[0], side2[1], side2[2]]
		l3 = [nor[0], nor[1], nor[2]]
		
		mtx = Matrix(l1, l2, l3)
		
		# Zero area 2d tri, even tho we throw away zerop area faces
		# the projection UV can result in a zero area UV.
		if not mtx.determinant():
			dict_matrix[key] = None
			return False
		
		mtx.invert()
		
		dict_matrix[key] = mtx
	
	uvw = (v - v1) * mtx
	return 0 <= uvw[0] and 0 <= uvw[1] and uvw[0] + uvw[1] <= 1

	
def boundsIsland(faces):
	minx = maxx = faces[0].uv[0][0] # Set initial bounds.
	miny = maxy = faces[0].uv[0][1]
	# print len(faces), minx, maxx, miny , maxy
	for f in faces:
		for uv in f.uv:
			x= uv.x
			y= uv.y
			if x<minx: minx= x
			if y<miny: miny= y
			if x>maxx: maxx= x
			if y>maxy: maxy= y
	
	return minx, miny, maxx, maxy

"""
def boundsEdgeLoop(edges):
	minx = maxx = edges[0][0] # Set initial bounds.
	miny = maxy = edges[0][1]
	# print len(faces), minx, maxx, miny , maxy
	for ed in edges:
		for pt in ed:
			print 'ass'
			x= pt[0]
			y= pt[1]
			if x<minx: x= minx
			if y<miny: y= miny
			if x>maxx: x= maxx
			if y>maxy: y= maxy
	
	return minx, miny, maxx, maxy
"""

# Turns the islands into a list of unpordered edges (Non internal)
# Onlt for UV's
# only returns outline edges for intersection tests. and unique points.

def island2Edge(island):
	
	# Vert index edges
	edges = {}
	
	unique_points= {}
	
	for f in island:
		f_uvkey= map(tuple, f.uv)
		
		
		for vIdx, edkey in enumerate(f.edge_keys):
			unique_points[f_uvkey[vIdx]] = f.uv[vIdx]
			
			if f.v[vIdx].index > f.v[vIdx-1].index:
				i1= vIdx-1;	i2= vIdx
			else:		
				i1= vIdx;	i2= vIdx-1
			
			try:	edges[ f_uvkey[i1], f_uvkey[i2] ] *= 0 # sets eny edge with more then 1 user to 0 are not returned.
			except:	edges[ f_uvkey[i1], f_uvkey[i2] ] = (f.uv[i1] - f.uv[i2]).length, 
	
	# If 2 are the same then they will be together, but full [a,b] order is not correct.
	
	# Sort by length
	
		
	length_sorted_edges = [(Vector(key[0]), Vector(key[1]), value) for key, value in edges.iteritems() if value != 0]
	
	try:	length_sorted_edges.sort(key = lambda A: -A[2]) # largest first
	except:	length_sorted_edges.sort(lambda A, B: cmp(B[2], A[2]))
	
	# Its okay to leave the length in there.
	#for e in length_sorted_edges:
	#	e.pop(2)
	
	# return edges and unique points
	return length_sorted_edges, [v.__copy__().resize3D() for v in unique_points.itervalues()]
	
# ========================= NOT WORKING????
# Find if a points inside an edge loop, un-orderd.
# pt is and x/y
# edges are a non ordered loop of edges.
# #offsets are the edge x and y offset.
"""
def pointInEdges(pt, edges):
	#
	x1 = pt[0] 
	y1 = pt[1]
	
	# Point to the left of this line.
	x2 = -100000
	y2 = -10000
	intersectCount = 0
	for ed in edges:
		xi, yi = lineIntersection2D(x1,y1, x2,y2, ed[0][0], ed[0][1], ed[1][0], ed[1][1])
		if xi != None: # Is there an intersection.
			intersectCount+=1
	
	return intersectCount % 2
"""

def pointInIsland(pt, island):
	vec1 = Vector(); vec2 = Vector(); vec3 = Vector()	
	for f in island:
		vec1.x, vec1.y = f.uv[0]
		vec2.x, vec2.y = f.uv[1]
		vec3.x, vec3.y = f.uv[2]

		if pointInTri2D(pt, vec1, vec2, vec3):
			return True
		
		if len(f.v) == 4:
			vec1.x, vec1.y = f.uv[0]
			vec2.x, vec2.y = f.uv[2]
			vec3.x, vec3.y = f.uv[3]			
			if pointInTri2D(pt, vec1, vec2, vec3):
				return True
	return False


# box is (left,bottom, right, top)
def islandIntersectUvIsland(source, target, SourceOffset):
	# Is 1 point in the box, inside the vertLoops
	edgeLoopsSource = source[6] # Pretend this is offset
	edgeLoopsTarget = target[6]
	
	# Edge intersect test	
	for ed in edgeLoopsSource:
		for seg in edgeLoopsTarget:
			i = Geometry.LineIntersect2D(\
			seg[0], seg[1], SourceOffset+ed[0], SourceOffset+ed[1])
			if i:
				return 1 # LINE INTERSECTION
	
	# 1 test for source being totally inside target
	SourceOffset.resize3D()
	for pv in source[7]:
		if pointInIsland(pv+SourceOffset, target[0]):
			return 2 # SOURCE INSIDE TARGET
	
	# 2 test for a part of the target being totaly inside the source.
	for pv in target[7]:
		if pointInIsland(pv-SourceOffset, source[0]):
			return 3 # PART OF TARGET INSIDE SOURCE.

	return 0 # NO INTERSECTION




# Returns the X/y Bounds of a list of vectors.
def testNewVecLs2DRotIsBetter(vecs, mat=-1, bestAreaSoFar = -1):
	
	# UV's will never extend this far.
	minx = miny = BIG_NUM
	maxx = maxy = -BIG_NUM
	
	for i, v in enumerate(vecs):
		
		# Do this allong the way
		if mat != -1:
			v = vecs[i] = v*mat
			x= v.x
			y= v.y
			if x<minx: minx= x
			if y<miny: miny= y
			if x>maxx: maxx= x
			if y>maxy: maxy= y
		
		# Spesific to this algo, bail out if we get bigger then the current area
		if bestAreaSoFar != -1 and (maxx-minx) * (maxy-miny) > bestAreaSoFar:
			return (BIG_NUM, None), None
	w = maxx-minx
	h = maxy-miny
	return (w*h, w,h), vecs # Area, vecs
	
# Takes a list of faces that make up a UV island and rotate
# until they optimally fit inside a square.
ROTMAT_2D_POS_90D = RotationMatrix( 90, 2)
ROTMAT_2D_POS_45D = RotationMatrix( 45, 2)

RotMatStepRotation = []
rot_angle = 22.5 #45.0/2
while rot_angle > 0.1:
	RotMatStepRotation.append([\
	 RotationMatrix( rot_angle, 2),\
	 RotationMatrix( -rot_angle, 2)])
	
	rot_angle = rot_angle/2.0
	

def optiRotateUvIsland(faces):
	global currentArea
	
	# Bestfit Rotation
	def best2dRotation(uvVecs, MAT1, MAT2):
		global currentArea
		
		newAreaPos, newfaceProjectionGroupListPos =\
		testNewVecLs2DRotIsBetter(uvVecs[:], MAT1, currentArea[0])
		
		
		# Why do I use newpos here? May as well give the best area to date for an early bailout
		# some slight speed increase in this.
		# If the new rotation is smaller then the existing, we can 
		# avoid copying a list and overwrite the old, crappy one.
		
		if newAreaPos[0] < currentArea[0]:
			newAreaNeg, newfaceProjectionGroupListNeg =\
			testNewVecLs2DRotIsBetter(uvVecs, MAT2, newAreaPos[0])  # Reuse the old bigger list.
		else:
			newAreaNeg, newfaceProjectionGroupListNeg =\
			testNewVecLs2DRotIsBetter(uvVecs[:], MAT2, currentArea[0])  # Cant reuse, make a copy.
		
		
		# Now from the 3 options we need to discover which to use
		# we have cerrentArea/newAreaPos/newAreaNeg
		bestArea = min(currentArea[0], newAreaPos[0], newAreaNeg[0])
		
		if currentArea[0] == bestArea:
			return uvVecs
		elif newAreaPos[0] == bestArea:
			uvVecs = newfaceProjectionGroupListPos
			currentArea = newAreaPos		
		elif newAreaNeg[0] == bestArea:
			uvVecs = newfaceProjectionGroupListNeg
			currentArea = newAreaNeg
		
		return uvVecs
		
	
	# Serialized UV coords to Vectors
	uvVecs = [uv for f in faces  for uv in f.uv]
	
	# Theres a small enough number of these to hard code it
	# rather then a loop.
	
	# Will not modify anything
	currentArea, dummy =\
	testNewVecLs2DRotIsBetter(uvVecs)
	
	
	# Try a 45d rotation
	newAreaPos, newfaceProjectionGroupListPos = testNewVecLs2DRotIsBetter(uvVecs[:], ROTMAT_2D_POS_45D, currentArea[0])
	
	if newAreaPos[0] < currentArea[0]:
		uvVecs = newfaceProjectionGroupListPos
		currentArea = newAreaPos
	# 45d done
	
	# Testcase different rotations and find the onfe that best fits in a square
	for ROTMAT in RotMatStepRotation:
		uvVecs = best2dRotation(uvVecs, ROTMAT[0], ROTMAT[1])
	
	# Only if you want it, make faces verticle!
	if currentArea[1] > currentArea[2]:
		# Rotate 90d
		# Work directly on the list, no need to return a value.
		testNewVecLs2DRotIsBetter(uvVecs, ROTMAT_2D_POS_90D)
	
	
	# Now write the vectors back to the face UV's
	i = 0 # count the serialized uv/vectors
	for f in faces:
		#f.uv = [uv for uv in uvVecs[i:len(f)+i] ]
		for j, k in enumerate(xrange(i, len(f.v)+i)):
			f.uv[j][:] = uvVecs[k]
		i += len(f.v)


# Takes an island list and tries to find concave, hollow areas to pack smaller islands into.
def mergeUvIslands(islandList):
	global USER_FILL_HOLES
	global USER_FILL_HOLES_QUALITY
	
	
	# Pack islands to bottom LHS
	# Sync with island
	
	#islandTotFaceArea = [] # A list of floats, each island area
	#islandArea = [] # a list of tuples ( area, w,h)
	
	
	decoratedIslandList = []
	
	islandIdx = len(islandList)
	while islandIdx:
		islandIdx-=1
		minx, miny, maxx, maxy = boundsIsland(islandList[islandIdx])
		w, h = maxx-minx, maxy-miny
		
		totFaceArea = 0
		offset= Vector(minx, miny)
		for f in islandList[islandIdx]:
			for uv in f.uv:
				uv -= offset
			
			totFaceArea += f.area
		
		islandBoundsArea = w*h
		efficiency = abs(islandBoundsArea - totFaceArea)
		
		# UV Edge list used for intersections as well as unique points.
		edges, uniqueEdgePoints = island2Edge(islandList[islandIdx])
		
		decoratedIslandList.append([islandList[islandIdx], totFaceArea, efficiency, islandBoundsArea, w,h, edges, uniqueEdgePoints]) 
		
	
	# Sort by island bounding box area, smallest face area first.
	# no.. chance that to most simple edge loop first.
	decoratedIslandListAreaSort =decoratedIslandList[:]
	
	try:	decoratedIslandListAreaSort.sort(key = lambda A: A[3])
	except:	decoratedIslandListAreaSort.sort(lambda A, B: cmp(A[3], B[3]))
	
	
	# sort by efficiency, Least Efficient first.
	decoratedIslandListEfficSort = decoratedIslandList[:]
	# decoratedIslandListEfficSort.sort(lambda A, B: cmp(B[2], A[2]))

	try:	decoratedIslandListEfficSort.sort(key = lambda A: -A[2])
	except:	decoratedIslandListEfficSort.sort(lambda A, B: cmp(B[2], A[2]))

	# ================================================== THESE CAN BE TWEAKED.
	# This is a quality value for the number of tests.
	# from 1 to 4, generic quality value is from 1 to 100
	USER_STEP_QUALITY =   ((USER_FILL_HOLES_QUALITY - 1) / 25.0) + 1
	
	# If 100 will test as long as there is enough free space.
	# this is rarely enough, and testing takes a while, so lower quality speeds this up.
	
	# 1 means they have the same quality 
	USER_FREE_SPACE_TO_TEST_QUALITY = 1 + (((100 - USER_FILL_HOLES_QUALITY)/100.0) *5)
	
	#print 'USER_STEP_QUALITY', USER_STEP_QUALITY
	#print 'USER_FREE_SPACE_TO_TEST_QUALITY', USER_FREE_SPACE_TO_TEST_QUALITY
	
	removedCount = 0
	
	areaIslandIdx = 0
	ctrl = Window.Qual.CTRL
	BREAK= False
	while areaIslandIdx < len(decoratedIslandListAreaSort) and not BREAK:
		sourceIsland = decoratedIslandListAreaSort[areaIslandIdx]
		# Alredy packed?
		if not sourceIsland[0]:
			areaIslandIdx+=1
		else:
			efficIslandIdx = 0
			while efficIslandIdx < len(decoratedIslandListEfficSort) and not BREAK:
				
				if Window.GetKeyQualifiers() & ctrl:
					BREAK= True
					break
				
				# Now we have 2 islands, is the efficience of the islands lowers theres an
				# increasing likely hood that we can fit merge into the bigger UV island.
				# this ensures a tight fit.
				
				# Just use figures we have about user/unused area to see if they might fit.
				
				targetIsland = decoratedIslandListEfficSort[efficIslandIdx]
				
				
				if sourceIsland[0] == targetIsland[0] or\
				not targetIsland[0] or\
				not sourceIsland[0]:
					pass
				else:
					
					# ([island, totFaceArea, efficiency, islandArea, w,h])
					# Waisted space on target is greater then UV bounding island area.
					
					
					# if targetIsland[3] > (sourceIsland[2]) and\ #
					# print USER_FREE_SPACE_TO_TEST_QUALITY, 'ass'
					if targetIsland[2] > (sourceIsland[1] * USER_FREE_SPACE_TO_TEST_QUALITY) and\
					targetIsland[4] > sourceIsland[4] and\
					targetIsland[5] > sourceIsland[5]:
						
						# DEBUG # print '%.10f  %.10f' % (targetIsland[3], sourceIsland[1])
						
						# These enough spare space lets move the box until it fits
						
						# How many times does the source fit into the target x/y
						blockTestXUnit = targetIsland[4]/sourceIsland[4]
						blockTestYUnit = targetIsland[5]/sourceIsland[5]
						
						boxLeft = 0
						
						
						# Distllllance we can move between whilst staying inside the targets bounds.
						testWidth = targetIsland[4] - sourceIsland[4]
						testHeight = targetIsland[5] - sourceIsland[5]
						
						# Increment we move each test. x/y
						xIncrement = (testWidth / (blockTestXUnit * ((USER_STEP_QUALITY/50)+0.1)))
						yIncrement = (testHeight / (blockTestYUnit * ((USER_STEP_QUALITY/50)+0.1)))

						# Make sure were not moving less then a 3rg of our width/height
						if xIncrement<sourceIsland[4]/3:
							xIncrement= sourceIsland[4]
						if yIncrement<sourceIsland[5]/3:
							yIncrement= sourceIsland[5]
						
						
						boxLeft = 0 # Start 1 back so we can jump into the loop.
						boxBottom= 0 #-yIncrement
						
						##testcount= 0
						
						while boxBottom <= testHeight:
							# Should we use this? - not needed for now.
							#if Window.GetKeyQualifiers() & ctrl:
							#	BREAK= True
							#	break
							
							##testcount+=1
							#print 'Testing intersect'
							Intersect = islandIntersectUvIsland(sourceIsland, targetIsland, Vector(boxLeft, boxBottom))
							#print 'Done', Intersect
							if Intersect == 1:  # Line intersect, dont bother with this any more
								pass
							
							if Intersect == 2:  # Source inside target
								'''
								We have an intersection, if we are inside the target 
								then move us 1 whole width accross,
								Its possible this is a bad idea since 2 skinny Angular faces
								could join without 1 whole move, but its a lot more optimal to speed this up
								since we have alredy tested for it.
								
								It gives about 10% speedup with minimal errors.
								'''
								#print 'ass'
								# Move the test allong its width + SMALL_NUM
								#boxLeft += sourceIsland[4] + SMALL_NUM
								boxLeft += sourceIsland[4]
							elif Intersect == 0: # No intersection?? Place it.
								# Progress
								removedCount +=1
								Window.DrawProgressBar(0.0, 'Merged: %i islands, Ctrl to finish early.' % removedCount)
								
								# Move faces into new island and offset
								targetIsland[0].extend(sourceIsland[0])
								offset= Vector(boxLeft, boxBottom)
								
								for f in sourceIsland[0]:
									for uv in f.uv:
										uv+= offset
								
								sourceIsland[0][:] = [] # Empty
								

								# Move edge loop into new and offset.
								# targetIsland[6].extend(sourceIsland[6])
								#while sourceIsland[6]:
								targetIsland[6].extend( [ (\
									 (e[0]+offset, e[1]+offset, e[2])\
								) for e in sourceIsland[6] ] )
								
								sourceIsland[6][:] = [] # Empty
								
								# Sort by edge length, reverse so biggest are first.
								
								try: 	targetIsland[6].sort(key = lambda A: A[2])
								except:	targetIsland[6].sort(lambda B,A: cmp(A[2], B[2] ))
								
								
								targetIsland[7].extend(sourceIsland[7])
								offset= Vector(boxLeft, boxBottom, 0)
								for p in sourceIsland[7]:
									p+= offset
								
								sourceIsland[7][:] = []
								
								
								# Decrement the efficiency
								targetIsland[1]+=sourceIsland[1] # Increment totFaceArea
								targetIsland[2]-=sourceIsland[1] # Decrement efficiency
								# IF we ever used these again, should set to 0, eg
								sourceIsland[2] = 0 # No area if anyone wants to know
								
								break
							
							
							# INCREMENR NEXT LOCATION
							if boxLeft > testWidth:
								boxBottom += yIncrement
								boxLeft = 0.0
							else:
								boxLeft += xIncrement
						##print testcount
				
				efficIslandIdx+=1
		areaIslandIdx+=1
	
	# Remove empty islands
	i = len(islandList)
	while i:
		i-=1
		if not islandList[i]:
			del islandList[i] # Can increment islands removed here.

# Takes groups of faces. assumes face groups are UV groups.
def getUvIslands(faceGroups, me):
	
	# Get seams so we dont cross over seams
	edge_seams = {} # shoudl be a set
	SEAM = Mesh.EdgeFlags.SEAM
	for ed in me.edges:
		if ed.flag & SEAM:
			edge_seams[ed.key] = None # dummy var- use sets!			
	# Done finding seams
	
	
	islandList = []
	
	Window.DrawProgressBar(0.0, 'Splitting %d projection groups into UV islands:' % len(faceGroups))
	#print '\tSplitting %d projection groups into UV islands:' % len(faceGroups),
	# Find grouped faces
	
	faceGroupIdx = len(faceGroups)
	
	while faceGroupIdx:
		faceGroupIdx-=1
		faces = faceGroups[faceGroupIdx]
		
		if not faces:
			continue
		
		# Build edge dict
		edge_users = {}
		
		for i, f in enumerate(faces):
			for ed_key in f.edge_keys:
				if edge_seams.has_key(ed_key): # DELIMIT SEAMS! ;)
					edge_users[ed_key] = [] # so as not to raise an error
				else:
					try:		edge_users[ed_key].append(i)
					except:		edge_users[ed_key] = [i]
		
		# Modes
		# 0 - face not yet touched.
		# 1 - added to island list, and need to search
		# 2 - touched and searched - dont touch again.
		face_modes = [0] * len(faces) # initialize zero - untested.
		
		face_modes[0] = 1 # start the search with face 1
		
		newIsland = []
		
		newIsland.append(faces[0])
		
		
		ok = True
		while ok:
			
			ok = True
			while ok:
				ok= False
				for i in xrange(len(faces)):
					if face_modes[i] == 1: # search
						for ed_key in faces[i].edge_keys:
							for ii in edge_users[ed_key]:
								if i != ii and face_modes[ii] == 0:
									face_modes[ii] = ok = 1 # mark as searched
									newIsland.append(faces[ii])
								
						# mark as searched, dont look again.
						face_modes[i] = 2
			
			islandList.append(newIsland)
			
			ok = False
			for i in xrange(len(faces)):
				if face_modes[i] == 0:
					newIsland = []
					newIsland.append(faces[i])
					
					face_modes[i] = ok = 1
					break
			# if not ok will stop looping
	
	Window.DrawProgressBar(0.1, 'Optimizing Rotation for %i UV Islands' % len(islandList))
	
	for island in islandList:
		optiRotateUvIsland(island)
	
	return islandList
	

def packIslands(islandList):
	if USER_FILL_HOLES:
		Window.DrawProgressBar(0.1, 'Merging Islands (Ctrl: skip merge)...')
		mergeUvIslands(islandList) # Modify in place
		
	
	# Now we have UV islands, we need to pack them.
	
	# Make a synchronised list with the islands
	# so we can box pak the islands.
	packBoxes = []
	
	# Keep a list of X/Y offset so we can save time by writing the 
	# uv's and packed data in one pass.
	islandOffsetList = [] 
	
	islandIdx = 0
	
	while islandIdx < len(islandList):
		minx, miny, maxx, maxy = boundsIsland(islandList[islandIdx])
		
		w, h = maxx-minx, maxy-miny
		
		if USER_ISLAND_MARGIN:
			minx -= USER_ISLAND_MARGIN# *w
			miny -= USER_ISLAND_MARGIN# *h
			maxx += USER_ISLAND_MARGIN# *w
			maxy += USER_ISLAND_MARGIN# *h
		
			# recalc width and height
			w, h = maxx-minx, maxy-miny
		
		if w < 0.00001 or h < 0.00001:
			del islandList[islandIdx]
			islandIdx -=1
			continue
		
		'''Save the offset to be applied later,
		we could apply to the UVs now and allign them to the bottom left hand area
		of the UV coords like the box packer imagines they are
		but, its quicker just to remember their offset and
		apply the packing and offset in 1 pass '''
		islandOffsetList.append((minx, miny))
		
		# Add to boxList. use the island idx for the BOX id.
		packBoxes.append([0, 0, w, h])
		islandIdx+=1
	
	# Now we have a list of boxes to pack that syncs
	# with the islands.
	
	#print '\tPacking UV Islands...'
	Window.DrawProgressBar(0.7, 'Packing %i UV Islands...' % len(packBoxes) )
	
	time1 = sys.time()
	packWidth, packHeight = Geometry.BoxPack2D(packBoxes)
	
	# print 'Box Packing Time:', sys.time() - time1
	
	#if len(pa	ckedLs) != len(islandList):
	#	raise "Error packed boxes differes from original length"
	
	#print '\tWriting Packed Data to faces'
	Window.DrawProgressBar(0.8, 'Writing Packed Data to faces')
	
	# Sort by ID, so there in sync again
	islandIdx = len(islandList)
	# Having these here avoids devide by 0
	if islandIdx:
		
		if USER_STRETCH_ASPECT:
			# Maximize to uv area?? Will write a normalize function.
			xfactor = 1.0 / packWidth
			yfactor = 1.0 / packHeight	
		else:
			# Keep proportions.
			xfactor = yfactor = 1.0 / max(packWidth, packHeight)
	
	while islandIdx:
		islandIdx -=1
		# Write the packed values to the UV's
		
		xoffset = packBoxes[islandIdx][0] - islandOffsetList[islandIdx][0]
		yoffset = packBoxes[islandIdx][1] - islandOffsetList[islandIdx][1]
		
		for f in islandList[islandIdx]: # Offsetting the UV's so they fit in there packed box
			for uv in f.uv:
				uv.x= (uv.x+xoffset) * xfactor
				uv.y= (uv.y+yoffset) * yfactor
			
			

def VectoMat(vec):
	a3 = vec.__copy__().normalize()
	
	up = Vector(0,0,1)
	if abs(a3.dot(up)) == 1.0:
		up = Vector(0,1,0)
	
	a1 = a3.cross(up).normalize()
	a2 = a3.cross(a1)
	return Matrix([a1[0], a1[1], a1[2]], [a2[0], a2[1], a2[2]], [a3[0], a3[1], a3[2]])



class thickface(object):
	__slost__= 'v', 'uv', 'no', 'area', 'edge_keys'
	def __init__(self, face):
		self.v = face.v
		self.uv = face.uv
		self.no = face.no
		self.area = face.area
		self.edge_keys = face.edge_keys

global ob
ob = None
def main():
	global USER_FILL_HOLES
	global USER_FILL_HOLES_QUALITY
	global USER_STRETCH_ASPECT
	global USER_ISLAND_MARGIN
	
	objects= bpy.data.scenes.active.objects
	
	# we can will tag them later.
	obList =  [ob for ob in objects.context if ob.type == 'Mesh']
	
	# Face select object may not be selected.
	ob = objects.active
	if ob and ob.sel == 0 and ob.type == 'Mesh':
		# Add to the list
		obList =[ob]
	del objects
	
	if not obList:
		Draw.PupMenu('error, no selected mesh objects')
		return
	
	# Create the variables.
	USER_PROJECTION_LIMIT = Draw.Create(66)
	USER_ONLY_SELECTED_FACES = Draw.Create(1)
	USER_SHARE_SPACE = Draw.Create(1) # Only for hole filling.
	USER_STRETCH_ASPECT = Draw.Create(1) # Only for hole filling.
	USER_ISLAND_MARGIN = Draw.Create(0.0) # Only for hole filling.
	USER_FILL_HOLES = Draw.Create(0)
	USER_FILL_HOLES_QUALITY = Draw.Create(50) # Only for hole filling.
	USER_VIEW_INIT = Draw.Create(0) # Only for hole filling.
	USER_AREA_WEIGHT = Draw.Create(1) # Only for hole filling.
	
	
	pup_block = [\
	'Projection',\
	('Angle Limit:', USER_PROJECTION_LIMIT, 1, 89, 'lower for more projection groups, higher for less distortion.'),\
	('Selected Faces Only', USER_ONLY_SELECTED_FACES, 'Use only selected faces from all selected meshes.'),\
	('Init from view', USER_VIEW_INIT, 'The first projection will be from the view vector.'),\
	('Area Weight', USER_AREA_WEIGHT, 'Weight projections vector by face area.'),\
	'',\
	'',\
	'',\
	'UV Layout',\
	('Share Tex Space', USER_SHARE_SPACE, 'Objects Share texture space, map all objects into 1 uvmap.'),\
	('Stretch to bounds', USER_STRETCH_ASPECT, 'Stretch the final output to texture bounds.'),\
	('Island Margin:', USER_ISLAND_MARGIN, 0.0, 0.5, 'Margin to reduce bleed from adjacent islands.'),\
	'Fill in empty areas',\
	('Fill Holes', USER_FILL_HOLES, 'Fill in empty areas reduced texture waistage (slow).'),\
	('Fill Quality:', USER_FILL_HOLES_QUALITY, 1, 100, 'Depends on fill holes, how tightly to fill UV holes, (higher is slower)'),\
	]
	
	# Reuse variable
	if len(obList) == 1:
		ob = "Unwrap %i Selected Mesh"
	else:
		ob = "Unwrap %i Selected Meshes"
	
	# HACK, loop until mouse is lifted.
	'''
	while Window.GetMouseButtons() != 0:
		sys.sleep(10)
	'''
	
	if not Draw.PupBlock(ob % len(obList), pup_block):
		return
	del ob
	
	# Convert from being button types
	USER_PROJECTION_LIMIT = USER_PROJECTION_LIMIT.val
	USER_ONLY_SELECTED_FACES = USER_ONLY_SELECTED_FACES.val
	USER_SHARE_SPACE = USER_SHARE_SPACE.val
	USER_STRETCH_ASPECT = USER_STRETCH_ASPECT.val
	USER_ISLAND_MARGIN = USER_ISLAND_MARGIN.val
	USER_FILL_HOLES = USER_FILL_HOLES.val
	USER_FILL_HOLES_QUALITY = USER_FILL_HOLES_QUALITY.val
	USER_VIEW_INIT = USER_VIEW_INIT.val
	USER_AREA_WEIGHT = USER_AREA_WEIGHT.val
	
	USER_PROJECTION_LIMIT_CONVERTED = cos(USER_PROJECTION_LIMIT * DEG_TO_RAD)
	USER_PROJECTION_LIMIT_HALF_CONVERTED = cos((USER_PROJECTION_LIMIT/2) * DEG_TO_RAD)
	
	
	# Toggle Edit mode
	is_editmode = Window.EditMode()
	if is_editmode:
		Window.EditMode(0)
	# Assume face select mode! an annoying hack to toggle face select mode because Mesh dosent like faceSelectMode.
	
	if USER_SHARE_SPACE:
		# Sort by data name so we get consistant results
		try:	obList.sort(key = lambda ob: ob.getData(name_only=1))
		except:	obList.sort(lambda ob1, ob2: cmp( ob1.getData(name_only=1), ob2.getData(name_only=1) ))
		
		collected_islandList= []
	
	Window.WaitCursor(1)
	
	time1 = sys.time()
	
	# Tag as False se we dont operate on teh same mesh twice.
	bpy.data.meshes.tag = False 
	
	for ob in obList:
		me = ob.getData(mesh=1)
		
		if me.tag or me.lib:
			continue
		
		# Tag as used
		me.tag = True
		
		if not me.faceUV: # Mesh has no UV Coords, dont bother.
			me.faceUV= True
		
		if USER_ONLY_SELECTED_FACES:
			meshFaces = [thickface(f) for f in me.faces if f.sel]
		else:
			meshFaces = map(thickface, me.faces)
		
		if not meshFaces:
			continue
		
		Window.DrawProgressBar(0.1, 'SmartProj UV Unwrapper, mapping "%s", %i faces.' % (me.name, len(meshFaces)))
		
		# =======
		# Generate a projection list from face normals, this is ment to be smart :)
		
		# make a list of face props that are in sync with meshFaces		
		# Make a Face List that is sorted by area.
		# meshFaces = []
		
		# meshFaces.sort( lambda a, b: cmp(b.area , a.area) ) # Biggest first.
		try:	meshFaces.sort( key = lambda a: -a.area ) 
		except:	meshFaces.sort( lambda a, b: cmp(b.area , a.area) )
			
		# remove all zero area faces
		while meshFaces and meshFaces[-1].area <= SMALL_NUM:
			# Set their UV's to 0,0
			for uv in meshFaces[-1].uv:
				uv.zero()
			meshFaces.pop()
		
		# Smallest first is slightly more efficient, but if the user cancels early then its better we work on the larger data.
		
		# Generate Projection Vecs
		# 0d is   1.0
		# 180 IS -0.59846
		
		
		# Initialize projectVecs
		if USER_VIEW_INIT:
			# Generate Projection
			projectVecs = [Vector(Window.GetViewVector()) * ob.matrixWorld.copy().invert().rotationPart()] # We add to this allong the way
		else:
			projectVecs = []
		
		newProjectVec = meshFaces[0].no
		newProjectMeshFaces = []	# Popping stuffs it up.
		
		
		# Predent that the most unique angke is ages away to start the loop off
		mostUniqueAngle = -1.0
		
		# This is popped
		tempMeshFaces = meshFaces[:]
		
		
		
		# This while only gathers projection vecs, faces are assigned later on.
		while 1:
			# If theres none there then start with the largest face
			
			# add all the faces that are close.
			for fIdx in xrange(len(tempMeshFaces)-1, -1, -1):
				# Use half the angle limit so we dont overweight faces towards this
				# normal and hog all the faces.
				if newProjectVec.dot(tempMeshFaces[fIdx].no) > USER_PROJECTION_LIMIT_HALF_CONVERTED:
					newProjectMeshFaces.append(tempMeshFaces.pop(fIdx))
			
			# Add the average of all these faces normals as a projectionVec
			averageVec = Vector(0,0,0)
			if USER_AREA_WEIGHT:
				for fprop in newProjectMeshFaces:
					averageVec += (fprop.no * fprop.area)
			else:
				for fprop in newProjectMeshFaces:
					averageVec += fprop.no
					
			if averageVec.x != 0 or averageVec.y != 0 or averageVec.z != 0: # Avoid NAN
				projectVecs.append(averageVec.normalize())
			
			
			# Get the next vec!
			# Pick the face thats most different to all existing angles :)
			mostUniqueAngle = 1.0 # 1.0 is 0d. no difference.
			mostUniqueIndex = 0 # dummy
			
			for fIdx in xrange(len(tempMeshFaces)-1, -1, -1):
				angleDifference = -1.0 # 180d difference.
				
				# Get the closest vec angle we are to.
				for p in projectVecs:
					temp_angle_diff= p.dot(tempMeshFaces[fIdx].no)
					
					if angleDifference < temp_angle_diff:
						angleDifference= temp_angle_diff
				
				if angleDifference < mostUniqueAngle:
					# We have a new most different angle
					mostUniqueIndex = fIdx
					mostUniqueAngle = angleDifference
			
			if mostUniqueAngle < USER_PROJECTION_LIMIT_CONVERTED:
				#print 'adding', mostUniqueAngle, USER_PROJECTION_LIMIT, len(newProjectMeshFaces)
				# Now weight the vector to all its faces, will give a more direct projection
				# if the face its self was not representive of the normal from surrounding faces.
				
				newProjectVec = tempMeshFaces[mostUniqueIndex].no
				newProjectMeshFaces = [tempMeshFaces.pop(mostUniqueIndex)]
				
			
			else:
				if len(projectVecs) >= 1: # Must have at least 2 projections
					break
		
		
		# If there are only zero area faces then its possible
		# there are no projectionVecs
		if not len(projectVecs):
			Draw.PupMenu('error, no projection vecs where generated, 0 area faces can cause this.')
			return
		
		faceProjectionGroupList =[[] for i in xrange(len(projectVecs)) ]
		
		# MAP and Arrange # We know there are 3 or 4 faces here 
		
		for fIdx in xrange(len(meshFaces)-1, -1, -1):
			fvec = meshFaces[fIdx].no
			i = len(projectVecs)
			
			# Initialize first
			bestAng = fvec.dot(projectVecs[0])
			bestAngIdx = 0
			
			# Cycle through the remaining, first alredy done
			while i-1:
				i-=1
				
				newAng = fvec.dot(projectVecs[i])
				if newAng > bestAng: # Reverse logic for dotvecs
					bestAng = newAng
					bestAngIdx = i
			
			# Store the area for later use.
			faceProjectionGroupList[bestAngIdx].append(meshFaces[fIdx])
		
		# Cull faceProjectionGroupList,
		
		
		# Now faceProjectionGroupList is full of faces that face match the project Vecs list
		for i in xrange(len(projectVecs)):
			# Account for projectVecs having no faces.
			if not faceProjectionGroupList[i]:
				continue
			
			# Make a projection matrix from a unit length vector.
			MatProj = VectoMat(projectVecs[i])
			
			# Get the faces UV's from the projected vertex.
			for f in faceProjectionGroupList[i]:
				f_uv = f.uv
				for j, v in enumerate(f.v):
					f_uv[j][:] = (MatProj * v.co)[:2]
		
		
		if USER_SHARE_SPACE:
			# Should we collect and pack later?
			islandList = getUvIslands(faceProjectionGroupList, me)
			collected_islandList.extend(islandList)
			
		else:
			# Should we pack the islands for this 1 object?
			islandList = getUvIslands(faceProjectionGroupList, me)
			packIslands(islandList)
		
		
		# update the mesh here if we need to.
	
	# We want to pack all in 1 go, so pack now
	if USER_SHARE_SPACE:
		Window.DrawProgressBar(0.9, "Box Packing for all objects...")
		packIslands(collected_islandList)
	
	print "Smart Projection time: %.2f" % (sys.time() - time1)
	# Window.DrawProgressBar(0.9, "Smart Projections done, time: %.2f sec." % (sys.time() - time1))
	
	if is_editmode:
		Window.EditMode(1)
	
	Window.DrawProgressBar(1.0, "")
	Window.WaitCursor(0)
	Window.RedrawAll()

if __name__ == '__main__':
	main()
