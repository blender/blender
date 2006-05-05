#!BPY
"""
Name: 'Clean Weight...'
Blender: 241
Group: 'WeightPaint'
Tooltip: 'Removed verts from groups below a weight limit.'
"""

__author__ = ["Campbell Barton"]
__url__ = ("blender", "elysiun", "http://members.iinet.net.au/~cpbarton/ideasman/")
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

from Blender import Scene, Draw
import BPyMesh
SMALL_NUM= 0.000001
def actWeightNormalize(me, PREF_THRESH, PREF_KEEP_SINGLE):
	
	groupNames, vWeightDict= BPyMesh.meshWeight2Dict(me)
	act_group= me.activeGroup
	
	for wd in vWeightDict:
		if not PREF_KEEP_SINGLE or len(wd) > 1:
			try:
				w= wd[act_group]
				if w <= PREF_THRESH:
					# small weight, remove.
					del wd[act_group]
			except:
				pass
	
	# Copy weights back to the mesh.
	BPyMesh.dict2MeshWeight(me, groupNames, vWeightDict)


def main():
	scn= Scene.GetCurrent()
	ob= scn.getActiveObject()
	
	if not ob or ob.getType() != 'Mesh':
		Draw.PupMenu('Error, no active mesh object, aborting.')
		return
	
	me= ob.getData(mesh=1)
	
	PREF_PEAKWEIGHT= Draw.Create(0.005)
	PREF_KEEP_SINGLE= Draw.Create(1)
	
	pup_block= [\
	('Peak Weight:', PREF_PEAKWEIGHT, 0.01, 1.0, 'Upper weight for normalizing.'),\
	('Keep Single User', PREF_KEEP_SINGLE, 'Dont remove verts that are in this group only.'),\
	]
	
	if not Draw.PupBlock('Clean Selected Meshes...', pup_block):
		return
	
	PREF_PEAKWEIGHT= PREF_PEAKWEIGHT.val
	PREF_KEEP_SINGLE= PREF_KEEP_SINGLE.val
	
	actWeightNormalize(me, PREF_PEAKWEIGHT, PREF_KEEP_SINGLE)
	
if __name__=='__main__':
	main()