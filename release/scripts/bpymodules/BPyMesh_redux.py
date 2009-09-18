# ***** BEGIN GPL LICENSE BLOCK *****
#
# (C) Copyright 2006 MetaVR, Inc.
# http://www.metavr.com
# Written by Campbell Barton
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
import bpy
Vector= Blender.Mathutils.Vector
Ang= Blender.Mathutils.AngleBetweenVecs
MidpointVecs= Blender.Mathutils.MidpointVecs
import BPyMesh

# If python version is less than 2.4, try to get set stuff from module

try:
	set
except:
	try:
		from sets import Set as set
	except:
		set= None

def uv_key(uv):
	return round(uv.x, 5), round(uv.y, 5)
	
def uv_key_mix(uv1, uv2, w1, w2):
	# Weighted mix. w1+w2==1.0
	return w1*uv1[0]+w2*uv2[0], w1*uv1[1]+w2*uv2[1]

def col_key(col):
	return col.r, col.g, col.b
	
def col_key_mix(col1, col2,  w1, w2):
	# Weighted mix. w1+w2==1.0
	return int(w1*col1[0] + w2*col2[0]), int(w1*col1[1] + w2*col2[1]), int(w1*col1[2]+col2[2]*w2)


def redux(ob, REDUX=0.5, BOUNDRY_WEIGHT=2.0, REMOVE_DOUBLES=False, FACE_AREA_WEIGHT=1.0, FACE_TRIANGULATE=True, DO_UV=True, DO_VCOL=True, DO_WEIGHTS=True, VGROUP_INF_REDUX= None, VGROUP_INF_WEIGHT=0.5):
	"""
	BOUNDRY_WEIGHT - 0 is no boundry weighting. 2.0 will make them twice as unlikely to collapse.
	FACE_AREA_WEIGHT - 0 is no weight. 1 is normal, 2.0 is higher.
	"""
	
	if REDUX<0 or REDUX>1.0:
		raise 'Error, factor must be between 0 and 1.0'
	elif not set:
		raise 'Error, this function requires Python 2.4 or a full install of Python 2.3'
	
	BOUNDRY_WEIGHT= 1+BOUNDRY_WEIGHT
	
	""" # DEBUG!
	if Blender.Get('rt') == 1000:
		DEBUG=True
	else:
		DEBUG= False
	"""
	
	me= ob.getData(mesh=1)
	me.hide= False # unhide all data,.
	if len(me.faces)<5:
		return
	
	
	
	if FACE_TRIANGULATE or REMOVE_DOUBLES:
		me.sel= True
	
	if FACE_TRIANGULATE:
		me.quadToTriangle()
	
	if REMOVE_DOUBLES:
		me.remDoubles(0.0001)
	
	vgroups= me.getVertGroupNames()
	
	if not me.getVertGroupNames():
		DO_WEIGHTS= False
	
	if (VGROUP_INF_REDUX!= None and VGROUP_INF_REDUX not in vgroups) or\
	VGROUP_INF_WEIGHT==0.0:
		VGROUP_INF_REDUX= None
	
	try:
		VGROUP_INF_REDUX_INDEX= vgroups.index(VGROUP_INF_REDUX)
	except:
		VGROUP_INF_REDUX_INDEX= -1
	
	# del vgroups
	len_vgroups= len(vgroups)
	
	
	
	OLD_MESH_MODE= Blender.Mesh.Mode()
	Blender.Mesh.Mode(Blender.Mesh.SelectModes.VERTEX)
	
	if DO_UV and not me.faceUV:
		DO_UV= False
	
	if DO_VCOL and not me.vertexColors:
		DO_VCOL = False
	
	current_face_count= len(me.faces)
	target_face_count= int(current_face_count * REDUX)
	# % of the collapseable faces to collapse per pass.
	#collapse_per_pass= 0.333 # between 0.1 - lots of small nibbles, slow but high q. and 0.9 - big passes and faster.
	collapse_per_pass= 0.333 # between 0.1 - lots of small nibbles, slow but high q. and 0.9 - big passes and faster.
	
	"""# DEBUG!
	if DEBUG:
		COUNT= [0]
		def rd():
			if COUNT[0]< 330:
				COUNT[0]+=1
				return
			me.update()
			Blender.Window.RedrawAll()
			print 'Press key for next, count "%s"' % COUNT[0]
			try: input()
			except KeyboardInterrupt:
				raise "Error"
			except:
				pass
				
			COUNT[0]+=1
	"""
	
	class collapseEdge(object):
		__slots__ = 'length', 'key', 'faces', 'collapse_loc', 'v1', 'v2','uv1', 'uv2', 'col1', 'col2', 'collapse_weight'
		def __init__(self, ed):
			self.init_from_edge(ed) # So we can re-use the classes without using more memory.
		
		def init_from_edge(self, ed):
			self.key= ed.key
			self.length= ed.length
			self.faces= []
			self.v1= ed.v1
			self.v2= ed.v2
			if DO_UV or DO_VCOL:
				self.uv1= []
				self.uv2= []
				self.col1= []
				self.col2= []
				
			# self.collapse_loc= None # new collapse location.
			# Basic weighting.
			#self.collapse_weight= self.length *  (1+ ((ed.v1.no-ed.v2.no).length**2))
			self.collapse_weight= 1.0
		
		def collapse_locations(self, w1, w2):
			'''
			Generate a smart location for this edge to collapse to
			w1 and w2 are vertex location bias
			'''
			
			v1co= self.v1.co
			v2co= self.v2.co
			v1no= self.v1.no
			v2no= self.v2.no
			
			# Basic operation, works fine but not as good as predicting the best place.
			#between= ((v1co*w1) + (v2co*w2))
			#self.collapse_loc= between
			
			# normalize the weights of each vert - se we can use them as scalers.
			wscale= w1+w2
			if not wscale: # no scale?
				w1=w2= 0.5
			else:
				w1/=wscale
				w2/=wscale
			
			length= self.length
			between= MidpointVecs(v1co, v2co)
			
			# Collapse
			# new_location = between # Replace tricky code below. this code predicts the best collapse location.
			
			# Make lines at right angles to the normals- these 2 lines will intersect and be
			# the point of collapsing.
			
			# Enlarge so we know they intersect:  self.length*2
			cv1= v1no.cross(v1no.cross(v1co-v2co))
			cv2= v2no.cross(v2no.cross(v2co-v1co))
			
			# Scale to be less then the edge lengths.
			cv2.length = cv1.length = 1
			
			cv1 = cv1 * (length* 0.4)
			cv2 = cv2 * (length* 0.4)
			
			smart_offset_loc= between + (cv1 + cv2)
			
			# Now we need to blend between smart_offset_loc and w1/w2
			# you see were blending between a vert and the edges midpoint, so we cant use a normal weighted blend.
			if w1 > 0.5: # between v1 and smart_offset_loc
				#self.collapse_loc= v1co*(w2+0.5) + smart_offset_loc*(w1-0.5)
				w2*=2
				w1= 1-w2
				new_loc_smart= v1co*w1 + smart_offset_loc*w2
			else: # w between v2 and smart_offset_loc
				w1*=2
				w2= 1-w1
				new_loc_smart= v2co*w2 + smart_offset_loc*w1
				
			if new_loc_smart.x != new_loc_smart.x: # NAN LOCATION, revert to between
				new_loc_smart= None
			
			return new_loc_smart, between, v1co*0.99999 + v2co*0.00001, v1co*0.00001 + v2co*0.99999
		

	class collapseFace(object):
		__slots__ = 'verts', 'normal', 'area', 'index', 'orig_uv', 'orig_col', 'uv', 'col' # , 'collapse_edge_count'
		def __init__(self, f):
			self.init_from_face(f)
		
		def init_from_face(self, f):
			self.verts= f.v
			self.normal= f.no
			self.area= f.area
			self.index= f.index
			if DO_UV:
				self.orig_uv= [uv_key(uv) for uv in f.uv]
				self.uv= f.uv
			if DO_VCOL:
				self.orig_col= [col_key(col) for col in f.col]
				self.col= f.col
	
	collapse_edges= collapse_faces= None
	
	# So meshCalcNormals can avoid making a new list all the time.
	reuse_vertNormals= [ Vector() for v in xrange(len(me.verts)) ]
	
	while target_face_count <= len(me.faces):
		BPyMesh.meshCalcNormals(me, reuse_vertNormals)
		
		if DO_WEIGHTS:
			#groupNames, vWeightDict= BPyMesh.meshWeight2Dict(me)
			groupNames, vWeightList= BPyMesh.meshWeight2List(me)
		
		# THIS CRASHES? Not anymore.
		verts= list(me.verts)
		edges= list(me.edges)
		faces= list(me.faces)
		
		# THIS WORKS
		#verts= me.verts
		#edges= me.edges
		#faces= me.faces
		
		# if DEBUG: DOUBLE_CHECK= [0]*len(verts)
		me.sel= False
		
		if not collapse_faces: # Initialize the list.
			collapse_faces= [collapseFace(f) for f in faces]
			collapse_edges= [collapseEdge(ed) for ed in edges]
		else:
			for i, ed in enumerate(edges):
				collapse_edges[i].init_from_edge(ed)
			
			# Strip the unneeded end off the list
			collapse_edges[i+1:]= []
				
			for i, f in enumerate(faces):
				collapse_faces[i].init_from_face(f)
			
			# Strip the unneeded end off the list
			collapse_faces[i+1:]= []
			
			
		collapse_edges_dict= dict( [(ced.key, ced) for ced in collapse_edges] )
		
		# Store verts edges.
		vert_ed_users= [[] for i in xrange(len(verts))]
		for ced in collapse_edges:
			vert_ed_users[ced.key[0]].append(ced)
			vert_ed_users[ced.key[1]].append(ced)
		
		# Store face users
		vert_face_users= [[] for i in xrange(len(verts))]
		
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
				if DO_UV or DO_VCOL:
					# if the edge is flipped from its order in the face then we need to flip the order indicies.
					if cfa.verts[i]==ced.v1:	i1,i2 = i, i-1
					else:						i1,i2 = i-1, i
					
					if DO_UV:
						ced.uv1.append( cfa.orig_uv[i1] )
						ced.uv2.append( cfa.orig_uv[i2] )
					
					if DO_VCOL:
						ced.col1.append( cfa.orig_col[i1] )
						ced.col2.append( cfa.orig_col[i2] )
					
				
				# PERIMITER
				#face_perim[ii]+= ced.length
		
		
		
		# How weight the verts by the area of their faces * the normal difference.
		# when the edge collapses, to vert weights are taken into account 
		
		vert_weights= [0.5] * len(verts)
		
		for ii, vert_faces in enumerate(vert_face_users):
			for f in vert_faces:
				try:
					no_ang= (Ang(verts[ii].no, f[1].normal)/180) * f[1].area
				except:
					no_ang= 1.0
				
				vert_weights[ii] += no_ang
		
		# Use a vertex group as a weighting.
		if VGROUP_INF_REDUX!=None:
			
			# Get Weights from a vgroup.
			"""
			vert_weights_map= [1.0] * len(verts)
			for i, wd in enumerate(vWeightDict):
				try:	vert_weights_map[i]= 1+(wd[VGROUP_INF_REDUX] * VGROUP_INF_WEIGHT)
				except:	pass
			"""
			vert_weights_map= [1+(wl[VGROUP_INF_REDUX_INDEX]*VGROUP_INF_WEIGHT) for wl in vWeightList ]
			
		
		# BOUNDRY CHECKING AND WEIGHT EDGES. CAN REMOVE
		# Now we know how many faces link to an edge. lets get all the boundry verts
		if BOUNDRY_WEIGHT > 0:
			verts_boundry= [1] * len(verts)
			#for ed_idxs, faces_and_uvs in edge_faces_and_uvs.iteritems():
			for ced in collapse_edges:
				if len(ced.faces) < 2:
					for key in ced.key: # only ever 2 key indicies.
						verts_boundry[key]= 2
			
			for ced in collapse_edges:
				b1= verts_boundry[ced.key[0]]
				b2= verts_boundry[ced.key[1]]
				if b1 != b2:
					# Edge has 1 boundry and 1 non boundry vert. weight higher
					ced.collapse_weight= BOUNDRY_WEIGHT
				#elif b1==b2==2: # if both are on a seam then weigh half as bad.
				#	ced.collapse_weight= ((BOUNDRY_WEIGHT-1)/2) +1
			# weight the verts by their boundry status
			del b1
			del b2
			
			for ii, boundry in enumerate(verts_boundry):
				if boundry==2:
					vert_weights[ii] *= BOUNDRY_WEIGHT
			
			vert_collapsed= verts_boundry
			del verts_boundry
		else:
			vert_collapsed= [1] * len(verts)
		
		
				
		
		# Best method, no quick hacks here, Correction. Should be the best but needs tweaks.
		def ed_set_collapse_error(ced):
			# Use the vertex weights to bias the new location.
			new_locs= ced.collapse_locations(vert_weights[ced.key[0]], vert_weights[ced.key[1]])
			
			
			# Find the connecting faces of the 2 verts.
			i1, i2= ced.key
			test_faces= set()
			for i in (i1,i2): # faster then LC's
				for f in vert_face_users[i]:
					test_faces.add(f[1].index)
			for f in ced.faces:
				test_faces.remove(f.index)
			
			
			v1_orig= Vector(ced.v1.co)
			v2_orig= Vector(ced.v2.co)
			
			def test_loc(new_loc):
				'''
				Takes a location and tests the error without changing anything
				'''
				new_weight= ced.collapse_weight
				ced.v1.co= ced.v2.co= new_loc
				
				new_nos= [faces[i].no for i in test_faces]
				
				# So we can compare the befire and after normals
				ced.v1.co= v1_orig
				ced.v2.co= v2_orig
				
				# now see how bad the normals are effected
				angle_diff= 1.0
				
				for ii, i in enumerate(test_faces): # local face index, global face index
					cfa= collapse_faces[i] # this collapse face
					try:
						# can use perim, but area looks better.
						if FACE_AREA_WEIGHT:
							# Psudo code for wrighting
							# angle_diff= The before and after angle difference between the collapsed and un-collapsed face.
							# ... devide by 180 so the value will be between 0 and 1.0
							# ... add 1 so we can use it as a multiplyer and not make the area have no eefect (below)
							# area_weight= The faces original area * the area weight
							# ... add 1.0 so a small area face dosent make the angle_diff have no effect.
							#
							# Now multiply - (angle_diff * area_weight)
							# ... The weight will be a minimum of 1.0 - we need to subtract this so more faces done give the collapse an uneven weighting.
							
							angle_diff+= ((1+(Ang(cfa.normal, new_nos[ii])/180)) * (1+(cfa.area * FACE_AREA_WEIGHT))) -1 # 4 is how much to influence area
						else:
							angle_diff+= (Ang(cfa.normal), new_nos[ii])/180
							
					except:
						pass
								
				
				# This is very arbirary, feel free to modify
				try:		no_ang= (Ang(ced.v1.no, ced.v2.no)/180) + 1
				except:		no_ang= 2.0
				
				# do *= because we face the boundry weight to initialize the weight. 1.0 default.
				new_weight *=  ((no_ang * ced.length) * (1-(1/angle_diff)))# / max(len(test_faces), 1)
				return new_weight
			# End testloc
			
			
			# Test the collapse locatons
			collapse_loc_best= None
			collapse_weight_best= 1000000000
			ii= 0
			for collapse_loc in new_locs:
				if collapse_loc: # will only ever fail if smart loc is NAN
					test_weight= test_loc(collapse_loc)
					if test_weight < collapse_weight_best:
						iii= ii
						collapse_weight_best = test_weight
						collapse_loc_best= collapse_loc
					ii+=1
			
			ced.collapse_loc= collapse_loc_best
			ced.collapse_weight= collapse_weight_best
			
			
			# are we using a weight map
			if VGROUP_INF_REDUX:
				v= vert_weights_map[i1]+vert_weights_map[i2]
				ced.collapse_weight*= v
		# End collapse Error
		
		# We can calculate the weights on __init__ but this is higher qualuity.
		for ced in collapse_edges:
			if ced.faces: # dont collapse faceless edges.
				ed_set_collapse_error(ced)
		
		# Wont use the function again.
		del ed_set_collapse_error
		# END BOUNDRY. Can remove
		
		# sort by collapse weight
		try:	collapse_edges.sort(key = lambda ced: ced.collapse_weight) # edges will be used for sorting
		except:	collapse_edges.sort(lambda ced1, ced2: cmp(ced1.collapse_weight, ced2.collapse_weight)) # edges will be used for sorting
		
		
		vert_collapsed= [0]*len(verts)
		
		collapse_edges_to_collapse= []
		
		# Make a list of the first half edges we can collapse,
		# these will better edges to remove.
		collapse_count=0
		for ced in collapse_edges:
			if ced.faces:
				i1, i2= ced.key
				# Use vert selections 
				if vert_collapsed[i1] or vert_collapsed[i2]:
					pass
				else:
					# Now we know the verts havnyt been collapsed.
					vert_collapsed[i2]= vert_collapsed[i1]= 1 # Dont collapse again.
					collapse_count+=1
					collapse_edges_to_collapse.append(ced)
		
		# Get a subset of the entire list- the first "collapse_per_pass", that are best to collapse.
		if collapse_count > 4:
			collapse_count = int(collapse_count*collapse_per_pass)
		else:
			collapse_count = len(collapse_edges)
		# We know edge_container_list_collapse can be removed.
		for ced in collapse_edges_to_collapse:
			"""# DEBUG!
			if DEBUG:
				if DOUBLE_CHECK[ced.v1.index] or\
				DOUBLE_CHECK[ced.v2.index]:
					raise 'Error'
				else:
					DOUBLE_CHECK[ced.v1.index]=1
					DOUBLE_CHECK[ced.v2.index]=1
				
				tmp= (ced.v1.co+ced.v2.co)*0.5
				Blender.Window.SetCursorPos(tmp.x, tmp.y, tmp.z)
				Blender.Window.RedrawAll()
			"""
			
			# Chech if we have collapsed our quota.
			collapse_count-=1
			if not collapse_count:
				break
			
			current_face_count -= len(ced.faces)
			
			# Find and assign the real weights based on collapse loc.
			
			# Find the weights from the collapse error
			if DO_WEIGHTS or DO_UV or DO_VCOL:
				i1, i2= ced.key
				# Dont use these weights since they may not have been used to make the collapse loc.
				#w1= vert_weights[i1]
				#w2= vert_weights[i2]
				w1= (ced.v2.co-ced.collapse_loc).length
				w2= (ced.v1.co-ced.collapse_loc).length
				
				# Normalize weights
				wscale= w1+w2
				if not wscale: # no scale?
					w1=w2= 0.5
				else:
					w1/= wscale
					w2/= wscale
				
				
				# Interpolate the bone weights.
				if DO_WEIGHTS:
					
					# add verts vgroups to eachother
					wl1= vWeightList[i1] # v1 weight dict
					wl2= vWeightList[i2] # v2 weight dict
					for group_index in xrange(len_vgroups):
						wl1[group_index]= wl2[group_index]= (wl1[group_index]*w1) + (wl2[group_index]*w2)
				# Done finding weights.
				
				
				
				if DO_UV or DO_VCOL:
					# Handel UV's and vert Colors!
					for v, my_weight, other_weight, edge_my_uvs, edge_other_uvs, edge_my_cols, edge_other_cols in (\
					(ced.v1, w1, w2, ced.uv1, ced.uv2, ced.col1, ced.col2),\
					(ced.v2, w2, w1, ced.uv2, ced.uv1, ced.col2, ced.col1)\
					):
						uvs_mixed=   [ uv_key_mix(edge_my_uvs[iii],   edge_other_uvs[iii],  my_weight, other_weight)  for iii in xrange(len(edge_my_uvs))  ]
						cols_mixed=  [ col_key_mix(edge_my_cols[iii], edge_other_cols[iii], my_weight, other_weight) for iii in xrange(len(edge_my_cols)) ]
						
						for face_vert_index, cfa in vert_face_users[v.index]:
							if len(cfa.verts)==3 and cfa not in ced.faces: # if the face is apart of this edge then dont bother finding the uvs since the face will be removed anyway.
							
								if DO_UV:
									# UV COORDS
									uvk=  cfa.orig_uv[face_vert_index] 
									try:
										tex_index= edge_my_uvs.index(uvk)
									except:
										tex_index= None
										""" # DEBUG!
										if DEBUG:
											print 'not found', uvk, 'in', edge_my_uvs, 'ed index', ii, '\nwhat about', edge_other_uvs
										"""
									if tex_index != None: # This face uses a uv in the collapsing face. - do a merge
										other_uv= edge_other_uvs[tex_index]
										uv_vec= cfa.uv[face_vert_index]
										uv_vec.x, uv_vec.y= uvs_mixed[tex_index]
								
								# TEXFACE COLORS
								if DO_VCOL:
									colk= cfa.orig_col[face_vert_index] 
									try:    tex_index= edge_my_cols.index(colk)
									except: pass
									if tex_index != None:
										other_col= edge_other_cols[tex_index]
										col_ob= cfa.col[face_vert_index]
										col_ob.r, col_ob.g, col_ob.b= cols_mixed[tex_index]
								
								# DEBUG! if DEBUG: rd()
			
			# Execute the collapse
			ced.v1.sel= ced.v2.sel= True # Select so remove doubles removed the edges and faces that use it
			ced.v1.co= ced.v2.co=  ced.collapse_loc
				
			# DEBUG! if DEBUG: rd()
			if current_face_count <= target_face_count:
				break
		
		# Copy weights back to the mesh before we remove doubles.
		if DO_WEIGHTS:
			#BPyMesh.dict2MeshWeight(me, groupNames, vWeightDict)
			BPyMesh.list2MeshWeight(me, groupNames, vWeightList)
		
		doubles= me.remDoubles(0.0001) 
		current_face_count= len(me.faces)
		
		if current_face_count <= target_face_count or not doubles: # not doubles shoule never happen.
			break
	
	me.update()
	Blender.Mesh.Mode(OLD_MESH_MODE)


# Example usage
def main():
	Blender.Window.EditMode(0)
	scn= bpy.data.scenes.active
	active_ob= scn.objects.active
	t= Blender.sys.time()
	redux(active_ob, 0.5)
	print '%.4f' % (Blender.sys.time()-t)

if __name__=='__main__':
	main()
