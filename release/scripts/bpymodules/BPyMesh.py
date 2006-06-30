import Blender
from BPyMesh_redux import redux # seperated because of its size.

	
def meshWeight2Dict(me):
	''' Takes a mesh and return its group names and a list of dicts, one dict per vertex.
	using the group as a key and a float value for the weight.
	These 2 lists can be modified and then used with dict2MeshWeight to apply the changes.
	'''
	
	vWeightDict= [dict() for i in xrange(len(me.verts))] # Sync with vertlist.
	
	# Clear the vert group.
	groupNames= me.getVertGroupNames()
	
	for group in groupNames:
		for index, weight in me.getVertsFromGroup(group, 1): # (i,w)  tuples.
			vWeightDict[index][group]= weight
	
	# removed this because me may be copying teh vertex groups.
	#for group in groupNames:
	#	me.removeVertGroup(group)
	
	return groupNames, vWeightDict


def dict2MeshWeight(me, groupNames, vWeightDict):
	''' Takes a list of groups and a list of vertex Weight dicts as created by meshWeight2Dict
	and applys it to the mesh.'''
	
	if len(vWeightDict) != len(me.verts):
		raise 'Error, Lists Differ in size, do not modify your mesh.verts before updating the weights'
	
	# Clear the vert group.
	currentGroupNames= me.getVertGroupNames()
	for group in currentGroupNames:
		if group not in groupNames:
			me.removeVertGroup(group) # messes up the active group.
		else:
			me.removeVertsFromGroup(group)
	
	# Add clean unused vert groupNames back
	currentGroupNames= me.getVertGroupNames()
	for group in groupNames:
		if group not in currentGroupNames:
			me.addVertGroup(group)
	
	add_ = Blender.Mesh.AssignModes.ADD
	
	vertList= [None]
	for i, v in enumerate(me.verts):
		vertList[0]= i
		for group, weight in vWeightDict[i].iteritems():
			try:
				me.assignVertsToGroup(group, vertList, min(1, max(0, weight)), add_)
			except:
				pass # vert group is not used anymore.
	
	me.update()

def dictWeightMerge(dict_weights):
	'''
	Takes dict weight list and merges into 1 weight dict item and returns it
	'''
	
	if not dict_weights:
		return {}
	
	keys= []
	for weight in dict_weights:
		keys.extend([ (k, 0.0) for k in weight.iterkeys() ])
	
	new_wdict = dict(keys)
	
	len_dict_weights= len(dict_weights)
	
	for weight in dict_weights:
		for group, value in weight.iteritems():
			new_wdict[group] += value/len_dict_weights
	
	return new_wdict


FLIPNAMES=[\
('Left','Right'),\
('_L','_R'),\
('-L','-R'),\
('.L','.R'),\
]

def dictWeightFlipGroups(dict_weight, groupNames, createNewGroups):
	'''
	Returns a weight with flip names
	dict_weight - 1 vert weight.
	groupNames - because we may need to add new group names.
	dict_weight - Weather to make new groups where needed.
	'''
	
	def flipName(name):
		for n1,n2 in FLIPNAMES:
			for nA, nB in ( (n1,n2), (n1.lower(),n2.lower()), (n1.upper(),n2.upper()) ):
				if createNewGroups:
					newName= name.replace(nA,nB)
					if newName!=name:
						if newName not in groupNames:
							groupNames.append(newName)
						return newName
					
					newName= name.replace(nB,nA)
					if newName!=name:
						if newName not in groupNames:
							groupNames.append(newName)
						return newName
				
				else:
					newName= name.replace(nA,nB)
					if newName!=name and newName in groupNames:
						return newName
					
					newName= name.replace(nB,nA)
					if newName!=name and newName in groupNames:
						return newName
		
		return name
		
	if not dict_weight:
		return dict_weight, groupNames
	
	
	new_wdict = {}
	for group, weight in dict_weight.iteritems():
		flipname= flipName(group)
		new_wdict[flipname]= weight
	
	return new_wdict, groupNames
	

