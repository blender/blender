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

        layout.label("Influence:")
        layout.prop(probe, "influence_type", expand=True)

        if probe.influence_type == 'ELIPSOID':
            layout.prop(probe, "influence_distance", "Radius")
            layout.prop(probe, "falloff")
        else:
            layout.prop(probe, "influence_distance", "Size")
            layout.prop(probe, "falloff")

        layout.prop(probe, "show_influence")
        layout.separator()

        layout.label("Clipping:")
        row = layout.row(align=True)
        row.prop(probe, "clip_start", text="Start")
        row.prop(probe, "clip_end", text="End")


class DATA_PT_parallax(DataButtonsPanel, Panel):
    bl_label = "Parallax"
    COMPAT_ENGINES = {'BLENDER_CLAY', 'BLENDER_EEVEE'}

    def draw(self, context):
        layout = self.layout

        ob = context.object
        probe = context.probe

        layout.prop(probe, "use_custom_parallax")

        col = layout.column()
        col.active = probe.use_custom_parallax

        row = col.row()
        row.prop(probe, "parallax_type", expand=True)

        if probe.parallax_type == 'ELIPSOID':
            col.prop(probe, "parallax_distance", "Radius")
        else:
            col.prop(probe, "parallax_distance", "Size")

        col.prop(probe, "show_parallax")


classes = (
    DATA_PT_context_probe,
    DATA_PT_probe,
    DATA_PT_parallax,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
