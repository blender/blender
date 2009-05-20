#!BPY
"""
Name: 'Poly Reduce Selection (Unsubsurf)'
Blender: 245
Group: 'Mesh'
Tooltip: 'predictable mesh simplifaction maintaining face loops'
"""

from Blender import Scene, Mesh, Window, sys
import BPyMessages
import bpy

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


def my_mesh_util(me):
	me_verts = me.verts
	
	vert_faces = [ [] for v in me_verts]
	vert_faces_corner = [ [] for v in me_verts]
	
	
	# Ignore topology where there are not 2 faces connected to an edge.
	edge_count = {}
	for f in me.faces:
		for edkey in f.edge_keys:
			try:
				edge_count[edkey] += 1
			except:
				edge_count[edkey]  = 1
				
	for edkey, count in edge_count.iteritems():
		
		# Ignore verts that connect to edges with more than 2 faces.
		if count != 2:
			vert_faces[edkey[0]] = None
			vert_faces[edkey[1]] = None
	# Done
	
	
	
	def faces_set_verts(face_ls):
		unique_verts = set()
		for f in face_ls:
			for v in f:
				unique_verts.add(v.index)
		return unique_verts
	
	for f in me.faces:
		for corner, v in enumerate(f):
			i = v.index
			if vert_faces[i] != None:
				vert_faces[i].append(f)
				vert_faces_corner[i].append( corner )
	
	grid_data_ls = []
	
	for vi, face_ls in enumerate(vert_faces):
		if face_ls != None:
			if len(face_ls) == 4:
				if face_ls[0].sel and face_ls[1].sel and face_ls[2].sel and face_ls[3].sel:					
					# Support triangles also
					unique_vert_count = len(faces_set_verts(face_ls))
					quads = 0
					for f in face_ls:
						if len(f) ==4:
							quads += 1
					if unique_vert_count==5+quads: # yay we have a grid
						grid_data_ls.append( (vi, face_ls) )
			
			elif len(face_ls) == 3:
				if face_ls[0].sel and face_ls[1].sel and face_ls[2].sel:
					unique_vert_count = len(faces_set_verts(face_ls))
					if unique_vert_count==4: # yay we have 3 triangles to make into a bigger triangle
						grid_data_ls.append( (vi, face_ls) )
				
	
	
	# Now sort out which grid faces to use
	
	
	# This list will be used for items we can convert, vertex is key, faces are values
	grid_data_dict = {}
	
	if not grid_data_ls:
		print "doing nothing"
		return
	
	# quick lookup for the opposing corner of a qiad
	quad_diag_mapping = 2,3,0,1
	
	verts_used = [0] * len(me_verts) # 0 == untouched, 1==should touch, 2==touched
	verts_used[grid_data_ls[0][0]] = 1 # start touching 1!
	
	# From the corner vert, get the 2 edges that are not the corner or its opposing vert, this edge will make a new face
	quad_edge_mapping = (1,3), (2,0), (1,3), (0,2) # hi-low, low-hi order is intended
	tri_edge_mapping = (1,2), (0,2), (0,1)
	
	done_somthing = True
	while done_somthing:
		done_somthing = False
		grid_data_ls_index = -1
		
		for vi, face_ls in grid_data_ls:
			grid_data_ls_index += 1
			if len(face_ls) == 3:
				grid_data_dict[vi] = face_ls
				grid_data_ls.pop( grid_data_ls_index )
				break
			elif len(face_ls) == 4:
				# print vi
				if verts_used[vi] == 1:
					verts_used[vi] = 2 # dont look at this again.
					done_somthing = True
					
					grid_data_dict[vi] = face_ls
					
					# Tag all faces verts as used
					
					for i, f in enumerate(face_ls):
						# i == face index on vert, needed to recall which corner were on.
						v_corner = vert_faces_corner[vi][i]
						fv =f.v
						
						if len(f) == 4:
							v_other = quad_diag_mapping[v_corner]
							# get the 2 other corners
							corner1, corner2 = quad_edge_mapping[v_corner]
							if verts_used[fv[v_other].index] == 0:
								verts_used[fv[v_other].index] = 1 # TAG for touching!
						else:
							corner1, corner2 = tri_edge_mapping[v_corner]
						
						verts_used[fv[corner1].index] = 2 # Dont use these, they are 
						verts_used[fv[corner2].index] = 2
						
						
					# remove this since we have used it.
					grid_data_ls.pop( grid_data_ls_index )
					
					break
		
		if done_somthing == False:
			# See if there are any that have not even been tagged, (probably on a different island), then tag them.
			
			for vi, face_ls in grid_data_ls:
				if verts_used[vi] == 0:
					verts_used[vi] = 1
					done_somthing = True
					break
	
	
	# Now we have all the areas we will fill, calculate corner triangles we need to fill in.
	new_faces = []
	quad_del_vt_map = (1,2,3), (0,2,3), (0,1,3), (0,1,2)
	for vi, face_ls in grid_data_dict.iteritems():
		for i, f in enumerate(face_ls):
			if len(f) == 4:
				# i == face index on vert, needed to recall which corner were on.
				v_corner = vert_faces_corner[vi][i]
				v_other = quad_diag_mapping[v_corner]
				fv =f.v
				
				#print verts_used[fv[v_other].index]
				#if verts_used[fv[v_other].index] != 2: # DOSNT WORK ALWAYS
				
				if 1: # THIS IS LAzY - some of these faces will be removed after adding.
					# Ok we are removing half of this face, add the other half
					
					# This is probably slower
					# new_faces.append( [fv[ii].index for ii in (0,1,2,3) if ii != v_corner ] )
					
					# do this instead
					new_faces.append( (fv[quad_del_vt_map[v_corner][0]], fv[quad_del_vt_map[v_corner][1]], fv[quad_del_vt_map[v_corner][2]]) )
	
	del grid_data_ls
	
	
	# me.sel = 0
	def faceCombine4(vi, face_ls):
		edges = []
		
		for i, f in enumerate(face_ls):
			fv = f.v
			v_corner = vert_faces_corner[vi][i]
			if len(f)==4:	ed = quad_edge_mapping[v_corner]
			else:			ed = tri_edge_mapping[v_corner]
			
			edges.append( [fv[ed[0]].index, fv[ed[1]].index] )
		
		# get the face from the edges 
		face = edges.pop()
		while len(face) != 4:
			# print len(edges), edges, face
			for ed_idx, ed in enumerate(edges):
				if face[-1] == ed[0] and (ed[1] != face[0]):
					face.append(ed[1])
				elif face[-1] == ed[1] and (ed[0] != face[0]):
					face.append(ed[0])
				else:
					continue
				
				edges.pop(ed_idx) # we used the edge alredy
				break
		
		return face	
	
	for vi, face_ls in grid_data_dict.iteritems():
		if len(face_ls) == 4:
			new_faces.append( faceCombine4(vi, face_ls) )
			#pass
		if len(face_ls) == 3: # 3 triangles
			face = list(faces_set_verts(face_ls))
			face.remove(vi)
			new_faces.append( face )
			
	
	# Now remove verts surounded by 3 triangles
	

		
	# print new_edges
	# me.faces.extend(new_faces, ignoreDups=True)
	
	'''
	faces_remove = []
	for vi, face_ls in grid_data_dict.iteritems():
		faces_remove.extend(face_ls)
	'''
	
	orig_facelen = len(me.faces)
	
	orig_faces = list(me.faces)
	me.faces.extend(new_faces, ignoreDups=True)
	new_faces = list(me.faces)[len(orig_faces):]
	
	
	
	
	
	if me.faceUV:
		uvnames = me.getUVLayerNames()
		act_uvlay = me.activeUVLayer
		
		vert_faces_uvs =	[]
		vert_faces_images =	[]
			
			
		act_uvlay = me.activeUVLayer
		
		for uvlay in uvnames:
			me.activeUVLayer = uvlay
			vert_faces_uvs[:] = [None] * len(me.verts)
			vert_faces_images[:] = vert_faces_uvs[:]
			
			for i,f in enumerate(orig_faces):
				img = f.image
				fv = f.v
				uv = f.uv
				mat = f.mat
				for i,v in enumerate(fv):
					vi = v.index
					vert_faces_uvs[vi] = uv[i] # no nice averaging
					vert_faces_images[vi] = img
					
					
			# Now copy UVs across
			for f in new_faces:	
				fi = [v.index for v in f.v]
				f.image = vert_faces_images[fi[0]]
				uv = f.uv
				for i,vi in enumerate(fi):
					uv[i][:] = vert_faces_uvs[vi]
		
		if len(me.materials) > 1:
			vert_faces_mats = [None] * len(me.verts)
			for i,f in enumerate(orig_faces):
				mat = f.mat
				for i,v in enumerate(f.v):
					vi = v.index
					vert_faces_mats[vi] = mat
				
			# Now copy UVs across
			for f in new_faces:
				print vert_faces_mats[f.v[0].index]
				f.mat = vert_faces_mats[f.v[0].index]
				
	
	me.verts.delete(grid_data_dict.keys())
	
	# me.faces.delete(1, faces_remove)
	
	if me.faceUV:
		me.activeUVLayer = act_uvlay
	
	me.calcNormals()

def main():
	
	# Gets the current scene, there can be many scenes in 1 blend file.
	sce = bpy.data.scenes.active
	
	# Get the active object, there can only ever be 1
	# and the active object is always the editmode object.
	ob_act = sce.objects.active
	
	if not ob_act or ob_act.type != 'Mesh':
		BPyMessages.Error_NoMeshActive()
		return 
	
	is_editmode = Window.EditMode()
	if is_editmode: Window.EditMode(0)
	
	Window.WaitCursor(1)
	me = ob_act.getData(mesh=1) # old NMesh api is default
	t = sys.time()
	
	# Run the mesh editing function
	my_mesh_util(me)
	
	# Restore editmode if it was enabled
	if is_editmode: Window.EditMode(1)
	
	# Timing the script is a good way to be aware on any speed hits when scripting
	print 'My Script finished in %.2f seconds' % (sys.time()-t)
	Window.WaitCursor(0)
	
	
# This lets you can import the script without running it
if __name__ == '__main__':
	main()

