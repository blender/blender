#!BPY

"""
Name: 'Envelope via Group Objects'
Blender: 242
Group: 'WeightPaint'
Tooltip: 'Assigns weights to vertices via object envelopes'
"""

__author__ = ["Campbell Barton"]
__url__ = ("blender", "blenderartist.org")
__version__ = "0.1"

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

def intersection_data(ob):
	'''
	Takes an object and returns raw triangles and tri bounds.
	returns a list of (v1,v2,v3, xmin, ymin, xmax, ymax)
	... to be used for ray intersection
	'''
	global tot_ymin, tot_xmin, tot_ymax, tot_xmax, tot_zmin, tot_zmax
	tot_zmin = tot_ymin = tot_xmin =  100000000.0
	tot_zmax = tot_ymax = tot_xmax = -100000000.0
	
	def tribounds_xy(v1,v2,v3):
		'''
		return the 3 vecs with bounds, also update tot_*min/max
		'''
		global tot_ymin, tot_xmin, tot_ymax, tot_xmax, tot_zmin, tot_zmax
		xmin = xmax = v1.x
		ymin = ymax = v1.y
		
		x = v2.x
		y = v2.y
		if x>xmax: xmax = x
		if y>ymax: ymax = y
		if x<xmin: xmin = x
		if y<ymin: ymin = y
		
			
		x = v3.x
		y = v3.y
		if x>xmax: xmax = x
		if y>ymax: ymax = y
		if x<xmin: xmin = x
		if y<ymin: ymin = y
		
		# totbounds
		for z in v1.z, v2.z, v3.z:
			if z>tot_zmax: tot_zmax = z
			if z<tot_zmin: tot_zmin = z
		
		# maintain global min/max for x and y
		if xmax>tot_xmax: tot_xmax = xmax
		if xmin<tot_xmin: tot_xmin = xmin
		
		if ymax>tot_ymax: tot_ymax = ymax
		if ymin<tot_ymin: tot_ymin = ymin
		
		return v1,v2,v3, xmin, ymin, xmax, ymax
	
	me = BPyMesh.getMeshFromObject(ob)
	
	if not me:
		return []
	
	me.transform(ob.matrixWorld)
	
	mesh_data = []
	
	# copy all the vectors so when the mesh is free'd we still have access.
	mesh_vert_cos= [v.co.copy() for v in me.verts]
	
	for f in me.faces:
		f_v = [mesh_vert_cos[v.index] for v in f]
		mesh_data.append( tribounds_xy(f_v[0], f_v[1], f_v[2]) )
		if len(f) == 4:
			mesh_data.append( tribounds_xy(f_v[0], f_v[2], f_v[3]) )
	
	me.verts = None # free some memory
	
	return ob.name, mesh_data, tot_xmin, tot_ymin, tot_zmin, tot_xmax, tot_ymax, tot_zmax
	
from Blender import *
ray = Mathutils.Vector(0,0,-1)
def point_in_data(point, mesh_data_tuple):
	ob_name, mesh_data, tot_xmin, tot_ymin, tot_zmin, tot_xmax, tot_ymax, tot_zmax = mesh_data_tuple
	
	Intersect = Mathutils.Intersect
	x = point.x
	y = point.y
	z = point.z
	
	# Bouds check for all mesh data
	if not (\
	tot_zmin < z < tot_zmax and\
	tot_ymin < y < tot_ymax and\
	tot_xmin < x < tot_xmax\
	):
		return False
	
	def isect(tri_data):
		v1,v2,v3, xmin, ymin, xmax, ymax = tri_data
		if not (xmin < x < xmax and ymin < y < ymax):
			return False
		
		if v1.z < z and v2.z < z and v3.z < z:
			return False
		
		isect = Intersect(v1, v2,v3, ray, point, 1) # Clipped.
		if isect and isect.z > z: # This is so the ray only counts if its above the point. 
			return True
		else:
			return False
	
	# return len([None for tri_data in mesh_data if isect(tri_data)]) % 2
	return len( filter(isect, mesh_data) ) % 2

import BPyMesh
def env_from_group(ob_act, grp, PREF_UPDATE_ACT=True):
	
	me = ob_act.getData(mesh=1)
	
	if PREF_UPDATE_ACT:
		act_group = me.activeGroup
		if act_group == None:
			Draw.PupMenu('Error%t|No active vertex group.')
			return
		
		try:
			ob = Object.Get(act_group)
		except:
			Draw.PupMenu('Error%t|No object named "'+ act_group +'".')
			return
		
		group_isect = intersection_data(ob)
		
	else:
		
		# get intersection data
		# group_isect_data = [intersection_data(ob) for ob in group.objects]
		group_isect_data = []
		for ob in grp.objects:
			if ob != ob_act: # in case we're in the group.
				gid = intersection_data(ob)
				if gid[1]: # has some triangles?
					group_isect_data.append( gid )
					
					# we only need 1 for the active group
					if PREF_UPDATE_ACT:
						break
	
		# sort by name
		group_isect_data.sort()
	
	if PREF_UPDATE_ACT:
		group_names, vweight_list = BPyMesh.meshWeight2List(me)
		group_index = group_names.index(act_group)
	else:
		group_names = [gid[0] for gid in group_isect_data]
		vweight_list= [[0.0]* len(group_names) for i in xrange(len(me.verts))]
	
	
	
	ob_act_mat = ob_act.matrixWorld
	for vi, v in enumerate(me.verts):
		# Get all the groups for this vert
		co = v.co * ob_act_mat
		
		if PREF_UPDATE_ACT:
			# only update existing
			if point_in_data(co, group_isect):	w = 1.0
			else:								w = 0.0
			vweight_list[vi][group_index] = w
			
		else:
			# generate new vgroup weights.
			for group_index, group_isect in enumerate(group_isect_data):
				if point_in_data(co, group_isect):
					vweight_list[vi][group_index] = 1.0
	
	BPyMesh.list2MeshWeight(me, group_names, vweight_list)

import BPyMessages
def main():
	
	scn= Scene.GetCurrent()
	ob_act = scn.objects.active
	if not ob_act or ob_act.type != 'Mesh':
		BPyMessages.Error_NoMeshActive()
		return
	
	PREF_ENV_GROUPNAME= Draw.Create('')	
	PREF_UPDATE_ACT= Draw.Create(True)
	pup_block= [\
	('Update Active', PREF_UPDATE_ACT, 'Only apply envelope weights to the active group.'),\
	'...or initialize from group',\
	('GR:', PREF_ENV_GROUPNAME, 0, 21, 'The name of an existing groups, each member will be used as a weight envelope'),\
	]
	
	if not Draw.PupBlock('Envelope From Group...', pup_block):
		return
	
	PREF_UPDATE_ACT= PREF_UPDATE_ACT.val
	
	if not PREF_UPDATE_ACT:
		try:
			grp = Group.Get(PREF_ENV_GROUPNAME.val)
		except:	
			Draw.PupMenu('Error%t|Group "' + PREF_ENV_GROUPNAME.val + '" does not exist.')
			return
	else:
		grp = None
	
	Window.WaitCursor(1)
	t = sys.time()
	env_from_group(ob_act, grp, PREF_UPDATE_ACT)
	print 'assigned envelopes in:', sys.time() - t
	Window.WaitCursor(0)

if __name__ == '__main__':
	main()
