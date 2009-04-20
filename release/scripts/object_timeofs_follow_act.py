#!BPY
"""
Name: 'TimeOffset follow Active'
Blender: 245
Group: 'Object'
Tooltip: 'ActObs animated loc sets TimeOffset on other objects at closest frame'
"""
__author__= "Campbell Barton"
__url__= ["blender.org", "blenderartists.org"]
__version__= "1.0"

__bpydoc__= """
"""

# --------------------------------------------------------------------------
# TimeOffset follow Active v1.0 by Campbell Barton (AKA Ideasman42)
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
from Blender import Image, sys, Draw, Window, Scene, Group
import bpy
import BPyMessages


def main():

	sce = Scene.GetCurrent()

	ob_act = sce.objects.active
	 
	if not ob_act:
		Draw.PupMenu("Error%t|no active object")
		return
	
	objects = list(sce.objects.context)
	
	try:	objects.remove(ob_act)
	except: pass
	
	if not objects:
		Draw.PupMenu("Error%t|no objects selected")
		return
	
	curframe = Blender.Get('curframe')
	
	FRAME_START= Draw.Create( Blender.Get('staframe') )
	FRAME_END= Draw.Create( Blender.Get('endframe') )
		
	# Get USER Options
	pup_block= [\
	('Start:', FRAME_START, 1, 300000, 'Use the active objects position starting at this frame'),\
	('End:', FRAME_END, 1, 300000, 'Use the active objects position starting at this frame'),\
	]
	
	if not Draw.PupBlock('Set timeoffset...', pup_block):
		return
	
	FRAME_START = FRAME_START.val
	FRAME_END = FRAME_END.val
	
	if FRAME_START >= FRAME_END:
		Draw.PupMenu("Error%t|frames are not valid")
	
	
	# Ok - all error checking 
	locls_act = []
	for f in xrange((FRAME_END-FRAME_START)):
		i = FRAME_START+f
		Blender.Set('curframe', i)
		locls_act.append(ob_act.matrixWorld.translationPart())
	
	for ob in objects:
		loc = ob.matrixWorld.translationPart()
		best_frame = -1
		best_dist = 100000000
		for i, loc_act in enumerate(locls_act):
			dist = (loc_act-loc).length
			if dist < best_dist:
				best_dist = dist
				best_frame = i + FRAME_START
		
		ob.timeOffset = float(best_frame)
	
	# Set the original frame
	Blender.Set('curframe', curframe)

if __name__ == '__main__':
	main()

