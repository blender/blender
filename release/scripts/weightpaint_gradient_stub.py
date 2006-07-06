#!BPY
"""
Name: 'Weight Gradient...'
Blender: 241
Group: 'WeightPaint'
Tooltip: 'Click on the start and end grad points for the mesh for selected faces.'
"""

__author__ = ["Campbell Barton"]
__url__ = ("blender", "elysiun", "http://members.iinet.net.au/~cpbarton/ideasman/")
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

import __vertex_gradient__
reload(__vertex_gradient__)
import Blender

def main():
	scn= Blender.Scene.GetCurrent()
	ob= scn.getActiveObject()
	
	if not ob or ob.getType() != 'Mesh':
		Blender.Draw.PupMenu('Error, no active mesh object, aborting.')
		return
	# MODE 0 == VCOL
	# MODE 1 == WEIGHT
	MODE= 0
	__vertex_gradient__.vertexGradientPick(ob, MODE)


if __name__ == '__main__':
	main()
	