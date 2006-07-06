#!BPY
"""
Name: 'VCol Gradient...'
Blender: 241
Group: 'VertexPaint'
Tooltip: 'Click on the start and end grad points for the mesh for selected faces.'
"""

__author__ = ["Campbell Barton"]
__url__ = ("blender", "elysiun", "http://members.iinet.net.au/~cpbarton/ideasman/")
__version__ = "0.1"

import __vertex_gradient__
reload(__vertex_gradient__)
import Blender

def main():
	scn= Blender.Scene.GetCurrent()
	ob= scn.getActiveObject()
	
	if not ob or ob.getType() != 'Mesh':
		Blender.Draw.PupMenu('Error, no active mesh object, aborting.')
		return
	
	__vertex_gradient__.vertexGradientPick(ob, 1)
	
	
if __name__ == '__main__':
	main()