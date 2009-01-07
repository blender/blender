#!BPY
"""
Name: 'Clean Meshes'
Blender: 245
Group: 'Mesh'
Tooltip: 'Clean unused data from all selected mesh objects.'
"""

__author__ = "Campbell Barton aka ideasman42"
__url__ = ["www.blender.org", "blenderartists.org", "www.python.org"]
__version__ = "0.1"
__bpydoc__ = """\
Clean Meshes

Cleans unused data from selected meshes
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


from Blender import *
import bpy
from Blender.Mathutils import TriangleArea

import Blender
import BPyMesh
dict2MeshWeight= BPyMesh.dict2MeshWeight
meshWeight2Dict= BPyMesh.meshWeight2Dict

def rem_free_verts(me):
	vert_users= [0] * len(me.verts)
	for f in me.faces:
		for v in f:
			vert_users[v.index]+=1
	
	for e in me.edges:
		for v in e: # loop on edge verts
			vert_users[v.index]+=1
	
	verts_free= [i for i, users in enumerate(vert_users) if not users]
	
	if verts_free:
		pass
		me.verts.delete(verts_free)
	return len(verts_free)
	
def rem_free_edges(me, limit=None):
	''' Only remove based on limit if a limit is set, else remove all '''
	
	edgeDict= {} # will use a set when python 2.4 is standard.
	
	for f in me.faces:
		for edkey in f.edge_keys:
			edgeDict[edkey] = None
	
	edges_free= []
	for e in me.edges:
		if not edgeDict.has_key(e.key):
			edges_free.append(e)
	
	if limit != None:
		edges_free= [e for e in edges_free if e.length <= limit]
	
	me.edges.delete(edges_free)
	return len(edges_free)

def rem_area_faces(me, limit=0.001):
	''' Faces that have an area below the limit '''
	rem_faces= [f for f in me.faces if f.area <= limit]
	if rem_faces:
		me.faces.delete( 0, rem_faces )
	return len(rem_faces)

def rem_perimeter_faces(me, limit=0.001):
	''' Faces whos combine edge length is below the limit '''
	def faceEdLen(f):
		v= f.v
		if len(v) == 3:
			return\
			(v[0].co-v[1].co).length +\
			(v[1].co-v[2].co).length +\
			(v[2].co-v[0].co).length
		else: # 4
			return\
			(v[0].co-v[1].co).length +\
			(v[1].co-v[2].co).length +\
			(v[2].co-v[3].co).length +\
			(v[3].co-v[0].co).length
	rem_faces= [f for f in me.faces if faceEdLen(f) <= limit]
	if rem_faces:
		me.faces.delete( 0, rem_faces )
	return len(rem_faces)

def rem_unused_materials(me):
	materials= me.materials
	len_materials= len(materials)
	if len_materials < 2:
		return 0
		
	rem_materials= 0
	
	material_users= dict( [(i,0) for i in xrange(len_materials)] )
	
	for f in me.faces:
		f_mat = f.mat
		# Make sure the face index isnt too big. this happens sometimes.
		if f_mat >= len_materials:
			f_mat = f.mat = 0
		material_users[f_mat] += 1
	
	# mat_idx_subtract= 0
	# reindex_mapping= dict( [(i,0) for i in xrange(len_materials)] )
	
	reindex_mapping_ls = range(len_materials)
	for i in range(len_materials-1, -1, -1):
		if material_users[i] == 0:
			del reindex_mapping_ls[i]
			del materials[i]
			rem_materials+=1
	
	reindex_mapping= {}
	
	for i, mat in enumerate(reindex_mapping_ls):
		reindex_mapping[mat] = i		
	
	for f in me.faces:
		f.mat= reindex_mapping[f.mat]
	
	me.materials= materials
	return rem_materials


def rem_free_groups(me, groupNames, vWeightDict):
	''' cound how many vert users a group has and remove unused groups '''
	rem_groups		= 0
	groupUserDict= dict([(group,0) for group in groupNames])
	
	for vertexWeight in vWeightDict:
		for group, weight in vertexWeight.iteritems():
			groupUserDict[group] += 1
	
	i=len(groupNames)
	while i:
		i-=1
		group= groupNames[i]
		if groupUserDict[group] == 0:
			del groupNames[i]
			print '\tremoving, vgroup', group
			rem_groups+=1
	return rem_groups

def rem_zero_weights(me, limit, groupNames, vWeightDict):
	''' remove verts from a group when their weight is zero.'''
	rem_vweight_count= 0
	for vertexWeight in vWeightDict:
		items= vertexWeight.items()
		for group, weight in items:
			if weight < limit:
				del vertexWeight[group]
				rem_vweight_count+= 1

	return rem_vweight_count

	
def normalize_vweight(me, groupNames, vWeightDict):
	for vertexWeight in vWeightDict:
		unit= 0.0
		for group, weight in vertexWeight.iteritems():
			unit+= weight
		
		if unit != 1.0 and unit != 0.0:
			for group, weight in vertexWeight.iteritems():
				vertexWeight[group]= weight/unit

def isnan(f):
	fstring = str(f).lower()
	if 'nan' in fstring:
		return True
	if 'inf' in fstring:
		return True
	
	return False

def fix_nan_verts__internal(me):
	rem_nan = 0
	for v in me.verts:
		co = v.co
		for i in (0,1,2):
			if isnan(co[i]):
				co[i] = 0.0
				rem_nan += 1
	return rem_nan

def fix_nan_verts(me):
	rem_nan = 0
	key = me.key
	if key:
		# Find the object, and get a mesh thats thinked to the oblink.
		# this is a bit crap but needed to set the active key.
		me_oblink = None
		for ob in bpy.data.objects:
			me_oblink = ob.getData(mesh=1)
			if me_oblink == me:
				me = me_oblink
				break
		if not me_oblink:
			ob = None
	
	if key and ob:
		blocks = key.blocks
		# print blocks
		orig_pin = ob.pinShape
		orig_shape = ob.activeShape
		orig_relative = key.relative
		ob.pinShape = True
		for i, block in enumerate(blocks):
			ob.activeShape = i+1
			ob.makeDisplayList()
			rem_nan += fix_nan_verts__internal(me)
			me.update(block.name) # get the new verts
		ob.pinShape	= orig_pin
		ob.activeShape = orig_shape
		key.relative = orig_relative
		
	else: # No keys, simple operation
		rem_nan = fix_nan_verts__internal(me)
	
	return rem_nan

def fix_nan_uvs(me):
	rem_nan = 0
	if me.faceUV:
		orig_uvlayer = me.activeUVLayer
		for uvlayer in me.getUVLayerNames():
			me.activeUVLayer = uvlayer
			for f in me.faces:
				for uv in f.uv:
					for i in (0,1):
						if isnan(uv[i]):
							uv[i] = 0.0
							rem_nan += 1
		me.activeUVLayer = orig_uvlayer
	return rem_nan


def has_vcol(me):
	for f in me.faces:
		for col in f.col:
			if not (255 == col.r == col.g == col.b):
				return True
	return False

def rem_white_vcol_layers(me):
	vcols_removed = 0
	if me.vertexColors:
		for col in me.getColorLayerNames():
			me.activeColorLayer = col
			if not has_vcol(me): 
				me.removeColorLayer(col)
				vcols_removed += 1
	
	return vcols_removed


def main():	
	sce= bpy.data.scenes.active
	obsel= list(sce.objects.context)
	actob= sce.objects.active
	
	is_editmode= Window.EditMode()
	
	# Edit mode object is not active, add it to the list.
	if is_editmode and (not actob.sel):
		obsel.append(actob)
	
	
	#====================================#
	# Popup menu to select the functions #
	#====================================#
	
	CLEAN_ALL_DATA= Draw.Create(0)
	CLEAN_VERTS_FREE= Draw.Create(1)
	CLEAN_EDGE_NOFACE= Draw.Create(0)
	CLEAN_EDGE_SMALL= Draw.Create(0)
	CLEAN_FACE_PERIMETER= Draw.Create(0)
	CLEAN_FACE_SMALL= Draw.Create(0)
	
	CLEAN_MATERIALS= Draw.Create(0)
	CLEAN_WHITE_VCOL_LAYERS= Draw.Create(0)
	CLEAN_GROUP= Draw.Create(0)
	CLEAN_VWEIGHT= Draw.Create(0)
	CLEAN_WEIGHT_NORMALIZE= Draw.Create(0)
	limit= Draw.Create(0.01)
	
	CLEAN_NAN_VERTS= Draw.Create(0)
	CLEAN_NAN_UVS= Draw.Create(0)
	
	# Get USER Options
	
	pup_block= [\
	('Verts: free', CLEAN_VERTS_FREE, 'Remove verts that are not used by an edge or a face.'),\
	('Edges: free', CLEAN_EDGE_NOFACE, 'Remove edges that are not in a face.'),\
	('Edges: short', CLEAN_EDGE_SMALL, 'Remove edges that are below the length limit.'),\
	('Faces: small perimeter', CLEAN_FACE_PERIMETER, 'Remove faces below the perimeter limit.'),\
	('Faces: small area', CLEAN_FACE_SMALL, 'Remove faces below the area limit (may remove faces stopping T-face artifacts).'),\
	('limit: ', limit, 0.001, 1.0, 'Limit for the area and length tests above (a higher limit will remove more data).'),\
	('Material Clean', CLEAN_MATERIALS, 'Remove unused materials.'),\
	('Color Layers', CLEAN_WHITE_VCOL_LAYERS, 'Remove vertex color layers that are totaly white'),\
	('VGroup Clean', CLEAN_GROUP, 'Remove vertex groups that have no verts using them.'),\
	('Weight Clean', CLEAN_VWEIGHT, 'Remove zero weighted verts from groups (limit is zero threshold).'),\
	('WeightNormalize', CLEAN_WEIGHT_NORMALIZE, 'Make the sum total of vertex weights accross vgroups 1.0 for each vertex.'),\
	'Clean NAN values',\
	('NAN Verts', CLEAN_NAN_VERTS, 'Make NAN or INF verts (0,0,0)'),\
	('NAN UVs', CLEAN_NAN_UVS, 'Make NAN or INF UVs (0,0)'),\
	'',\
	('All Mesh Data', CLEAN_ALL_DATA, 'Warning! Operate on ALL mesh objects in your Blend file. Use with care'),\
	]
	
	if not Draw.PupBlock('Clean Selected Meshes...', pup_block):
		return
	
	CLEAN_VERTS_FREE= CLEAN_VERTS_FREE.val
	CLEAN_EDGE_NOFACE= CLEAN_EDGE_NOFACE.val
	CLEAN_EDGE_SMALL= CLEAN_EDGE_SMALL.val
	CLEAN_FACE_PERIMETER= CLEAN_FACE_PERIMETER.val
	CLEAN_FACE_SMALL= CLEAN_FACE_SMALL.val
	CLEAN_MATERIALS= CLEAN_MATERIALS.val
	CLEAN_WHITE_VCOL_LAYERS= CLEAN_WHITE_VCOL_LAYERS.val
	CLEAN_GROUP= CLEAN_GROUP.val
	CLEAN_VWEIGHT= CLEAN_VWEIGHT.val
	CLEAN_WEIGHT_NORMALIZE= CLEAN_WEIGHT_NORMALIZE.val
	limit= limit.val
	CLEAN_ALL_DATA= CLEAN_ALL_DATA.val
	CLEAN_NAN_VERTS= CLEAN_NAN_VERTS.val
	CLEAN_NAN_UVS= CLEAN_NAN_UVS.val
	
	if is_editmode: Window.EditMode(0)
	
	if CLEAN_ALL_DATA:
		if CLEAN_GROUP or CLEAN_VWEIGHT or CLEAN_WEIGHT_NORMALIZE:
			# For groups we need the objects linked to the mesh
			meshes= [ob.getData(mesh=1) for ob in bpy.data.objects if ob.type == 'Mesh' if not ob.lib]
		else:
			meshes= bpy.data.meshes
	else:
		meshes= [ob.getData(mesh=1) for ob in obsel if ob.type == 'Mesh']
	
	tot_meshes = len(meshes) # so we can decrement libdata
	rem_face_count= rem_edge_count= rem_vert_count= rem_material_count= rem_vcol_layer_count= rem_group_count= rem_vweight_count= fix_nan_vcount= fix_nan_uvcount= 0
	if not meshes:
		if is_editmode: Window.EditMode(1)
		Draw.PupMenu('No meshes to clean')
	
	Blender.Window.WaitCursor(1)
	bpy.data.meshes.tag = False
	for me in meshes:
		
		# Dont touch the same data twice
		if me.tag:
			tot_meshes -= 1
			continue
		me.tag = True
		
		if me.lib:
			tot_meshes -= 1
			continue
		
		if me.multires:
			multires_level_orig = me.multiresDrawLevel
			me.multiresDrawLevel = 1
			print 'Warning, cannot perform destructive operations on multires mesh:', me.name
		else:
			if CLEAN_FACE_SMALL:
				rem_face_count += rem_area_faces(me, limit)
				
			if CLEAN_FACE_PERIMETER:
				rem_face_count += rem_perimeter_faces(me, limit)
			
			if CLEAN_EDGE_SMALL: # for all use 2- remove all edges.
				rem_edge_count += rem_free_edges(me, limit)
			
			if CLEAN_EDGE_NOFACE:
				rem_edge_count += rem_free_edges(me)
			
			if CLEAN_VERTS_FREE:
				rem_vert_count += rem_free_verts(me)
		
		if CLEAN_MATERIALS:
			rem_material_count += rem_unused_materials(me)
		
		if CLEAN_WHITE_VCOL_LAYERS:
			rem_vcol_layer_count += rem_white_vcol_layers(me)
		
		if CLEAN_VWEIGHT or CLEAN_GROUP or CLEAN_WEIGHT_NORMALIZE:
			groupNames, vWeightDict= meshWeight2Dict(me)
			
			if CLEAN_VWEIGHT:
				rem_vweight_count += rem_zero_weights(me, limit, groupNames, vWeightDict)
			
			if CLEAN_GROUP:
				rem_group_count += rem_free_groups(me, groupNames, vWeightDict)
				pass
			
			if CLEAN_WEIGHT_NORMALIZE:
				normalize_vweight(me, groupNames, vWeightDict)
			
			# Copy back to mesh vertex groups.
			dict2MeshWeight(me, groupNames, vWeightDict)
		
		if CLEAN_NAN_VERTS:
			fix_nan_vcount = fix_nan_verts(me)
			
		if CLEAN_NAN_UVS:
			fix_nan_uvcount = fix_nan_uvs(me)
		
		# restore multires.
		if me.multires:
			me.multiresDrawLevel = multires_level_orig
		
	Blender.Window.WaitCursor(0)
	if is_editmode: Window.EditMode(0)
	stat_string= 'Removed from ' + str(tot_meshes) + ' Mesh(es)%t|'
	
	if CLEAN_VERTS_FREE:							stat_string+= 'Verts: %i|' % rem_vert_count
	if CLEAN_EDGE_SMALL or CLEAN_EDGE_NOFACE:		stat_string+= 'Edges: %i|' % rem_edge_count
	if CLEAN_FACE_SMALL or CLEAN_FACE_PERIMETER:	stat_string+= 'Faces: %i|' % rem_face_count
	if CLEAN_MATERIALS:								stat_string+= 'Materials: %i|' % rem_material_count
	if CLEAN_WHITE_VCOL_LAYERS:						stat_string+= 'Color Layers: %i|' % rem_vcol_layer_count
	if CLEAN_VWEIGHT:								stat_string+= 'VWeights: %i|' % rem_vweight_count
	if CLEAN_GROUP:									stat_string+= 'VGroups: %i|' % rem_group_count
	if CLEAN_NAN_VERTS:								stat_string+= 'Vert Nan Fix: %i|' % fix_nan_vcount
	if CLEAN_NAN_UVS:								stat_string+= 'UV Nan Fix: %i|' % fix_nan_uvcount
	Draw.PupMenu(stat_string)
	
	
if __name__ == '__main__':
	main()