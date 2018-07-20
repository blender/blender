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
        engine = context.engine
        return context.lightprobe and (engine in cls.COMPAT_ENGINES)


class DATA_PT_context_lightprobe(DataButtonsPanel, Panel):
    bl_label = ""
    bl_options = {'HIDE_HEADER'}
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    def draw(self, context):
        layout = self.layout

        ob = context.object
        probe = context.lightprobe
        space = context.space_data

        if ob:
            layout.template_ID(ob, "data")
        elif probe:
            layout.template_ID(space, "pin_id")


class DATA_PT_lightprobe(DataButtonsPanel, Panel):
    bl_label = "Probe"
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        ob = context.object
        probe = context.lightprobe

#        layout.prop(probe, "type")

        if probe.type == 'GRID':
            col = layout.column()
            col.prop(probe, "influence_distance", "Distance")
            col.prop(probe, "falloff")
            col.prop(probe, "intensity")

            col.separator()

            col.prop(probe, "grid_resolution_x", text="Resolution X")
            col.prop(probe, "grid_resolution_y", text="Y")
            col.prop(probe, "grid_resolution_z", text="Z")

        elif probe.type == 'PLANAR':
            col = layout.column()
            col.prop(probe, "influence_distance", "Distance")
            col.prop(probe, "falloff")
        else:
            col = layout.column()
            col.prop(probe, "influence_type")

            if probe.influence_type == 'ELIPSOID':
                col.prop(probe, "influence_distance", "Radius")
            else:
                col.prop(probe, "influence_distance", "Size")

            col.prop(probe, "falloff")
            col.prop(probe, "intensity")

        col = layout.column()
        sub = col.column()
        sub.prop(probe, "clip_start", text="Clipping Start")

        if probe.type != "PLANAR":
            sub.prop(probe, "clip_end", text="End")

        if probe.type == 'GRID':
            col.separator()
            col.label("Visibility")
            col.prop(probe, "visibility_buffer_bias", "Bias")
            col.prop(probe, "visibility_bleed_bias", "Bleed Bias")
            col.prop(probe, "visibility_blur", "Blur")

        col.separator()

        row = col.row(align=True)
        row.prop(probe, "visibility_collection")
        row.prop(probe, "invert_visibility_collection", text="", icon='ARROW_LEFTRIGHT')


class DATA_PT_lightprobe_parallax(DataButtonsPanel, Panel):
    bl_label = "Custom Parallax"
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    @classmethod
    def poll(cls, context):
        engine = context.engine
        return context.lightprobe and context.lightprobe.type == 'CUBEMAP' and (engine in cls.COMPAT_ENGINES)

    def draw_header(self, context):
        probe = context.lightprobe
        self.layout.prop(probe, "use_custom_parallax", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        probe = context.lightprobe

        col = layout.column()
        col.active = probe.use_custom_parallax

        col.prop(probe, "parallax_type")

        if probe.parallax_type == 'ELIPSOID':
            col.prop(probe, "parallax_distance", "Radius")
        else:
            col.prop(probe, "parallax_distance", "Size")


class DATA_PT_lightprobe_display(DataButtonsPanel, Panel):
    bl_label = "Display"
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        ob = context.object
        probe = context.lightprobe

        col = layout.column()

        if probe.type == 'PLANAR':
            col.prop(ob, "empty_draw_size", text="Arrow Size")
            col.prop(probe, "show_data")

        if probe.type in {'GRID', 'CUBEMAP'}:
            col.prop(probe, "show_influence")
            col.prop(probe, "show_clip")

        if probe.type == 'CUBEMAP':
            sub = col.column()
            sub.active = probe.use_custom_parallax
            sub.prop(probe, "show_parallax")


classes = (
    DATA_PT_context_lightprobe,
    DATA_PT_lightprobe,
    DATA_PT_lightprobe_parallax,
    DATA_PT_lightprobe_display,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
