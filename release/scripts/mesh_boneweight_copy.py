#!BPY
"""
Name: 'Bone Weight Copy'
Blender: 245
Group: 'Object'
Tooltip: 'Copy Bone Weights from 1 mesh, to all other selected meshes.'
"""

__author__ = "Campbell Barton aka ideasman42"
__url__ = ["www.blender.org", "blenderartists.org", "www.python.org"]
__version__ = "0.1"
__bpydoc__ = """\

Bone Weight Copy

This script is used to copy bone weights from 1 mesh with weights (the source mesh) to many (the target meshes).
Weights are copied from 1 mesh to another based on how close they are together.

For normal operation, select 1 source mesh with vertex weights and any number of unweighted meshes that overlap the source mesh.
Then run this script using default options and check the new weigh.


A differnt way to use this script is to update the weights an an alredy weighted mesh.
this is done using the "Copy to Selected" option enabled and works a bit differently,
With the target mesh, select the verts you want to update.
since all meshes have weights we cant just use the weighted mesh as the source,
so the Active Object is used for the source mesh.
Run the script and the selected verts on all non active meshes will be updated.
"""

# ***** BEGIN GPL LICENSE BLOCK *****
#
# Script copyright (C) Campbell J Barton
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

import Blender
from Blender import Armature, Object, Mathutils, Window, Mesh
Vector= Mathutils.Vector
SMALL_NUM= 0.000001
def copy_bone_influences(_from, _to, PREF_SEL_ONLY, PREF_NO_XCROSS):
	ob_from, me_from, world_verts_from, from_groups=  _from
	ob_to, me_to, world_verts_to, dummy=  _to	
	del dummy
	
	def getSnapIdx(seek_vec, vecs):
		'''
		Returns the closest vec to snap_points
		'''
		
		# First seek the closest Z axis vert idx/v
		seek_vec_x,seek_vec_y,seek_vec_z= seek_vec
		
		from_vec_idx= 0
		
		len_vecs= len(vecs)
		
		upidx= len_vecs-1
		loidx= 0
		
		while from_vec_idx < len_vecs and vecs[from_vec_idx][1].z < seek_vec_z:
			from_vec_idx+=1
		
		# Clamp if we overstepped.
		if from_vec_idx  >= len_vecs:
			from_vec_idx-=1
		
		close_dist= (vecs[from_vec_idx][1]-seek_vec).length
		close_idx= vecs[from_vec_idx][0]
		
		upidx= from_vec_idx+1
		loidx= from_vec_idx-1
		
		# Set uselo/useup. This means we can keep seeking up/down.
		if upidx >= len_vecs:	useup= False
		else:					useup= True
			
		if loidx < 0:			uselo= False
		else:					uselo= True
		
		# Seek up/down to find the closest v to seek vec.
		while uselo or useup:
			if useup:
				if upidx >= len_vecs:
					useup= False
				else:
					i,v= vecs[upidx]
					if (not PREF_NO_XCROSS) or ((v.x >= -SMALL_NUM and seek_vec_x >= -SMALL_NUM) or (v.x <= SMALL_NUM and seek_vec_x <= SMALL_NUM)): # enfoce  xcrossing
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
					if (not PREF_NO_XCROSS) or ((v.x >= -SMALL_NUM and seek_vec_x >= -SMALL_NUM) or (v.x <= SMALL_NUM and seek_vec_x  <= SMALL_NUM)): # enfoce  xcrossing
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
	
	
	to_groups= me_to.getVertGroupNames() # if not PREF_SEL_ONLY will always be []
	from_groups= me_from.getVertGroupNames()
	
	if PREF_SEL_ONLY: # remove selected verts from all groups.
		vsel= [v.index for v in me_to.verts if v.sel]
		for group in to_groups:
			me_to.removeVertsFromGroup(group, vsel)  
	else: # Add all groups.
		for group in from_groups:
			me_to.addVertGroup(group)
	
	add_ = Mesh.AssignModes.ADD
	
	for i, co in enumerate(world_verts_to):
		if (not PREF_SEL_ONLY) or (PREF_SEL_ONLY and me_to.verts[i].sel):
			
			Window.DrawProgressBar(0.99 * (i/float(len(world_verts_to))), 'Copy "%s" -> "%s" ' % (ob_from.name, ob_to.name))
			
			from_idx= getSnapIdx(co, world_verts_from)
			from_infs= me_from.getVertexInfluences(from_idx)
			
			for group, weight in from_infs:
				
				# Add where needed.
				if PREF_SEL_ONLY and group not in to_groups:
					me_to.addVertGroup(group)
					to_groups.append(group)
					
				me_to.assignVertsToGroup(group, [i], weight, add_)
	
	me_to.update()
	
