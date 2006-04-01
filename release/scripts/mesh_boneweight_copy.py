#!BPY
"""
Name: 'Bone Weight Copy'
Blender: 241
Group: 'Object'
Tooltip: 'Copy Bone Weights from 1 weighted mesh, to other unweighted meshes.'
"""
import Blender
from Blender import Armature, Object, Mathutils, Window, Mesh
Vector= Mathutils.Vector

def copy_bone_influences(_from, _to):
	ob_from, me_from, world_verts_from, from_groups=  _from
	ob_to, me_to, world_verts_to, dummy=  _to	
	del dummy
	
	def getSnapIdx(vec, vecs):
		'''
		Returns the closest vec to snap_points
		'''
		close_dist= 1<<30
		close_idx= None
		
		x,y,z= tuple(vec)
		for i, v in enumerate(vecs):
			# quick length cmp before a full length comparison.
			if\
			abs(x-v[0]) < close_dist and\
			abs(y-v[1]) < close_dist and\
			abs(z-v[2]) < close_dist:
				l= (v-vec).length
				if l<close_dist:
					close_dist= l
					close_idx= i
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
	sel=[]
	from_data= None
	for ob in Object.GetSelected():
		if ob.getType()=='Mesh':
			me= ob.getData(mesh=1)
			groups= me.getVertGroupNames()
			
			data= (ob, me, worldspace_verts(me, ob), groups)
			if groups:
				if from_data:
					Blender.Draw.PupMenu('More then 1 mesh has vertex weights, only select 1 mesh with weights. aborting.')
					return
				else:
					from_data= data
			else:
				sel.append(data)
	
	if not sel or from_data==None:
		Blender.Draw.PupMenu('Select 2 or more mesh objects, aborting.')
		return
	
	Window.WaitCursor(1)
	# Now do the copy.
	for data in sel:
		copy_bone_influences(from_data, data)
	Window.WaitCursor(0)

if __name__ == '__main__':
	main()