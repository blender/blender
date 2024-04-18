# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
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
    COMPAT_ENGINES = {'BLENDER_EEVEE', 'BLENDER_RENDER', 'BLENDER_EEVEE_NEXT'}

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

        if probe.type == 'VOLUME':
            col = layout.column()
            col.prop(probe, "influence_distance", text="Distance")
            col.prop(probe, "falloff")
            col.prop(probe, "intensity")

            sub = col.column(align=True)
            sub.prop(probe, "grid_resolution_x", text="Resolution X")
            sub.prop(probe, "grid_resolution_y", text="Y")
            sub.prop(probe, "grid_resolution_z", text="Z")

        elif probe.type == 'PLANE':
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
        if probe.type != 'PLANE':
            sub.prop(probe, "clip_start", text="Clipping Start")
        else:
            sub.prop(probe, "clip_start", text="Clipping Offset")

        if probe.type != 'PLANE':
            sub.prop(probe, "clip_end", text="End")


class DATA_PT_lightprobe_eevee_next(DataButtonsPanel, Panel):
    bl_label = "Probe"
    COMPAT_ENGINES = {'BLENDER_EEVEE_NEXT'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        probe = context.lightprobe

        if probe.type == 'VOLUME':
            col = layout.column()

            col.prop(probe, "intensity")

            col.separator()

            sub = col.column(align=True)
            sub.prop(probe, "grid_normal_bias")
            sub.prop(probe, "grid_view_bias")
            sub.prop(probe, "grid_facing_bias")

            col.separator()

            col.prop(probe, "grid_validity_threshold")
            sub = col.column(align=True)
            sub.prop(probe, "grid_dilation_threshold")
            sub.prop(probe, "grid_dilation_radius", text="Radius")

            col.separator()

        elif probe.type == 'SPHERE':
            col = layout.column()
            col.prop(probe, "influence_type")

            if probe.influence_type == 'ELIPSOID':
                influence_text = "Radius"
            else:
                influence_text = "Size"

            col.prop(probe, "influence_distance", text=influence_text)
            col.prop(probe, "falloff")

        elif probe.type == 'PLANE':
            col = layout.column()
            col.prop(probe, "influence_distance", text="Distance")
            pass
        else:
            # Currently unsupported
            pass


class DATA_PT_lightprobe_visibility(DataButtonsPanel, Panel):
    bl_label = "Visibility"
    bl_parent_id = "DATA_PT_lightprobe"
    COMPAT_ENGINES = {'BLENDER_EEVEE', 'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        probe = context.lightprobe

        col = layout.column()

        if probe.type == 'VOLUME':
            col.prop(probe, "visibility_buffer_bias", text="Bias")
            col.prop(probe, "visibility_bleed_bias", text="Bleed Bias")
            col.prop(probe, "visibility_blur", text="Blur")

        row = col.row(align=True)
        row.prop(probe, "visibility_collection")
        row.prop(probe, "invert_visibility_collection", text="", icon='ARROW_LEFTRIGHT')


class DATA_PT_lightprobe_capture(DataButtonsPanel, Panel):
    bl_label = "Capture"
    COMPAT_ENGINES = {'BLENDER_EEVEE_NEXT'}

    @classmethod
    def poll(cls, context):
        engine = context.engine
        return context.lightprobe and context.lightprobe.type in {'SPHERE', 'PLANE'} and (engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        probe = context.lightprobe

        col = layout.column()

        if probe.type == 'SPHERE':
            sub = col.column(align=True)
            sub.prop(probe, "clip_start", text="Clipping Start")
            sub.prop(probe, "clip_end", text="End")
        elif probe.type == 'PLANE':
            col.prop(probe, "clip_start", text="Clipping Offset")


class DATA_PT_lightprobe_bake(DataButtonsPanel, Panel):
    bl_label = "Bake"
    COMPAT_ENGINES = {'BLENDER_EEVEE_NEXT'}

    @classmethod
    def poll(cls, context):
        engine = context.engine
        return context.lightprobe and context.lightprobe.type == 'VOLUME' and (engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        probe = context.lightprobe

        col = layout.column()
        row = col.row(align=True)
        row.operator("object.lightprobe_cache_bake").subset = 'ACTIVE'
        row.operator("object.lightprobe_cache_free", text="", icon='TRASH').subset = 'ACTIVE'


class DATA_PT_lightprobe_bake_resolution(DataButtonsPanel, Panel):
    bl_label = "Resolution"
    bl_parent_id = "DATA_PT_lightprobe_bake"
    COMPAT_ENGINES = {'BLENDER_EEVEE_NEXT'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        probe = context.lightprobe

        col = layout.column()

        sub = col.column(align=True)
        sub.prop(probe, "grid_resolution_x", text="Resolution X")
        sub.prop(probe, "grid_resolution_y", text="Y")
        sub.prop(probe, "grid_resolution_z", text="Z")

        col.prop(probe, "grid_bake_samples")
        col.prop(probe, "surfel_density")


class DATA_PT_lightprobe_bake_capture(DataButtonsPanel, Panel):
    bl_label = "Capture"
    bl_parent_id = "DATA_PT_lightprobe_bake"
    COMPAT_ENGINES = {'BLENDER_EEVEE_NEXT'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        probe = context.lightprobe

        col = layout.column()

        col.prop(probe, "grid_capture_distance", text="Distance")

        col = layout.column(heading="Contributions", align=True)
        col.prop(probe, "grid_capture_world", text="World")
        col.prop(probe, "grid_capture_indirect", text="Indirect Light")
        col.prop(probe, "grid_capture_emission", text="Emission")


class DATA_PT_lightprobe_bake_offset(DataButtonsPanel, Panel):
    bl_label = "Offset"
    bl_parent_id = "DATA_PT_lightprobe_bake_capture"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE_NEXT'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        probe = context.lightprobe

        col = layout.column(align=True)
        col.prop(probe, "grid_surface_bias")
        col.prop(probe, "grid_escape_bias")


class DATA_PT_lightprobe_bake_clamping(DataButtonsPanel, Panel):
    bl_label = "Clamping"
    bl_parent_id = "DATA_PT_lightprobe_bake_capture"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE_NEXT'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        probe = context.lightprobe

        col = layout.column(align=True)
        col.prop(probe, "grid_clamp_direct", text="Direct Light")
        col.prop(probe, "grid_clamp_indirect", text="Indirect Light")


class DATA_PT_lightprobe_parallax(DataButtonsPanel, Panel):
    bl_label = "Custom Parallax"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE', 'BLENDER_RENDER', 'BLENDER_EEVEE_NEXT'}

    @classmethod
    def poll(cls, context):
        engine = context.engine
        return context.lightprobe and context.lightprobe.type == 'SPHERE' and (engine in cls.COMPAT_ENGINES)

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
        layout.use_property_decorate = False

        ob = context.object
        probe = context.lightprobe

        col = layout.column()

        if probe.type == 'PLANE':
            col.prop(ob, "empty_display_size", text="Arrow Size")
            col.prop(probe, "show_influence")

        if probe.type in {'VOLUME', 'SPHERE'}:
            col.prop(probe, "show_influence")
            col.prop(probe, "show_clip")

        if probe.type == 'SPHERE':
            sub = col.column()
            sub.active = probe.use_custom_parallax
            sub.prop(probe, "show_parallax")


class DATA_PT_lightprobe_display_eevee_next(DataButtonsPanel, Panel):
    bl_label = "Viewport Display"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE_NEXT'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        ob = context.object
        probe = context.lightprobe

        col = layout.column()

        if probe.type in {'VOLUME', 'SPHERE'}:
            row = col.row(heading="Data")
            row.prop(probe, "use_data_display", text="")
            subrow = row.row()
            subrow.active = probe.use_data_display
            subrow.prop(probe, "data_display_size", text="Size", slider=True)
            col.prop(probe, "show_clip")
            col.prop(probe, "show_influence")

        if probe.type == 'SPHERE':
            sub = col.column()
            sub.active = probe.use_custom_parallax
            sub.prop(probe, "show_parallax")

        if probe.type == 'PLANE':
            col.prop(ob, "empty_display_size", text="Arrow Size")
            col.prop(probe, "use_data_display", text="Capture")
            col.prop(probe, "show_influence")


classes = (
    DATA_PT_context_lightprobe,
    DATA_PT_lightprobe,
    DATA_PT_lightprobe_eevee_next,
    DATA_PT_lightprobe_capture,
    DATA_PT_lightprobe_bake,
    DATA_PT_lightprobe_bake_resolution,
    DATA_PT_lightprobe_bake_capture,
    DATA_PT_lightprobe_bake_clamping,
    DATA_PT_lightprobe_bake_offset,
    DATA_PT_lightprobe_visibility,
    DATA_PT_lightprobe_parallax,
    DATA_PT_lightprobe_display,
    DATA_PT_lightprobe_display_eevee_next,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
