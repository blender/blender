#!BPY
"""
Name: 'Same Weights...'
Blender: 245
Group: 'FaceSelect'
Tooltip: 'Select same faces with teh same weight for the active group.'
"""

__author__ = ["Campbell Barton aka ideasman42"]
__url__ = ["www.blender.org", "blenderartists.org", "www.python.org"]
__version__ = "0.1"
__bpydoc__ = """\

Select Same Weights

Select same weights as the active face on the active group.
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

from Blender import Scene, Draw, Mesh
import BPyMesh

def selSameWeights(me, PREF_TOLERENCE):
	
	# Check for missing data
	if not me.faceUV:	return
	
	act_group= me.activeGroup
	if not act_group:	return
	
	act_face = me.faces[me.activeFace]
	if act_face == None:	return
	
	
	
	groupNames, vWeightDict= BPyMesh.meshWeight2Dict(me)
	
	def get_face_weight(f):
		'''
		Return the faces median weight and weight range.
		'''
		wmin = 1.0
		wmax = 0.0
		w = 0.0
		for v in f:
			try:
				new_weight = vWeightDict[v.index][act_group]
				if wmin > new_weight: wmin = new_weight
				if wmax < new_weight: wmax = new_weight
				w += new_weight
			except:
				pass
		return w, wmax-wmin # weight, range
	
	weight_from, weight_range_from = get_face_weight(act_face)
	for f in me.faces:
		if (not f.sel) and f != act_face:
			weight, weight_range = get_face_weight(f)
			
			# Compare the 2 faces weight difference and difference in their contrast.
			if\
			abs(weight - weight_from) <= PREF_TOLERENCE and\
			abs(weight_range - weight_range_from) <= PREF_TOLERENCE:
				f.sel = True


def main():
	scn= Scene.GetCurrent()
	ob= scn.objects.active
	
	if not ob or ob.type != 'Mesh':
		Draw.PupMenu('Error, no active mesh object, aborting.')
		return
	
	me= ob.getData(mesh=1)
	
	PREF_TOLERENCE= Draw.Create(0.1)
	
	pup_block= [\
	('Tolerence:', PREF_TOLERENCE, 0.01, 1.0, 'Tolerence for selecting faces of the same weight.'),\
	]
	
	if not Draw.PupBlock('Select Same Weight...', pup_block):
		return
	
	PREF_TOLERENCE= PREF_TOLERENCE.val
	
	selSameWeights(me, PREF_TOLERENCE)
	
if __name__=='__main__':
	main()