def getMeshFromObject(ob, container_mesh=None, apply_modifiers=True, vgroups=True, scn=None):
	'''
	ob - the object that you want to get the mesh from
	container_mesh - a Blender.Mesh type mesh that is reused to avoid a new datablock per call to getMeshFromObject
	apply_modifiers - if enabled, subsurf bones etc. will be applied to the returned mesh. disable to get a copy of the mesh.
	vgroup - For mesh objects only, apply the vgroup to the the copied mesh. (slower)
	scn - Scene type. avoids getting the current scene each time getMeshFromObject is called.
	
	Returns Mesh or None
	'''
	
	if not scn:
		scn= Blender.Scene.GetCurrent()
	if not container_mesh:
		mesh = Blender.Mesh.New()	
	else:
		mesh= container_mesh
		mesh.verts= None
	
	
	type = ob.getType()
	dataname = ob.getData(1)
	tempob= None
	if apply_modifiers or type != 'Mesh':
		try:
			mesh.getFromObject(ob.name)
		except:
			return None
	
	else:
		'''
		Dont apply modifiers, copy the mesh. 
		So we can transform the data. its easiest just to get a copy of the mesh. 
		'''
		tempob= Blender.Object.New('Mesh')
		tempob.shareFrom(ob)
		scn.link(tempob)
		mesh.getFromObject(tempob.name)
		scn.unlink(tempob)
	
	if type == 'Mesh':
		if vgroups:
			if tempob==None:
				tempob= Blender.Object.New('Mesh')
			tempob.link(mesh)
			try:
				# Copy the influences if possible.
				groupNames, vWeightDict= meshWeight2Dict(tempMe)
				dict2MeshWeight(mesh, groupNames, vWeightDict)
			except:
				# if the modifier changes the vert count then it messes it up for us.
				pass
	
	return mesh


def faceRayIntersect(f, orig, dir):
	'''
	Returns face, side
	Side is the side of a quad we intersect.
		side 0 == 0,1,2
		side 1 == 0,2,3
	'''
	f_v= f.v
	isect= Blender.Mathutils.Intersect(f_v[0].co, f_v[1].co, f_v[2].co, dir, orig, 1) # 1==clip
	
	if isect:
		return isect, 0
	
	if len(f_v)==4:
		isect= Blender.Mathutils.Intersect(f_v[0].co, f_v[2].co, f_v[3].co, dir, orig, 1) # 1==clip
		if isect:
			return isect, 1
	return False, 0


def pickMeshRayFace(me, orig, dir):
	best_dist= 1000000
	best_isect= best_side= best_face= None
	for f in me.faces:
		isect, side= faceRayIntersect(f, orig, dir)
		if isect:
			dist= (isect-orig).length
			if dist<best_dist:
				best_dist= dist
				best_face= f
				best_side= side
				best_isect= isect
	f= best_face
	isect= best_isect
	side= best_side
	
	if f==None:
		return None, None, None, None, None
	
	f_v= [v.co for v in f.v]
	if side==1: # we can leave side 0 without changes.
		f_v = f_v[0], f_v[2], f_v[3]
	
	l0= (f_v[0]-isect).length
	l1= (f_v[1]-isect).length
	l2= (f_v[2]-isect).length
	
	w0 = (l1+l2)
	w1 = (l0+l2)
	w2 = (l1+l2)
	
	totw= w0 + w1 + w2
	w0=w0/totw
	w1=w1/totw
	w2=w2/totw
	
	return f, side, w0, w1, w2



def pickMeshGroupWeight(me, act_group, orig, dir):
	f, side, w0, w1, w2= pickMeshRayFace(me, orig, dir)
	
	f_v= f.v
	if side==0:
		f_vi= (f_v[0].index, f_v[1].index, f_v[2].index)
	else:
		f_vi= (f_v[0].index, f_v[2].index, f_v[3].index)
	
	vws= [0.0,0.0,0.0]
	for i in xrange(3):
		try:		vws[i]= me.getVertsFromGroup(act_group, 1, [f_vi[i],])[0][1]
		except:	pass
	
	return w0*vws[0] + w1*vws[1]  + w2*vws[2]

def pickMeshGroupVCol(me, orig, dir):
	Vector= Blender.Mathutils.Vector
	f, side, w0, w1, w2= pickMeshRayFace(me, orig, dir)
	
	def col2vec(c):
		return Vector(c.r, c.g, c.b)
	
	if side==0:
		idxs= 0,1,2
	else:
		idxs= 0,2,3
	f_c= f.col
	f_colvecs= [col2vec(f_c[i]) for i in idxs]
	return f_colvecs[0]*w0 +  f_colvecs[1]*w1 + f_colvecs[2]*w2

