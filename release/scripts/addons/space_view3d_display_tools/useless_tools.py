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

bl_info = {
    "name": "Useless Tools",
    "description": "Just a little collection of scripts and tools I use daily",
    "author": "Greg Zaal",
    "version": (1, 2, 2),
    "blender": (2, 75, 0),
    "location": "3D View > Tools",
    "warning": "",
    "wiki_url": "",
    "category": "Tools"}


import bpy
from bpy.types import Operator
from bpy.props import BoolProperty


def error_handlers(self, op_name, errors, reports="ERROR"):
    if self and reports:
        self.report({'INFO'},
                    reports + ": some operations could not be performed "
                    "(See Console for more info)")

    str_errors = "\n".join(errors)
    print("\n[Display Tools]\nOperator: {}\nErrors: {}\n".format(op_name, str_errors))


class UTSetSelectable(Operator):
    bl_idname = "ut.set_selectable"
    bl_label = "Set Selectable"
    bl_description = "Sets selectability for the selected objects"

    selectable = BoolProperty()

    def execute(self, context):
        errors = []
        for obj in bpy.context.selected_objects:
            try:
                if self.selectable is True:
                    obj.hide_select = False
                else:
                    obj.hide_select = True
            except Exception as k:
                name = getattr(obj, "name", "Nameless")
                errors.append("Error on {} - {}".format(name, k))
        if errors:
            error_handlers(self, "ut.set_selectable", errors, "Set Selectable")

        return {'FINISHED'}


class UTSetRenderable(Operator):
    bl_idname = "ut.set_renderable"
    bl_label = "Set Renderable"
    bl_description = "Sets renderability for the selected objects"

    renderable = BoolProperty()

    def execute(self, context):
        errors = []
        for obj in bpy.context.selected_objects:
            try:
                if self.renderable is True:
                    obj.hide_render = False
                else:
                    obj.hide_render = True
            except Exception as k:
                name = getattr(obj, "name", "Nameless")
                errors.append("Error on {} - {}".format(name, k))
        if errors:
            error_handlers(self, "ut.set_renderable", errors, "Set Renderable")

        return {'FINISHED'}


class UTAllSelectable(Operator):
    bl_idname = "ut.all_selectable"
    bl_label = "All Selectable"
    bl_description = "Allows all objects to be selected"

    def execute(self, context):
        errors = []
        for obj in bpy.data.objects:
            try:
                obj.hide_select = False
            except Exception as k:
                name = getattr(obj, "name", "Nameless")
                errors.append("Error on {} - {}".format(name, k))
        if errors:
            error_handlers(self, "ut.all_selectable", errors, "All Selectable")

        return {'FINISHED'}


class UTAllRenderable(Operator):
    bl_idname = "ut.all_renderable"
    bl_label = "All Renderable"
    bl_description = "Allow all objects to be rendered"

    def execute(self, context):
        errors = []
        for obj in bpy.data.objects:
            try:
                obj.hide_render = False
            except Exception as k:
                name = getattr(obj, "name", "Nameless")
                errors.append("Error on {} - {}".format(name, k))
        if errors:
            error_handlers(self, "ut.all_renderable", errors, "All Renderable")

        return {'FINISHED'}


class UTSelNGon(Operator):
    bl_idname = "ut.select_ngons"
    bl_label = "Select NGons"
    bl_description = "Select faces with more than 4 vertices"

    @classmethod
    def poll(cls, context):
        if not context.active_object or context.mode != 'EDIT_MESH':
            return False
        return True

    def execute(self, context):
        errors = []
        try:
            context.tool_settings.mesh_select_mode = (False, False, True)
            bpy.ops.mesh.select_face_by_sides(number=4, type='GREATER', extend=True)
        except Exception as k:
            errors.append("Error - {}".format(k))
        if errors:
            error_handlers(self, "ut.select_ngons", errors, "Select NGons")

        return {'FINISHED'}


