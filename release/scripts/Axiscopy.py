#!BPY

""" Registration info for Blender menus: <- these words are ignored
Name: 'Axis Orientation Copy'
Blender: 233
Group: 'Object'
Tip: 'Copy the axis orientation of the active object to all selected mesh object'
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

if lenob<2:
    Draw.PupMenu("Select at least 2 objects")
else :    
    source=oblist[0]
    nsource=source.name
    texte="Copy axis orientation from : " + nsource + " ?%t|OK"
    result=Draw.PupMenu(texte)


    for cible in oblist[1:]:
        if cible.getType()=='Mesh':
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
