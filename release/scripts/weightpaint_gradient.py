#!BPY
"""
Name: 'Weight Gradient...'
Blender: 245
Group: 'WeightPaint'
Tooltip: 'Click on the start and end grad points for the mesh for selected faces.'
"""

__author__ = "Campbell Barton aka ideasman42"
__url__ = ["www.blender.org", "blenderartists.org", "www.python.org"]
__version__ = "0.1"
__bpydoc__=\
'''
This script is used to fill the selected faces with a gradient
To use the script, switch to "Face Select" mode then "Vertex Paint" mode
Select the faces you wish to apply the gradient to.
Click twice on the mesh to set the start and end points of the gradient.
The color under the mouse will be used for the start and end blend colors.
Note:
Holding Shift or clicking outside the mesh on the second click will blend the first colour to nothing.	
'''

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

import mesh_gradient
import Blender

def main():
	scn= Blender.Scene.GetCurrent()
	ob= scn.objects.active
	
	if not ob or ob.type != 'Mesh':
		Blender.Draw.PupMenu('Error, no active mesh object, aborting.')
		return
	# MODE 0 == VCOL
	# MODE 1 == WEIGHT
	MODE= 0
	mesh_gradient.vertexGradientPick(ob, MODE)


if __name__ == '__main__':
	main()
	
