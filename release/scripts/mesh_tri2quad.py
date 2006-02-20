#!BPY

"""
Name: 'Triangles to Quads'
Blender: 240
Group: 'Mesh'
Tooltip: 'Triangles to Quads for all selected mesh objects.'
"""

__author__ = "Campbell Barton AKA Ideasman"
__url__ = ["http://members.iinet.net.au/~cpbarton/ideasman/", "blender", "elysiun"]

__bpydoc__ = """\
This script joins any triangles into quads for all selected mesh objects.

Usage:

Select the mesh(es) and run this script. Mesh data will be edited in place.
so make a backup copy first if your not sure of the results

The limit value allows you to choose how pedantic the algorithum is when detecting errors between 2 triangles.
Over 50 could result in quads that are not correct.

The joining of quads takes into account UV mapping, UV Images and Vertex colours
and will not join faces that have mis-matching data.
"""


from Blender import Scene, Object, Mathutils, Draw, Window, sys

TRI_LIST = (0,1,2)

vecAngle = Mathutils.AngleBetweenVecs
TriangleNormal = Mathutils.TriangleNormal

#=============================================================================#
# All measurement algorithums for face compatibility when joining into quads					 #
# every function returns a value between 0.0 and 1.0																	 #
#=============================================================================#
# total diff is 1.0, no diff is 0.0
# measure accross 2 possible triangles in the imagined quad.
def isfaceNoDiff(imagQuag):
	# Divide the quad one way and measure normals
	noA1 = TriangleNormal(imagQuag[0].co, imagQuag[1].co, imagQuag[2].co)
	noA2 = TriangleNormal(imagQuag[0].co, imagQuag[2].co, imagQuag[3].co)
	
	if noA1 == noA2:
		normalADiff = 0.0
	else:
		try:
			normalADiff = vecAngle(noA1, noA2)
		except:
			#print noA1, noA2
			normalADiff = 179
		
	#print normalADiff, noA1, noA2
	
	# Alternade division of the quad
	noB1 = TriangleNormal(imagQuag[1].co, imagQuag[2].co, imagQuag[3].co)
	noB2 = TriangleNormal(imagQuag[3].co, imagQuag[0].co, imagQuag[1].co)	
	if noB1 == noB2:
		normalBDiff = 0.0
	else:
		try:
			normalBDiff = vecAngle(noB1, noB2)
		except:
			# print noB1, noB2
			normalBDiff = 179
	
	# Should never be NAN anymore.
	'''
	if normalBDiff != normalBDiff or normalADiff != normalADiff:
		raise "NAN"
	'''
	# The greatest possible difference is 180 for each
	return (normalADiff/360) + (normalBDiff/360)


# 4 90d angles == 0, each corner diff from 90 is added together.
# 360 is total possible difference,
def isfaceCoLin(imagQuag): 
	
	edgeVec1 = imagQuag[0].co - imagQuag[1].co
	edgeVec2 = imagQuag[1].co - imagQuag[2].co
	edgeVec3 = imagQuag[2].co - imagQuag[3].co
	edgeVec4 = imagQuag[3].co - imagQuag[0].co
	
	# Work out how different from 90 each edge is.
	diff = 0
	try:
		diff = abs(vecAngle(edgeVec1, edgeVec2) - 90)
		diff = max(diff, abs(vecAngle(edgeVec2, edgeVec3) - 90))
		diff = max(diff, abs(vecAngle(edgeVec3, edgeVec4) - 90))
		diff = max(diff, abs(vecAngle(edgeVec4, edgeVec1) - 90))
	except:
		return 1.0
	
	# Avoid devide by 0
	if not diff:
		return 0.0
	
	return min(diff/180, 1.0)


