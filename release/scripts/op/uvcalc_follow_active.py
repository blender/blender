# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

#for full docs see...
# http://mediawiki.blender.org/index.php/Scripts/Manual/UV_Calculate/Follow_active_quads

import bpy

def extend(obj, operator, EXTEND_MODE):
	me = obj.data
	me_verts = me.verts
	# script will fail without UVs
	if not me.active_uv_texture:
		me.add_uv_texture()
	

	# Toggle Edit mode
	is_editmode = (obj.mode == 'EDIT')
	if is_editmode:
		bpy.ops.object.mode_set(mode='OBJECT')

	#t = sys.time()
	edge_average_lengths = {}
	
	OTHER_INDEX = 2,3,0,1
	FAST_INDICIES = 0,2,1,3 # order is faster
	def extend_uvs(face_source, face_target, edge_key):
		'''
		Takes 2 faces,
		Projects its extends its UV coords onto the face next to it.
		Both faces must share an edge.
		'''
		
		def face_edge_vs(vi):
			# assume a quad
			return [(vi[0], vi[1]), (vi[1], vi[2]), (vi[2], vi[3]), (vi[3], vi[0])]
	
		vidx_source = face_source.verts 
		vidx_target = face_target.verts

		faceUVsource = me.active_uv_texture.data[face_source.index]
		uvs_source = [faceUVsource.uv1,faceUVsource.uv2,faceUVsource.uv3,faceUVsource.uv4] 

		faceUVtarget = me.active_uv_texture.data[face_target.index]
		uvs_target = [faceUVtarget.uv1,faceUVtarget.uv2,faceUVtarget.uv3,faceUVtarget.uv4] 

		# vertex index is the key, uv is the value

		uvs_vhash_source = dict( [ (vindex, uvs_source[i]) for i, vindex in enumerate(vidx_source)] )

		uvs_vhash_target = dict( [ (vindex, uvs_target[i]) for i, vindex in enumerate(vidx_target)] )
		
		edge_idxs_source = face_edge_vs(vidx_source)
		edge_idxs_target = face_edge_vs(vidx_target)
		
		source_matching_edge = -1
		target_matching_edge = -1
		
		edge_key_swap = edge_key[1], edge_key[0]
		
		try:	source_matching_edge = edge_idxs_source.index(edge_key)
		except: source_matching_edge = edge_idxs_source.index(edge_key_swap)
		try:	target_matching_edge = edge_idxs_target.index(edge_key)
		except: target_matching_edge = edge_idxs_target.index(edge_key_swap)
		

		
		edgepair_inner_source = edge_idxs_source[source_matching_edge]
		edgepair_inner_target = edge_idxs_target[target_matching_edge]
		edgepair_outer_source = edge_idxs_source[OTHER_INDEX[source_matching_edge]]
		edgepair_outer_target = edge_idxs_target[OTHER_INDEX[target_matching_edge]]
		
		if edge_idxs_source[source_matching_edge] == edge_idxs_target[target_matching_edge]:
			iA= 0; iB= 1 # Flipped, most common
		else: # The normals of these faces must be different
			iA= 1; iB= 0

		
		# Set the target UV's touching source face, no tricky calc needed,
		uvs_vhash_target[edgepair_inner_target[0]][:] = uvs_vhash_source[edgepair_inner_source[iA]]
		uvs_vhash_target[edgepair_inner_target[1]][:] = uvs_vhash_source[edgepair_inner_source[iB]]


		# Set the 2 UV's on the target face that are not touching
		# for this we need to do basic expaning on the source faces UV's
		if EXTEND_MODE == 'LENGTH':
			
			try: # divide by zero is possible
				'''
				measure the length of each face from the middle of each edge to the opposite
				allong the axis we are copying, use this
				'''
				i1a= edgepair_outer_target[iB]
				i2a= edgepair_inner_target[iA]
				if i1a>i2a: i1a, i2a = i2a, i1a
				
				i1b= edgepair_outer_source[iB]
				i2b= edgepair_inner_source[iA]
				if i1b>i2b: i1b, i2b = i2b, i1b
				# print edge_average_lengths
				factor = edge_average_lengths[i1a, i2a][0] / edge_average_lengths[i1b, i2b][0]
			except:
				# Div By Zero?
				factor = 1.0
			
			uvs_vhash_target[edgepair_outer_target[iB]][:] = uvs_vhash_source[edgepair_inner_source[0]]  +factor * (uvs_vhash_source[edgepair_inner_source[0]] - uvs_vhash_source[edgepair_outer_source[1]])
			uvs_vhash_target[edgepair_outer_target[iA]][:] = uvs_vhash_source[edgepair_inner_source[1]]  +factor * (uvs_vhash_source[edgepair_inner_source[1]] - uvs_vhash_source[edgepair_outer_source[0]])
		
		else:
			# same as above but with no factor
			uvs_vhash_target[edgepair_outer_target[iB]][:] = uvs_vhash_source[edgepair_inner_source[0]] + (uvs_vhash_source[edgepair_inner_source[0]] - uvs_vhash_source[edgepair_outer_source[1]])
			uvs_vhash_target[edgepair_outer_target[iA]][:] = uvs_vhash_source[edgepair_inner_source[1]] + (uvs_vhash_source[edgepair_inner_source[1]] - uvs_vhash_source[edgepair_outer_source[0]])


	if me.active_uv_texture == None:
		me.add_uv_texture
	
	face_act =  me.faces.active
	if face_act == -1:
		operator.report({'ERROR'}, "No active face.")
		return
	
	face_sel= [f for f in me.faces if len(f.verts) == 4 and f.selected]
	
	face_act_local_index = -1
	for i, f in enumerate(face_sel):
		if f.index == face_act:
			face_act_local_index = i
			break
	
	if face_act_local_index == -1:
		operator.report({'ERROR'}, "Active face not selected.")
		return
	
	
	
	# Modes 
	# 0 unsearched
	# 1:mapped, use search from this face. - removed!!
	# 2:all siblings have been searched. dont search again.
	face_modes = [0] * len(face_sel)
	face_modes[face_act_local_index] = 1 # extend UV's from this face.
	
	
	# Edge connectivty
	edge_faces = {}
	for i, f in enumerate(face_sel):
		for edkey in f.edge_keys:
			try:	edge_faces[edkey].append(i)
			except: edge_faces[edkey] = [i]
	
	#SEAM = me.edges.seam
	
	if EXTEND_MODE == 'LENGTH':
		edge_loops = me.edge_loops(face_sel, [ed.key for ed in me.edges if ed.seam] )
		me_verts = me.verts
		for loop in edge_loops:
			looplen = [0.0]
			for ed in loop:
				edge_average_lengths[ed] = looplen
				looplen[0] += (me_verts[ed[0]].co - me_verts[ed[1]].co).length
			looplen[0] = looplen[0] / len(loop)
		
	
	
	# remove seams, so we dont map accross seams.
	for ed in me.edges:
		if ed.seam:
			# remove the edge pair if we can
			try:	del edge_faces[ed.key]
			except: pass
	# Done finding seams
	
	
	# face connectivity - faces around each face
	# only store a list of indicies for each face.
	face_faces = [[] for i in range(len(face_sel))]
	
	for edge_key, faces in edge_faces.items():
		if len(faces) == 2: # Only do edges with 2 face users for now
			face_faces[faces[0]].append((faces[1], edge_key))
			face_faces[faces[1]].append((faces[0], edge_key))
	
	
	# Now we know what face is connected to what other face, map them by connectivity
	ok = True
	while ok:
		ok = False
		for i in range(len(face_sel)):
			if face_modes[i] == 1: # searchable
				for f_sibling, edge_key in face_faces[i]:
					if face_modes[f_sibling] == 0:
						face_modes[f_sibling] = 1 # mapped and search from.
						extend_uvs(face_sel[i], face_sel[f_sibling], edge_key)
						face_modes[i] = 1 # we can map from this one now.
						ok= True # keep searching
				
				face_modes[i] = 2 # dont search again
	
	if is_editmode:
		bpy.ops.object.mode_set(mode='EDIT')
	else:
		me.update()
	


def main(context, operator):
	obj = context.active_object

	extend(obj, operator, operator.properties.mode)

class FollowActiveQuads(bpy.types.Operator):
	'''Follow UVs from active quads along continuous face loops.'''
	bl_idname = "uv.follow_active_quads"
	bl_label = "Follow Active Quads"

	bl_register = True
	bl_undo = True

	mode = bpy.props.EnumProperty(items=(("EVEN", "Client", "Space all UVs evently"), ("LENGTH", "Length", "Average space UVs edge length of each loop.")),
						name="Edge Length Mode",
						description="Method to space UV edge loops",
						default="LENGTH")

	def poll(self, context):
		obj = context.active_object
		return (obj is not None and obj.type == 'MESH')

	def execute(self, context):
		main(context, self)
		return ('FINISHED',)

bpy.ops.add(FollowActiveQuads)

# Add to a menu
import dynamic_menu

menu_func = (lambda self, context: self.layout.operator(FollowActiveQuads.bl_idname))

menu_item = dynamic_menu.add(bpy.types.VIEW3D_MT_uv_map, menu_func)

if __name__ == '__main__':
	bpy.ops.uv.follow_active_quads()

