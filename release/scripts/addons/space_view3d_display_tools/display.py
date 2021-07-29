# space_view_3d_display_tools.py Copyright (C) 2014, Jordi Vall-llovera
# Multiple display tools for fast navigate/interact with the viewport
# wire tools by Lapineige

# ***** BEGIN GPL LICENSE BLOCK *****
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
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ***** END GPL LICENCE BLOCK *****


import bpy
from bpy.types import Operator
from bpy.props import (
        BoolProperty,
        EnumProperty,
        )


# define base dummy class for inheritance
class BasePollCheck:
    @classmethod
    def poll(cls, context):
        return True


class View3D_AF_Wire_All(Operator):
    bl_idname = "af_ops.wire_all"
    bl_label = "Wire on All Objects"
    bl_description = "Toggle Wire on all objects in the scene"

    @classmethod
    def poll(cls, context):
        return (context.active_object is not None and
                not context.scene.display_tools.WT_handler_enable)

    def execute(self, context):

        for obj in bpy.data.objects:
            if obj.show_wire:
                obj.show_wire = False
            else:
                obj.show_wire = True

        return {'FINISHED'}


# Change draw type
class DisplayDrawChange(Operator, BasePollCheck):
    bl_idname = "view3d.display_draw_change"
    bl_label = "Draw Type"
    bl_description = "Change Display objects' mode"

    drawing = EnumProperty(
            items=[('TEXTURED', 'Texture', 'Texture display mode'),
                   ('SOLID', 'Solid', 'Solid display mode'),
                   ('WIRE', 'Wire', 'Wire display mode'),
                   ('BOUNDS', 'Bounds', 'Bounds display mode'),
                   ],
            name="Draw Type",
            default='SOLID'
            )

    def execute(self, context):
        try:
            view = context.space_data
            view.viewport_shade = 'TEXTURED'
            context.scene.game_settings.material_mode = 'GLSL'
            selection = context.selected_objects

            if not selection:
                for obj in bpy.data.objects:
                    obj.draw_type = self.drawing
            else:
                for obj in selection:
                    obj.draw_type = self.drawing
        except:
            self.report({'ERROR'}, "Setting Draw Type could not be applied")
            return {'CANCELLED'}

        return {'FINISHED'}


# Bounds switch
class DisplayBoundsSwitch(Operator, BasePollCheck):
    bl_idname = "view3d.display_bounds_switch"
    bl_label = "On/Off"
    bl_description = "Display/Hide Bounding box overlay"

    bounds = BoolProperty(default=False)

    def execute(self, context):
        try:
            scene = context.scene.display_tools
            selection = context.selected_objects

            if not selection:
                for obj in bpy.data.objects:
                    obj.show_bounds = self.bounds
                    if self.bounds:
                        obj.draw_bounds_type = scene.BoundingMode
            else:
                for obj in selection:
                    obj.show_bounds = self.bounds
                    if self.bounds:
                        obj.draw_bounds_type = scene.BoundingMode
        except:
            self.report({'ERROR'}, "Display/Hide Bounding box overlay failed")
            return {'CANCELLED'}

        return {'FINISHED'}


# Double Sided switch
class DisplayDoubleSidedSwitch(Operator, BasePollCheck):
    bl_idname = "view3d.display_double_sided_switch"
    bl_label = "On/Off"
    bl_description = "Turn on/off face double shaded mode"

    double_side = BoolProperty(default=False)

    def execute(self, context):
        try:
            selection = bpy.context.selected_objects

            if not selection:
                for mesh in bpy.data.meshes:
                    mesh.show_double_sided = self.double_side
            else:
                for sel in selection:
                    if sel.type == 'MESH':
                        mesh = sel.data
                        mesh.show_double_sided = self.double_side
        except:
            self.report({'ERROR'}, "Turn on/off face double shaded mode failed")
            return {'CANCELLED'}

        return {'FINISHED'}


# XRay switch
class DisplayXRayOn(Operator, BasePollCheck):
    bl_idname = "view3d.display_x_ray_switch"
    bl_label = "On"
    bl_description = "X-Ray display on/off"

    xrays = BoolProperty(default=False)

    def execute(self, context):
        try:
            selection = context.selected_objects

            if not selection:
                for obj in bpy.data.objects:
                    obj.show_x_ray = self.xrays
            else:
                for obj in selection:
                    obj.show_x_ray = self.xrays
        except:
            self.report({'ERROR'}, "Turn on/off X-ray mode failed")
            return {'CANCELLED'}

        return {'FINISHED'}


# wire tools by Lapineige
class WT_HideAllWire(Operator):
    bl_idname = "object.wt_hide_all_wire"
    bl_label = "Hide Wire And Edges"
    bl_description = "Hide All Objects' wire and edges"

    @classmethod
    def poll(cls, context):
        return not context.scene.display_tools.WT_handler_enable

    def execute(self, context):
        for obj in bpy.data.objects:
            if hasattr(obj, "show_wire"):
                obj.show_wire, obj.show_all_edges = False, False
        return {'FINISHED'}


class WT_SelectionHandlerToggle(Operator):
    bl_idname = "object.wt_selection_handler_toggle"
    bl_label = "Wire Selection (auto)"
    bl_description = "Display the wire of the selection, auto update when selecting another object"
    bl_options = {'INTERNAL'}

    def execute(self, context):
        display_tools = context.scene.display_tools
        if display_tools.WT_handler_enable:
            try:
                bpy.app.handlers.scene_update_post.remove(wire_on_selection_handler)
            except:
                self.report({'INFO'},
                            "Wire Selection: auto mode exit seems to have failed. If True, reload the file")

            display_tools.WT_handler_enable = False
            if hasattr(context.object, "show_wire"):
                context.object.show_wire, context.object.show_all_edges = False, False
        else:
            bpy.app.handlers.scene_update_post.append(wire_on_selection_handler)
            display_tools.WT_handler_enable = True
            if hasattr(context.object, "show_wire"):
                context.object.show_wire, context.object.show_all_edges = True, True
        return {'FINISHED'}


# handler
def wire_on_selection_handler(scene):
    obj = bpy.context.object

    if not scene.display_tools.WT_handler_previous_object:
        if hasattr(obj, "show_wire"):
            obj.show_wire, obj.show_all_edges = True, True
            scene.display_tools.WT_handler_previous_object = obj.name
    else:
        if scene.display_tools.WT_handler_previous_object != obj.name:
            previous_obj = bpy.data.objects[scene.display_tools.WT_handler_previous_object]
            if hasattr(previous_obj, "show_wire"):
                previous_obj.show_wire, previous_obj.show_all_edges = False, False

            scene.display_tools.WT_handler_previous_object = obj.name

            if hasattr(obj, "show_wire"):
                obj.show_wire, obj.show_all_edges = True, True


# Register
def register():
    bpy.utils.register_module(__name__)


def unregister():
    bpy.utils.unregister_module(__name__)


if __name__ == "__main__":
    register()
