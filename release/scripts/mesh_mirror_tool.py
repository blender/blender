#!BPY
"""
Name: 'Mirror Vertex Locations & Weight'
Blender: 241
Group: 'Mesh'
Tooltip: 'Snap Verticies to X mirrord locations and weights.'
"""

__author__ = "Campbell Barton aka ideasman42"
__url__ = ["www.blender.org", "blenderartists.org", "www.python.org"]
__version__= '1.0'
__bpydoc__= '''\
This script is used to mirror vertex locations and weights
It is usefull if you have a model that was made symetrical
but has verts that have moved from their mirrored locations slightly,
casuing blenders X-Mirror options not to work.

Weights can be mirrored too, this is usefull if you want to model 1 side of a mesh, copy the mesh and flip it.
You can then use this script to mirror to the copy, even creating new flipped vertex groups, renaming group name left to right or .L to .R

Vertex positions are mirrored by doing a locational lookup,
finding matching verts on both sides of a mesh and moving to the left/right or mid location.

The vertex weights work differently, they are mirrored my location also,
but they mirror in pairs, rather it works by finding the closest vertex on the flip side and using its weight.

When a location mirror is finished, verts that have not been mirrored will remain selected.
a good way to check both sides are mirrord is to select the mirrored parts,
run this script with default options and then see of there are any selected verts.

For details on each option read the tooltips.
'''

# ***** BEGIN GPL LICENSE BLOCK *****
#
# Script copyright (C) Campbell Barton
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


from Blender import Draw, Window, Scene, Mesh, Mathutils, sys, Object
import BPyMesh

BIGNUM= 1<<30

