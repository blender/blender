# space_view_3d_display_tools.py Copyright (C) 2014, Jordi Vall-llovera
# Multiple display tools for fast navigate/interact with the viewport

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

"""
Additional links:
    Author Site: http://www.jordiart.com
"""

import bpy
from bpy.types import Operator
from bpy.props import (
        IntProperty,
        BoolProperty,
        )


# function taken from space_view3d_modifier_tools.py
class DisplayApplyModifiersView(Operator):
    bl_idname = "view3d.toggle_apply_modifiers_view"
    bl_label = "Hide Viewport"
    bl_description = "Shows/Hide modifiers of the active / selected object(s) in 3d View"

    @classmethod
    def poll(cls, context):
        return context.active_object is not None

    def execute(self, context):
        is_apply = True
        message_a = ""
        for mod in context.active_object.modifiers:
            if mod.show_viewport:
                is_apply = False
                break

        # active object - no selection
        for mod in context.active_object.modifiers:
            mod.show_viewport = is_apply

        for obj in context.selected_objects:
            for mod in obj.modifiers:
                mod.show_viewport = is_apply

        if is_apply:
            message_a = "Displaying modifiers in the 3d View"
        else:
            message_a = "Hiding modifiers in the 3d View"

        self.report(type={"INFO"}, message=message_a)

        return {'FINISHED'}


# define base dummy class for inheritance
class BasePollCheck:
    @classmethod
    def poll(cls, context):
        return True


# Set Render Settings
def set_render_settings(context):
    scene = context.scene
    render = scene.render
    render.simplify_subdivision = 0
    render.simplify_shadow_samples = 0
    render.simplify_child_particles = 0
    render.simplify_ao_sss = 0


# Display Modifiers Render Switch
class DisplayModifiersRenderSwitch(Operator, BasePollCheck):
    bl_idname = "view3d.display_modifiers_render_switch"
    bl_label = "On/Off"
    bl_description = "Display/Hide modifiers on render"

    mod_render = BoolProperty(default=True)

    def execute(self, context):
        try:
            if self.mod_render:
                scene = context.scene.display_tools
                scene.Simplify = 1

            selection = context.selected_objects

            if not selection:
                for obj in bpy.data.objects:
                    for mod in obj.modifiers:
                        mod.show_render = self.mod_render
            else:
                for obj in selection:
                    for mod in obj.modifiers:
                        mod.show_render = self.mod_render
        except:
            self.report({'ERROR'}, "Display/Hide all modifiers for render failed")
            return {'CANCELLED'}

        return {'FINISHED'}


# Display Modifiers Viewport switch
class DisplayModifiersViewportSwitch(Operator, BasePollCheck):
    bl_idname = "view3d.display_modifiers_viewport_switch"
    bl_label = "On/Off"
    bl_description = "Display/Hide modifiers in the viewport"

    mod_switch = BoolProperty(default=True)

    def execute(self, context):
        try:
            selection = context.selected_objects

            if not(selection):
                for obj in bpy.data.objects:
                    for mod in obj.modifiers:
                        mod.show_viewport = self.mod_switch
            else:
                for obj in selection:
                    for mod in obj.modifiers:
                        mod.show_viewport = self.mod_switch
        except:
            self.report({'ERROR'}, "Display/Hide modifiers in the viewport failed")
            return {'CANCELLED'}

        return {'FINISHED'}


# Display Modifiers Edit Switch
class DisplayModifiersEditSwitch(Operator, BasePollCheck):
    bl_idname = "view3d.display_modifiers_edit_switch"
    bl_label = "On/Off"
    bl_description = "Display/Hide modifiers during edit mode"

    mod_edit = BoolProperty(default=True)

    def execute(self, context):
        try:
            selection = context.selected_objects

            if not(selection):
                for obj in bpy.data.objects:
                    for mod in obj.modifiers:
                        mod.show_in_editmode = self.mod_edit
            else:
                for obj in selection:
                    for mod in obj.modifiers:
                        mod.show_in_editmode = self.mod_edit
        except:
            self.report({'ERROR'}, "Display/Hide all modifiers failed")
            return {'CANCELLED'}

        return {'FINISHED'}


class DisplayModifiersCageSet(Operator, BasePollCheck):
    bl_idname = "view3d.display_modifiers_cage_set"
    bl_label = "On/Off"
    bl_description = "Display modifiers editing cage during edit mode"

    set_cage = BoolProperty(default=True)

    def execute(self, context):
        selection = context.selected_objects
        try:
            if not selection:
                for obj in bpy.data.objects:
                    for mod in obj.modifiers:
                        mod.show_on_cage = self.set_cage
            else:
                for obj in selection:
                    for mod in obj.modifiers:
                        mod.show_on_cage = self.set_cage
        except:
            self.report({'ERROR'}, "Setting Editing Cage all modifiers failed")
            return {'CANCELLED'}

        return {'FINISHED'}


class ModifiersSubsurfLevel_Set(Operator, BasePollCheck):
    bl_idname = "view3d.modifiers_subsurf_level_set"
    bl_label = "Set Subsurf level"
    bl_description = "Change subsurf modifier level"

    level = IntProperty(
        name="Subsurf Level",
        description="Change subsurf modifier level",
        default=1,
        min=0,
        max=10,
        soft_min=0,
        soft_max=6
        )

    def execute(self, context):
        selection = context.selected_objects
        try:
            if not selection:
                for obj in bpy.data.objects:
                    context.scene.objects.active = obj
                    bpy.ops.object.modifier_add(type='SUBSURF')
                    value = 0
                    for mod in obj.modifiers:
                        if mod.type == 'SUBSURF':
                            value = value + 1
                            mod.levels = self.level
                        if value > 1:
                            bpy.ops.object.modifier_remove(modifier="Subsurf")
            else:
                for obj in selection:
                    bpy.ops.object.subdivision_set(level=self.level, relative=False)
                    for mod in obj.modifiers:
                        if mod.type == 'SUBSURF':
                            mod.levels = self.level
        except:
            self.report({'ERROR'}, "Setting the Subsurf level could not be applied")
            return {'CANCELLED'}

        return {'FINISHED'}


# Register
def register():
    bpy.utils.register_module(__name__)


def unregister():
    bpy.utils.unregister_module(__name__)


if __name__ == "__main__":
    register()
