# --------------------------------------------------------------------------
# BPyImage.py version 0.15
# --------------------------------------------------------------------------
# helper functions to be used by other scripts
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

from Blender import *

def curve2vecs(ob, WORLDSPACE= True):
	'''
	Takes a curve object and retuirns a list of vec lists (polylines)
	one list per curve
	
	This is usefull as a way to get a polyline per curve
	so as not to have to deal with the spline types directly
	'''
	if ob.type != 'Curve':
		raise 'must be a curve object'
	
	me_dummy = Mesh.New()
	me_dummy.getFromObject(ob)
	
	if WORLDSPACE:
		me_dummy.transform(ob.matrixWorld)
	
	# build an edge dict
	edges = {} # should be a set
	
	def sort_pair(i1, i2):
		if i1 > i2:		return i2, i1
		else:			return i1, i2
	
	for ed in me_dummy.edges:
		edges[sort_pair(ed.v1.index,ed.v2.index)] = None # dummy value
	
	# now set the curves
	first_time = True
	
	current_vecs = []
	vec_list = [current_vecs]
	
	for v in me_dummy.verts:
		if first_time:
			first_time = False
			current_vecs.append(v.co.copy())
			last_index = v.index
		else:
			index = v.index
			if edges.has_key(sort_pair(index, last_index)):
				current_vecs.append( v.co.copy() )
			else:
				current_vecs = []
				vec_list.append(current_vecs)
			
			last_index = index
	
	me_dummy.verts = None
	
	return vec_list
	

