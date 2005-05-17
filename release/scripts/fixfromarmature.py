#!BPY

""" Registration info for Blender menus: <- these words are ignored
Name: 'Fix From Everything'
Blender: 236
Group: 'Mesh'
Tip: 'Fix armature/lattice/RVK/curve deform and taper/soft body deformation (without bake)'
"""

__author__ = "Jean-Michel Soler (jms)"
__url__ = ("blender", "elysiun",
"Script's homepage, http://jmsoler.free.fr/util/blenderfile/py/fixfromarmature.py",
"Communicate problems and errors, http://www.zoo-logique.org/3D.Blender/newsportal/thread.php?group=3D.Blender")
__version__ = "05/2005"

__bpydoc__ = """\
This script creates a copy of the active mesh with deformations fixed.

Usage:

Select the mesh and run this script.  A fixed copy of it will be created.
"""

# $Id$
#
#----------------------------------------------
# jm soler  05/2004-->04/2005 :   'FixfromArmature'
#----------------------------------------------
# Official Page :
#   http://jmsoler.free.fr/util/blenderfile/py/fixfromarmature.py
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

def fix_mesh(nomdelobjet):
	Mesh=Blender.NMesh.GetRawFromObject(nomdelobjet)
	Obis = Blender.Object.New ('Mesh')
	Obis.link(Mesh)
	Obis.setMatrix(Ozero.getMatrix())
	scene = Blender.Scene.getCurrent()
	scene.link (Obis)

	Mesh2=Obis.getData()
	Mesh1=Ozero.getData()

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


Ozero=Blender.Object.GetSelected()[0]

errormsg = ''

if not Ozero:
	errormsg = "no mesh object selected"
elif Ozero.getType() != "Mesh":
	errormsg = "selected (active) object must be a mesh"

if errormsg:
	Blender.Draw.PupMenu("ERROR: %s" % errormsg)

else:
	fix = 1
	curframe = Blender.Get('curframe')
	if Ozero.isSB() and curframe != 1:
		softbodies=Blender.Draw.PupMenu("Soft Body: play anim up to the current frame to fix it?%t|Yes%x1|No %x2|Cancel %x3")
		if softbodies==3:
			fix = 0
		elif softbodies==1:
			for f in range(1, curframe + 1):
				Blender.Set('curframe',f)
	if fix: fix_mesh(Ozero.getName())

