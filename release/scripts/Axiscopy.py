#!BPY

""" Registration info for Blender menus: <- these words are ignored
Name: 'Axis Orientation Copy'
Blender: 242
Group: 'Object'
Tip: 'Copy local axis orientation of active object to all selected meshes (changes mesh data)'
"""

__author__ = "A Vanpoucke (xand)"
__url__ = ("blenderartists.org", "www.blender.org",
"French Blender support forum, http://www.zoo-logique.org/3D.Blender/newsportal/thread.php?group=3D.Blender")
__version__ = "2 17/12/05"

__bpydoc__ = """\
This script copies the axis orientation -- X, Y and Z rotations -- of the
active object to all selected meshes.

It's useful to align the orientations of all meshes of a structure, a human
skeleton, for example.

Usage:

Select all mesh objects that need to have their orientations changed
(reminder: keep SHIFT pressed after the first, to add each new one to the
selection), then select the object whose orientation will be copied from and
finally run this script to update the angles.

Notes:<br>
    This script changes mesh data: the vertices are transformed.<br>
	Before copying the orientation to each object, the script stores its
transformation matrix.	Then the angles are copied and after that the object's
vertices are transformed "back" so that they still have the same positions as
before.  In other words, the rotations are updated, but you won't notice that
just from looking at the objects.<br>
	Checking their X, Y and Z rotation values with "Transform Properties" in
the 3D View's Object menu shows the angles are now the same of the active
object. Or simply look at the transform manipulator handles in local transform
orientation.
"""


# $Id$
#
#----------------------------------------------
# A Vanpoucke (xand)
#from the previous script realignaxis
#----------------------------------------------
# Communiquer les problemes et erreurs sur:
#	http://www.zoo-logique.org/3D.Blender/newsportal/thread.php?group=3D.Blender
# --------------------------------------------------------------------------
# ***** BEGIN GPL LICENSE BLOCK *****
#
# Copyright (C) 2003, 2004: A Vanpoucke
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# ***** END GPL LICENCE BLOCK *****
# --------------------------------------------------------------------------

from Blender import *
from Blender import Mathutils
from Blender.Mathutils import *
import BPyMessages

def realusers(data):
	users = data.users
	if data.fakeUser: users -= 1
	return users

	

def main():
	
	scn_obs= Scene.GetCurrent().objects
	ob_act = scn_obs.active
	scn_obs = scn_obs.context
	
	if not ob_act:
		BPyMessages.Error_NoActive()
	
	obs = [(ob, ob.getData(mesh=1)) for ob in scn_obs if ob != ob_act]
	
	for ob, me in obs:
		
		if ob.type != 'Mesh':
			Draw.PupMenu("Error%t|Selection must be made up of mesh objects only")
			return
		
		if realusers(me) != 1:
			Draw.PupMenu("Error%t|Meshes must be single user")
			return
	
	if len(obs) < 1:
		Draw.PupMenu("Error: you must select at least 2 objects")
		return
	
	result = Draw.PupMenu("Copy axis orientation from: " + ob_act.name + " ?%t|OK")
	if result == -1:
		return
	
	for ob_target, me_target in obs:
		if ob_act.rot != ob_target.rot:
			rot_target = ob_target.matrixWorld.rotationPart().toEuler().toMatrix()
			rot_source = ob_act.matrixWorld.rotationPart().toEuler().toMatrix()
			rot_source_inv = rot_source.copy().invert()
			tx_mat = rot_target * rot_source_inv
			tx_mat.resize4x4()
			me_target.transform(tx_mat)
			ob_target.rot=ob_act.rot

if __name__ == '__main__':
	main()