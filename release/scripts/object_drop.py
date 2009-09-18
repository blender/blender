#!BPY
"""
Name: 'Drop Onto Ground'
Blender: 249
Group: 'Object'
Tooltip: 'Drop the selected objects onto "ground" objects'
"""
__author__= "Campbell Barton"
__url__= ["blender.org", "blenderartists.org"]
__version__= "1.1"

__bpydoc__= """
"""

# --------------------------------------------------------------------------
# Drop Objects v1.0 by Campbell Barton (AKA Ideasman42)
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


from Blender import Draw, Geometry, Mathutils, Window
from Blender.Mathutils import Vector, AngleBetweenVecs, RotationMatrix
import bpy


GLOBALS = {}
GLOBALS['GROUND_SOURCE'] = [Draw.Create(1), Draw.Create(0)]
GLOBALS['GROUND_GROUP_NAME'] = Draw.Create('terrain')
GLOBALS['DROP_AXIS'] = [Draw.Create(1), Draw.Create(0)]						# on what axis will we drop?
GLOBALS['DROP_OVERLAP_CHECK'] = Draw.Create(1)				# is the terrain a single skin?
GLOBALS['DROP_ORIENT'] = Draw.Create(1)
GLOBALS['DROP_ORIENT_VALUE'] = Draw.Create(100.0)
GLOBALS['EVENT'] = 2
GLOBALS['MOUSE'] = None

def collect_terrain_triangles(obs_terrain):
	terrain_tris = []
	me = bpy.data.meshes.new()
	
	for ob in obs_terrain:
		def blend_face_to_terrain_tris(f):
			no = f.no
			cos = [v.co for v in f]
			if len(cos) == 4:	return [(cos[0], cos[1], cos[2], no), (cos[0], cos[2], cos[3], no)]
			else:				return [(cos[0], cos[1], cos[2], no), ]
		
		# Clear
		me.verts = None
		try:	me.getFromObject(ob)
		except:	pass
		
		me.transform(ob.matrixWorld)
		for f in me.faces: # may be [], thats ok
			terrain_tris.extend( blend_face_to_terrain_tris(f) )
	
	me.verts = None # clear to save ram
	return terrain_tris

def calc_drop_loc(ob, terrain_tris, axis):
	pt = Vector(ob.loc)
	
	isect = None
	isect_best = None
	isect_best_no = None
	isect_best_len = 0.0
	
	for t1,t2,t3, no in terrain_tris:
		#if Geometry.PointInTriangle2D(pt, t1,t2,t3):
		isect = Mathutils.Intersect(t1, t2, t3, axis, pt, 1) # 1==clip
		if isect:
			if not GLOBALS['DROP_OVERLAP_CHECK'].val:
				# Find the first location
				return isect, no
			else:
				if isect_best:
					isect_len = (pt-isect).length
					if isect_len < isect_best_len:
						isect_best_len = isect_len
						isect_best = isect
						isect_best_no = no
					
				else:
					isect_best_len = (pt-isect).length
					isect_best = isect;
					isect_best_no = no
	
	return isect_best, isect_best_no


def error_nogroup():
	Draw.PupMenu('The Group name does not exist')
def error_noact():
	Draw.PupMenu('There is no active object')
def error_noground():
	Draw.PupMenu('No triangles could be found to drop the objects onto')
def error_no_obs():
	Draw.PupMenu('No objects selected to drop')

# event and value arnt used
def terrain_clamp(event, value):
	
	sce = bpy.data.scenes.active
	if GLOBALS['GROUND_SOURCE'][0].val:
		obs_terrain = [sce.objects.active]
		if not obs_terrain[0]:
			error_noact()
			return
	else:
		try:	obs_terrain = bpy.data.groups[ GLOBALS['GROUND_GROUP_NAME'].val ].objects
		except:
			error_nogroup()
			return
	
	obs_clamp = [ob for ob in sce.objects.context if ob not in obs_terrain and not ob.lib]
	if not obs_clamp:
		error_no_obs()
		return
	
	terrain_tris = collect_terrain_triangles(obs_terrain)
	if not terrain_tris:
		error_noground()
		return
	
	
	
	if GLOBALS['DROP_AXIS'][0].val:
		axis = Vector(0,0,-1)
	else:
		axis = Vector(Window.GetViewVector())
	
	do_orient = GLOBALS['DROP_ORIENT'].val
	do_orient_val = GLOBALS['DROP_ORIENT_VALUE'].val/100.0
	if not do_orient_val: do_orient = False
	
	for ob in obs_clamp:
		loc, no = calc_drop_loc(ob, terrain_tris, axis)
		if loc:
			if do_orient:
				try:	ang = AngleBetweenVecs(no, axis)
				except:ang = 0.0
				if ang > 90.0:
					no = -no
					ang = 180.0-ang
				
				if ang > 0.0001:
					ob_matrix = ob.matrixWorld * RotationMatrix(ang * do_orient_val, 4, 'r', axis.cross(no))
					ob.setMatrix(ob_matrix)
			
			ob.loc = loc
	
	# to make the while loop exist
	GLOBALS['EVENT'] = EVENT_EXIT