class UTWireShowHideSelAll(Operator):
    bl_idname = "ut.wire_show_hide"
    bl_label = "Show / Hide Wire Selected or All"
    bl_description = "Change the status of the Wire display on Selected Objects"

    show = BoolProperty(
            default=False
            )
    selected = BoolProperty(
            default=False
            )

    @classmethod
    def poll(cls, context):
        return not context.scene.display_tools.WT_handler_enable

    def execute(self, context):
        errors = []
        objects = bpy.context.selected_objects if self.selected else bpy.data.objects
        for e in objects:
            try:
                e.show_wire = self.show
            except Exception as k:
                name = getattr(e, "name", "Nameless")
                errors.append("Error on {} - {}".format(name, k))
        if errors:
            error_handlers(self, "ut.wire_show_hide", errors,
                           "Show / Hide Wire Selected or All")

        return {'FINISHED'}


class UTSubsurfHideSelAll(Operator):
    bl_idname = "ut.subsurf_show_hide"
    bl_label = "Subsurf Show/Hide"
    bl_description = ("Sets the Subsurf modifier on objects:\n"
                      "Hide and Show operate on Selected Objects only\n"
                      "Hide All and Show All operate on All Objects in the data")

    show = BoolProperty(
            default=False
            )
    selected = BoolProperty(
            default=False
            )

    def execute(self, context):
        errors = []
        objects = bpy.context.selected_objects if self.selected else bpy.data.objects
        for e in objects:
            try:
                if e.type not in {"LAMP", "CAMERA", "EMPTY"}:
                    e.modifiers['Subsurf'].show_viewport = self.show
            except Exception as k:
                name = getattr(e, "name", "Nameless")
                errors.append(
                    "No subsurf on {} or it is not named Subsurf\nError: {}".format(name, k))
        if errors:
            error_handlers(self, "ut.subsurf_show_hide", errors, "Subsurf Show/Hide")

        return {'FINISHED'}


class UTOptimalDisplaySelAll(Operator):
    bl_idname = "ut.optimaldisplay"
    bl_label = "Optimal Display"
    bl_description = "Disables Optimal Display for all Subsurf modifiers on objects"

    on = BoolProperty(
            default=False
            )
    selected = BoolProperty(
            default=False
            )

    def execute(self, context):
        errors = []
        objects = bpy.context.selected_objects if self.selected else bpy.data.objects
        for e in objects:
            try:
                if e.type not in {"LAMP", "CAMERA", "EMPTY"}:
                    e.modifiers['Subsurf'].show_only_control_edges = self.on
            except Exception as k:
                name = getattr(e, "name", "Nameless")
                errors.append(
                    "No subsurf on {} or it is not named Subsurf\nError: {}".format(name, k))
        if errors:
            error_handlers(self, "ut.optimaldisplay", errors, "Optimal Display")

        return {'FINISHED'}


class UTAllEdges(Operator):
    bl_idname = "ut.all_edges"
    bl_label = "All Edges"
    bl_description = "Change the status of All Edges overlay on all objects"

    on = BoolProperty(
            default=False
            )

    def execute(self, context):
        errors = []
        for e in bpy.data.objects:
            try:
                e.show_all_edges = self.on
            except Exception as k:
                name = getattr(e, "name", "Nameless")
                errors.append(
                    "Enabling All Edges  on {} \nError: {}".format(name, k))
        if errors:
            error_handlers(self, "ut.all_edges", errors, "All Edges")

        return {'FINISHED'}


class UTDoubleSided(Operator):
    bl_idname = "ut.double_sided"
    bl_label = "Double Sided Normals"
    bl_description = "Disables Double Sided Normals for all objects"

    on = BoolProperty(
            default=False
            )

    def execute(self, context):
        errors = []
        for e in bpy.data.meshes:
            try:
                e.show_double_sided = self.on
            except Exception as k:
                name = getattr(e, "name", "Nameless")
                errors.append(
                    "Applying Double Sided Normals on {} \nError: {}".format(name, k))
        if errors:
            error_handlers(self, "ut.double_sided", errors, "Double Sided Normals")

        return {'FINISHED'}


# Register
def register():
    bpy.utils.register_module(__name__)


def unregister():
    bpy.utils.unregister_module(__name__)


if __name__ == "__main__":
    register()
