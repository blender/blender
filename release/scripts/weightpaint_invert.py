#!BPY
"""
Name: 'Invert Active Group'
Blender: 245
Group: 'WeightPaint'
Tooltip: 'Invert the active vertex group'
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
from Blender import Scene, Mesh, Window, sys

import BPyMessages
import bpy

def vgroup_invert(ob_orig, me):
	if not me.getVertGroupNames():
		return
	group_act = me.activeGroup
	if group_act == None:
		return
	
	group_data = me.getVertsFromGroup(group_act, 1)
	
	weights= [1.0] * len(me.verts) # 1.0 - initialize inverted
	
	group_data = me.getVertsFromGroup(group_act, 1) # (i,w)  tuples.
	
	me.removeVertGroup(group_act) # messes up the active group.
	for i,w in group_data:
		weights[i] = 1.0-w
	
	me.addVertGroup(group_act)
	
	rep = Blender.Mesh.AssignModes.REPLACE
	vertList= [None]
	for i,weight in enumerate(weights):
		vertList[0] = i
		me.assignVertsToGroup(group_act, vertList, weight, rep)
	
	me.activeGroup = group_act
	me.update()

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
	
	Window.WaitCursor(1)
	me = ob_act.getData(mesh=1) # old NMesh api is default
	t = sys.time()
	
	# Run the mesh editing function
	vgroup_invert(ob_act, me)
	
	# Timing the script is a good way to be aware on any speed hits when scripting
	print 'Invert VGroup in %.2f seconds' % (sys.time()-t)
	Window.WaitCursor(0)
	if is_editmode: Window.EditMode(1)
	
# This lets you can import the script without running it
if __name__ == '__main__':
	main()