# reuse me more.
def sorted_edge_indicies(ed):
	i1= ed.v1.index
	i2= ed.v2.index
	if i1>i2:
		i1,i2= i2,i1
	return i1, i2

def edge_face_users(me):
	''' 
	Takes a mesh and returns a list aligned with the meshes edges.
	Each item is a list of the faces that use the edge
	would be the equiv for having ed.face_users as a property
	'''
	
	face_edges_dict= dict([(sorted_edge_indicies(ed), (ed.index, [])) for ed in me.edges])
	for f in me.faces:
		fvi= [v.index for v in f.v]# face vert idx's
		for i in xrange(len(f)):
			i1= fvi[i]
			i2= fvi[i-1]
			
			if i1>i2:
				i1,i2= i2,i1
			
			face_edges_dict[i1,i2][1].append(f)
	
	face_edges= [None] * len(me.edges)
	for ed_index, ed_faces in face_edges_dict.itervalues():
		face_edges[ed_index]= ed_faces
	
	return face_edges
		
		
def face_edges(me):
	'''
	Returns a list alligned to the meshes faces.
	each item is a list of lists: that is 
	face_edges -> face indicies
	face_edges[i] -> list referencs local faces v indicies 1,2,3 &| 4
	face_edges[i][j] -> list of faces that this edge uses.
	crap this is tricky to explain :/
	'''
	face_edges= [ [None] * len(f) for f in me.faces ]
	
	face_edges_dict= dict([(sorted_edge_indicies(ed), []) for ed in me.edges])
	for fidx, f in enumerate(me.faces):
		fvi= [v.index for v in f.v]# face vert idx's
		for i in xrange(len(f)):
			i1= fvi[i]
			i2= fvi[i-1]
			
			if i1>i2:
				i1,i2= i2,i1
			
			edge_face_users= face_edges_dict[i1,i2]
			edge_face_users.append(f)
			
			face_edges[fidx][i]= edge_face_users
			
	return face_edges
	

def facesPlanerIslands(me):
	DotVecs= Blender.Mathutils.DotVecs
	
	def roundvec(v):
		return round(v[0], 4), round(v[1], 4), round(v[2], 4)
	
	face_props= [(cent, no, roundvec(no), DotVecs(cent, no)) for f in me.faces    for no, cent in ((f.no, f.cent),)]
	
	face_edge_users= face_edges(me)
	islands= []
	
	used_faces= [0] * len(me.faces)
	while True:
		new_island= False
		for i, used_val in enumerate(used_faces):
			if used_val==0:
				island= [i]
				new_island= True
				used_faces[i]= 1
				break
		
		if not new_island:
			break
		
		island_growing= True
		while island_growing:
			island_growing= False
			for fidx1 in island[:]:
				if used_faces[fidx1]==1:
					used_faces[fidx1]= 2
					face_prop1= face_props[fidx1]
					for ed in face_edge_users[fidx1]:
						for f2 in ed:
							fidx2= f2.index
							if fidx1 != fidx2 and used_faces[fidx2]==0:
								island_growing= True
								face_prop2= face_props[fidx2]
								# normals are the same?
								if face_prop1[2]==face_prop2[2]:
									if abs(face_prop1[3] - DotVecs(face_prop1[1], face_prop2[0])) < 0.000001:
										used_faces[fidx2]= 1
										island.append(fidx2)
		islands.append([me.faces[i] for i in island])
	return islands



