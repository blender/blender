#!BPY

"""
Name: 'Selection to DupliGroup'
Blender: 243
Group: 'Object'
Tooltip: 'Turn the selection into a dupliGroup using the active objects transformation, objects are moved into a new scene'
"""

__bpydoc__=\
'''
'''

# ***** BEGIN GPL LICENSE BLOCK *****
#
# Script copyright (C) Campbell Barton
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

def main():
	scn = Scene.GetCurrent()
	ob_act = scn.objects.active
	
	if not ob_act:
		Draw.PupMenu('Error%t|No active object')
	
	dup_name = ob_act.name
	
	obsel = list(scn.objects.context)
	
	# Sanity check
	for ob in obsel:
		parent = ob.parent
		if parent:
			if not parent.sel or parent not in obsel:
				Draw.PupMenu('Error%t|Objects "'+ob.name+'" parent "'+parent.name+'" is not in the selection')
				return
	
	mat_act = ob_act.matrixWorld
	
	# new group
	grp = Group.New(dup_name)
	grp.objects = obsel
	
	# Create the new empty object to be the dupli
	ob_dup = scn.objects.new('Empty')
	ob_dup.setMatrix(mat_act)
	ob_dup.name = dup_name + '_dup'
	
	ob_dup.enableDupGroup = True
	ob_dup.DupGroup = grp
	
	scn_new = Scene.New(dup_name)
	
	# Transform the objects to remove the active objects matrix.
	mat_act_inv = mat_act.copy().invert()
	for ob in obsel:
		if not ob.parent:
			ob.setMatrix(ob.matrixWorld * mat_act_inv)
		
		scn_new.objects.link(ob)
		scn.objects.unlink(ob)	


if __name__ == '__main__':
	main()