# Meause the areas of the 2 possible ways of subdividing the imagined quad.
# if 1 is very different then the quad is concave.
# We should probably throw out any pairs that are at all concave,
# since even a slightly concacve paor will definetly be co linear.
# though even virging on this can be a recipe for a bad join.
def isfaceConcave(imagQuag):
	# Add the 2 areas the deviding one way
	areaA =\
	Mathutils.TriangleArea(imagQuag[0].co, imagQuag[1].co, imagQuag[2].co) +\
	Mathutils.TriangleArea(imagQuag[0].co, imagQuag[2].co, imagQuag[3].co)
	
	# Add the tri's triangulated the alternate way.
	areaB =\
	Mathutils.TriangleArea(imagQuag[1].co, imagQuag[2].co, imagQuag[3].co) +\
	Mathutils.TriangleArea(imagQuag[3].co, imagQuag[0].co, imagQuag[1].co)
	
	# Make a ratio of difference so they are between 1 and 0
	# Need to invert the value so 1.0 is 0
	minarea = min(areaA, areaB)
	maxarea = max(areaA, areaB)
	
	# Aviod devide by 0
	if maxarea == 0.0:
		return 1
	else:
		return 1 - (minarea / maxarea)



# This returns a list of verts, to test
# dosent modify the actual faces.
def meshJoinFacesTest(f1, f2, V1FREE, V2FREE): 
	# pretend face
	dummyFace = f1.v[:]
	
	# We know the 2 free verts. insert the f2 free vert in 
	if V1FREE is 0:
		dummyFace.insert(2, f2.v[V2FREE])
	elif V1FREE is 1:
		dummyFace.append(f2.v[V2FREE])
	elif V1FREE is 2:
		dummyFace.insert(1, f2.v[V2FREE])
	
	return dummyFace


# Measure how good a quad the 2 tris will make,
def measureFacePair(f1, f2, f1free, f2free, limit):
	# Make a imaginary quad. from 2 tris into 4 verts
	imagFace = meshJoinFacesTest(f1, f2, f1free, f2free)
	if imagFace is False:
		return False
	
	measure = 0 # start at 0, a lower value is better.
	
	# Do a series of tests,
	# each value will add to the measure value
	# and be between 0 and 1.0
	
	measure+= isfaceNoDiff(imagFace)/3
	if measure > limit: return False
	measure+= isfaceCoLin(imagFace)/3
	if measure > limit: return False
	measure+= isfaceConcave(imagFace)/3
	if measure > limit: return False
	
	'''
	# For debugging.
	
	a1= isfaceNoDiff(imagFace)
	a2= isfaceCoLin(imagFace)
	a3= isfaceConcave(imagFace)
	
	#print 'a1 NODIFF  %f' % a1
	#print 'a2 COLIN   %f' % a2
	#print 'a3 CONCAVE %f' % a3
	
	measure = (a1+a2+a3)/3
	if measure > limit: return False
	'''
	
	return [f1,f2, measure, f1free, f2free]


# We know the faces are good to join, simply execute the join
# by making f1 into a quad and f2 inde an edge (2 vert face.)
INSERT_LOOKUP = (2,3,1)
OTHER_LOOKUP = ((1,2),(0,2),(0,1))
def meshJoinFaces(f1, f2, V1FREE, V2FREE, mesh):
	
	# Only used if we have edges.
	# DEBUG
	edgeVert1, edgeVert2 = OTHER_LOOKUP[V1FREE]
	edgeVert1, edgeVert2 = f1[edgeVert1], f1[edgeVert2]
	
	
	fverts = f1.v[:]
	if mesh.hasFaceUV():
		fuvs = f1.uv[:]
	if f1.col:
		fcols = f1.col[:]
		
	
	# We know the 2 free verts. insert the f2 free vert in 
	# Work out which vert to insert
	i = INSERT_LOOKUP[V1FREE]
	
	# Insert the vert in the desired location.
	fverts.insert(i, f2.v[V2FREE])
	if mesh.hasFaceUV():
		fuvs.insert(i, f2.uv[V2FREE])
	if f1.col:
		fcols.insert(i, f2.col[V2FREE])		
	
	# Assign the data to the faces.
	f1.v = fverts
	if mesh.hasFaceUV():
		f1.uv = fuvs
	if f1.col:
		f1.col = fcols
	
	# Make an edge from the 2nd vert.
	# removing anything other then the free vert will
	# remove the edge from accress the new quad
	f2.v.pop(not V2FREE)
	
	mesh.removeEdge(edgeVert1, edgeVert2)
	#return f2



def compare2(v1, v2, limit):
	if v1[0] + limit > v2[0] and v1[0] - limit < v2[0] and\
	v1[1] + limit > v2[1] and v1[1] - limit < v2[1]:
				return True
	return False


