import Blender
Vector= Blender.Mathutils.Vector
LineIntersect= Blender.Mathutils.LineIntersect
CrossVecs= Blender.Mathutils.CrossVecs
import BPyMesh


def uv_key(uv):
	return round(uv.x, 5), round(uv.y, 5)
	
def ed_key(ed):
	i1= ed.v1.index
	i2= ed.v2.index
	if i1<i2: return i1,i2
	return i2,i1

def col_key(col):
	return col.r, col.g, col.b

def redux(ob, REDUX=0.5, BOUNDRY_WEIGHT=2.0, FACE_AREA_WEIGHT=1.0, FACE_TRIANGULATE=True):
	'''
	BOUNDRY_WEIGHT - 0 is no boundry weighting. 2.0 will make them twice as unlikely to collapse.
	FACE_AREA_WEIGHT - 0 is no weight. 1 is normal, 2.0 is higher.
	'''
	
	me= ob.getData(mesh=1)
	
	if REDUX>1.0 or REDUX<0.0 or len(me.faces)<4:
		return
	
	if FACE_TRIANGULATE:
		me.quadToTriangle() 
	
	OLD_MESH_MODE= Blender.Mesh.Mode()
	Blender.Mesh.Mode(Blender.Mesh.SelectModes.VERTEX)
	
	faceUV= me.faceUV
	current_face_count= len(me.faces)
	target_face_count= int(current_face_count * REDUX)
	# % of the collapseable faces to collapse per pass.
	#collapse_per_pass= 0.333 # between 0.1 - lots of small nibbles, slow but high q. and 0.9 - big passes and faster.
	collapse_per_pass= 0.333 # between 0.1 - lots of small nibbles, slow but high q. and 0.9 - big passes and faster.
	
	
	class collapseEdge(object):
		__slots__ = 'length', 'key', 'faces', 'collapse_loc', 'v1', 'v2','uv1', 'uv2', 'col1', 'col2', 'collapse_weight'
		def __init__(self, ed):
			self.key= ed_key(ed)
			self.length= ed.length
			self.faces= []
			self.v1= ed.v1
			self.v2= ed.v2
			if faceUV:
				self.uv1= []
				self.uv2= []
				self.col1= []
				self.col2= []
				# self.collapse_loc= None # new collapse location.
				
				# Basic weighting.
				#self.collapse_weight= self.length *  (1+ ((ed.v1.no-ed.v2.no).length**2))

	class collapseFace(object):
		__slots__ = 'verts', 'normal', 'area', 'index', 'orig_uv', 'orig_col', 'uv', 'col'
		def __init__(self, f):
			self.verts= f.v
			self.normal= f.no
			self.area= f.area
			self.index= f.index
			if faceUV:
				self.orig_uv= [uv_key(uv) for uv in f.uv]
				self.orig_col= [col_key(col) for col in f.col]
				self.uv= f.uv
				self.col= f.col
	
	
	
	for v in me.verts:
		v.hide=0
	
	collapse_edges= collapse_faces= None
	
	while target_face_count <= len(me.faces):
		BPyMesh.meshCalcNormals(me)
		
		for v in me.verts:
			v.sel= False
		
		# Backup colors
		if faceUV:
			orig_texface= [[(uv_key(f.uv[i]), col_key(f.col[i])) for i in xrange(len(f.v))] for f in me.faces]
		
		collapse_faces= [collapseFace(f) for f in me.faces]
		collapse_edges= [collapseEdge(ed) for ed in me.edges]
		collapse_edges_dict= dict( [(ced.key, ced) for ced in collapse_edges] )
		
		# Store verts edges.
		vert_ed_users= [[] for i in xrange(len(me.verts))]
		for ced in collapse_edges:
			vert_ed_users[ced.v1.index].append(ced)
			vert_ed_users[ced.v2.index].append(ced)
		
		# Store face users
		vert_face_users= [[] for i in xrange(len(me.verts))]
		
		# Have decieded not to use this. area is better.
		#face_perim= [0.0]* len(me.faces)
		
		for ii, cfa in enumerate(collapse_faces):
			for i, v1 in enumerate(cfa.verts):
				vert_face_users[v1.index].append( (i,cfa) )
				
				# add the uv coord to the vert
				v2 = cfa.verts[i-1]
				i1= v1.index
				i2= v2.index
				
				if i1>i2: ced= collapse_edges_dict[i2,i1]
				else: ced= collapse_edges_dict[i1,i2]
				
				ced.faces.append(cfa)
				if faceUV:
					ced.uv1.append( cfa.orig_uv[i] )
					ced.uv2.append( cfa.orig_uv[i-1] )
					
					ced.col1.append( cfa.orig_col[i] )
					ced.col2.append( cfa.orig_col[i-1] )
				
				# PERIMITER
				#face_perim[ii]+= ced.length
		
		
		def ed_set_collapse_loc(ced):
			v1co= ced.v1.co
			v2co= ced.v2.co
			v1no= ced.v1.co
			v2no= ced.v2.co
			length= ced.length
			# Collapse
			# new_location = between # Replace tricky code below. this code predicts the best collapse location.
			
			# Make lines at right angles to the normals- these 2 lines will intersect and be
			# the point of collapsing.
			
			# Enlarge so we know they intersect:  ced.length*2
			cv1= CrossVecs(v1no, CrossVecs(v1no, v1co-v2co))
			cv2= CrossVecs(v2no, CrossVecs(v2no, v2co-v1co))
			
			# Scale to be less then the edge lengths.
			cv1.normalize()
			cv2.normalize()
			cv1 = cv1 * length* 0.333
			cv2 = cv2 * length* 0.333
			
			ced.collapse_loc = ((v1co + v2co) * 0.5) + (cv1 + cv2)
		
		
		# Best method, no quick hacks here, Correction. Should be the best but needs tweaks.
		def ed_set_collapse_error(ced):
			Ang= Blender.Mathutils.AngleBetweenVecs
			
			i1= ced.v1.index
			i2= ced.v1.index
			test_faces= set()
			
			for i in (i1,i2):
				for f in vert_face_users[i]:
					test_faces.add(f[1].index)
			
			for f in ced.faces:
				test_faces.remove(f.index)
			
			test_faces= tuple(test_faces) # keep order
			
			# test_faces is now faces used by ed.v1 and ed.v2 that will not be removed in the collapse.
			# orig_nos= [face_normals[i] for i in test_faces]
			
			v1_orig= Vector(ced.v1.co)
			v2_orig= Vector(ced.v2.co)
			
			ced.v1.co= ced.v2.co= ced.collapse_loc
			
			new_nos= [me.faces[i].no for i in test_faces]
			
			ced.v1.co= v1_orig
			ced.v2.co= v2_orig
			
			# now see how bad the normals are effected
			angle_diff= 1.0
			
			for ii, i in enumerate(test_faces): # local face index, global face index
				cfa= collapse_faces[i] # this collapse face
				try:
					# can use perim, but area looks better.
					if FACE_AREA_WEIGHT:
						angle_diff+= (Ang(cfa.normal, new_nos[ii])/180) * (1+(cfa.area * FACE_AREA_WEIGHT)) # 4 is how much to influence area
					else:
						angle_diff+= (Ang(cfa.normal, new_nos[ii])/180)
						
				except:
					pass
			
			# This is very arbirary, feel free to modify
			try:
				no_ang= (Ang(ced.v1.no, ced.v2.no)/180) + 1
			except:
				no_ang= 2.0
			ced.collapse_weight=  (no_ang * ced.length) * (1-(1/angle_diff))# / max(len(test_faces), 1)
		
		# We can calculate the weights on __init__ but this is higher qualuity.
		for ced in collapse_edges:
			ed_set_collapse_loc(ced)
			ed_set_collapse_error(ced)
		
		# Wont use the function again.
		del ed_set_collapse_error
		del ed_set_collapse_loc
		
		
		# BOUNDRY CHECKING AND WEIGHT EDGES. CAN REMOVE
		# Now we know how many faces link to an edge. lets get all the boundry verts
		if BOUNDRY_WEIGHT > 0:
			verts_boundry= [1] * len(me.verts)
			#for ed_idxs, faces_and_uvs in edge_faces_and_uvs.iteritems():
			for ced in collapse_edges:
				if len(ced.faces) < 2:
					verts_boundry[ced.key[0]]= 2
					verts_boundry[ced.key[1]]= 2
			
			for ced in collapse_edges:
				if verts_boundry[ced.v1.index] != verts_boundry[ced.v2.index]:
					# Edge has 1 boundry and 1 non boundry vert. weight higher
					ced.collapse_weight*=BOUNDRY_WEIGHT
			vert_collapsed= verts_boundry
			del verts_boundry
		else:
			vert_collapsed= [1] * len(me.verts)

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
		else:
			collapse_count = len(collapse_edges)
		# We know edge_container_list_collapse can be removed.
		for ced in collapse_edges:
			# Chech if we have collapsed our quota.
			collapse_count-=1
			if not collapse_count:
				ced.collapse_loc= None
				break
				
			current_face_count -= len(ced.faces)
			if faceUV:
				# Handel UV's and vert Colors!
				v1= ced.v1
				v2= ced.v2
				for v, edge_my_uvs, edge_other_uvs, edge_my_cols, edge_other_cols in ((v2, ced.uv1, ced.uv2, ced.col1, ced.col2),(v1, ced.uv2, ced.uv1, ced.col2, ced.col1)):
					for face_vert_index, cfa in vert_face_users[v.index]:
						uvk=  cfa.orig_uv[face_vert_index] 
						colk= cfa.orig_col[face_vert_index] 
						
						# UV COORDS
						tex_index= None
						try:
							tex_index= edge_my_uvs.index(uvk)
						except ValueError:
							pass
						
						if tex_index != None:
							# This face uses a uv in the collapsing face. - do a merge
							other_uv= edge_other_uvs[tex_index]
							uv_vec= cfa.uv[face_vert_index]
							uv_vec.x= (uvk[0] + other_uv[0])*0.5
							uv_vec.y= (uvk[1] + other_uv[1])*0.5
						
						# TEXFACE COLOURS
						tex_index= None
						try:
							tex_index= edge_my_cols.index(colk)
						except ValueError:
							pass
						
						if tex_index != None:
							# Col
							other_col= edge_other_cols[tex_index]
							# f= me.faces[cfa.index]
							col_ob= cfa.col[face_vert_index]
							# col_ob= me.faces[cfa.index].col[face_vert_index]
							
							col_ob.r = int((colk[0] + other_col[0])*0.5)
							col_ob.g = int((colk[1] + other_col[1])*0.5)
							col_ob.b = int((colk[2] + other_col[2])*0.5)
			
			if current_face_count <= target_face_count:
				ced.collapse_loc= None
				break
		
		# Execute the collapse
		for ced in collapse_edges:
			# Since the list is ordered we can stop once the first non collapsed edge if sound.
			if not ced.collapse_loc:
				break
			ced.v1.sel= ced.v2.sel= True
			ced.v1.co= ced.v2.co=  ced.collapse_loc
		
		doubles= me.remDoubles(0.0001) 
		me= ob.getData(mesh=1)
		current_face_count= len(me.faces)
		if doubles==0: # should never happen.
			break
			
		if current_face_count <= target_face_count:
			ced.collapse_loc= None
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
