import Blender

def meshWeight2Dict(me):
	''' Takes a mesh and return its group names and a list of dicts, one dict per vertex.
	using the group as a key and a float value for the weight.
	These 2 lists can be modified and then used with dict2MeshWeight to apply the changes.
	'''
	
	vWeightDicts= [dict() for i in xrange(len(me.verts))] # Sync with vertlist.
	
	# Clear the vert group.
	groupNames= me.getVertGroupNames()
		
	for group in groupNames:
		for index, weight in me.getVertsFromGroup(group, 1): # (i,w)  tuples.
			vWeightDicts[index][group]= weight
		
	for group in groupNames:
		me.removeVertGroup(group)
	
	return groupNames, vWeightDicts


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
	


#~ # Test normalize. 
#~ if __name__ == '__main__':
	#~ ob= Blender.Scene.GetCurrent().getActiveObject()
	#~ me= ob.getData(mesh=1)
	
	#~ wdct= meshWeight2Dict(me)
	#~ wdct_new= [w.copy() for w in wdct] # Make a copy for the new data. so verts dont get blured unevenly.
	
	#~ '''
	#~ for wv in wdct: # Weight verts.
		#~ for key,val in wv.iteritems():
			#~ wv[key]= val*0.5
	#~ '''
	#~ # Normalize between bones.
	#~ '''
	#~ for wv in wdct: # Weight verts.
		#~ no=0.0
		#~ for val in wv.itervalues():
			#~ no+=val
		
		#~ if no>0:
			#~ for key,val in wv.iteritems():
				#~ wv[key]/=no
	#~ '''
	
	#~ # remove 
	
	
	
	
	#~ '''
	#~ radius= 0.1
	#~ strength=0.5
	#~ # Distance based radial blur,
	#~ vertEdgeUsers= [list() for i in xrange(len(me.verts))]
	
	#~ # Build edge lengths and face users for this data.
	#~ edgeLengths= [(ed.v1.co-ed.v2.co).length for ed in me.edges\
	#~ if vertEdgeUsers[ed.v1.index].append(ed)== None and\
	   #~ vertEdgeUsers[ed.v2.index].append(ed) == None  ]
	
		
		
	#~ for i, vertShared, in enumerate(vertEdgeUsers):
		#~ vert_hub= me.verts[i]
		#~ dummy_weight= {}
		#~ for cnctEd in vertShared:
			#~ if cnctEd.v1==vert_hub:
				#~ cnctVt= cnctEd.v2
			#~ else:
				#~ cnctVt= cnctEd.v1
			
			
			#~ cnct_weight= wdct[cnctVt.index] # copy from, old var
			
			#~ for group, weight in cnct_weight.iteritems():
				#~ w= weight / len(vertShared) # Scale the weight...
				#~ try:
					#~ dummy_weight[group] += w
				#~ except:
					#~ dummy_weight[group] = w
		
		#~ # New add the collected dumy weight to the vertex.
		
		
		#~ length= edgeLengths[cnctEd.index]
		
		#~ if length != 0 and length < radius:
			#~ factor= strength #length/radius # < 1
			#~ factor_inv= 1.0-factor 
			
			#~ # Add the cnctVt's weight to the vert_hub's.
			#~ hub_weight= wdct_new[i] # copy to new var
			#~ cnct_weight= wdct[cnctVt.index] # copy from, old var
			
			#~ for group, weight in dummy_weight.iteritems():
				#~ try:
					#~ hub_weight[group]= ((hub_weight[group]*factor) + (weight*factor_inv)) * 0.9
				#~ except:
					#~ hub_weight[group]= (weight*factor_inv)* 0.9
			
			#~ for group, weight in hub_weight.iteritems():
				#~ try:
					#~ dummy_weight[group]
				#~ except:
					#~ hub_weight[group]= weight*factor
	#~ '''
	#~ dict2MeshWeight(me, wdct_new)
	
	
	
	