def compare3(v1, v2, limit):
	if v1[0] + limit > v2[0] and v1[0] - limit < v2[0] and\
	v1[1] + limit > v2[1] and v1[1] - limit < v2[1] and\
	v1[2] + limit > v2[2] and v1[2] - limit < v2[2]:
		return True
	return False


UV_LIMIT = 0.005 # 0.0 to 1.0, can be greater then these bounds tho
def compareFaceUV(f1, f2):
	if f1.image == None and f1.image == None:
		# No Image, ignore uv's
		return True
	elif f1.image != f2.image:
		# Images differ, dont join faces.
		return False
	
	# We know 2 of these will match.
	for v1i in TRI_LIST:
		for v2i in TRI_LIST:
			if f1[v1i] is f2[v2i]:
				# We have a vertex index match.
				# now match the UV's
				if not compare2(f1.uv[v1i], f2.uv[v2i], UV_LIMIT):
					# UV's are different
					return False
	
	return True


COL_LIMIT = 3 # 0 to 255
def compareFaceCol(f1, f2):
	# We know 2 of these will match.
	for v1i in TRI_LIST:
		for v2i in TRI_LIST:
			if f1[v1i] is f2[v2i]:
				# We have a vertex index match.
				# now match the UV's
				if not compare3(f1.col[v1i], f2.col[v2i], COL_LIMIT):
					# UV's are different
					return False
					
	return True	


def tri2quad(mesh, limit, selectedFacesOnly, respectUVs, respectVCols):
	print '\nStarting tri2quad for mesh: %s' % mesh.name
	print '\t...finding pairs'
	time1 = sys.time()	# Time the function
	pairCount = 0
	
	# each item in this list will be a list
	# [face1, face2, measureFacePairValue]
	facePairLs = [] 
	
	if selectedFacesOnly:
		faceList = [f for f in mesh.faces if f.sel if len(f) is 3 if not f.hide]
	else:
		faceList = [f for f in mesh.faces if len(f) == 3]
	
	# Set if applicable for this mesh.
	has_face_uv= has_vert_col = False
	if respectUVs:
		has_face_uv = mesh.hasFaceUV()
	if respectVCols:
		has_vert_col = mesh.hasVertexColours() 
	
	
	# Build a list of edges and tris that use those edges.
	# This is so its faster to find tri pairs.
	edgeFaceUsers = {}
	for f in faceList:
		
		edkey1a= edkey3b= f.v[0].index
		edkey1b= edkey2a= f.v[1].index
		edkey2b= edkey3a= f.v[2].index
		
		if edkey1a > edkey1b:  edkey1a, edkey1b = edkey1b, edkey1a
		if edkey2a > edkey2b:  edkey2a, edkey2b = edkey2b, edkey2a
		if edkey3a > edkey3b:  edkey3a, edkey3b = edkey3b, edkey3a
		
		# The second int in the tuple is the free vert, its easier to store then to work it out again.
		try: edgeFaceUsers[edkey1a, edkey1b].append((f, 2))
		except:	edgeFaceUsers[edkey1a, edkey1b] = [(f, 2)]
		
		try: edgeFaceUsers[edkey2a, edkey2b].append((f, 0))
		except:	edgeFaceUsers[edkey2a, edkey2b] = [(f, 0)]
		
		try: edgeFaceUsers[edkey3a, edkey3b].append((f, 1))
		except:	edgeFaceUsers[edkey3a, edkey3b] = [(f, 1)]
	
	
	edgeDoneCount = 0
	for faceListEdgeShared in edgeFaceUsers.itervalues():
		if len(faceListEdgeShared) == 2:
			f1, f1free = faceListEdgeShared[0]
			f2, f2free = faceListEdgeShared[1]
			
			if f1.mat != f2.mat:
				pass # faces have different materials.
			elif has_face_uv and (not compareFaceUV(f1, f2)):
				pass # UV's are there but dont match.
			elif has_vert_col and not compareFaceCol(f1, f2):
				pass # Colours are there but dont match.
			else:
				# We can now store the qpair and measure
				# there eligability for becoming 1 quad.
				pair = measureFacePair(f1, f2, f1free, f2free, limit)
				#if pair is not False and pair[2] < limit: # Some terraible error
				if pair is not False: # False means its above the limit.
					facePairLs.append(pair)
					pairCount += 1
			
			edgeDoneCount += 1
			if not edgeDoneCount % 20:
				p = float(edgeDoneCount) / len(edgeFaceUsers)
				Window.DrawProgressBar(p*0.5, 'Found pairs: %i' % pairCount)
	
	
	# Sort, best options first :)
	facePairLs.sort(lambda a,b: cmp(a[2], b[2]))
	draws = 0
	print '\t...joining pairs'
	joinCount = 0
	len_facePairLs = len(facePairLs)
	
	#faces_to_remove = []
	
	for pIdx, pair in enumerate(facePairLs):
		# We know the last item is the best option, and no other face pairs will get in the way.
		# now join the faces.
		
		# If any of the faces have alredy been used then they will not have a lengh of 3 verts
		if len(pair[0]) is 3 and len(pair[1]) is 3:
			joinCount +=1
			# print 'joining faces', joinCount, 'Limit:', facePairLs[-1][2]
			#faces_to_remove.append( meshJoinFaces(pair[0], pair[1], mesh) )
			meshJoinFaces(pair[0], pair[1], pair[3], pair[4], mesh)
			
			if not pIdx % 20:
				p = (0.5 + ((float((len_facePairLs - (len_facePairLs - pIdx))))/len_facePairLs*0.5)) * 0.99
				Window.DrawProgressBar(p, 'Joining Face count: %i of %i' % (joinCount, len_facePairLs))
				draws +=1
				
	# print 'Draws', draws
	
	# Remove faces, due to a bug in ZR's new BF-Blender CVS
	
	fIdx = len(mesh.faces)
	while fIdx:
		fIdx -=1
		if len(mesh.faces[fIdx]) < 3:
			mesh.faces.pop(fIdx)	
	
	if joinCount:
		print "tri2quad time for %s: %s	joined %s tri's into quads" % (mesh.name, sys.time()-time1, joinCount)
		
		#mesh.update(0, (mesh.edges != []), 0)
		mesh.update(0, 0, 0)
		
	else:
		print "tri2quad nothing done %s: %s	joined none" % (mesh.name, sys.time()-time1)
		


