# ##### BEGIN GPL LICENSE BLOCK #####
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
# ##### END GPL LICENCE BLOCK #####


bl_info = {
    "name": "Select Tools",
    "author": "Jakub Belcik",
    "version": (1, 0, 2),
    "blender": (2, 7, 3),
    "location": "3D View > Tools",
    "description": "Selection Tools",
    "warning": "",
    "wiki_url": "",
    "category": ""
}

import bpy
from bpy.types import Operator
from bpy.props import (
        BoolProperty,
        EnumProperty,
        )


class ShowHideObject(Operator):
    bl_idname = "opr.show_hide_object"
    bl_label = "Show/Hide Object"
    bl_description = ("Flip the viewport visibility for all objects in the Data\n"
                      "(Hidden to Visible and Visible to Hidden)")
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        if context.object is None:
            self.report({'INFO'},
                        "Show/Hide: No Object found. Operation Cancelled")
            return {'CANCELLED'}

        if context.object.mode != 'OBJECT':
            self.report({'INFO'},
                        "Show/Hide: This operation can be performed only in object mode")
            return {'CANCELLED'}

        for i in bpy.data.objects:
            try:
                if i.hide:
                    i.hide = False
                    i.hide_select = False
                    i.hide_render = False
                else:
                    i.hide = True
                    i.select = False

                    if i.type not in ['CAMERA', 'LAMP']:
                        i.hide_render = True
            except:
                continue

        return {'FINISHED'}


class ShowAllObjects(Operator):
    bl_idname = "opr.show_all_objects"
    bl_label = "Show All Objects"
    bl_description = "Show all objects"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        for i in bpy.data.objects:
            try:
                i.hide = False
                i.hide_select = False
                i.hide_render = False
            except:
                continue

        return {'FINISHED'}


class HideAllObjects(Operator):
    bl_idname = "opr.hide_all_objects"
    bl_label = "Hide All Inactive"
    bl_description = "Hide all inactive objects"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        if context.object is None:
            for i in bpy.data.objects:
                i.hide = True
                i.select = False

                if i.type not in ['CAMERA', 'LAMP']:
                    i.hide_render = True
        else:
            obj_name = context.object.name

            for i in bpy.data.objects:
                if i.name != obj_name:
                    i.hide = True
                    i.select = False

                    if i.type not in ['CAMERA', 'LAMP']:
                        i.hide_render = True

        return {'FINISHED'}


class SelectAll(Operator):
    bl_idname = "opr.select_all"
    bl_label = "(De)select All"
    bl_description = "(De)select all objects, verticies, edges or faces"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        if context.object is None:
            bpy.ops.object.select_all(action='TOGGLE')
        elif context.object.mode == 'EDIT':
            bpy.ops.mesh.select_all(action='TOGGLE')
        elif context.object.mode == 'OBJECT':
            bpy.ops.object.select_all(action='TOGGLE')
        else:
            self.report({'ERROR'},
                        "(De)select All: Cannot perform this operation in this mode")
            return {'CANCELLED'}

        return {'FINISHED'}


class InverseSelection(Operator):
    bl_idname = "opr.inverse_selection"
    bl_label = "Inverse Selection"
    bl_description = "Inverse selection"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        if context.object is None:
            bpy.ops.object.select_all(action='INVERT')
        elif context.object.mode == 'EDIT':
            bpy.ops.mesh.select_all(action='INVERT')
        elif context.object.mode == 'OBJECT':
            bpy.ops.object.select_all(action='INVERT')
        else:
            self.report({'ERROR'},
                         "Inverse Selection: Cannot perform this operation in this mode")
            return {'CANCELLED'}

        return {'FINISHED'}


class LoopMultiSelect(Operator):
    bl_idname = "opr.loop_multi_select"
    bl_label = "Edge Loop Select"
    bl_description = "Select a loop of connected edges"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        return context.active_object is not None

    def execute(self, context):
        if context.object.mode != 'EDIT':
            self.report({'ERROR'}, "This operation can be performed only in edit mode")
            return {'CANCELLED'}
        try:
            bpy.ops.mesh.loop_multi_select(ring=False)
        except:
            self.report({'WARNING'},
                        "Edge loop select: Operation could not be performed (See Console for more info)")
            return {'CANCELLED'}

        return {'FINISHED'}


