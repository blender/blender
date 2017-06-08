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


class DataButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        engine = context.scene.render.engine
        return context.probe and (engine in cls.COMPAT_ENGINES)


class DATA_PT_context_probe(DataButtonsPanel, Panel):
    bl_label = ""
    bl_options = {'HIDE_HEADER'}
    COMPAT_ENGINES = {'BLENDER_CLAY', 'BLENDER_EEVEE'}

    def draw(self, context):
        layout = self.layout

        ob = context.object
        probe = context.probe
        space = context.space_data

        if ob:
            layout.template_ID(ob, "data")
        elif probe:
            layout.template_ID(space, "pin_id")


class DATA_PT_probe(DataButtonsPanel, Panel):
    bl_label = "Probe"
    COMPAT_ENGINES = {'BLENDER_CLAY', 'BLENDER_EEVEE'}

    def draw(self, context):
        layout = self.layout

        ob = context.object
        probe = context.probe

        layout.prop(probe, "type", expand=True)

        split = layout.split()

        col = split.column(align=True)
        col.label("Influence:")
        col.prop(probe, "influence_distance", text="Distance")
        col.prop(probe, "falloff")

        col = split.column(align=True)
        col.label("Clipping:")
        col.prop(probe, "clip_start", text="Start")
        col.prop(probe, "clip_end", text="End")


classes = (
    DATA_PT_context_probe,
    DATA_PT_probe,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
