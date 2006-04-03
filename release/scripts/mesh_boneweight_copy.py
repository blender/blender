#!BPY
"""
Name: 'Bone Weight Copy'
Blender: 241
Group: 'Object'
Tooltip: 'Copy Bone Weights from 1 weighted mesh, to other unweighted meshes.'
"""
# 37.314122 sec
# 8.9 sec sec


import Blender
from Blender import Armature, Object, Mathutils, Window, Mesh
Vector= Mathutils.Vector

def copy_bone_influences(_from, _to):
	ob_from, me_from, world_verts_from, from_groups=  _from
	ob_to, me_to, world_verts_to, dummy=  _to	
	del dummy
	
	def getSnapIdx(seek_vec, vecs):
		'''
		Returns the closest vec to snap_points
		'''
		
		# First seek the closest Z axis vert idx/v
		seek_vec_x,seek_vec_y,seek_vec_z= tuple(seek_vec)
		
		from_vec_idx= 0
		
		len_vecs= len(vecs)
		
		upidx= len_vecs-1
		loidx= 0
		
		_range=upidx-loidx 
		# Guess the right index, keep re-adjusting the high and the low.
		while _range > 3:
			half= _range/2
			z= vecs[upidx-half][1].z
			if z >= seek_vec_z:
				upidx= upidx-half
			elif z < seek_vec_z:
				loidx= loidx+half
			
			_range=upidx-loidx 
		
		from_vec_idx= loidx
		
		# Seek the rest of the way. should only need to seek 2 or 3 items at the most.
		while from_vec_idx < len_vecs and vecs[from_vec_idx][1].z < seek_vec_z:
			from_vec_idx+=1
		
		# Clamp if we overstepped.
		if from_vec_idx  >= len_vecs:
			from_vec_idx-=1
		
		close_dist= (vecs[from_vec_idx][1]-seek_vec).length
		close_idx= vecs[from_vec_idx][0]
		
		upidx= from_vec_idx+1
		loidx= from_vec_idx-1
		
		uselo=useup= True # This means we can keep seeking up/down.
		
		# Seek up/down to find the closest v to seek vec.
		while uselo or useup:
			if useup:
				
				if upidx >= len_vecs:
					useup= False
				else:
					i,v= vecs[upidx]
					if v.z-seek_vec_z > close_dist:
						# the verticle distance is greater then the best distance sofar. we can stop looking up.
						useup= False
					elif abs(seek_vec_y-v.y) < close_dist and abs(seek_vec_x-v.x) < close_dist:
						# This is in the limit measure it.
						l= (seek_vec-v).length
						if l<close_dist:
							close_dist= l
							close_idx= i
					upidx+=1
			
			if uselo:
				if loidx == 0:
					uselo= False
				else:
					i,v= vecs[loidx]
					if seek_vec_z-v.z > close_dist:
						# the verticle distance is greater then the best distance sofar. we can stop looking up.
						uselo= False
					elif abs(seek_vec_y-v.y) < close_dist and abs(seek_vec_x-v.x) < close_dist:
						# This is in the limit measure it.
						l= (seek_vec-v).length
						if l<close_dist:
							close_dist= l
							close_idx= i
					loidx-=1
				
		return close_idx
		
	from_groups= me_from.getVertGroupNames()
	for group in from_groups:
		me_to.addVertGroup(group)
	
	add_ = Mesh.AssignModes.ADD
	
	for i, co in enumerate(world_verts_to):
		from_idx= getSnapIdx(co, world_verts_from)
		from_infs= me_from.getVertexInfluences(from_idx)
		
		for group, weight in from_infs:
			me_to.assignVertsToGroup(group, [i], weight, add_)
	
	me_to.update()
	
# ZSORT return (i/co) tuples, used for fast seeking of the snapvert.
def worldspace_verts_idx(me, ob):
	mat= ob.matrixWorld
	def worldvert(v):
		vec= Vector(v)
		vec.resize4D()
		vec= vec*mat
		vec.resize3D()
		return vec
	verts_zsort= [ (i, worldvert(v.co)) for i, v in enumerate(me.verts) ]
	
	# Sorts along the Z Axis so we can optimize the getsnap.
	verts_zsort.sort(lambda a,b: cmp(a[1].z, b[1].z,))
	return verts_zsort


def worldspace_verts(me, ob):
	mat= ob.matrixWorld
	def worldvert(v):
		vec= Vector(v)
		vec.resize4D()
		vec= vec*mat
		vec.resize3D()
		return vec
	return [ worldvert(v.co) for v in me.verts ]
	

def main():
	print '\nStarting BoneWeight Copy...'
	sel=[]
	from_data= None
	for ob in Object.GetSelected():
		if ob.getType()=='Mesh':
			me= ob.getData(mesh=1)
			groups= me.getVertGroupNames()
			if groups:
				if from_data:
					Blender.Draw.PupMenu('More then 1 mesh has vertex weights, only select 1 mesh with weights. aborting.')
					return
				else:
					# This uses worldspace_verts_idx which gets (idx,co) pairs, then zsorts.
					data= (ob, me, worldspace_verts_idx(me, ob), groups)
					from_data= data
			else:
				data= (ob, me, worldspace_verts(me, ob), groups)
				sel.append(data)
	
	if not sel or from_data==None:
		Blender.Draw.PupMenu('Select 2 or more mesh objects, aborting.')
		return
	t= Blender.sys.time()
	Window.WaitCursor(1)
	# Now do the copy.
	print '\tCopying from "%s" to %i other meshe(s).' % (from_data[0].name, len(sel))
	for data in sel:
		copy_bone_influences(from_data, data)
	print 'Copy Compleate in %.6f sec' % (Blender.sys.time()-t)
	Window.WaitCursor(0)

if __name__ == '__main__':
	main()