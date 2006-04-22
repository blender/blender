#!BPY

""" Registration info for Blender menus: <- these words are ignored
Name: 'Fix From Armature'
Blender: 240
Group: 'Mesh'
Tip: 'Fix armature/lattice/RVK/curve deform and taper/softBodies deformation (without bake)'
"""

__author__ = "Jean-Michel Soler (jms)"
__url__ = ("blender", "elysiun",
"Script's homepage, http://jmsoler.free.fr/didacticiel/blender/tutor/cpl_rawfromobject_en.htm#softbodiesveretxgroups",
"Communicate problems and errors, http://www.zoo-logique.org/3D.Blender/newsportal/thread.php?group=3D.Blender")
__version__ = "04/2006"

__bpydoc__ = """\
This script creates a copy of the active mesh
with armature, lattice, shape key or curve deformation
fixed in the modified state . It can also create a mesh
copy of any other object, curve, surface nurbs, text,
deformed, or not, by a shape key or an absolute key . 

Usage:

Select the mesh, or anything else (take care of selecting
the main metaball if you try the  script on blobby object),
and run this script.

"""

# $Id$
#
#----------------------------------------------
# jm soler  05/2004-->04/2006 :   'FixfromArmature'
#----------------------------------------------
# Official Page :
#   http://jmsoler.free.fr/didacticiel/blender/tutor/cpl_rawfromobject_en.htm#softbodiesveretxgroups
# Communicate problems and errors on:
#   http://www.zoo-logique.org/3D.Blender/newsportal/thread.php?group=3D.Blender
#---------------------------------------------
# Page officielle :
#   http://jmsoler.free.fr/util/blenderfile/py/fixfromarmature.py
# Communiquer les problemes et erreurs sur:
#   http://www.zoo-logique.org/3D.Blender/newsportal/thread.php?group=3D.Blender
#---------------------------------------------
# ce script est proposé sous licence GPL pour etre associe
# a la distribution de Blender 2.33 et suivant
# --------------------------------------------------------------------------
# this script is released under GPL licence
# for the Blender 2.33 scripts package
# --------------------------------------------------------------------------
# ***** BEGIN GPL LICENSE BLOCK *****
#
# Script copyright (C) 2003, 2004: Jean-Michel Soler 
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
try:
 softbodies=0
 softbodies=Blender.Draw.PupMenu("Is this mesh a soft bodies ?%t|Not %x1|Yes %x2")
 if softbodies==2:
     softbodies=Blender.Draw.PupMenu("Softbodies can be fixed but we need to play anim upto the current frame ?%t|Not %x1 |Yes %x2")
     if softbodies==2:
        curframe=Blender.Get('curframe')
        for f in range(curframe):
               Blender.Set('curframe',f+1)
               Blender.Window.RedrawAll()

               
 Ozero=Blender.Object.GetSelected()[0]
 nomdelobjet=Ozero.getName()
 Mesh=Blender.NMesh.GetRawFromObject(nomdelobjet)
 Obis = Blender.Object.New ('Mesh')
 Obis.link(Mesh)
 Obis.setMatrix(Ozero.getMatrix())
 scene = Blender.Scene.getCurrent()
 scene.link (Obis)

 Mesh2=Obis.getData()
 Mesh1=Ozero.getData()
 
 if Ozero.getType()=='Mesh' :
    if len(Mesh2.verts)==len(Mesh1.verts): 
       for VertGroupName in Mesh1.getVertGroupNames():
         VertexList = Mesh1.getVertsFromGroup(VertGroupName, True)
         Mesh2.addVertGroup(VertGroupName)
         for Vertex in VertexList:
             Mesh2.assignVertsToGroup(VertGroupName, [Vertex[0]], Vertex[1], 'add')
    else:
      for vgroupname in Mesh1.getVertGroupNames():
        Mesh2.addVertGroup(vgroupname)
 Mesh2.update()

except:
 Blender.Draw.PupMenu("Error%t|Not the main metaball or no object selected ")