#!BPY
"""
Name: 'Poly Reducer'
Blender: 241
Group: 'Mesh'
Tooltip: 'Removed polygons from a mesh while maintaining the shape, textures and weights.'
"""

from Blender import Draw, Window, Scene, Mesh, Mathutils, sys, Object
import BPyMesh
reload(BPyMesh)


def main():
	scn = Scene.GetCurrent()
	act_ob= scn.getActiveObject()
	if not act_ob or act_ob.getType()!='Mesh':
		Draw.PupMenu('Error, select a mesh as your active object')
		return
	
	
	act_me= act_ob.getData(mesh=1)
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
	
	pup_block = [\
	('Poly Reduce:', PREF_REDUX, 0.05, 0.95, 'Scale the meshes poly count by this value.'),\
	'',\
	('Boundry Weight:', PREF_BOUNDRY_WEIGHT, 0.0, 20.0, 'Weight boundry verts by this scale, 0.0 for no boundry weighting.'),\
	('Area Weight:', PREF_FACE_AREA_WEIGHT, 0.0, 20.0, 'Collapse edges effecting lower area faces first.'),\
	('Triangulate', PREF_FACE_TRIANGULATE, 'Convert quads to tris before reduction, for more choices of edges to collapse.'),\
	'',\
	('VGroup Weighting', VGROUP_INF_ENABLE, 'Use a vertex group to influence the reduction, higher weights for higher quality '),\
	('vgroup name: ', VGROUP_INF_REDUX, 0, 32, 'The name of the vertex group to use for the weight map'),\
	('vgroup mult: ', VGROUP_INF_WEIGHT, 0.0, 100.0, 'How much to make the weight effect the reduction'),\
	'',\
	('UV Coords', PREF_DO_UV, 'Interpolate UV Coords.'),\
	('Vert Colors', PREF_DO_VCOL, 'Interpolate Vertex Colors'),\
	('Vert Weights', PREF_DO_WEIGHTS, 'Interpolate Vertex Weights'),\
	('Remove Doubles', PREF_REM_DOUBLES, 'Remove doubles before reducing to avoid boundry tearing.'),\
	]
	
	if not Draw.PupBlock("X Mirror mesh tool", pup_block):
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
	
	
	
	t= sys.time()
	
	is_editmode = Window.EditMode() # Exit Editmode.
	if is_editmode: Window.EditMode(0)
	Window.WaitCursor(1)	
	
	BPyMesh.redux(act_ob, PREF_REDUX, PREF_BOUNDRY_WEIGHT, PREF_REM_DOUBLES, PREF_FACE_AREA_WEIGHT, PREF_FACE_TRIANGULATE, PREF_DO_UV, PREF_DO_VCOL, PREF_DO_WEIGHTS, VGROUP_INF_REDUX, VGROUP_INF_WEIGHT)
	
	if is_editmode: Window.EditMode(1)
	Window.WaitCursor(0)
	Window.RedrawAll()
	
	print 'Reduction done in %.6f sec.' % (sys.time()-t)

if __name__ == '__main__':
	main()