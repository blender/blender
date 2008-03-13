#!BPY
"""
Name: 'Vertex Groups Island Average'
Blender: 243
Group: 'WeightPaint'
Tooltip: 'Average the vertex weights for each connected set of verts'
"""

# -------------------------------------------------------------------------- 
# ***** BEGIN GPL LICENSE BLOCK ***** 
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
from Blender import Scene, Mesh, Window, sys, Draw
from BPyMesh import meshWeight2List, list2MeshWeight, mesh2linkedFaces

import BPyMessages
import bpy

def faceGroups2VertSets(face_groups):
	'''	Return the face groups as sets of vert indicies	'''
	return [set([v.index for f in fg for v in f]) for fg in face_groups]


def vgroup_average(ob_orig, me, sce, PREF_ALL_VGROUPS=True):
	if not me.getVertGroupNames():
		return
	
	weight_names, weight_list = meshWeight2List(me)
	
	weight_names_len = len(weight_names)
	vgroup_dummy = [0.0] * weight_names_len
	vgroup_range = range(weight_names_len)
	
	if not PREF_ALL_VGROUPS:
		weight_active_index = weight_names.index(me.activeGroup)
	
	for vert_set in faceGroups2VertSets( mesh2linkedFaces(me) ):
		if not vert_set:
			continue
		
		
		if PREF_ALL_VGROUPS:
			# We need to average the vgroups
			collected_group = vgroup_dummy[:]
			for i in vert_set:
				vert_group = weight_list[i]			# get the original weight
				weight_list[i] = collected_group	# replace with the collected group
				
				for j in vgroup_range: # iter through the vgroups
					collected_group[j] += vert_group[j]
			
			for j in vgroup_range:
				collected_group[j] /= len(vert_set)
		else:
			# Active group only
			vert_weight = 0.0
			for i in vert_set:
				vert_weight += weight_list[i][weight_active_index]
			
			vert_weight /= len(vert_set)
			
			for i in vert_set:
				weight_list[i][weight_active_index] = vert_weight
	
	list2MeshWeight(me, weight_names, weight_list)

def main():
	
	# Gets the current scene, there can be many scenes in 1 blend file.
	sce = bpy.data.scenes.active
	
	# Get the active object, there can only ever be 1
	# and the active object is always the editmode object.
	ob_act = sce.objects.active
	
	if not ob_act or ob_act.type != 'Mesh':
		BPyMessages.Error_NoMeshActive()
		return 
	
	# Saves the editmode state and go's out of 
	# editmode if its enabled, we cant make
	# changes to the mesh data while in editmode.
	is_editmode = Window.EditMode()
	Window.EditMode(0)
	
	PREF_ALL_VGROUPS = Draw.PupMenu("All Groups?%t|All Groups%x1|Active Group Only%x0")
	if PREF_ALL_VGROUPS==-1:
		return
	
	Window.WaitCursor(1)
	me = ob_act.getData(mesh=1) # old NMesh api is default
	t = sys.time()
	
	# Run the mesh editing function
	vgroup_average(ob_act, me, sce, PREF_ALL_VGROUPS)
	
	# Timing the script is a good way to be aware on any speed hits when scripting
	print 'Average VGroups in %.2f seconds' % (sys.time()-t)
	Window.WaitCursor(0)
	if is_editmode: Window.EditMode(1)
	
	
# This lets you can import the script without running it
if __name__ == '__main__':
	main()