def facesUvIslands(me, PREF_IMAGE_DELIMIT=True):
	DotVecs= Blender.Mathutils.DotVecs
	def roundvec(v):
		return round(v[0], 4), round(v[1], 4)
	
	if not me.faceUV:
		return [ list(me.faces), ]
	
	# make a list of uv dicts
	face_uvs= [ [roundvec(uv) for uv in f.uv] for f in me.faces]
	
	# key - face uv || value - list of face idxs
	uv_connect_dict= dict([ (uv, [] ) for f_uvs in face_uvs for uv in f_uvs])
	
	for i, f_uvs in enumerate(face_uvs):
		for uv in f_uvs: # loops through rounded uv values
			uv_connect_dict[uv].append(i)
	islands= []
	
	used_faces= [0] * len(me.faces)
	while True:
		new_island= False
		for i, used_val in enumerate(used_faces):
			if used_val==0:
				island= [i]
				new_island= True
				used_faces[i]= 1
				break
		
		if not new_island:
			break
		
		island_growing= True
		while island_growing:
			island_growing= False
			for fidx1 in island[:]:
				if used_faces[fidx1]==1:
					used_faces[fidx1]= 2
					for uv in face_uvs[fidx1]:
						for fidx2 in uv_connect_dict[uv]:
							if fidx1 != fidx2 and used_faces[fidx2]==0:
								if not PREF_IMAGE_DELIMIT or me.faces[fidx1].image==me.faces[fidx2].image:
									island_growing= True
									used_faces[fidx2]= 1
									island.append(fidx2)
		
		islands.append([me.faces[i] for i in island])
	return islands

#def faceUvBounds(me, faces= None):
	

def facesUvRotate(me, deg, faces= None, pivot= (0,0)):
	'''
	Faces can be None an all faces will be used
	pivot is just the x/y well rotated about
	
	positive deg value for clockwise rotation
	'''
	if faces==None: faces= me.faces
	pivot= Blender.Mathutils.Vector(pivot)
	
	rotmat= Blender.Mathutils.RotationMatrix(-deg, 2)
	
	for f in faces:
		f.uv= [((uv-pivot)*rotmat)+pivot for uv in f.uv]

def facesUvScale(me, sca, faces= None, pivot= (0,0)):
	'''
	Faces can be None an all faces will be used
	pivot is just the x/y well rotated about
	sca can be wither an int/float or a vector if you want to
	  scale x/y seperately.
	  a sca or (1.0, 1.0) will do nothing.
	'''
	def vecmulti(v1,v2):
		'''V2 is unchanged'''
		v1[:]= (v1.x*v2.x, v1.y*v2.y)
		return v1
	
	sca= Blender.Mathutils.Vector(sca)
	if faces==None: faces= me.faces
	pivot= Blender.Mathutils.Vector(pivot)
	
	for f in faces:
		f.uv= [vecmulti(uv-pivot, sca)+pivot for uv in f.uv]

	
def facesUvTranslate(me, tra, faces= None, pivot= (0,0)):
	'''
	Faces can be None an all faces will be used
	pivot is just the x/y well rotated about
	'''
	if faces==None: faces= me.faces
	tra= Blender.Mathutils.Vector(tra)
	
	for f in faces:
		f.uv= [uv+tra for uv in f.uv]

	

def edgeFaceUserCount(me, faces= None):
	'''
	Return an edge aligned list with the count for all the faces that use that edge. -
	can spesify a subset of the faces, so only those will be counted.
	'''
	if faces==None:
		faces= me.faces
		max_vert= len(me.verts)
	else:
		# find the lighest vert index
		pass
	
	edge_users= [0] * len(me.edges)
	
	edges_idx_dict= dict([(sorted_edge_indicies(ed), ed.index) for ed in me.edges])

	for f in faces:
		fvi= [v.index for v in f.v]# face vert idx's
		for i in xrange(len(f)):
			i1= fvi[i]
			i2= fvi[i-1]
			
			if i1>i2:
				i1,i2= i2,i1
			
			edge_users[edges_idx_dict[i1,i2]] += 1 
			
	return edge_users


