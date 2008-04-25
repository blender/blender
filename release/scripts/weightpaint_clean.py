#!BPY
"""
Name: 'Clean Weight...'
Blender: 245
Group: 'WeightPaint'
Tooltip: 'Removed verts from groups below a weight limit.'
"""

__author__ = "Campbell Barton aka ideasman42"
__url__ = ["www.blender.org", "blenderartists.org", "www.python.org"]
__version__ = "0.1"
__bpydoc__ = """\

Clean Weight

This Script is to be used only in weight paint mode,
It removes very low weighted verts from the current group with a weight option.
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

from Blender import Scene, Draw, Object
import BPyMesh
def weightClean(me, PREF_THRESH, PREF_KEEP_SINGLE, PREF_OTHER_GROUPS):
	
	groupNames, vWeightDict= BPyMesh.meshWeight2Dict(me)
	act_group= me.activeGroup
	
	rem_count = 0
	
	if PREF_OTHER_GROUPS:
		for wd in vWeightDict:
			l = len(wd)
			if not PREF_KEEP_SINGLE or l > 1:
				for group in wd.keys():
					w= wd[group]
					if w <= PREF_THRESH:
						# small weight, remove.
						del wd[group]
						rem_count +=1
					l-=1
					
					if PREF_KEEP_SINGLE and l == 1:
						break
	
	else:
		for wd in vWeightDict:
			if not PREF_KEEP_SINGLE or len(wd) > 1:
				try:
					w= wd[act_group]
					if w <= PREF_THRESH:
						# small weight, remove.
						del wd[act_group]
						rem_count +=1
				except:
					pass
	
	# Copy weights back to the mesh.
	BPyMesh.dict2MeshWeight(me, groupNames, vWeightDict)
	return rem_count


def main():
	scn= Scene.GetCurrent()
	ob= scn.objects.active
	
	if not ob or ob.type != 'Mesh':
		Draw.PupMenu('Error, no active mesh object, aborting.')
		return
	
	me= ob.getData(mesh=1)
	
	PREF_PEAKWEIGHT= Draw.Create(0.001)
	PREF_KEEP_SINGLE= Draw.Create(1)
	PREF_OTHER_GROUPS= Draw.Create(0)
	
	pup_block= [\
	('Peak Weight:', PREF_PEAKWEIGHT, 0.005, 1.0, 'Remove verts from groups below this weight.'),\
	('All Other Groups', PREF_OTHER_GROUPS, 'Clean all groups, not just the current one.'),\
	('Keep Single User', PREF_KEEP_SINGLE, 'Keep verts in at least 1 group.'),\
	]
	
	if not Draw.PupBlock('Clean Selected Meshes...', pup_block):
		return
	
	rem_count = weightClean(me, PREF_PEAKWEIGHT.val, PREF_KEEP_SINGLE.val, PREF_OTHER_GROUPS.val)
	
	# Run on entire blend file. usefull sometimes but dont let users do it.
	'''
	rem_count = 0
	for ob in Object.Get():
		if ob.type != 'Mesh':
			continue
		me= ob.getData(mesh=1)
		
		rem_count += weightClean(me, PREF_PEAKWEIGHT.val, PREF_KEEP_SINGLE.val, PREF_OTHER_GROUPS.val)
	'''
	Draw.PupMenu('Removed %i verts from groups' % rem_count)
	
if __name__=='__main__':
	main()