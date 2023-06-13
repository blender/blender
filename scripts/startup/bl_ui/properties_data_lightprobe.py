# SPDX-License-Identifier: GPL-2.0-or-later
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
    COMPAT_ENGINES = {'BLENDER_EEVEE', 'BLENDER_RENDER'}

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
    COMPAT_ENGINES = {'BLENDER_EEVEE', 'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        probe = context.lightprobe

#        layout.prop(probe, "type")

        if probe.type == 'GRID':
            col = layout.column()
            col.prop(probe, "influence_distance", text="Distance")
            col.prop(probe, "falloff")
            col.prop(probe, "intensity")

            sub = col.column(align=True)
            sub.prop(probe, "grid_resolution_x", text="Resolution X")
            sub.prop(probe, "grid_resolution_y", text="Y")
            sub.prop(probe, "grid_resolution_z", text="Z")

        elif probe.type == 'PLANAR':
            col = layout.column()
            col.prop(probe, "influence_distance", text="Distance")
            col.prop(probe, "falloff")
        else:
            col = layout.column()
            col.prop(probe, "influence_type")

            if probe.influence_type == 'ELIPSOID':
                col.prop(probe, "influence_distance", text="Radius")
            else:
                col.prop(probe, "influence_distance", text="Size")

            col.prop(probe, "falloff")
            col.prop(probe, "intensity")

        sub = col.column(align=True)
        if probe.type != 'PLANAR':
            sub.prop(probe, "clip_start", text="Clipping Start")
        else:
            sub.prop(probe, "clip_start", text="Clipping Offset")

        if probe.type != 'PLANAR':
            sub.prop(probe, "clip_end", text="End")


class DATA_PT_lightprobe_visibility(DataButtonsPanel, Panel):
    bl_label = "Visibility"
    bl_parent_id = "DATA_PT_lightprobe"
    COMPAT_ENGINES = {'BLENDER_EEVEE', 'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        probe = context.lightprobe

        col = layout.column()

        if probe.type == 'GRID':
            col.prop(probe, "visibility_buffer_bias", text="Bias")
            col.prop(probe, "visibility_bleed_bias", text="Bleed Bias")
            col.prop(probe, "visibility_blur", text="Blur")

        row = col.row(align=True)
        row.prop(probe, "visibility_collection")
        row.prop(probe, "invert_visibility_collection", text="", icon='ARROW_LEFTRIGHT')


class DATA_PT_lightprobe_parallax(DataButtonsPanel, Panel):
    bl_label = "Custom Parallax"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE', 'BLENDER_RENDER'}

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
            col.prop(probe, "parallax_distance", text="Radius")
        else:
            col.prop(probe, "parallax_distance", text="Size")


class DATA_PT_lightprobe_display(DataButtonsPanel, Panel):
    bl_label = "Viewport Display"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE', 'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        ob = context.object
        probe = context.lightprobe

        col = layout.column()

        if probe.type == 'PLANAR':
            col.prop(ob, "empty_display_size", text="Arrow Size")
            col.prop(probe, "show_influence")
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
    DATA_PT_lightprobe_visibility,
    DATA_PT_lightprobe_parallax,
    DATA_PT_lightprobe_display,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
