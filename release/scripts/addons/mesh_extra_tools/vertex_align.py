# -*- coding: utf-8 -*-

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

# Note: Property group was moved to __init__

bl_info = {
    "name": "Vertex Align",
    "author": "",
    "version": (0, 1, 7),
    "blender": (2, 6, 1),
    "location": "View3D > Tool Shelf",
    "description": "",
    "warning": "",
    "wiki_url": "",
    "category": "Mesh"}


import bpy
from bpy.props import (
        BoolVectorProperty,
        FloatVectorProperty,
        )
from mathutils import Vector
from bpy.types import Operator


# Edit Mode Toggle
def edit_mode_out():
    bpy.ops.object.mode_set(mode='OBJECT')


def edit_mode_in():
    bpy.ops.object.mode_set(mode='EDIT')


def get_mesh_data_():
    edit_mode_out()
    ob_act = bpy.context.active_object
    me = ob_act.data
    edit_mode_in()
    return me


def list_clear_(l):
    l[:] = []
    return l


class va_buf():
    list_v = []
    list_0 = []


# Store The Vertex coordinates
class Vertex_align_store(Operator):
    bl_idname = "vertex_align.store_id"
    bl_label = "Active Vertex"
    bl_description = ("Store Selected Vertex coordinates as an align point\n"
                      "Single Selected Vertex only")

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return (obj and obj.type == 'MESH' and context.mode == 'EDIT_MESH')

    def execute(self, context):
        try:
            me = get_mesh_data_()
            list_0 = [v.index for v in me.vertices if v.select]

            if len(list_0) == 1:
                list_clear_(va_buf.list_v)
                for v in me.vertices:
                    if v.select:
                        va_buf.list_v.append(v.index)
                        bpy.ops.mesh.select_all(action='DESELECT')
            else:
                self.report({'WARNING'}, "Please select just One Vertex")
                return {'CANCELLED'}
        except:
            self.report({'WARNING'}, "Storing selection could not be completed")
            return {'CANCELLED'}

        self.report({'INFO'}, "Selected Vertex coordinates are stored")

        return {'FINISHED'}


# Align to original
class Vertex_align_original(Operator):
    bl_idname = "vertex_align.align_original"
    bl_label = "Align to original"
    bl_description = "Align selection to stored single vertex coordinates"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return (obj and obj.type == 'MESH' and context.mode == 'EDIT_MESH')

    def draw(self, context):
        layout = self.layout
        layout.label("Axis:")

        row = layout.row(align=True)
        row.prop(context.scene.mesh_extra_tools, "vert_align_axis",
                 text="X", index=0, toggle=True)
        row.prop(context.scene.mesh_extra_tools, "vert_align_axis",
                 text="Y", index=1, toggle=True)
        row.prop(context.scene.mesh_extra_tools, "vert_align_axis",
                 text="Z", index=2, toggle=True)

    def execute(self, context):
        edit_mode_out()
        ob_act = context.active_object
        me = ob_act.data
        cen1 = context.scene.mesh_extra_tools.vert_align_axis
        list_0 = [v.index for v in me.vertices if v.select]

        if len(va_buf.list_v) == 0:
            self.report({'INFO'},
                        "Original vertex not stored in memory. Operation Cancelled")
            edit_mode_in()
            return {'CANCELLED'}

        elif len(va_buf.list_v) != 0:
            if len(list_0) == 0:
                self.report({'INFO'}, "No vertices selected. Operation Cancelled")
                edit_mode_in()
                return {'CANCELLED'}

            elif len(list_0) != 0:
                vo = (me.vertices[va_buf.list_v[0]].co).copy()
                if cen1[0] is True:
                    for i in list_0:
                        v = (me.vertices[i].co).copy()
                        me.vertices[i].co = Vector((vo[0], v[1], v[2]))
                if cen1[1] is True:
                    for i in list_0:
                        v = (me.vertices[i].co).copy()
                        me.vertices[i].co = Vector((v[0], vo[1], v[2]))
                if cen1[2] is True:
                    for i in list_0:
                        v = (me.vertices[i].co).copy()
                        me.vertices[i].co = Vector((v[0], v[1], vo[2]))
        edit_mode_in()

        return {'FINISHED'}