#====================================#
# Sanity checks                      #
#====================================#
def error(str):
	Draw.PupMenu('ERROR%t|'+str)

def main():
	scn= Scene.GetCurrent()
	
	#selection = Object.Get()
	selection = Object.GetSelected()
	actob = scn.getActiveObject()
	if not actob.sel:
		selection.append(actob)
	
	if len(selection) is 0:
		error('No object selected')
		return

	#	GET UNIQUE MESHES.
	meshDict = {}
	# Mesh names
	for ob in selection:
		if ob.getType() == 'Mesh':
			meshDict[ob.getData(1)] = ob # dont do doubles.
	
	# Create the variables.
	selectedFacesOnly = Draw.Create(1)
	respectUVs = Draw.Create(1)
	respectVCols = Draw.Create(1)
	limit = Draw.Create(25)
	
	
	pup_block = [\
	('Selected Faces Only', selectedFacesOnly, 'Use only selected faces from all selected meshes.'),\
	('UV Delimit', respectUVs, 'Only join pairs that have matching UVs on the joining edge.'),\
	('VCol Delimit', respectVCols, 'Only join pairs that have matching Vert Colors on the joining edge.'),\
	('Limit: ', limit, 1, 100, 'A higher value will join more tris to quads, even if the quads are not perfect.'),\
	]
	selectedFacesOnly = selectedFacesOnly.val
	respectUVs = respectUVs.val
	respectVCols = respectVCols.val
	limit = limit.val
	
	if not Draw.PupBlock('Tri2Quad for %i mesh object(s)' % len(meshDict), pup_block):
		return	
	
	# We now know we can execute
	is_editmode = Window.EditMode()
	if is_editmode: Window.EditMode(0)
	
	limit = limit/100.0 # Make between 1 and 0
	
	for ob in meshDict.itervalues():
		mesh = ob.getData()
		tri2quad(mesh, limit, selectedFacesOnly, respectUVs, respectVCols)
	if is_editmode: Window.EditMode(1)

# Dont run when importing
if __name__ == '__main__':
	Window.DrawProgressBar(0.0, 'Triangles to Quads 1.1 ')
	main()
	Window.DrawProgressBar(1.0, '')