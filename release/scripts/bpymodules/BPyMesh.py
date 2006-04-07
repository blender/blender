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
		me.removeVertGroup(group)
	
	# Add clean unused vert groupNames back
	for group in groupNames:
		me.addVertGroup(group)	
	
	add_ = Blender.Mesh.AssignModes.ADD
	
	vertList= [None]
	for i, v in enumerate(me.verts):
		vertList[0]= i
		for group, weight in vWeightDict[i].iteritems():
			try:
				me.assignVertsToGroup(group, vertList, weight, add_)
			except:
				pass # vert group is not used anymore.
	
	me.update()



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