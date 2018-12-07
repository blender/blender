# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

import bpy
from bpy.app.handlers import persistent


@persistent
def load_handler(dummy):
    import bpy
    # Apply subdivision modifier on startup
    bpy.ops.object.mode_set(mode='OBJECT')
    if bpy.app.opensubdiv.supported:
        bpy.ops.object.modifier_apply(modifier="Subdivision")
        bpy.ops.object.mode_set(mode='EDIT')
        bpy.ops.transform.tosphere(value=1.0)
    else:
        bpy.ops.object.modifier_remove(modifier="Subdivision")
        bpy.ops.object.mode_set(mode='EDIT')
        bpy.ops.mesh.subdivide(number_cuts=6, smoothness=1.0)
    bpy.ops.object.mode_set(mode='SCULPT')

def register():
    bpy.app.handlers.load_factory_startup_post.append(load_handler)

def unregister():
    bpy.app.handlers.load_factory_startup_post.remove(load_handler)
