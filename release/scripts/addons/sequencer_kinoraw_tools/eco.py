# File sequencer_slide_strip.py

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
from bpy.types import (
        Operator,
        Panel,
        )
from . import functions


class EcoPanel(Panel):
    bl_label = "Eco Tool"
    bl_idname = "OBJECT_PT_EcoTool"
    bl_space_type = "SEQUENCE_EDITOR"
    bl_region_type = "UI"

    @staticmethod
    def has_sequencer(context):
        return (context.space_data.view_type in
               {'SEQUENCER', 'SEQUENCER_PREVIEW'})

    @classmethod
    def poll(self, context):
        if context.space_data.view_type in {'SEQUENCER', 'SEQUENCER_PREVIEW'}:
            strip = functions.act_strip(context)
            scn = context.scene
            preferences = context.user_preferences
            prefs = preferences.addons[__package__].preferences
            if scn and scn.sequence_editor and scn.sequence_editor.active_strip:
                if prefs.use_eco_tools:
                    return strip.type in ('META')
        else:
            return False

    def draw_header(self, context):
        layout = self.layout
        layout.label(text="", icon="FORCE_HARMONIC")

    def draw(self, context):
        strip = functions.act_strip(context)
        seq_type = strip.type

        preferences = context.user_preferences
        prefs = preferences.addons[__package__].preferences

        if seq_type in ('MOVIE', 'IMAGE', 'META', 'MOVIECLIP', 'SCENE'):
            layout = self.layout
            col = layout.column()

            col.prop(prefs, "eco_value", text="Ecos")
            col.prop(prefs, "eco_offset", text="Offset")
            col.prop(prefs, "eco_use_add_blend_mode", text="Use add blend mode")
            col.operator("sequencer.eco")


class OBJECT_OT_EcoOperator(Operator):
    bl_idname = "sequencer.eco"
    bl_label = "Eco operator"
    bl_description = "Generate an echo effect by duplicating the selected strip"
    bl_options = {'REGISTER', 'UNDO'}

    @staticmethod
    def has_sequencer(context):
        return (context.space_data.view_type in
               {'SEQUENCER', 'SEQUENCER_PREVIEW'})

    @classmethod
    def poll(self, context):
        strip = functions.act_strip(context)
        scn = context.scene
        if scn and scn.sequence_editor and scn.sequence_editor.active_strip:
            return strip.type in ('META')
        else:
            return False

    def execute(self, context):
        active_strip = functions.act_strip(context)

        preferences = context.user_preferences
        prefs = preferences.addons[__package__].preferences

        eco = prefs.eco_value
        offset = prefs.eco_offset

        active_strip.blend_type = 'REPLACE'
        active_strip.blend_alpha = 1
        for i in range(eco):
            bpy.ops.sequencer.duplicate(mode='TRANSLATION')
            bpy.ops.transform.seq_slide(
                    value=(offset, 1), snap=False, snap_target='CLOSEST',
                    snap_point=(0, 0, 0), snap_align=False,
                    snap_normal=(0, 0, 0), release_confirm=False
                    )

            active_strip = functions.act_strip(context)

            if prefs.eco_use_add_blend_mode:
                active_strip.blend_type = 'ADD'
                active_strip.blend_alpha = 1 - 1 / eco
            else:
                active_strip.blend_type = 'ALPHA_OVER'
                active_strip.blend_alpha = 1 / eco

        bpy.ops.sequencer.select_all(action='TOGGLE')
        bpy.ops.sequencer.select_all(action='TOGGLE')
        bpy.ops.sequencer.meta_make()

        return {'FINISHED'}
