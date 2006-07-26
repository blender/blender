#!BPY

"""
Name: 'Randomize Loc Size Rot'
Blender: 241
Group: 'Object'
Tooltip: 'Randomize the selected objects Loc Size Rot'
"""

__bpydoc__=\
'''
This script randomizes the selected objects location/size/rotation.
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

from Blender import Object, Draw
from Blender.Mathutils import Rand

def rnd():
	return Rand()-0.5

def randomize(sel, PREF_LOC, PREF_SIZE, PREF_ROT, PREF_LINK_AXIS, PREF_X_AXIS, PREF_Y_AXIS, PREF_Z_AXIS):
	for ob in sel:
		if PREF_LOC:
			if PREF_LINK_AXIS:
				rand = PREF_LOC*rnd()
				ob.loc = (ob.LocX+(rand*PREF_X_AXIS),  ob.LocY+(rand*PREF_Y_AXIS),  ob.LocZ+(rand*PREF_Z_AXIS))
			else:
				ob.loc = (ob.LocX+(PREF_LOC*rnd()),  ob.LocY+(PREF_LOC*rnd()),  ob.LocZ+(PREF_LOC*rnd()))
				
		if PREF_SIZE:
			if PREF_LINK_AXIS:
				rand = 1 + (PREF_SIZE*rnd())
				if PREF_X_AXIS:	x= rand
				else:			x= 1
				if PREF_Y_AXIS:	y= rand
				else:			y= 1
				if PREF_Z_AXIS:	z= rand
				else:			z= 1
				ob.size = (ob.SizeX*x,  ob.SizeY*y,  ob.SizeZ*z)
			else:
				if PREF_X_AXIS:	x= 1+ PREF_SIZE*rnd()
				else:			x= 1
				if PREF_Y_AXIS:	y= 1+ PREF_SIZE*rnd()
				else:			y= 1
				if PREF_Z_AXIS:	z= 1+ PREF_SIZE*rnd()
				else:			z= 1
				
				ob.size = (ob.SizeX*x,  ob.SizeY*y,  ob.SizeZ*z)
		if PREF_ROT:
			if PREF_LINK_AXIS:
				rand = PREF_ROT*rnd()
				ob.rot = (ob.RotX+rand,  ob.RotY+rand,  ob.RotZ+rand)
			else:
				ob.rot = (ob.RotX+(PREF_X_AXIS*PREF_ROT*rnd()),  ob.RotY+(PREF_Y_AXIS*PREF_ROT*rnd()),  ob.RotZ+(PREF_Z_AXIS*PREF_ROT*rnd()))
	

def main():
	sel= Object.GetSelected()
	if not sel:
		return
	
	PREF_LOC= Draw.Create(0.0)
	PREF_SIZE= Draw.Create(0.0)
	PREF_ROT= Draw.Create(0.0)
	PREF_LINK_AXIS= Draw.Create(0)
	PREF_X_AXIS= Draw.Create(1)
	PREF_Y_AXIS= Draw.Create(1)
	PREF_Z_AXIS= Draw.Create(1)
	
	pup_block = [\
	'Randomize...',\
	('loc:', PREF_LOC, 0.0, 10.0, 'Amount to randomize the location'),\
	('size:', PREF_SIZE, 0.0, 10.0,  'Amount to randomize the size'),\
	('rot:', PREF_ROT, 0.0, 10.0, 'Amount to randomize the rotation'),\
	'',\
	('Link Axis', PREF_LINK_AXIS, 'Use the same random value for each objects XYZ'),\
	'',\
	('X Axis', PREF_X_AXIS, 'Enable X axis randomization'),\
	('Y Axis', PREF_Y_AXIS, 'Enable Y axis randomization'),\
	('Z Axis', PREF_Z_AXIS, 'Enable Z axis randomization'),\
	]
	
	if not Draw.PupBlock('Object Randomize', pup_block):
		return
	
	randomize(sel, PREF_LOC.val, PREF_SIZE.val, PREF_ROT.val, PREF_LINK_AXIS.val, PREF_X_AXIS.val, PREF_Y_AXIS.val, PREF_Z_AXIS.val)
	
if __name__ == '__main__':
	main()