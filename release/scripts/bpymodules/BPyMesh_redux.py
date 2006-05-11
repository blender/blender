import Blender
Vector= Blender.Mathutils.Vector
Ang= Blender.Mathutils.AngleBetweenVecs
LineIntersect= Blender.Mathutils.LineIntersect
import BPyMesh


#import psyco
#psyco.full()

def uv_key(uv):
	return round(uv.x, 5), round(uv.y, 5)
	
def ed_key(ed):
	i1= ed.v1.index
	i2= ed.v2.index
	if i1<i2: return i1,i2
	return i2,i1
	
def redux(ob, factor=0.5):
	me= ob.getData(mesh=1)
	# BUG MUST REMOVE GROUPS 
	if factor>1.0 or factor<0.0 or len(me.faces)<4:
		return
	
	OLD_MESH_MODE= Blender.Mesh.Mode()
	Blender.Mesh.Mode(Blender.Mesh.SelectModes.VERTEX)
	
	target_face_count= int(len(me.faces) * factor)
	# % of the collapseable faces to collapse per pass.
	collapse_per_pass= 0.333 # between 0.1 - lots of small nibbles, slow but high q. and 0.9 - big passes and faster.
	
	
	for v in me.verts:
		v.hide=0
	
	while target_face_count <= len(me.faces):
		BPyMesh.meshCalcNormals(me)
		
		#groupNames, vWeightDict= meshWeight2Dict(act_me)
		
		# Select all verts, de-select as you collapse.
		for v in me.verts:
			v.sel=0
			
		
		# Store new locations for collapsed edges here
		edge_new_locations= [None] * len(me.edges)
		
		
		# For collapsing uv coords, we need to store the uv coords of edges as used by face users.
		# each dict key is min/max edge vert indicies
		# each value is a tuple of 2 lists, v1 uv's  and  v2 uvs. and the last list is for faces.
		# VALUE
		# edge_key : [faces, v1uvs, v2uvs] 
		# faces, v1uvs, v2uvs - are all in synk.
		edge_faces_and_uvs= dict([ (ed_key(ed), ([], [], [])) for ed in me.edges])
		
		
		# Store verts edges.
		vert_ed_users= [[] for i in xrange(len(me.verts))]
		for ed in me.edges:
			vert_ed_users[ed.v1.index].append(ed)
			vert_ed_users[ed.v2.index].append(ed)
		
		# Store face users
		vert_face_users= [[] for i in xrange(len(me.verts))]
		
		for f in me.faces:
			
			#f.uvSel= uvs
			for i, v1 in enumerate(f.v):
				vert_face_users[v1.index].append( (i,f) )
				
				# add the uv coord to the vert
				v2 = f.v[i-1]
				i1= v1.index
				i2= v2.index
				
				if i1>i2:
					edge_face_list, edge_v2_uvs, edge_v1_uvs= edge_faces_and_uvs[i2,i1]
				else:
					edge_face_list, edge_v1_uvs, edge_v2_uvs= edge_faces_and_uvs[i1,i2]
				
				edge_face_list.append(f)
				if me.faceUV:
					edge_v1_uvs.append( uv_key(f.uv[i  ]) )
					edge_v2_uvs.append( uv_key(f.uv[i-1]) )
		
		
		
		'''
		face_normals= [f.no for f in me.faces]
		face_areas= [f.area for f in me.faces]
		
		# Best method, no quick hacks here
		def ed_test_collapse_error(ed):
			
			i1= ed.v1.index
			i2= ed.v1.index
			test_faces= set()
			for i in (i1,i2):
				test_faces.union( set([f[1].index for f in vert_face_users[i]]) )
			
			# 
			test_faces= test_faces - set( [ f.index for f in edge_faces_and_uvs[ed_key(ed)][0] ] )
			
			# test_faces is now faces used by ed.v1 and ed.v2 that will not be removed in the collapse.
			
			orig_nos= [face_normals.normal for i in test_faces]
			
			v1_orig= Vector(ed.v1.co)
			v2_orig= Vector(ed.v2.co)
			
			ed.v1.co= ed.v2.co= (v1_orig+v2_orig) * 0.5
			
			new_nos= [face_normals.normal for i in test_faces]
			
			ed.v1.co= v1_orig
			ed.v2.co= v2_orig
			
			# now see how bad the normals are effected
			angle_diff= 0
			
			for i in test_faces:
				try:
					angle_diff+= (Ang(orig_nos[i], new_nos[i])/180) * face_areas[i]
				except:
					pass
			# This is very arbirary, feel free to modify
			return angle_diff * ((ed.v1.no - ed.v2.no).length * ed.length)
			
		'''
		
		# Store egde lengths - Used 
		# edge_lengths= [ed.length for ed in me.edges]
		
		# Better method of weighting - edge length * normal difference.
		edge_lengths= [ed.length * (1 + ((ed.v1.no-ed.v2.no).length**2) ) for ed in me.edges]
		
		# tricky but somehow looks crap!!
		#edge_lengths= [ed_test_collapse_error(ed) for ed in me.edges]
		
		
		# Wont use the function again.
		#del ed_test_collapse_error
		
		# BOUNDRY CHECKING AND WEIGHT EDGES. CAN REMOVE
		# Now we know how many faces link to an edge. lets get all the boundry verts
		verts_boundry= [0]*len(me.verts)
		for ed_idxs, faces_and_uvs in edge_faces_and_uvs.iteritems():
			if len(faces_and_uvs[0]) < 2:
				verts_boundry[ed_idxs[0]]= 1
				verts_boundry[ed_idxs[1]]= 1
		
		for i, ed in enumerate(me.edges):
			if verts_boundry[ed.v1.index] != verts_boundry[ed.v2.index]:
				# Edge has 1 boundry and 1 non boundry vert. weight higher
				edge_lengths[i]*=2
		del verts_boundry
		# END BOUNDRY. Can remove
		
		# sort by edge length
		# sorted edge lengths
		edge_container_list= [ (edge_lengths[i], ed) for i, ed in enumerate(me.edges) ]
		edge_container_list.sort() # edges will be used for sorting
		
		# Make a list of the first half edges we can collapse,
		# these will better edges to remove.
		edge_container_list_collapse= []
		for length, ed in edge_container_list:
			#print 'Heho', len(me.faces)- current_removed_faces, target_face_count
			i= ed.index
			
			v1= ed.v1
			v2= ed.v2
			# Use vert selections 
			if v1.sel or v2.sel:
				pass
				
			else:
				# Now we know the verts havnyt been collapsed.
				v1.sel= v2.sel= 1 # Dont collapse again.
				edge_container_list_collapse.append((length, ed))
		
		# Get a subset of the entire list- the first "collapse_per_pass", that are best to collapse.
		if len(edge_container_list_collapse) > 4:
			edge_container_list_collapse = edge_container_list_collapse[:int(len(edge_container_list_collapse)*collapse_per_pass)]
		
		
		# We know edge_container_list_collapse can be removed.
		for length, ed in edge_container_list_collapse:
			#print 'Heho', len(me.faces)- current_removed_faces, target_face_count
			i= ed.index
			
			v1= ed.v1
			v2= ed.v2
			
			edge_face_list, edge_v2_uvs, edge_v1_uvs= edge_faces_and_uvs[ed_key(ed)]
			#current_removed_faces += len(edge_face_list) # dosent work for quads.
			
			if me.faceUV:
				for v, edge_my_uvs, edge_other_uvs in ((v2, edge_v1_uvs, edge_v2_uvs),(v1, edge_v2_uvs, edge_v1_uvs)):
					for face_vert_index, f in vert_face_users[v.index]:
						# we have a face and 
						uvk= uv_key(f.uv[face_vert_index])
						
						uv_index= None
						try:
							uv_index= edge_my_uvs.index(uvk)
						except ValueError:
							pass
						
						if uv_index != None:
							# This face uses a uv in the collapsing face. - do a merge
							other_uv= edge_other_uvs[uv_index]
							uv_vec= f.uv[face_vert_index]
							uv_vec.x= (uvk[0] + other_uv[0])*0.5
							uv_vec.y= (uvk[1] + other_uv[1])*0.5
			
			# Collapse
			between= (v1.co + v2.co) * 0.5
			# new_location = between # Replace tricky code below
			
			# Collect edges from the faces that use this edge- dont use these in the new collapsed ver locatioin calc
			exclude_edges= set()
			for f in edge_face_list:
				for ii, v in enumerate(f.v):
					i1= v.index
					i2= f.v[ii-1].index
					if i1>i2:
						i1,i2= i2,i1
					
					exclude_edges.add((i1,i2))
			
			# move allong the combine normal of both 
			# make a normal thats no longer then the edge length
			nor= v1.no + v2.no
			nor.normalize()
			nor= nor*length
			
			new_location= Vector()
			new_location_count =0 
			
			# make a line we can do intersection with.
			for v in (ed.v1, ed.v2):
				for ed_user in vert_ed_users[v.index]:
					if ed_user != ed and ed_key(ed_user) not in exclude_edges: 
						ed_between= (ed_user.v1.co+ed_user.v2.co) * 0.5
						v1_scale= ed_between + ((ed_user.v1.co-ed_between) * 100)
						v2_scale= ed_between + ((ed_user.v2.co-ed_between) * 100)
						line1x, line2x= LineIntersect(between-nor, between+nor, v1_scale, v2_scale)
						
						new_location_count += 1
						new_location+= line1x
						
			if not new_location_count:
				new_location= between
			else:
				new_location= new_location * (1.0/new_location_count)
				new_location = (new_location + between) * 0.5
				if new_location.x!=new_location.x:
					# NAN
					new_location= between
			# NEW NEW LOCATUON
			
			# Store the collapse location to apply later
			edge_new_locations[i] = new_location
			
				
		# Execute the collapse
		for i , ed in enumerate(me.edges):
			loc= edge_new_locations[i]
			if loc:
				v1= ed.v1
				v2= ed.v2
				v1.co= v2.co=  loc
		
		doubles= me.remDoubles(0.0001) 
		# print 'doubles', doubles
		me= ob.getData(mesh=1)
		if doubles==0:
			break
	
	# Cleanup.
	'''
	vert_face_user_count= [0]*len(me.verts)
	for f in me.faces:
		for v in f.v:
			vert_face_user_count[v.index] +=1
	
	del_verts= [i for i in xrange(len(me.verts)) if not vert_face_user_count[i]]
	me.verts.delete( del_verts )
	'''
	me.update()
	Blender.Mesh.Mode(OLD_MESH_MODE)


# Example usage
def main():
	Blender.Window.EditMode(0)
	scn= Blender.Scene.GetCurrent()
	active_ob= scn.getActiveObject()
	
	redux(active_ob, 0.5)

if __name__=='__main__':
	main()
