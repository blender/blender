# <pep8-80 compliant>

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

__author__ = "kgeogeo, mem, Nutti <nutti.metro@gmail.com>"
__status__ = "production"
__version__ = "4.4"
__date__ = "2 Aug 2017"

import bpy
import bmesh
from mathutils import Vector


class MUV_MVUV(bpy.types.Operator):
    """
    Operator class: Move UV from View3D
    """

    bl_idname = "view3d.muv_mvuv"
    bl_label = "Move the UV from View3D"
    bl_options = {'REGISTER', 'UNDO'}

    def __init__(self):
        self.__topology_dict = []
        self.__prev_mouse = Vector((0.0, 0.0))
        self.__offset_uv = Vector((0.0, 0.0))
        self.__prev_offset_uv = Vector((0.0, 0.0))
        self.__first_time = True
        self.__ini_uvs = []
        self.__running = False

    def __find_uv(self, context):
        bm = bmesh.from_edit_mesh(context.object.data)
        topology_dict = []
        uvs = []
        active_uv = bm.loops.layers.uv.active
        for fidx, f in enumerate(bm.faces):
            for vidx, v in enumerate(f.verts):
                if v.select:
                    uvs.append(f.loops[vidx][active_uv].uv.copy())
                    topology_dict.append([fidx, vidx])

        return topology_dict, uvs

    @classmethod
    def poll(cls, context):
        return context.edit_object

    def modal(self, context, event):
        if self.__first_time is True:
            self.__prev_mouse = Vector((
                event.mouse_region_x, event.mouse_region_y))
            self.__first_time = False
            return {'RUNNING_MODAL'}

        # move UV
        div = 10000
        self.__offset_uv += Vector((
            (event.mouse_region_x - self.__prev_mouse.x) / div,
            (event.mouse_region_y - self.__prev_mouse.y) / div))
        ouv = self.__offset_uv
        pouv = self.__prev_offset_uv
        vec = Vector((ouv.x - ouv.y, ouv.x + ouv.y))
        dv = vec - pouv
        self.__prev_offset_uv = vec
        self.__prev_mouse = Vector((
            event.mouse_region_x, event.mouse_region_y))

        # check if operation is started
        if self.__running is True:
            if event.type == 'LEFTMOUSE' and event.value == 'RELEASE':
                self.__running = False
            return {'RUNNING_MODAL'}

        # update UV
        obj = context.object
        bm = bmesh.from_edit_mesh(obj.data)
        active_uv = bm.loops.layers.uv.active
        for fidx, vidx in self.__topology_dict:
            l = bm.faces[fidx].loops[vidx]
            l[active_uv].uv = l[active_uv].uv + dv
        bmesh.update_edit_mesh(obj.data)

        # check mouse preference
        if context.user_preferences.inputs.select_mouse == 'RIGHT':
            confirm_btn = 'LEFTMOUSE'
            cancel_btn = 'RIGHTMOUSE'
        else:
            confirm_btn = 'RIGHTMOUSE'
            cancel_btn = 'LEFTMOUSE'

        # cancelled
        if event.type == cancel_btn and event.value == 'PRESS':
            for (fidx, vidx), uv in zip(self.__topology_dict, self.__ini_uvs):
                bm.faces[fidx].loops[vidx][active_uv].uv = uv
            return {'FINISHED'}
        # confirmed
        if event.type == confirm_btn and event.value == 'PRESS':
            return {'FINISHED'}

        return {'RUNNING_MODAL'}

    def execute(self, context):
        self.__first_time = True
        self.__running = True
        context.window_manager.modal_handler_add(self)
        self.__topology_dict, self.__ini_uvs = self.__find_uv(context)
        return {'RUNNING_MODAL'}