# ZSORT return (i/co) tuples, used for fast seeking of the snapvert.
def worldspace_verts_idx(me, ob):
	mat= ob.matrixWorld
	verts_zsort= [ (i, v.co*mat) for i, v in enumerate(me.verts) ]
	
	# Sorts along the Z Axis so we can optimize the getsnap.
	try:	verts_zsort.sort(key = lambda a: a[1].z)
	except:	verts_zsort.sort(lambda a,b: cmp(a[1].z, b[1].z,))
	
	return verts_zsort


def worldspace_verts(me, ob):
	mat= ob.matrixWorld
	return [ v.co*mat for v in me.verts ]
	
def subdivMesh(me, subdivs):
	oldmode = Mesh.Mode()
	Mesh.Mode(Mesh.SelectModes['FACE'])
	me.sel= 1
	for i in xrange(subdivs):
		me.subdivide(0)
	Mesh.Mode(oldmode)


def main():
	print '\nStarting BoneWeight Copy...'
	scn= Blender.Scene.GetCurrent()
	contextSel= Object.GetSelected()
	if not contextSel:
		Blender.Draw.PupMenu('Error%t|2 or more mesh objects need to be selected.|aborting.')
		return
	
	PREF_QUALITY= Blender.Draw.Create(0)
	PREF_NO_XCROSS= Blender.Draw.Create(0)
	PREF_SEL_ONLY= Blender.Draw.Create(0)
	
	pup_block = [\
	('Quality:', PREF_QUALITY, 0, 4, 'Generate interpolated verts for a higher quality result.'),\
	('No X Crossing', PREF_NO_XCROSS, 'Do not snap across the zero X axis'),\
	'',\
	'"Update Selected" copies',\
	'active object weights to',\
	'selected verts on the other',\
	'selected mesh objects.',\
	('Update Selected', PREF_SEL_ONLY, 'Only copy new weights to selected verts on the target mesh. (use active object as source)'),\
	]
	
	
	if not Blender.Draw.PupBlock("Copy Weights for %i Meshs" % len(contextSel), pup_block):
		return
	
	PREF_SEL_ONLY= PREF_SEL_ONLY.val
	PREF_NO_XCROSS= PREF_NO_XCROSS.val
	quality=  PREF_QUALITY.val
	
	act_ob= scn.objects.active
	if PREF_SEL_ONLY and act_ob==None:
		Blender.Draw.PupMenu('Error%t|When dealing with 2 or more meshes with vgroups|There must be an active object|to be used as a source|aborting.')
		return

	sel=[]
	from_data= None
	
	for ob in contextSel:
		if ob.type=='Mesh':
			me= ob.getData(mesh=1)
			groups= me.getVertGroupNames()
			
			# If this is the only mesh with a group OR if its one of many, but its active.
			if groups and ((ob==act_ob and PREF_SEL_ONLY) or (not PREF_SEL_ONLY)):
				if from_data:
					Blender.Draw.PupMenu('More then 1 mesh has vertex weights, only select 1 mesh with weights. aborting.')
					return
				else:
					# This uses worldspace_verts_idx which gets (idx,co) pairs, then zsorts.
					if quality:
						for _ob in contextSel:
							_ob.sel=0
						ob.sel=1
						Object.Duplicate(mesh=1)
						ob= scn.objects.active
						me= ob.getData(mesh=1)
						# groups will be the same
						print '\tGenerating higher %ix quality weights.' % quality
						subdivMesh(me, quality)
						scn.unlink(ob)
					from_data= (ob, me, worldspace_verts_idx(me, ob), groups)
					
			else:
				data= (ob, me, worldspace_verts(me, ob), groups)
				sel.append(data)
	
	if not from_data:
		Blender.Draw.PupMenu('Error%t|No mesh with vertex groups found.')
		return
	
	if not sel:
		Blender.Draw.PupMenu('Error%t|Select 2 or more mesh objects, aborting.')
		if quality:	from_data[1].verts= None
		return
	
	t= Blender.sys.time()
	Window.WaitCursor(1)
	
	# Now do the copy.
	print '\tCopying from "%s" to %i other mesh(es).' % (from_data[0].name, len(sel))
	for data in sel:
		copy_bone_influences(from_data, data, PREF_SEL_ONLY, PREF_NO_XCROSS)
	
	# We cant unlink the mesh, but at least remove its data.
	if quality:
		from_data[1].verts= None
	
	print 'Copy Complete in %.6f sec' % (Blender.sys.time()-t)
	Window.DrawProgressBar(1.0, '')
	Window.WaitCursor(0)

if __name__ == '__main__':
	main()