class ShowRenderAllSelected(Operator):
    bl_idname = "op.render_show_all_selected"
    bl_label = "Render On"
    bl_description = "Render all objects"

    def execute(self, context):
        for ob in bpy.data.objects:
            try:
                if ob.select is True:
                    ob.hide_render = False
            except:
                continue

        return {'FINISHED'}


class HideRenderAllSelected(Operator):
    bl_idname = "op.render_hide_all_selected"
    bl_label = "Render Off"
    bl_description = "Hide Selected Object(s) from Render"

    def execute(self, context):
        for ob in bpy.data.objects:
            try:
                if ob.select is True:
                    ob.hide_render = True
            except:
                continue

        return {'FINISHED'}


class OBJECT_OT_HideShowByTypeTemplate():

    bl_options = {'UNDO', 'REGISTER'}

    type = EnumProperty(
            items=(
                ('MESH', 'Mesh', ''),
                ('CURVE', 'Curve', ''),
                ('SURFACE', 'Surface', ''),
                ('META', 'Meta', ''),
                ('FONT', 'Font', ''),
                ('ARMATURE', 'Armature', ''),
                ('LATTICE', 'Lattice', ''),
                ('EMPTY', 'Empty', ''),
                ('CAMERA', 'Camera', ''),
                ('LAMP', 'Lamp', ''),
                ('ALL', 'All', '')),
            name="Type",
            description="Type",
            default='LAMP',
            options={'ANIMATABLE'}
            )

    def execute(self, context):

        scene = bpy.context.scene
        objects = []
        eligible_objects = []

        # Only Selected?
        if self.hide_selected:
            objects = bpy.context.selected_objects
        else:
            objects = scene.objects

        # Only Specific Types? + Filter layers
        for obj in objects:
            for i in range(0, 20):
                if obj.layers[i] & scene.layers[i]:
                    if self.type == 'ALL' or obj.type == self.type:
                        if obj not in eligible_objects:
                            eligible_objects.append(obj)
        objects = eligible_objects
        eligible_objects = []

        # Only Render Restricted?
        if self.hide_render_restricted:
            for obj in objects:
                if obj.hide_render == self.hide_or_show:
                    eligible_objects.append(obj)
            objects = eligible_objects
            eligible_objects = []

        # Perform Hiding / Showing
        for obj in objects:
            obj.hide = self.hide_or_show

        return {'FINISHED'}

    def invoke(self, context, event):
        return self.execute(context)


# show hide by type # by Felix Schlitter
class OBJECT_OT_HideByType(OBJECT_OT_HideShowByTypeTemplate, Operator):
    bl_idname = "object.hide_by_type"
    bl_label = "Hide By Type"

    hide_or_show = BoolProperty(
            name="Hide",
            description="Inverse effect",
            options={'HIDDEN'},
            default=1
            )
    hide_selected = BoolProperty(
            name="Selected",
            description="Hide only selected objects",
            default=0
            )
    hide_render_restricted = BoolProperty(
            name="Only Render-Restricted",
            description="Hide only render restricted objects",
            default=0
            )


class OBJECT_OT_ShowByType(OBJECT_OT_HideShowByTypeTemplate, Operator):
    bl_idname = "object.show_by_type"
    bl_label = "Show By Type"

    hide_or_show = BoolProperty(
            name="Hide",
            description="Inverse effect",
            options={'HIDDEN'},
            default=0
            )
    hide_selected = BoolProperty(
            name="Selected",
            options={'HIDDEN'},
            default=0
            )
    hide_render_restricted = BoolProperty(
            name="Only Renderable",
            description="Show only non render restricted objects",
            default=0
            )


# Register
def register():
    bpy.utils.register_module(__name__)


def unregister():
    bpy.utils.unregister_module(__name__)


if __name__ == '__main__':
    register()
