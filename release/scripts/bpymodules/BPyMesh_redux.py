import Blender
Vector= Blender.Mathutils.Vector
# Ang= Blender.Mathutils.AngleBetweenVecs
LineIntersect= Blender.Mathutils.LineIntersect
import BPyMesh

try:
	import psyco
	psyco.full()
except:
	pass

def uv_key(uv):
	return round(uv.x, 5), round(uv.y, 5)
	
def ed_key(ed):
	i1= ed.v1.index
	i2= ed.v2.index
	if i1<i2: return i1,i2
	return i2,i1

def col_key(col):
	return col.r, col.g, col.b

class collapseEdge(object):
	__slots__ = 'length', 'key', 'faces', 'collapse_loc', 'v1', 'v2','uv1', 'uv2', 'col1', 'col2', 'collapse_weight'
	def __init__(self, ed):
		self.key= ed_key(ed)
		self.length= ed.length
		self.faces= []
		self.v1= ed.v1
		self.v2= ed.v2
		self.uv1= []
		self.uv2= []
		self.col1= []
		self.col2= []
		self.collapse_loc= None # new collapse location.
		self.collapse_weight= self.length *  (1+ ((ed.v1.no-ed.v2.no).length**2))



def redux(ob, factor=0.5):
	me= ob.getData(mesh=1)
	# BUG MUST REMOVE GROUPS 
	if factor>1.0 or factor<0.0 or len(me.faces)<4:
		return
	
	OLD_MESH_MODE= Blender.Mesh.Mode()
	Blender.Mesh.Mode(Blender.Mesh.SelectModes.VERTEX)
	
	faceUV= me.faceUV
	
	target_face_count= int(len(me.faces) * factor)
	# % of the collapseable faces to collapse per pass.
	collapse_per_pass= 0.333 # between 0.1 - lots of small nibbles, slow but high q. and 0.9 - big passes and faster.
	
	for v in me.verts:
		v.hide=0
	
	while target_face_count <= len(me.faces):
		BPyMesh.meshCalcNormals(me)
		
		# Backup colors
		if me.faceUV:
			orig_texface= [[(uv_key(f.uv[i]), col_key(f.col[i])) for i in xrange(len(f.v))] for f in me.faces]
		
		collapse_edges= [collapseEdge(ed) for ed in me.edges]
		collapse_edges_dict= dict( [(ce.key, ce) for ce in collapse_edges] )
		del ed
		
		# Store verts edges.
		vert_ed_users= [[] for i in xrange(len(me.verts))]
		for ced in collapse_edges:
			vert_ed_users[ced.v1.index].append(ced)
			vert_ed_users[ced.v2.index].append(ced)
		
		# Store face users
		vert_face_users= [[] for i in xrange(len(me.verts))]
		
		for ii, f in enumerate(me.faces):
			f_v= f.v
			if faceUV:
				tex_keys= orig_texface[ii]

			for i, v1 in enumerate(f_v):
				vert_face_users[v1.index].append( (i,f) )
				
				# add the uv coord to the vert
				v2 = f_v[i-1]
				i1= v1.index
				i2= v2.index
				
				if i1>i2: ced= collapse_edges_dict[i2,i1]
				else: ced= collapse_edges_dict[i1,i2]
				
				ced.faces.append(f)
				if faceUV:
					ced.uv1.append( tex_keys[i][0] )
					ced.uv2.append( tex_keys[i-1][0] )
					
					ced.col1.append( tex_keys[i][1] )
					ced.col2.append( tex_keys[i-1][1] )
		
		'''
		face_normals= [f.no for f in me.faces]
		face_areas= [f.area for f in me.faces]
		
		# Best method, no quick hacks here, Correction. Should be the best but needs tweaks.
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
		# edge_lengths= [ed.length * (1 + ((ed.v1.no-ed.v2.no).length**2) ) for ed in me.edges]
		
		# tricky but somehow looks crap!!
		#edge_lengths= [ed_test_collapse_error(ed) for ed in me.edges]
		
		
		# Wont use the function again.
		#del ed_test_collapse_error
		
		# BOUNDRY CHECKING AND WEIGHT EDGES. CAN REMOVE
		# Now we know how many faces link to an edge. lets get all the boundry verts
		verts_boundry= [1]*len(me.verts)
		#for ed_idxs, faces_and_uvs in edge_faces_and_uvs.iteritems():
		for ced in collapse_edges:
			if len(ced.faces) < 2:
				verts_boundry[ced.key[0]]= 2
				verts_boundry[ced.key[1]]= 2
		
		for ced in collapse_edges:
			if verts_boundry[ced.v1.index] != verts_boundry[ced.v2.index]:
				# Edge has 1 boundry and 1 non boundry vert. weight higher
				ced.collapse_weight*=2
		
		
		vert_collapsed= verts_boundry
		del verts_boundry
		# END BOUNDRY. Can remove
		
		# sort by collapse weight
		collapse_edges.sort(lambda ced1, ced2: cmp(ced1.collapse_weight, ced2.collapse_weight)) # edges will be used for sorting
		
		# Make a list of the first half edges we can collapse,
		# these will better edges to remove.
		collapse_count=0
		for ced in collapse_edges:
			v1= ced.v1
			v2= ced.v2
			# Use vert selections 
			if vert_collapsed[v1.index]==0 or vert_collapsed[v2.index]==0:
				pass
			else:
				# Now we know the verts havnyt been collapsed.
				vert_collapsed[v1.index]= vert_collapsed[v2.index]= 0 # Dont collapse again.
				collapse_count+=1
		
		# Get a subset of the entire list- the first "collapse_per_pass", that are best to collapse.
		if collapse_count > 4:
			collapse_count = int(collapse_count*collapse_per_pass)
		
		# We know edge_container_list_collapse can be removed.
		for ced in collapse_edges:
			collapse_count-=1
			if not collapse_count:
				break
			v1= ced.v1
			v2= ced.v2
			
			#edge_face_list, edge_v2_uvs, edge_v1_uvs= edge_faces_and_uvs[ed_key(ed)]
			#current_removed_faces += len(edge_face_list) # dosent work for quads.
			
			if faceUV:
				for v, edge_my_uvs, edge_other_uvs, edge_my_cols, edge_other_cols in ((v2, ced.uv1, ced.uv2, ced.col1, ced.col2),(v1, ced.uv2, ced.uv1, ced.col2, ced.col1)):
					for face_vert_index, f in vert_face_users[v.index]:
						uvk, colk = orig_texface[f.index][face_vert_index]
						# UV COORDS
						tex_index= None
						try:
							tex_index= edge_my_uvs.index(uvk)
						except ValueError:
							pass
						
						if tex_index != None:
							# This face uses a uv in the collapsing face. - do a merge
							other_uv= edge_other_uvs[tex_index]
							uv_vec= f.uv[face_vert_index]
							uv_vec.x= (uvk[0] + other_uv[0])*0.5
							uv_vec.y= (uvk[1] + other_uv[1])*0.5
						
						# TEXFACE COLOURS
						#colk = col_key(f.col[face_vert_index])
						
						tex_index= None
						try:
							tex_index= edge_my_cols.index(colk)
						except ValueError:
							pass
						
						if tex_index != None:
							# Col
							other_col= edge_other_cols[tex_index]
							col_ob= f.col[face_vert_index]
							col_ob.r = int((colk[0] + other_col[0])*0.5)
							col_ob.g = int((colk[1] + other_col[1])*0.5)
							col_ob.b = int((colk[2] + other_col[2])*0.5)
			
			# Collapse
			between= (v1.co + v2.co) * 0.5
			# new_location = between # Replace tricky code below
			
			# Collect edges from the faces that use this edge- dont use these in the new collapsed ver locatioin calc
			exclude_edges= set()
			for f in ced.faces:
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
			nor= nor*ced.length
			
			new_location= Vector()
			new_location_count =0 
			
			# make a line we can do intersection with.
			for v in (ced.v1, ced.v2):
				for ed_user in vert_ed_users[v.index]:
					if ed_user != ced and ed_user.key not in exclude_edges: 
						ed_between= (ed_user.v1.co+ed_user.v2.co) * 0.5
						v1_scale= ed_between + ((ed_user.v1.co-ed_between) * 100)
						v2_scale= ed_between + ((ed_user.v2.co-ed_between) * 100)
						line_xs= LineIntersect(between-nor, between+nor, v1_scale, v2_scale)
						if line_xs: # did we intersect? - None if we didnt
							new_location_count += 1
							new_location+= line_xs[0]
						
			# Failed to generate a new location or x!=X (NAN)
			# or, out new location is crazy and further away from the edge center then the edge length.
			if not new_location_count or\
			new_location.x!=new_location.x or\
			(new_location-between).length > (ced.length/2):
				new_location= between
			else:
				new_location= new_location * (1.0/new_location_count)
				new_location = (new_location + between) * 0.5
				
			# Store the collapse location to apply later
			ced.collapse_loc = new_location
		
		# Execute the collapse
		for ced in collapse_edges:
			# Since the list is ordered we can stop once the first non collapsed edge if sound.
			if not ced.collapse_loc:
				break
			
			ced.v1.co= ced.v2.co=  ced.collapse_loc
		
		doubles= me.remDoubles(0.0001) 
		me= ob.getData(mesh=1)
		if doubles==0: # should never happen.
			break
	
	# Cleanup. BUGGY?
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
	t= Blender.sys.time()
	redux(active_ob, 0.5)
	print '%.4f' % (Blender.sys.time()-t)

if __name__=='__main__':
	main()