def mesh_mirror(me, PREF_MIRROR_LOCATION, PREF_XMID_SNAP, PREF_MAX_DIST, PREF_XZERO_THRESH, PREF_MODE, PREF_SEL_ONLY, PREF_EDGE_USERS, PREF_MIRROR_WEIGHTS, PREF_FLIP_NAMES, PREF_CREATE_FLIP_NAMES):
	'''
	PREF_MIRROR_LOCATION, Will we mirror locations?
	PREF_XMID_SNAP, Should we snap verts to X-0?
	PREF_MAX_DIST, Maximum distance to test snapping verts.
	PREF_XZERO_THRESH, How close verts must be to the middle before they are considered X-Zero verts.
	PREF_MODE, 0:middle, 1: Left. 2:Right.
	PREF_SEL_ONLY, only snap the selection
	PREF_EDGE_USERS, match only verts with the same number of edge users.
	PREF_MIRROR_LOCATION, 
	'''
	
	
	# Operate on all verts
	if not PREF_SEL_ONLY:
		for v in me.verts:
			v.sel=1
	
	
	if PREF_EDGE_USERS:
		edge_users= [0]*len(me.verts)
		for ed in me.edges:
			edge_users[ed.v1.index]+=1
			edge_users[ed.v2.index]+=1
	
	
	
	if PREF_XMID_SNAP: # Do we snap locations at all?
		for v in me.verts:
			if v.sel:
				if abs(v.co.x) <= PREF_XZERO_THRESH:
					v.co.x= 0
					v.sel= 0
		
		# alredy de-selected verts
		neg_vts = [v for v in me.verts if v.sel and v.co.x < 0]
		pos_vts = [v for v in me.verts if v.sel and v.co.x > 0]
		
	else:
		# Use a small margin verts must be outside before we mirror them.
		neg_vts = [v for v in me.verts if v.sel if v.co.x <  -PREF_XZERO_THRESH]
		pos_vts = [v for v in me.verts if v.sel if v.co.x >   PREF_XZERO_THRESH]
	
	
	
	#*Mirror Location*********************************************************#
	if PREF_MIRROR_LOCATION:
		mirror_pairs= []
		# allign the negative with the positive.
		flipvec= Mathutils.Vector()
		len_neg_vts= float(len(neg_vts))
		for i1, nv in enumerate(neg_vts):
			if nv.sel: # we may alredy be mirrored, if so well be deselected
				nv_co= nv.co
				for i2, pv in enumerate(pos_vts):
					if pv.sel:
						# Enforce edge users.
						if not PREF_EDGE_USERS or edge_users[i1]==edge_users[i2]:
							flipvec[:]= pv.co
							flipvec.x= -flipvec.x
							l= (nv_co-flipvec).length
							
							if l==0.0: # Both are alredy mirrored so we dont need to think about them.
								# De-Select so we dont use again/
								pv.sel= nv.sel= 0
							
							# Record a match.
							elif l<=PREF_MAX_DIST:
								
								# We can adjust the length by the normal, now we know the length is under the limit.
								# DISABLED, WASNT VERY USEFULL
								'''
								if PREF_NOR_WEIGHT>0:
									# Get the normal and flipm reuse flipvec
									flipvec[:]= pv.no
									flipvec.x= -flipvec.x
									try:
										ang= Mathutils.AngleBetweenVecs(nv.no, flipvec)/180.0
									except: # on rare occasions angle between vecs will fail.- zero length vec.
										ang= 0
									
									l=l*(1+(ang*PREF_NOR_WEIGHT))
								'''
								# Record the pairs for sorting to see who will get joined
								mirror_pairs.append((l, nv, pv))
				
				# Update every 20 loops
				if i1 % 10 == 0:
					Window.DrawProgressBar(0.8 * (i1/len_neg_vts), 'Mirror verts %i of %i' % (i1, len_neg_vts))
		
		Window.DrawProgressBar(0.9, 'Mirror verts: Updating locations')
		
		# Now we have a list of the pairs we might use, lets find the best and do them first.
		# de-selecting as we go. so we can makke sure not to mess it up.
		try:	mirror_pairs.sort(key = lambda a: a[0])
		except:	mirror_pairs.sort(lambda a,b: cmp(a[0], b[0]))
		
		for dist, v1,v2 in mirror_pairs: # dist, neg, pos
			if v1.sel and v2.sel:
				if PREF_MODE==0: # Middle
					flipvec[:]= v2.co # positive
					flipvec.x= -flipvec.x # negatve
					v2.co= v1.co= (flipvec+v1.co)*0.5 # midway
					v2.co.x= -v2.co.x
				elif PREF_MODE==2: # Left
					v2.co= v1.co
					v2.co.x= -v2.co.x
				elif PREF_MODE==1: # Right
					v1.co= v2.co
					v1.co.x= -v1.co.x
				v1.sel= v2.sel= 0
	
	
	#*Mirror Weights**********************************************************#
	if PREF_MIRROR_WEIGHTS:
		
		groupNames, vWeightDict= BPyMesh.meshWeight2Dict(me)
		mirror_pairs_l2r= [] # Stor a list of matches for these verts.
		mirror_pairs_r2l= [] # Stor a list of matches for these verts.
		
		# allign the negative with the positive.
		flipvec= Mathutils.Vector()
		len_neg_vts= float(len(neg_vts))
		
		# Here we make a tuple to look through, if were middle well need to look through both.
		if PREF_MODE==0: # Middle
			find_set= ((neg_vts, pos_vts, mirror_pairs_l2r), (pos_vts, neg_vts, mirror_pairs_r2l))
		elif PREF_MODE==1: # Left
			find_set= ((neg_vts, pos_vts, mirror_pairs_l2r), )
		elif PREF_MODE==2: # Right
			find_set= ((pos_vts, neg_vts, mirror_pairs_r2l), )
		
		
		# Do a locational lookup again :/ - This isnt that good form but if we havnt mirrored weights well need to do it anyway.
		# The Difference with this is that we dont need to have 1:1 match for each vert- just get each vert to find another mirrored vert
		# and use its weight.
		# Use  "find_set" so we can do a flipped search L>R and R>L without duplicate code.
		for vtls_A, vtls_B, pair_ls  in  find_set:
			for i1, vA in enumerate(vtls_A):
				best_len=1<<30 # BIGNUM
				best_idx=-1
				
				# Find the BEST match
				vA_co= vA.co
				for i2, vB in enumerate(vtls_B):
					# Enforce edge users.
					if not PREF_EDGE_USERS or edge_users[i1]==edge_users[i2]:
						flipvec[:]= vB.co
						flipvec.x= -flipvec.x
						l= (vA_co-flipvec).length
						
						if l<best_len:
							best_len=l
							best_idx=i2
				
				if best_idx != -1:
					pair_ls.append((vtls_A[i1].index, vtls_B[best_idx].index)) # neg, pos.
		
		# Now we can merge the weights
		if PREF_MODE==0: # Middle
			newVWeightDict= [vWeightDict[i] for i in xrange(len(me.verts))] # Have empty dicts just incase
			for pair_ls in (mirror_pairs_l2r, mirror_pairs_r2l):
				if PREF_FLIP_NAMES:
					for i1, i2 in pair_ls:
						flipWeight, groupNames= BPyMesh.dictWeightFlipGroups( vWeightDict[i2], groupNames, PREF_CREATE_FLIP_NAMES )
						newVWeightDict[i1]= BPyMesh.dictWeightMerge([vWeightDict[i1], flipWeight] )
				else:
					for i1, i2 in pair_ls:
						newVWeightDict[i1]= BPyMesh.dictWeightMerge([vWeightDict[i1], vWeightDict[i2]])
			
			vWeightDict= newVWeightDict
		
		elif PREF_MODE==1: # Left
			if PREF_FLIP_NAMES:
				for i1, i2 in mirror_pairs_l2r:
					vWeightDict[i2], groupNames= BPyMesh.dictWeightFlipGroups(vWeightDict[i1], groupNames, PREF_CREATE_FLIP_NAMES)
			else:
				for i1, i2 in mirror_pairs_l2r:
					vWeightDict[i2]= vWeightDict[i1] # Warning Multiple instances of the same data, its ok in this case but dont modify later.
			
		elif PREF_MODE==2: # Right
			if PREF_FLIP_NAMES:
				for i1, i2 in mirror_pairs_r2l:
					vWeightDict[i2], groupNames= BPyMesh.dictWeightFlipGroups(vWeightDict[i1], groupNames, PREF_CREATE_FLIP_NAMES)
			else:
				for i1, i2 in mirror_pairs_r2l:
					vWeightDict[i2]= vWeightDict[i1] # Warning, ditto above
		
		BPyMesh.dict2MeshWeight(me, groupNames, vWeightDict)
	
	me.update()
	
