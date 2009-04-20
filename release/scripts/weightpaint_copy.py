#!BPY
"""
Name: 'Copy Active Group...'
Blender: 243
Group: 'WeightPaint'
Tooltip: 'Copy the active group to a new one'
"""

__author__ = ["Campbell Barton"]
__url__ = ("blender.org",)
__version__ = "0.1"
__bpydoc__ = """\

Active Group Copy

This script makes a copy of the active group
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

from Blender import Scene, Draw, Window, Mesh
import BPyMesh
SMALL_NUM= 0.000001
def copy_act_vgroup(me, PREF_NAME, PREF_SEL_ONLY):
	Window.WaitCursor(1)
	groupNames, vWeightDict= BPyMesh.meshWeight2Dict(me)
	act_group= me.activeGroup
	
	if not PREF_SEL_ONLY:
		for wd in vWeightDict:
			try:		wd[PREF_NAME] = wd[act_group]
			except:		pass
	else:
		# Selected faces only
		verts = {} # should use set
		for f in me.faces:
			if f.sel:
				for v in f:
					verts[v.index] = None
		
		for i in verts.iterkeys():
			wd = vWeightDict[i]
			try:		wd[PREF_NAME] = wd[act_group]
			except:		pass
		
		
	
	groupNames.append(PREF_NAME)
	# Copy weights back to the mesh.
	BPyMesh.dict2MeshWeight(me, groupNames, vWeightDict)
	Window.WaitCursor(0)

def main():
	scn= Scene.GetCurrent()
	ob= scn.objects.active
	
	if not ob or ob.type != 'Mesh':
		Draw.PupMenu('Error, no active mesh object, aborting.')
		return
	
	me= ob.getData(mesh=1)
	act_group= me.activeGroup
	
	PREF_NAME= Draw.Create(act_group + '_copy')
	PREF_SEL_ONLY = Draw.Create(0)
	
	pup_block= [\
	('', PREF_NAME, 0, 31, 'Name of group copy.'),\
	('Only Selected', PREF_SEL_ONLY, 'Only include selected faces in the new grop.'),\
	]
	
	if not Draw.PupBlock('New Name...', pup_block):
		return
	PREF_NAME = PREF_NAME.val
	PREF_SEL_ONLY = PREF_SEL_ONLY.val
	copy_act_vgroup(me, PREF_NAME, PREF_SEL_ONLY)
	
	try:	me.activeGroup = PREF_NAME
	except: pass

if __name__=='__main__':
	main()