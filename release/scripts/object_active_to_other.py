#!BPY
"""
Name: 'Copy Active to Selected'
Blender: 249
Group: 'Object'
Tooltip: 'For every selected object, copy the active to their loc/size/rot'
"""

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


from Blender import Window, sys, Draw
import bpy

def my_object_util(sce):
	ob_act = sce.objects.active
	
	if not ob_act:
		Draw.PupMenu('Error%t|No active object selected')
		return
	
	mats = [(ob, ob.matrixWorld) for ob in sce.objects.context if ob != ob_act]
	
	for ob, m in mats:
		ob_copy = ob_act.copy()
		sce.objects.link(ob_copy)
		ob_copy.setMatrix(m)
		ob_copy.Layers = ob.Layers & (1<<20)-1
		

def main():
	sce = bpy.data.scenes.active
	
	Window.WaitCursor(1)
	my_object_util(sce)
	Window.WaitCursor(0)

if __name__ == '__main__':
	main()
