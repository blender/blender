#!BPY

"""
Name: 'Apply Deformation'
Blender: 233
Group: 'Mesh'
Tooltip: 'Create fixed copies of deformed meshes'
""" 
# $Id$
#
# --------------------------------------------------------------------------
# ***** BEGIN GPL LICENSE BLOCK *****
#
# Copyright (C) 2003: Martin Poirier, theeth@yahoo.com
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

ob_list = Blender.Object.GetSelected()
for ob in ob_list:
    if ob.getType() == "Mesh":
        name = ob.getName()
        new_name = name + "_deformed"
        num = 0
        new_mesh = Blender.NMesh.GetRawFromObject(name)
        mesh = Blender.NMesh.GetRaw(new_name)
        while mesh:
            num += 1
            new_name = name + "_deformed." + "%03i" % num
            mesh = Blender.NMesh.GetRaw(new_name)
        new_ob = Blender.NMesh.PutRaw(new_mesh, new_name)
        new_ob.setMatrix(ob.getMatrix())
        try:
            new_ob = Blender.Object.Get(new_name)
            while 1:
                num += 1
                new_name = name + "_deformed." + "%03i" % num
                new_ob = Blender.Object.Get(new_name)
        except:
            pass
        new_ob.setName(new_name)

Blender.Window.EditMode(1)