# Align to custom coordinates
class Vertex_align_coord_list(Operator):
    bl_idname = "vertex_align.coord_list_id"
    bl_label = ""
    bl_description = "Align to custom coordinates"

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return (obj and obj.type == 'MESH' and context.mode == 'EDIT_MESH')

    def execute(self, context):
        edit_mode_out()
        ob_act = context.active_object
        me = ob_act.data
        list_clear_(va_buf.list_0)
        va_buf.list_0 = [v.index for v in me.vertices if v.select][:]

        if len(va_buf.list_0) == 0:
            self.report({'INFO'}, "No vertices selected. Operation Cancelled")
            edit_mode_in()
            return {'CANCELLED'}

        elif len(va_buf.list_0) != 0:
            bpy.ops.vertex_align.coord_menu_id('INVOKE_DEFAULT')

        edit_mode_in()

        return {'FINISHED'}


# Align to custom coordinates menu
class Vertex_align_coord_menu(Operator):
    bl_idname = "vertex_align.coord_menu_id"
    bl_label = "Tweak custom coordinates"
    bl_description = "Change the custom coordinates for aligning"
    bl_options = {'REGISTER', 'UNDO'}

    def_axis_coord = FloatVectorProperty(
            name="",
            description="Enter the values of coordinates",
            default=(0.0, 0.0, 0.0),
            min=-100.0, max=100.0,
            step=1, size=3,
            subtype='XYZ',
            precision=3
            )
    use_axis_coord = BoolVectorProperty(
            name="Axis",
            description="Choose Custom Coordinates axis",
            default=(False,) * 3,
            size=3,
            )
    is_not_undo = False

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return (obj and obj.type == 'MESH')

    def using_store(self, context):
        scene = context.scene
        return scene.mesh_extra_tools.vert_align_use_stored

    def draw(self, context):
        layout = self.layout

        if self.using_store(context) and self.is_not_undo:
            layout.label("Using Stored Coordinates", icon="INFO")

        row = layout.split(0.25)
        row.prop(self, "use_axis_coord", index=0, text="X")
        row.prop(self, "def_axis_coord", index=0)

        row = layout.split(0.25)
        row.prop(self, "use_axis_coord", index=1, text="Y")
        row.prop(self, "def_axis_coord", index=1)

        row = layout.split(0.25)
        row.prop(self, "use_axis_coord", index=2, text="Z")
        row.prop(self, "def_axis_coord", index=2)

    def invoke(self, context, event):
        self.is_not_undo = True
        scene = context.scene
        if self.using_store(context):
            self.def_axis_coord = scene.mesh_extra_tools.vert_align_store_axis

        return context.window_manager.invoke_props_dialog(self, width=200)

    def execute(self, context):
        self.is_not_undo = False
        edit_mode_out()
        ob_act = context.active_object
        me = ob_act.data

        for i in va_buf.list_0:
            v = (me.vertices[i].co).copy()
            tmp = Vector((v[0], v[1], v[2]))

            if self.use_axis_coord[0] is True:
                tmp[0] = self.def_axis_coord[0]
            if self.use_axis_coord[1] is True:
                tmp[1] = self.def_axis_coord[1]
            if self.use_axis_coord[2] is True:
                tmp[2] = self.def_axis_coord[2]
            me.vertices[i].co = tmp

        edit_mode_in()

        return {'FINISHED'}


#  Register
classes = (
    Vertex_align_store,
    Vertex_align_original,
    Vertex_align_coord_list,
    Vertex_align_coord_menu,
    )


def register():
    for cls in classes:
        bpy.utils.register_class(cls)


def unregister():
    for cls in classes:
        bpy.utils.unregister_class(cls)


if __name__ == "__main__":
    register()
