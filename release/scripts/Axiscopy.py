#!BPY

""" Registration info for Blender menus: <- these words are ignored
Name: 'Axis Orientation Copy'
Blender: 233
Group: 'Object'
Tip: 'Copy the axis orientation of the active object to all selected mesh objects'
"""

__author__ = "A Vanpoucke (xand)"
__url__ = ("blender", "elysiun",
"French Blender support forum, http://www.zoo-logique.org/3D.Blender/newsportal/thread.php?group=3D.Blender")
__version__ = "1.1 11/05/04"

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
    Before copying the orientation to each object, the script stores its
transformation matrix.  Then the angles are copied and after that the object's
vertices are transformed "back" so that they still have the same positions as
before.  In other words, the rotations are updated, but you won't notice that
just from looking at the objects.<br>
    Checking their X, Y and Z rotation values with "Transform Properties" in
the 3D View's Object menu shows the angles are now the same of the active
object.
"""


# $Id$
#
#----------------------------------------------
# A Vanpoucke (xand)
#from the previous script realignaxis
#----------------------------------------------
# Communiquer les problemes et erreurs sur:
#   http://www.zoo-logique.org/3D.Blender/newsportal/thread.php?group=3D.Blender
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
from Blender import Mathutils
from Blender.Mathutils import *


def applyTransform(mesh,mat):
  for v in mesh.verts:
      vec = VecMultMat(v.co,mat)
      v.co[0], v.co[1], v.co[2] = vec[0], vec[1], vec[2]




oblist =Object.GetSelected()
lenob=len(oblist)

error = 0
for o in oblist[1:]:
    if o.getType() != "Mesh":
        Draw.PupMenu("ERROR%t|Selected objects must be meshes")
        error = 1

if not error:
    if lenob<2:
        Draw.PupMenu("ERROR%t|You must select at least 2 objects")
    else :    
        source=oblist[0]
        nsource=source.name
        texte="Copy axis orientation from: " + nsource + " ?%t|OK"
        result=Draw.PupMenu(texte)


        for cible in oblist[1:]:
            if source.rot!=cible.rot:
                rotcible=cible.mat.toEuler().toMatrix()
                rotsource=source.mat.toEuler().toMatrix()
                rotsourcet = CopyMat(rotsource)
                rotsourcet.invert()
                mat=rotcible*rotsourcet
                ncible=cible.name
                me=NMesh.GetRaw(ncible)
                applyTransform(me,mat)
                NMesh.PutRaw(me,ncible)
                cible.makeDisplayList()
                cible.rot=source.rot