#============================================================================#
# Takes a face, and a pixel x/y on the image and returns a worldspace x/y/z  #
# will return none if the pixel is not inside the faces UV                   #
#============================================================================#
def getUvPixelLoc(face, pxLoc, img_size = None, uvArea = None):
	TriangleArea= Blender.Mathutils.TriangleArea
	Vector= Blender.Mathutils.Vector
	
	if not img_size:
		w,h = face.image.size
	else:
		w,h= img_size
	
	scaled_uvs= [Vector(uv.x*w, uv.y*h) for uv in f.uv]
	
	if len(scaled_uvs)==3:
		indicies= ((0,1,2),)
	else:
		indicies= ((0,1,2), (0,2,3))
	
	for fidxs in indicies:
		for i1,i2,i3 in fidxs:
			# IS a point inside our triangle?
			# UVArea could be cached?
			uv_area = TriangleArea(scaled_uvs[i1], scaled_uvs[i2], scaled_uvs[i3])
			area0 = TriangleArea(pxLoc, scaled_uvs[i2], scaled_uvs[i3])
			area1 = TriangleArea(pxLoc, scaled_uvs[i1],	scaled_uvs[i3])
			area2 = TriangleArea(pxLoc, scaled_uvs[i1], scaled_uvs[i2])
			if area0 + area1 + area2 > uv_area + 1: # 1 px bleed/error margin.
				pass # if were a quad the other side may contain the pixel so keep looking.
			else:
				# We know the point is in the tri
				area0 /= uv_area
				area1 /= uv_area
				area2 /= uv_area
				
				# New location
				return Vector(\
					face.v[i1].co[0]*area0 + face.v[i2].co[0]*area1 + face.v[i3].co[0]*area2,\
					face.v[i1].co[1]*area0 + face.v[i2].co[1]*area1 + face.v[i3].co[1]*area2,\
					face.v[i1].co[2]*area0 + face.v[i2].co[2]*area1 + face.v[i3].co[2]*area2\
				)
				
	return None


