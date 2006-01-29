#!BPY

"""
Name: 'Apply Deformation'
Blender: 234
Group: 'Mesh'
Tooltip: 'Create fixed copies of deformed meshes'
""" 

__author__ = "Martin 'theeth' Poirier"
__url__ = ("http://www.blender.org", "http://www.elysiun.com")
__version__ = "1.5 09/21/04"

__bpydoc__ = """\
This script creates "raw" copies of deformed meshes.

Usage:

Select the mesh(es) and run this script.  A fixed copy of each selected mesh
will be created, with the word "_deformed" appended to its name. If an object with
the same name already exists, it appends a number at the end as Blender itself does.

Meshes in Blender can be deformed by armatures, lattices, curve objects and subdivision, but this will only change its appearance on screen and rendered
images -- the actual mesh data is still simpler, with vertices in an original
"rest" position and less vertices than the subdivided version.

Use this script if you want a "real" version of the deformed mesh, so you can
directly manipulate or export its data.
"""


# $Id$
#
# --------------------------------------------------------------------------
# ***** BEGIN GPL LICENSE BLOCK *****
#
# Copyright (C) 2003: Martin Poirier, theeth@yahoo.com
#
# Thanks to Jonathan Hudson for help with the vertex groups part
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


import Blender

Blender.Window.EditMode(0)

NAME_LENGTH = 19
PREFIX = "_def"
PREFIX_LENGTH = len(PREFIX)

ob_list = Blender.Object.GetSelected()

for ob in ob_list:
	ob.sel = 0

used_names = [ob.name for ob in Blender.Object.Get()]
used_names.extend(Blender.NMesh.GetNames())

deformedList = []
for ob in ob_list:
    if ob.getType() == "Mesh":
        name = ob.getName()
        new_name = "%s_def" % name[:NAME_LENGTH-PREFIX_LENGTH]
        num = 0
        new_mesh = Blender.NMesh.GetRawFromObject(name)
        while new_name in used_names:
            new_name = "%s_def.%.3i" % (name[:NAME_LENGTH-(PREFIX_LENGTH+PREFIX_LENGTH)], num)
            num += 1
        
        used_names.append(new_name)
        
        new_ob = Blender.NMesh.PutRaw(new_mesh, new_name)
        new_ob.setMatrix(ob.getMatrix())
        new_ob.setName(new_name)
        deformedList.append(new_ob)
        
        # Vert groups.
        ob_mesh = ob.getData()
        new_ob_mesh = new_ob.getData()
        
        for vgroupname in ob_mesh.getVertGroupNames():
            new_ob_mesh.addVertGroup(vgroupname)
            if len(ob_mesh.verts) == len(new_ob_mesh.verts):
                vlist = ob_mesh.getVertsFromGroup(vgroupname, True)
                try:
                    for vpair in vlist:
                        new_ob_mesh.assignVertsToGroup(vgroupname, [vpair[0]], vpair[1], 'add')
                except:
                    pass

for ob in deformedList:
	ob.sel = 1
deformedList[0].sel = 1 # Keep the same object active.