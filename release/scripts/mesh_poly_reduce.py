#!BPY
"""
Name: 'Poly Reducer'
Blender: 243
Group: 'Mesh'
Tooltip: 'Removed polygons from a mesh while maintaining the shape, textures and weights.'
"""

__author__ = "Campbell Barton"
__url__ = ("blender", "blenderartists.org")
__version__ = "1.0 2006/02/07"

__bpydoc__ = """\
This script simplifies the mesh by removing faces, keeping the overall shape of the mesh.
"""

from Blender import Draw, Window, Scene, Mesh, Mathutils, sys, Object
import BPyMesh
# reload(BPyMesh)
import BPyMessages

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


def main():
	scn = Scene.GetCurrent()
	act_ob= scn.objects.active
	if not act_ob or act_ob.type != 'Mesh':
		BPyMessages.Error_NoMeshActive()
		return
	
	act_me= act_ob.getData(mesh=1)
	
	if act_me.multires:
		BPyMessages.Error_NoMeshMultiresEdit()
		return
	
	act_group= act_me.activeGroup
	if not act_group: act_group= ''
	
	
	# Defaults
	PREF_REDUX= Draw.Create(0.5)
	PREF_BOUNDRY_WEIGHT= Draw.Create(5.0)
	PREF_REM_DOUBLES= Draw.Create(1)
	PREF_FACE_AREA_WEIGHT= Draw.Create(1.0)
	PREF_FACE_TRIANGULATE= Draw.Create(1)
	
	VGROUP_INF_ENABLE= Draw.Create(0)
	VGROUP_INF_REDUX= Draw.Create(act_group)
	VGROUP_INF_WEIGHT= Draw.Create(10.0)
	
	PREF_DO_UV= Draw.Create(1)
	PREF_DO_VCOL= Draw.Create(1)
	PREF_DO_WEIGHTS= Draw.Create(1)
	PREF_OTHER_SEL_OBS= Draw.Create(0)
	
	pup_block = [\
	('Poly Reduce:', PREF_REDUX, 0.05, 0.95, 'Scale the meshes poly count by this value.'),\
	('Boundry Weight:', PREF_BOUNDRY_WEIGHT, 0.0, 20.0, 'Weight boundry verts by this scale, 0.0 for no boundry weighting.'),\
	('Area Weight:', PREF_FACE_AREA_WEIGHT, 0.0, 20.0, 'Collapse edges effecting lower area faces first.'),\
	('Triangulate', PREF_FACE_TRIANGULATE, 'Convert quads to tris before reduction, for more choices of edges to collapse.'),\
	'',\
	('VGroup Weighting', VGROUP_INF_ENABLE, 'Use a vertex group to influence the reduction, higher weights for higher quality '),\
	('vgroup name: ', VGROUP_INF_REDUX, 0, 32, 'The name of the vertex group to use for the weight map'),\
	('vgroup mult: ', VGROUP_INF_WEIGHT, 0.0, 100.0, 'How much to make the weight effect the reduction'),\
	('Other Selected Obs', PREF_OTHER_SEL_OBS, 'reduce other selected objects.'),\
	'',\
	'',\
	'',\
	('UV Coords', PREF_DO_UV, 'Interpolate UV Coords.'),\
	('Vert Colors', PREF_DO_VCOL, 'Interpolate Vertex Colors'),\
	('Vert Weights', PREF_DO_WEIGHTS, 'Interpolate Vertex Weights'),\
	('Remove Doubles', PREF_REM_DOUBLES, 'Remove doubles before reducing to avoid boundry tearing.'),\
	]
	
	if not Draw.PupBlock("Poly Reducer", pup_block):
		return
	
	PREF_REDUX= PREF_REDUX.val
	PREF_BOUNDRY_WEIGHT= PREF_BOUNDRY_WEIGHT.val
	PREF_REM_DOUBLES= PREF_REM_DOUBLES.val
	PREF_FACE_AREA_WEIGHT= PREF_FACE_AREA_WEIGHT.val
	PREF_FACE_TRIANGULATE= PREF_FACE_TRIANGULATE.val
	
	VGROUP_INF_ENABLE= VGROUP_INF_ENABLE.val
	VGROUP_INF_WEIGHT= VGROUP_INF_WEIGHT.val
	
	if VGROUP_INF_ENABLE and VGROUP_INF_WEIGHT:
		VGROUP_INF_REDUX= VGROUP_INF_REDUX.val
	else:
		VGROUP_INF_WEIGHT= 0.0
		VGROUP_INF_REDUX= None
		
		
	PREF_DO_UV= PREF_DO_UV.val
	PREF_DO_VCOL= PREF_DO_VCOL.val
	PREF_DO_WEIGHTS= PREF_DO_WEIGHTS.val
	PREF_OTHER_SEL_OBS= PREF_OTHER_SEL_OBS.val
	
	
	t= sys.time()
	
	is_editmode = Window.EditMode() # Exit Editmode.
	if is_editmode: Window.EditMode(0)
	Window.WaitCursor(1)	
	print 'reducing:', act_ob.name, act_ob.getData(1)
	BPyMesh.redux(act_ob, PREF_REDUX, PREF_BOUNDRY_WEIGHT, PREF_REM_DOUBLES, PREF_FACE_AREA_WEIGHT, PREF_FACE_TRIANGULATE, PREF_DO_UV, PREF_DO_VCOL, PREF_DO_WEIGHTS, VGROUP_INF_REDUX, VGROUP_INF_WEIGHT)
	
	if PREF_OTHER_SEL_OBS:
		for ob in scn.objects.context:
			if ob.type == 'Mesh' and ob != act_ob:
				print 'reducing:', ob.name, ob.getData(1)
				BPyMesh.redux(ob, PREF_REDUX, PREF_BOUNDRY_WEIGHT, PREF_REM_DOUBLES, PREF_FACE_AREA_WEIGHT, PREF_FACE_TRIANGULATE, PREF_DO_UV, PREF_DO_VCOL, PREF_DO_WEIGHTS, VGROUP_INF_REDUX, VGROUP_INF_WEIGHT)
				Window.RedrawAll()
	
	if is_editmode: Window.EditMode(1)
	Window.WaitCursor(0)
	Window.RedrawAll()
	
	print 'Reduction done in %.6f sec.' % (sys.time()-t)

if __name__ == '__main__':
	main()