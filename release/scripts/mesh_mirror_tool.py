#!BPY
"""
Name: 'Mirror Verts Loc & Weight'
Blender: 241
Group: 'Mesh'
Tooltip: 'Move verts so they snap to their mirrored locations.'
"""

from Blender import Draw, Window, Scene, Mesh, Mathutils, sys
import BPyMesh
reload(BPyMesh)

def mesh_mirror(me, PREF_MIRROR_LOCATION, PREF_MAX_DIST, PREF_MODE, PREF_NOR_WEIGHT, PREF_SEL_ONLY, PREF_EDGE_USERS, PREF_MIRROR_WEIGHTS, PREF_FLIP_NAMES, PREF_CREATE_FLIP_NAMES):
	'''
	PREF_MAX_DIST, Maximum distance to test snapping verts.
	PREF_MODE, 0:middle, 1: Left. 2:Right.
	PREF_NOR_WEIGHT, use normal angle difference to weight distances.
	PREF_SEL_ONLY, only snap the selection
	PREF_EDGE_USERS, match only verts with the same number of edge users.
	PREF_MIRROR_LOCATION, 
	'''
	
	is_editmode = Window.EditMode() # Exit Editmode.
	if is_editmode: Window.EditMode(0)
	Window.WaitCursor(1)
	Mesh.Mode(Mesh.SelectModes['VERTEX'])
	
	# Operate on all verts
	if not PREF_SEL_ONLY:
		for v in me.verts:
			v.sel=1
	
	
	if PREF_EDGE_USERS:
		edge_users= [0]*len(me.verts)
		for ed in me.edges:
			edge_users[ed.v1.index]+=1
			edge_users[ed.v2.index]+=1
	
	
	neg_vts = [v for v in me.verts if v.sel and v.co.x >  0.000001]
	pos_vts = [v for v in me.verts if v.sel and v.co.x < -0.000001]
	
	
	#*Mirror Location*********************************************************#
	if PREF_MIRROR_LOCATION:
		mirror_pairs= []
		# allign the negative with the positive.
		flipvec= Mathutils.Vector()
		len_neg_vts= float(len(neg_vts))
		for i1, nv in enumerate(neg_vts):
			nv_co= nv.co
			for i2, pv in enumerate(pos_vts):
				# Enforce edge users.
				if not PREF_EDGE_USERS or edge_users[i1]==edge_users[i2]:
					flipvec[:]= pv.co
					flipvec.x= -flipvec.x
					l= (nv_co-flipvec).length
					
					# Record a match.
					if l<=PREF_MAX_DIST:
						
						# We can adjust the length by the normal, now we know the length is under the limit.
						if PREF_NOR_WEIGHT>0:
							# Get the normal and flipm reuse flipvec
							flipvec[:]= pv.no
							flipvec.x= -flipvec.x
							try:
								ang= Mathutils.AngleBetweenVecs(nv.no, flipvec)/180.0
							except: # on rare occasions angle between vecs will fail.- zero length vec.
								ang= 0
							
							l=l*(1+(ang*PREF_NOR_WEIGHT))
						
						mirror_pairs.append((l, nv, pv))
			
			# Update every 20 loops
			if i1 % 10 == 0:
				Window.DrawProgressBar(0.8 * (i1/len_neg_vts), 'Mirror verts %i of %i' % (i1, len_neg_vts))
		
		Window.DrawProgressBar(0.9, 'Mirror verts: Updating locations')
		# Now we have a list of the pairs we might use, lets find the best and do them first.
		# de-selecting as we go. so we can makke sure not to mess it up.
		mirror_pairs.sort(lambda a,b: cmp(a[0], b[0]))
		
		for dist, v1,v2 in mirror_pairs: # dist, neg, pos
			if v1.sel and v2.sel:
				if PREF_MODE==0: # Middle
					flipvec[:]= v2.co # positive
					flipvec.x= -flipvec.x # negatve
					v2.co[:]= v1.co[:]= (flipvec+v1.co)*0.5 # midway
					v2.co.x= -v2.co.x
				elif PREF_MODE==2: # Left
					v2.co[:]= v1.co
					v2.co.x= -v2.co.x
				elif PREF_MODE==1: # Right
					v1.co[:]= v2.co
					v1.co.x= -v1.co.x
				v1.sel= 0
				v2.sel= 0
	
	#*Mirror Weights**********************************************************#
	if PREF_MIRROR_WEIGHTS:
		
		groupNames, vWeightDict= BPyMesh.meshWeight2Dict(me)
		mirror_pairs_l2r= [] # Stor a list of matches for these verts.
		mirror_pairs_r2l= [] # Stor a list of matches for these verts.
		
		# allign the negative with the positive.
		flipvec= Mathutils.Vector()
		len_neg_vts= float(len(neg_vts))
		
		if PREF_MODE==0: # Middle
			find_set= ((neg_vts, pos_vts, mirror_pairs_l2r), (pos_vts, neg_vts, mirror_pairs_r2l))
		elif PREF_MODE==1: # Left
			find_set= ((neg_vts, pos_vts, mirror_pairs_l2r), )
		elif PREF_MODE==2: # Right
			find_set= ((pos_vts, neg_vts, mirror_pairs_r2l), )
			
		for vtls_A, vtls_B, pair_ls in ((neg_vts, pos_vts, mirror_pairs_l2r), (pos_vts, neg_vts, mirror_pairs_r2l)):
			for i1, vA in enumerate(vtls_A):
				best_len=1<<30
				best_idx=-1
				
				# Find the BEST match.
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
					vWeightDict[i2]= vWeightDict[i1] # Warning Multiple instances of the same data
			
		elif PREF_MODE==2: # Right
			if PREF_FLIP_NAMES:
				for i1, i2 in mirror_pairs_r2l:
					vWeightDict[i2], groupNames= BPyMesh.dictWeightFlipGroups(vWeightDict[i1], groupNames, PREF_CREATE_FLIP_NAMES)
			else:
				for i1, i2 in mirror_pairs_r2l:
					vWeightDict[i2]= vWeightDict[i1]
		
		BPyMesh.dict2MeshWeight(me, groupNames, vWeightDict)
	
	me.update()
	
	if is_editmode: Window.EditMode(1)
	Window.WaitCursor(0)
	Window.DrawProgressBar(1.0, '')
	Window.RedrawAll()
	