type_tuple= type( (0,) )
type_list= type( [] )
def ngon(from_data, indices, PREF_FIX_LOOPS= True):
	'''
	takes a polyline of indices (fgon)
	and returns a list of face indicie lists.
	Designed to be used for importers that need indices for an fgon to create from existing verts.
	
	from_data: either a mesh, or a list/tuple of vectors.
	indices: a list of indicies to use this list is the ordered closed polyline to fill, and can be a subset of the data given.
	PREF_FIX_LOOPS: If this is enabled polylines that use loops to make ultiple polylines are delt with correctly.
	'''
	Mesh= Blender.Mesh
	Window= Blender.Window
	Scene= Blender.Scene
	Object= Blender.Object
	
	if len(indices) < 4:
		return [indices]
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
		
	if type(from_data) in (type_tuple, type_list):
		# From a list/tuple of vectors
		temp_mesh.verts.extend( [from_data[i] for i in indices] )
		#temp_mesh.edges.extend( [(temp_mesh.verts[i], temp_mesh.verts[i-1]) for i in xrange(len(temp_mesh.verts))] )
		edges= [(i, i-1) for i in xrange(len(temp_mesh.verts))]
	else:
		# From a mesh
		temp_mesh.verts.extend( [from_data.verts[i].co for i in indices] )
		#temp_mesh.edges.extend( [(temp_mesh.verts[i], temp_mesh.verts[i-1]) for i in xrange(len(temp_mesh.verts))] )
		edges= [(i, i-1) for i in xrange(len(temp_mesh.verts))]
	if edges:
		edges[0]= (0,len(temp_mesh.verts)-1)
	
	def rvec(co): return round(co.x, 6), round(co.y, 6), round(co.z, 6)
	def mlen(co): return co[0]+co[1]+co[2] # manhatten length of a vector
	
	if PREF_FIX_LOOPS:
		edge_used_count= {}
		
		rounded_verts= [rvec(v.co) for v in temp_mesh.verts] # rounded verts we can use as dict keys.
		
		# We need to check if any edges are used twice location based.
		for ed_idx, ed in enumerate(edges):
			ed_v1= rounded_verts[ed[0]]
			ed_v2= rounded_verts[ed[1]]
			
			if ed_v1==ed_v2: # Same locations, remove the edge.
				edges[ed_idx]= None
			else:
				if mlen(ed_v1) < mlen(ed_v2):
					edkey= ed_v1, ed_v2
				else:
					edkey= ed_v2, ed_v1
				
				try:
					edge_user_list= edge_used_count[edkey]
					edge_user_list.append(ed_idx)
					
					# remove edges if there are doubles.
					if len(edge_user_list) > 1:
						for edidx in edge_user_list:\
							edges[edidx]= None
				except:
					edge_used_count[edkey]= [ed_idx]
		
		
		# Now remove double verts
		vert_doubles= {}
		for edidx, ed in enumerate(edges):
			if ed != None:
				ed_v1= rounded_verts[ed[0]]
				ed_v2= rounded_verts[ed[1]]
				
				if ed_v1==ed_v2:
					edges[edidx]= None # will clear later, edge is zero length.
					#print 'REMOVING DOUBLES'
					
				else:
					# Try and replace with an existing vert or add teh one we use.
					try:	edges[edidx]= vert_doubles[ed_v1], ed[1]
					except:
						vert_doubles[ed_v1]= ed[0]
						#print 'REMOVING DOUBLES'
						
					try:	edges[edidx]= ed[0], vert_doubles[ed_v2]
					except:
						vert_doubles[ed_v2]= ed[1]
						#print 'REMOVING DOUBLES'
		
		edges= [ed for ed in edges if ed != None] # != None
		# Done removing double edges!
		
	# DONE DEALING WITH LOOP FIXING
	
	
	
	temp_mesh.edges.extend(edges)
		
	# Move verts to middle and normalize.
	# For a good fill we need to normalize and scale the vert location.
	
	xmax=ymax=zmax= -1<<30
	xmin=ymin=zmin= 1<<30
	for v in temp_mesh.verts:
		co= v.co
		x= co.x
		y= co.y
		z= co.z
		if x<xmin: xmin=x
		if y<ymin: ymin=y
		if z<zmin: zmin=z
		if x>xmax: xmax=x
		if y>ymax: ymax=y
		if z>zmax: zmax=z
	
	# get the bounds on the largist axis
	size= xmax-xmin
	size= max(size, ymax-ymin)
	size= max(size, zmax-zmin)
	
	xmid= (xmin+xmax)/2
	ymid= (ymin+ymax)/2
	zmid= (zmin+zmax)/2

	x=x/len(temp_mesh.verts)
	y=y/len(temp_mesh.verts)
	z=z/len(temp_mesh.verts)
	
	for v in temp_mesh.verts:
		co= v.co
		co.x= (co.x-xmid)/size
		co.y= (co.y-ymid)/size
		co.z= (co.z-zmid)/size
	# finished resizing the verts.
	
	oldmode = Mesh.Mode()
	Mesh.Mode(Mesh.SelectModes['VERTEX'])
	temp_mesh.sel= 1 # Select all verst	
	
	# Must link to scene
	scn= Scene.GetCurrent()
	temp_ob= Object.New('Mesh')
	temp_ob.link(temp_mesh)
	scn.link(temp_ob)
	
	temp_mesh.fill()
	scn.unlink(temp_ob)
	Mesh.Mode(oldmode)
	
	new_indices= [ [v.index for v in f.v]  for f in temp_mesh.faces ]
	
	if not new_indices: # JUST DO A FAN, Cant Scanfill
		print 'Warning Cannot scanfill!- Fallback on a triangle fan.'
		new_indices = [ [0, i-1, i] for i in xrange(2, len(indices)) ]
	else:
		# Use real scanfill.
		# See if its flipped the wrong way.
		flip= None
		for fi in new_indices:
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
			for fi in new_indices:
				fi.reverse()
	
	if is_editmode:
		Window.EditMode(1)
		
	# Save some memory and forget about the verts.
	# since we cant unlink the mesh.
	temp_mesh.verts= None 
	
	return new_indices
	


# EG
'''
scn= Scene.GetCurrent()
me = scn.getActiveObject().getData(mesh=1)
ind= [v.index for v in me.verts if v.sel] # Get indices

indices = ngon(me, ind) # fill the ngon.

# Extand the faces to show what the scanfill looked like.
print len(indices)
me.faces.extend([[me.verts[ii] for ii in i] for i in indices])
'''

