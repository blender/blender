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

# <pep8 compliant>
import bpy
from bpy.types import Panel
from rna_prop_ui import PropertyPanel


class DataButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        engine = context.engine
        return context.speaker and (engine in cls.COMPAT_ENGINES)


class DATA_PT_context_speaker(DataButtonsPanel, Panel):
    bl_label = ""
    bl_options = {'HIDE_HEADER'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    def draw(self, context):
        layout = self.layout

        ob = context.object
        speaker = context.speaker
        space = context.space_data

        split = layout.split(percentage=0.65)

        if ob:
            split.template_ID(ob, "data")
        elif speaker:
            split.template_ID(space, "pin_id")


class DATA_PT_speaker(DataButtonsPanel, Panel):
    bl_label = "Sound"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    def draw(self, context):
        layout = self.layout

        speaker = context.speaker

        layout.template_ID(speaker, "sound", open="sound.open_mono")

        layout.use_property_split = True

        layout.prop(speaker, "muted")

        col = layout.column()
        col.active = not speaker.muted
        col.prop(speaker, "volume", slider=True)
        col.prop(speaker, "pitch")


class DATA_PT_distance(DataButtonsPanel, Panel):
    bl_label = "Distance"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    def draw(self, context):
        layout = self.layout

        layout.use_property_split = True

        speaker = context.speaker
        layout.active = not speaker.muted

        col = layout.column()
        sub = col.column(align=True)
        sub.prop(speaker, "volume_min", slider=True, text="Volume Min")
        sub.prop(speaker, "volume_max", slider=True, text="Max")
        col.prop(speaker, "attenuation")

        col.separator()
        col.prop(speaker, "distance_max", text="Max Distance")
        col.prop(speaker, "distance_reference", text="Distance Reference")


class DATA_PT_cone(DataButtonsPanel, Panel):
    bl_label = "Cone"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    def draw(self, context):
        layout = self.layout

        layout.use_property_split = True

        speaker = context.speaker
        layout.active = not speaker.muted

        col = layout.column()

        sub = col.column(align=True)
        sub.prop(speaker, "cone_angle_outer", text="Angle Outer")
        sub.prop(speaker, "cone_angle_inner", text="Inner")

        col.separator()

        col.prop(speaker, "cone_volume_outer", slider=True)


class DATA_PT_custom_props_speaker(DataButtonsPanel, PropertyPanel, Panel):
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}
    _context_path = "object.data"
    _property_type = bpy.types.Speaker


classes = (
    DATA_PT_context_speaker,
    DATA_PT_speaker,
    DATA_PT_distance,
    DATA_PT_cone,
    DATA_PT_custom_props_speaker,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