def main():
	try:
		scn = Scene.GetCurrent()
		ob= scn.getActiveObject()
		me= ob.getData(mesh=1)
	except:
		Draw.PupMenu('Error, select a mesh as your active object')
	
	# Defaults
	PREF_MIRROR_LOCATION= Draw.Create(1)
	PREF_MAX_DIST= Draw.Create(0.2)
	PREF_MODE= Draw.Create(0)
	PREF_NOR_WEIGHT= Draw.Create(0.0)
	PREF_SEL_ONLY= Draw.Create(1)
	PREF_EDGE_USERS= Draw.Create(0)
	# Weights
	PREF_MIRROR_WEIGHTS= Draw.Create(0)
	PREF_FLIP_NAMES= Draw.Create(1)
	PREF_CREATE_FLIP_NAMES= Draw.Create(1)
	
	pup_block = [\
	('MaxDist:', PREF_MAX_DIST, 0.0, 1.0, 'Generate interpolated verts so closer vert weights can be copied.'),\
	('Mode:', PREF_MODE, 0, 2, 'New Location/Weight (0:AverageL/R, 1:Left>Right 2:Right>Left)'),\
	('NorWeight:', PREF_NOR_WEIGHT, 0.0, 1.0, 'Generate interpolated verts so closer vert weights can be copied.'),\
	('Sel Only', PREF_SEL_ONLY, 'Only mirror selected verts. Else try and mirror all'),\
	('Edge Users', PREF_EDGE_USERS, 'Only match up verts that have the same number of edge users.'),\
	'Locations',\
	('Mirror Location', PREF_MIRROR_LOCATION, 'Mirror vertex locations.'),\
	'Weights',\
	('Mirror Weights', PREF_MIRROR_WEIGHTS, 'Mirror vertex locations.'),\
	('Flip Groups', PREF_FLIP_NAMES, 'Mirror flip names.'),\
	('New Flip Groups', PREF_CREATE_FLIP_NAMES, 'Make new groups for flipped names.'),\
	]
	
	if not Draw.PupBlock("Mirror mesh tool", pup_block):
		return
	
	PREF_MIRROR_LOCATION= PREF_MIRROR_LOCATION.val
	PREF_MAX_DIST= PREF_MAX_DIST.val
	PREF_MODE= PREF_MODE.val
	PREF_NOR_WEIGHT= PREF_NOR_WEIGHT.val
	PREF_SEL_ONLY= PREF_SEL_ONLY.val
	PREF_EDGE_USERS=  PREF_EDGE_USERS.val
	# weights
	PREF_MIRROR_WEIGHTS= PREF_MIRROR_WEIGHTS.val
	PREF_FLIP_NAMES= PREF_FLIP_NAMES.val
	PREF_CREATE_FLIP_NAMES= PREF_CREATE_FLIP_NAMES.val
	
	t= sys.time()
	mesh_mirror(me, PREF_MIRROR_LOCATION, PREF_MAX_DIST, PREF_MODE, PREF_NOR_WEIGHT, PREF_SEL_ONLY, PREF_EDGE_USERS, PREF_MIRROR_WEIGHTS, PREF_FLIP_NAMES, PREF_CREATE_FLIP_NAMES)
	print 'Mirror done in %.6f sec.' % (sys.time()-t)

if __name__ == '__main__':
	main()