def meshCalcNormals(me, vertNormals=None):
	'''
	takes a mesh and returns very high quality normals 1 normal per vertex.
	The normals should be correct, indipendant of topology
	
	vertNormals - a list of vectors at least as long as the number of verts in the mesh
	'''
	Ang= Blender.Mathutils.AngleBetweenVecs
	Vector= Blender.Mathutils.Vector
	SMALL_NUM=0.000001
	# Weight the edge normals by total angle difference
	# EDGE METHOD
	
	if not vertNormals:
		vertNormals= [ Vector() for v in xrange(len(me.verts)) ]
	else:
		for v in vertNormals:
			v.zero()
		
	edges={}
	for f in me.faces:
		for i in xrange(len(f)):
			i1, i2= f.v[i].index, f.v[i-1].index
			if i1<i2:
				i1,i2= i2,i1
				
			try:
				edges[i1, i2].append(f.no)
			except:
				edges[i1, i2]= [f.no]
				
	# Weight the edge normals by total angle difference
	for fnos in edges.itervalues():
		
		len_fnos= len(fnos)
		if len_fnos>1:
			totAngDiff=0
			for j in reversed(xrange(len_fnos)):
				for k in reversed(xrange(j)):
					#print j,k
					try:
						totAngDiff+= (Ang(fnos[j], fnos[k])) # /180 isnt needed, just to keeop the vert small.
					except:
						pass # Zero length face
			
			# print totAngDiff
			if totAngDiff > SMALL_NUM:
				'''
				average_no= Vector()
				for no in fnos:
					average_no+=no
				'''
				average_no= reduce(lambda a,b: a+b, fnos, Vector())
				fnos.append(average_no*totAngDiff) # average no * total angle diff
			#else:
			#	fnos[0]
		else:
			fnos.append(fnos[0])
	
	for ed, v in edges.iteritems():
		vertNormals[ed[0]]+= v[-1]
		vertNormals[ed[1]]+= v[-1]
	for i, v in enumerate(me.verts):
		v.no= vertNormals[i]




def pointInsideMesh(ob, pt):
	Intersect = Blender.Mathutils.Intersect # 2 less dict lookups.
	Vector = Blender.Mathutils.Vector
	
	def ptInFaceXYBounds(f, pt):
			
		co= f.v[0].co
		xmax= xmin= co.x
		ymax= ymin= co.y
		
		co= f.v[1].co
		xmax= max(xmax, co.x)
		xmin= min(xmin, co.x)
		ymax= max(ymax, co.y)
		ymin= min(ymin, co.y)
		
		co= f.v[2].co
		xmax= max(xmax, co.x)
		xmin= min(xmin, co.x)
		ymax= max(ymax, co.y)
		ymin= min(ymin, co.y)
		
		if len(f)==4: 
			co= f.v[3].co
			xmax= max(xmax, co.x)
			xmin= min(xmin, co.x)
			ymax= max(ymax, co.y)
			ymin= min(ymin, co.y)
		
		# Now we have the bounds, see if the point is in it.
		if\
		pt.x < xmin or\
		pt.y < ymin or\
		pt.x > xmax or\
		pt.y > ymax:
			return False # point is outside face bounds
		else:
			return True # point inside.
		#return xmax, ymax, xmin, ymin
	
	def faceIntersect(f):
		isect = Intersect(f.v[0].co, f.v[1].co, f.v[2].co, ray, obSpacePt, 1) # Clipped.
		if not isect and len(f) == 4:
			isect = Intersect(f.v[0].co, f.v[2].co, f.v[3].co, ray, obSpacePt, 1) # Clipped.
				
		if isect and isect.z > obSpacePt.z: # This is so the ray only counts if its above the point. 
			return True
		else:
			return False
	
	
	obImvMat = Blender.Mathutils.Matrix(ob.matrixWorld)
	obImvMat.invert()
	pt.resize4D()
	obSpacePt = pt* obImvMat
	pt.resize3D()
	obSpacePt.resize3D()
	ray = Vector(0,0,-1)
	me= ob.getData(mesh=1)
	
	# Here we find the number on intersecting faces, return true if an odd number (inside), false (outside) if its true.
	return len([None for f in me.faces if ptInFaceXYBounds(f, obSpacePt) if faceIntersect(f)]) % 2