# UI STUFF ------------------------
def do_axis_z(e,v):	
	GLOBALS['DROP_AXIS'][0].val = 1
	GLOBALS['DROP_AXIS'][1].val = 0
	GLOBALS['EVENT'] = e

def do_axis_view(e,v):
	GLOBALS['DROP_AXIS'][0].val = 0
	GLOBALS['DROP_AXIS'][1].val = 1
	GLOBALS['EVENT'] = e

def do_ground_source_act(e,v):
	GLOBALS['GROUND_SOURCE'][0].val = 1
	GLOBALS['GROUND_SOURCE'][1].val = 0
	GLOBALS['EVENT'] = e

def do_ground_source_group(e,v):
	GLOBALS['GROUND_SOURCE'][0].val = 0
	GLOBALS['GROUND_SOURCE'][1].val = 1
	GLOBALS['EVENT'] = e

def do_ground_group_name(e,v):
	try: g =	bpy.data.groups[v]
	except: g =	None
	if not g:	error_nogroup()
	GLOBALS['EVENT'] = e
	
def do_dummy(e,v):
	GLOBALS['EVENT'] = e

EVENT_NONE = 0
EVENT_EXIT = 1
EVENT_REDRAW = 2
def terain_clamp_ui():
	
	# Only to center the UI
	x,y = GLOBALS['MOUSE']
	x-=40
	y-=70
	
	Draw.Label('Drop Axis', x-70,y+120, 60, 20)
	Draw.BeginAlign()
	GLOBALS['DROP_AXIS'][0] = Draw.Toggle('Z',		EVENT_REDRAW, x+20, y+120, 30, 20, GLOBALS['DROP_AXIS'][0].val, 'Drop down on the global Z axis', do_axis_z)
	GLOBALS['DROP_AXIS'][1] = Draw.Toggle('View Z',	EVENT_REDRAW, x+50, y+120, 70, 20, GLOBALS['DROP_AXIS'][1].val, 'Drop allong the view vector', do_axis_view)
	Draw.EndAlign()
	
	# Source
	Draw.Label('Drop on to...', x-70,y+90, 120, 20)
	Draw.BeginAlign()
	GLOBALS['GROUND_SOURCE'][0] = Draw.Toggle('Active Object',	EVENT_REDRAW, x-70, y+70, 110, 20, GLOBALS['GROUND_SOURCE'][0].val, '', do_ground_source_act)
	GLOBALS['GROUND_SOURCE'][1] = Draw.Toggle('Group',			EVENT_REDRAW, x+40, y+70, 80, 20, GLOBALS['GROUND_SOURCE'][1].val, '', do_ground_source_group)
	if GLOBALS['GROUND_SOURCE'][1].val:
		GLOBALS['GROUND_GROUP_NAME'] = Draw.String('GR:',	EVENT_REDRAW+1001, x-70, y+50, 190, 20, GLOBALS['GROUND_GROUP_NAME'].val, 21, '', do_ground_group_name)
	Draw.EndAlign()
	
	GLOBALS['DROP_OVERLAP_CHECK'] = Draw.Toggle('Overlapping Terrain', EVENT_NONE, x-70, y+20, 190, 20, GLOBALS['DROP_OVERLAP_CHECK'].val, "Check all terrain triangles and use the top most (slow)")
	
	Draw.BeginAlign()
	GLOBALS['DROP_ORIENT'] = Draw.Toggle('Orient Normal', EVENT_REDRAW, x-70, y-10, 110, 20, GLOBALS['DROP_ORIENT'].val, "Rotate objects to the face normal", do_dummy)
	if GLOBALS['DROP_ORIENT'].val:
		GLOBALS['DROP_ORIENT_VALUE'] = Draw.Number('', EVENT_NONE, x+40, y-10, 80, 20, GLOBALS['DROP_ORIENT_VALUE'].val, 0.0, 100.0, "Percentage to orient 0.0 - 100.0")
	Draw.EndAlign()
	
	Draw.PushButton('Drop Objects', EVENT_EXIT, x+20, y-40, 100, 20, 'Drop the selected objects', terrain_clamp)
	
	# So moving the mouse outside the popup exits the while loop
	GLOBALS['EVENT'] = EVENT_EXIT

def main():
	
	# This is to set the position if the popup
	GLOBALS['MOUSE'] = Window.GetMouseCoords()
	
	# hack so the toggle buttons redraw. this is not nice at all
	while GLOBALS['EVENT'] == EVENT_REDRAW:
		Draw.UIBlock(terain_clamp_ui, 0)
	
if __name__ == '__main__':
	main()

GLOBALS.clear()

