import Blender

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
		tempMe = ob.getData(mesh=1)
		mesh.materials = tempMe.materials
		mesh.degr = tempMe.degr
		try: mesh.mode = tempMe.mode # Mesh module needs fixing.
		except: pass
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
		
	else:
		try:
			# Will only work for curves!!
			# Text- no material access in python interface.
			# Surf- no python interface
			# MBall- no material access in python interface.
			
			data = ob.getData()
			materials = data.getMaterials()
			mesh.materials = materials
			print 'assigning materials for non mesh'
		except:
			print 'Cant assign materials to', type
	
	return mesh

type_tuple= type( (0,) )
type_list= type( [] )
def ngon(from_data, indices):
	'''
	takes a polyline of indices (fgon)
	and returns a list of face indicie lists.
	Designed to be used for importers that need indices for an fgon to create from existing verts.
	
	from_data is either a mesh, or a list/tuple of vectors.
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
		temp_mesh.edges.extend( [(temp_mesh.verts[i], temp_mesh.verts[i-1]) for i in xrange(len(temp_mesh.verts))] )
	else:
		# From a mesh
		temp_mesh.verts.extend( [from_data.verts[i].co for i in indices] )
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
	
	new_indices= [ [v.index for v in f.v]  for f in temp_mesh.faces ]
	
	if not new_indices: # JUST DO A FAN, Cant Scanfill
		print 'Warning Cannot scanfill!- Fallback on a triangle fan.'
		new_indices = [ [indices[0], indices[i-1], indices[i]] for i in xrange(2, len(indices)) ]
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








from Blender import *

def pointInsideMesh(ob, pt):
	Intersect = Mathutils.Intersect # 2 less dict lookups.
	Vector = Mathutils.Vector
	
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
		
		if len(f.v)==4: 
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
		if not isect and len(f.v) == 4:
			isect = Intersect(f.v[0].co, f.v[2].co, f.v[3].co, ray, obSpacePt, 1) # Clipped.
				
		if isect and isect.z > obSpacePt.z: # This is so the ray only counts if its above the point. 
			return True
		else:
			return False
	
	
	obImvMat = Mathutils.Matrix(ob.matrixWorld)
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