# NMesh wrapper
Vector= Blender.Mathutils.Vector
class NMesh(object):
	__slots__= 'verts', 'faces', 'edges', 'faceUV', 'materials', 'realmesh'
	def __init__(self, mesh):
		'''
		This is an NMesh wrapper that
		mesh is an Mesh as returned by Blender.Mesh.New()
		This class wraps NMesh like access into Mesh
		
		Running NMesh.update() - with this wrapper,
		Will update the realmesh.
		'''
		self.verts= []
		self.faces= []
		self.edges= []
		self.faceUV= False
		self.materials= []
		self.realmesh= mesh
	
	def addFace(self, nmf):
		self.faces.append(nmf)
	
	def Face(self, v=[]):
		return NMFace(v)
	def Vert(self, x,y,z):
		return NMVert(x,y,z)
	
	def hasFaceUV(self, flag):
		if flag:
			self.faceUV= True
		else:
			self.faceUV= False
	
	def addMaterial(self, mat):
		self.materials.append(mat)
	
	def update(self, recalc_normals=False): # recalc_normals is dummy
		mesh= self.realmesh
		mesh.verts= None # Clears the 
		
		# Add in any verts from faces we may have not added.
		for nmf in self.faces:
			for nmv in nmf.v:
				if nmv.index==-1:
					nmv.index= len(self.verts)
					self.verts.append(nmv)
					
		
		mesh.verts.extend([nmv.co for nmv in self.verts])
		for i, nmv in enumerate(self.verts):
			nmv.index= i
			mv= mesh.verts[i]
			mv.sel= nmv.sel
		
		good_faces= [nmf for nmf in self.faces if len(nmf.v) in (3,4)]
		#print len(good_faces), 'AAA'
		
		
		#mesh.faces.extend([nmf.v for nmf in self.faces])
		mesh.faces.extend([[mesh.verts[nmv.index] for nmv in nmf.v] for nmf in good_faces])
		if len(mesh.faces):
			if self.faceUV:
				mesh.faceUV= 1
			
			#for i, nmf in enumerate(self.faces):
			for i, nmf in enumerate(good_faces):
				mf= mesh.faces[i]
				if self.faceUV:
					if len(nmf.uv) == len(mf.v):
						mf.uv= [Vector(uv[0], uv[1]) for uv in nmf.uv]
					if len(nmf.col) == len(mf.v):
						for c, i in enumerate(mf.col):
							c.r, c.g, c.b= nmf.col[i].r, nmf.col[i].g, nmf.col[i].b
					if nmf.image:
						mf.image= nmf.image
		
		mesh.materials= self.materials[:16]

class NMVert(object):
	__slots__= 'co', 'index', 'no', 'sel', 'uvco'
	def __init__(self, x,y,z):
		self.co= Vector(x,y,z)
		self.index= None # set on appending.
		self.no= Vector(0,0,1) # dummy
		self.sel= 0
		self.uvco= None
class NMFace(object):
	__slots__= 'col', 'flag', 'hide', 'image', 'mat', 'materialIndex', 'mode', 'normal',\
	'sel', 'smooth', 'transp', 'uv', 'v'
	
	def __init__(self, v=[]):
		self.col= []
		self.flag= 0
		self.hide= 0
		self.image= None
		self.mat= 0 # materialIndex needs support too.
		self.mode= 0
		self.normal= Vector(0,0,1)
		self.uv= []
		self.sel= 0
		self.smooth= 0
		self.transp= 0
		self.uv= []
		self.v= [] # a list of nmverts.
	
class NMCol(object):
	__slots__ = 'r', 'g', 'b', 'a'
	def __init__(self):
		self.r= 255
		self.g= 255
		self.b= 255
		self.a= 255


'''
# 
verts_split= [dict() for i in xrange(len(me.verts))]

tot_verts= 0
for f in me.faces:
	f_uv= f.uv
	for i, v in enumerate(f.v):
		vert_index= v.index # mesh index
		vert_dict= verts_split[vert_index] # get the dict for this vert
		
		uv= f_uv[i]
		# now we have the vert and the face uv well make a unique dict.
		
		vert_key= v.x, v.y, v.x, uv.x, uv.y # ADD IMAGE NAME HETR IF YOU WANT TO SPLIT BY THAT TOO
		value= vert_index, tot_verts # ADD WEIGHT HERE IF YOU NEED.
		try:
			vert_dict[vert_key] # if this is missing it will fail.
		except:
			# this stores a mapping between the split and orig vert indicies
			vert_dict[vert_key]= value 
			tot_verts+= 1

# a flat list of split verts - can add custom weight data here too if you need
split_verts= [None]*tot_verts

for vert_split_dict in verts_split:
	for key, value in vert_split_dict.iteritems():
		local_index, split_index= value
		split_verts[split_index]= key

# split_verts - Now you have a list of verts split by their UV.
'''