def main():
	scn = Scene.GetCurrent()
	act_ob= scn.getActiveObject()
	if act_ob.getType()!='Mesh':
		act_ob= None
	
	sel= [ob for ob in Object.GetSelected() if ob.getType()=='Mesh' if ob != act_ob]
	if not sel and not act_ob:
		Draw.PupMenu('Error, select a mesh as your active object')
		return
	
	# Defaults
	PREF_EDITMESH_ONLY= Draw.Create(1)
	PREF_MIRROR_LOCATION= Draw.Create(1)
	PREF_XMID_SNAP= Draw.Create(1)
	PREF_MAX_DIST= Draw.Create(0.02)
	PREF_XZERO_THRESH= Draw.Create(0.002)
	
	#PREF_MODE= Draw.Create(0) # THIS IS TOOO CONFUSING, HAVE 2 BUTTONS AND MAKE THE MODE FROM THEM.
	PREF_MODE_L2R= Draw.Create(1)
	PREF_MODE_R2L= Draw.Create(0)
	
	PREF_SEL_ONLY= Draw.Create(1)
	PREF_EDGE_USERS= Draw.Create(0)
	# Weights
	PREF_MIRROR_WEIGHTS= Draw.Create(0)
	PREF_FLIP_NAMES= Draw.Create(1)
	PREF_CREATE_FLIP_NAMES= Draw.Create(1)
	
	pup_block = [\
	('EditMesh Only', PREF_EDITMESH_ONLY, 'If disabled, will mirror all selected meshes.'),\
	'Left (-), Right (+)',\
	('Left > Right', PREF_MODE_L2R, 'Copy from the Left to Right of the mesh. Enable Both for a mid loc/weight.'),\
	('Right > Left', PREF_MODE_R2L, 'Copy from the Right to Left of the mesh. Enable Both for a mid loc/weight.'),\
	'',\
	('MaxDist:', PREF_MAX_DIST, 0.0, 1.0, 'Generate interpolated verts so closer vert weights can be copied.'),\
	('XZero limit:', PREF_XZERO_THRESH, 0.0, 1.0, 'Mirror verts above this distance from the middle, else lock to X/zero.'),\
	('Sel Verts Only', PREF_SEL_ONLY, 'Only mirror selected verts. Else try and mirror all'),\
	('Edge Users', PREF_EDGE_USERS, 'Only match up verts that have the same number of edge users.'),\
	'Location Prefs',\
	('Mirror Location', PREF_MIRROR_LOCATION, 'Mirror vertex locations.'),\
	('XMidSnap Verts', PREF_XMID_SNAP, 'Snap middle verts to X Zero (uses XZero limit)'),\
	'Weight Prefs',\
	('Mirror Weights', PREF_MIRROR_WEIGHTS, 'Mirror vertex locations.'),\
	('Flip Groups', PREF_FLIP_NAMES, 'Mirror flip names.'),\
	('New Flip Groups', PREF_CREATE_FLIP_NAMES, 'Make new groups for flipped names.'),\
	]
	
	if not Draw.PupBlock("X Mirror mesh tool", pup_block):
		return	
	
	# WORK OUT THE MODE 0
	# PREF_MODE, 0:middle, 1: Left. 2:Right.
	PREF_MODE_R2L= PREF_MODE_R2L.val
	PREF_MODE_L2R= PREF_MODE_L2R.val
	
	if PREF_MODE_R2L and PREF_MODE_L2R:
		PREF_MODE= 0 # Middle
	elif not PREF_MODE_R2L and PREF_MODE_L2R:
		PREF_MODE= 1 # Left to Right
	elif PREF_MODE_R2L and not PREF_MODE_L2R:
		PREF_MODE= 2 # Right to Left
	else: # Neither Selected. Do middle anyway
		PREF_MODE= 0

	
	PREF_EDITMESH_ONLY= PREF_EDITMESH_ONLY.val
	PREF_MIRROR_LOCATION= PREF_MIRROR_LOCATION.val
	PREF_XMID_SNAP= PREF_XMID_SNAP.val
	PREF_MAX_DIST= PREF_MAX_DIST.val
	PREF_XZERO_THRESH= PREF_XZERO_THRESH.val
	PREF_SEL_ONLY= PREF_SEL_ONLY.val
	PREF_EDGE_USERS=  PREF_EDGE_USERS.val
	# weights
	PREF_MIRROR_WEIGHTS= PREF_MIRROR_WEIGHTS.val
	PREF_FLIP_NAMES= PREF_FLIP_NAMES.val
	PREF_CREATE_FLIP_NAMES= PREF_CREATE_FLIP_NAMES.val
	
	t= sys.time()
	
	is_editmode = Window.EditMode() # Exit Editmode.
	if is_editmode: Window.EditMode(0)
	Mesh.Mode(Mesh.SelectModes['VERTEX'])
	Window.WaitCursor(1)
	
	if act_ob:
		mesh_mirror(act_ob.getData(mesh=1), PREF_MIRROR_LOCATION, PREF_XMID_SNAP, PREF_MAX_DIST, PREF_XZERO_THRESH, PREF_MODE, PREF_SEL_ONLY, PREF_EDGE_USERS, PREF_MIRROR_WEIGHTS, PREF_FLIP_NAMES, PREF_CREATE_FLIP_NAMES)
	if (not PREF_EDITMESH_ONLY) and sel:
		for ob in sel:
			mesh_mirror(ob.getData(mesh=1), PREF_MIRROR_LOCATION, PREF_XMID_SNAP, PREF_MAX_DIST, PREF_XZERO_THRESH, PREF_MODE, PREF_SEL_ONLY, PREF_EDGE_USERS, PREF_MIRROR_WEIGHTS, PREF_FLIP_NAMES, PREF_CREATE_FLIP_NAMES)
	
	if is_editmode: Window.EditMode(1)
	Window.WaitCursor(0)
	Window.DrawProgressBar(1.0, '')
	Window.RedrawAll()
	
	print 'Mirror done in %.6f sec.' % (sys.time()-t)

if __name__ == '__main__':
	main()
