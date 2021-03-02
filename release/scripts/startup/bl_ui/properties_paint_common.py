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
from bpy.types import Menu


class UnifiedPaintPanel:
    # subclass must set
    # bl_space_type = 'IMAGE_EDITOR'
    # bl_region_type = 'UI'

    @staticmethod
    def get_brush_mode(context):
        """ Get the correct mode for this context. For any context where this returns None,
            no brush options should be displayed."""
        mode = context.mode

        if mode == 'PARTICLE':
            # Particle brush settings currently completely do their own thing.
            return None

        from bl_ui.space_toolsystem_common import ToolSelectPanelHelper
        tool = ToolSelectPanelHelper.tool_active_from_context(context)

        if not tool:
            # If there is no active tool, then there can't be an active brush.
            return None

        if not tool.has_datablock:
            # tool.has_datablock is always true for tools that use brushes.
            return None

        space_data = context.space_data
        tool_settings = context.tool_settings

        if space_data:
            space_type = space_data.type
            if space_type == 'IMAGE_EDITOR':
                if space_data.show_uvedit:
                    return 'UV_SCULPT'
                return 'PAINT_2D'
            elif space_type in {'VIEW_3D', 'PROPERTIES'}:
                if mode == 'PAINT_TEXTURE':
                    if tool_settings.image_paint:
                        return mode
                    else:
                        return None
                return mode
        return None

    @staticmethod
    def paint_settings(context):
        tool_settings = context.tool_settings

        mode = UnifiedPaintPanel.get_brush_mode(context)

        # 3D paint settings
        if mode == 'SCULPT':
            return tool_settings.sculpt
        elif mode == 'PAINT_VERTEX':
            return tool_settings.vertex_paint
        elif mode == 'PAINT_WEIGHT':
            return tool_settings.weight_paint
        elif mode == 'PAINT_TEXTURE':
            return tool_settings.image_paint
        elif mode == 'PARTICLE':
            return tool_settings.particle_edit
        # 2D paint settings
        elif mode == 'PAINT_2D':
            return tool_settings.image_paint
        elif mode == 'UV_SCULPT':
            return tool_settings.uv_sculpt
        # Grease Pencil settings
        elif mode == 'PAINT_GPENCIL':
            return tool_settings.gpencil_paint
        elif mode == 'SCULPT_GPENCIL':
            return tool_settings.gpencil_sculpt_paint
        elif mode == 'WEIGHT_GPENCIL':
            return tool_settings.gpencil_weight_paint
        elif mode == 'VERTEX_GPENCIL':
            return tool_settings.gpencil_vertex_paint
        return None

    @staticmethod
    def prop_unified(
            layout,
            context,
            brush,
            prop_name,
            unified_name=None,
            pressure_name=None,
            icon='NONE',
            text=None,
            slider=False,
            header=False,
    ):
        """ Generalized way of adding brush options to the UI,
            along with their pen pressure setting and global toggle, if they exist. """
        row = layout.row(align=True)
        ups = context.tool_settings.unified_paint_settings
        prop_owner = brush
        if unified_name and getattr(ups, unified_name):
            prop_owner = ups

        row.prop(prop_owner, prop_name, icon=icon, text=text, slider=slider)

        if pressure_name:
            row.prop(brush, pressure_name, text="")

        if unified_name and not header:
            # NOTE: We don't draw UnifiedPaintSettings in the header to reduce clutter. D5928#136281
            row.prop(ups, unified_name, text="", icon='BRUSHES_ALL')

        return row

    @staticmethod
    def prop_unified_color(parent, context, brush, prop_name, *, text=None):
        ups = context.tool_settings.unified_paint_settings
        prop_owner = ups if ups.use_unified_color else brush
        parent.prop(prop_owner, prop_name, text=text)

    @staticmethod
    def prop_unified_color_picker(parent, context, brush, prop_name, value_slider=True):
        ups = context.tool_settings.unified_paint_settings
        prop_owner = ups if ups.use_unified_color else brush
        parent.template_color_picker(prop_owner, prop_name, value_slider=value_slider)


### Classes to let various paint modes' panels share code, by sub-classing these classes. ###
class BrushPanel(UnifiedPaintPanel):
    @classmethod
    def poll(cls, context):
        return cls.get_brush_mode(context) is not None


class BrushSelectPanel(BrushPanel):
    bl_label = "Brushes"

    def draw(self, context):
        layout = self.layout
        settings = self.paint_settings(context)
        brush = settings.brush

        row = layout.row()
        large_preview = True
        if large_preview:
            row.column().template_ID_preview(settings, "brush", new="brush.add", rows=3, cols=8, hide_buttons=False)
        else:
            row.column().template_ID(settings, "brush", new="brush.add")
        col = row.column()
        col.menu("VIEW3D_MT_brush_context_menu", icon='DOWNARROW_HLT', text="")

        if brush is not None:
            col.prop(brush, "use_custom_icon", toggle=True, icon='FILE_IMAGE', text="")

            if brush.use_custom_icon:
                layout.prop(brush, "icon_filepath", text="")


class ColorPalettePanel(BrushPanel):
    bl_label = "Color Palette"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        if not super().poll(context):
            return False

        settings = cls.paint_settings(context)
        brush = settings.brush

        if context.space_data.type == 'IMAGE_EDITOR' or context.image_paint_object:
            capabilities = brush.image_paint_capabilities
            return capabilities.has_color

        elif context.vertex_paint_object:
            capabilities = brush.vertex_paint_capabilities
            return capabilities.has_color

        elif context.sculpt_object:
            capabilities = brush.sculpt_capabilities
            return capabilities.has_color
        return False

    def draw(self, context):
        layout = self.layout
        settings = self.paint_settings(context)

        layout.template_ID(settings, "palette", new="palette.new")
        if settings.palette:
            layout.template_palette(settings, "palette", color=True)


class ClonePanel(BrushPanel):
    bl_label = "Clone"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        if not super().poll(context):
            return False

        settings = cls.paint_settings(context)

        mode = cls.get_brush_mode(context)
        if mode == 'PAINT_TEXTURE':
            brush = settings.brush
            return brush.image_tool == 'CLONE'
        return False

    def draw_header(self, context):
        settings = self.paint_settings(context)
        self.layout.prop(settings, "use_clone_layer", text="")

    def draw(self, context):
        layout = self.layout
        settings = self.paint_settings(context)

        layout.active = settings.use_clone_layer

        ob = context.active_object
        col = layout.column()

        if settings.mode == 'MATERIAL':
            if len(ob.material_slots) > 1:
                col.label(text="Materials")
                col.template_list(
                    "MATERIAL_UL_matslots", "",
                    ob, "material_slots",
                    ob, "active_material_index",
                    rows=2,
                )

            mat = ob.active_material
            if mat:
                col.label(text="Source Clone Slot")
                col.template_list(
                    "TEXTURE_UL_texpaintslots", "",
                    mat, "texture_paint_images",
                    mat, "paint_clone_slot",
                    rows=2,
                )

        elif settings.mode == 'IMAGE':
            mesh = ob.data

            clone_text = mesh.uv_layer_clone.name if mesh.uv_layer_clone else ""
            col.label(text="Source Clone Image")
            col.template_ID(settings, "clone_image")
            col.label(text="Source Clone UV Map")
            col.menu("VIEW3D_MT_tools_projectpaint_clone", text=clone_text, translate=False)


class TextureMaskPanel(BrushPanel):
    bl_label = "Texture Mask"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        brush = context.tool_settings.image_paint.brush
        mask_tex_slot = brush.mask_texture_slot

        col = layout.column()
        col.template_ID_preview(mask_tex_slot, "texture", new="texture.new", rows=3, cols=8)

        # map_mode
        layout.row().prop(mask_tex_slot, "mask_map_mode", text="Mask Mapping")

        if mask_tex_slot.map_mode == 'STENCIL':
            if brush.mask_texture and brush.mask_texture.type == 'IMAGE':
                layout.operator("brush.stencil_fit_image_aspect").mask = True
            layout.operator("brush.stencil_reset_transform").mask = True

        col = layout.column()
        col.prop(brush, "use_pressure_masking", text="Pressure Masking")
        # angle and texture_angle_source
        if mask_tex_slot.has_texture_angle:
            col = layout.column()
            col.prop(mask_tex_slot, "angle", text="Angle")
            if mask_tex_slot.has_texture_angle_source:
                col.prop(mask_tex_slot, "use_rake", text="Rake")

                if brush.brush_capabilities.has_random_texture_angle and mask_tex_slot.has_random_texture_angle:
                    col.prop(mask_tex_slot, "use_random", text="Random")
                    if mask_tex_slot.use_random:
                        col.prop(mask_tex_slot, "random_angle", text="Random Angle")

        # scale and offset
        col.prop(mask_tex_slot, "offset")
        col.prop(mask_tex_slot, "scale")


class StrokePanel(BrushPanel):
    bl_label = "Stroke"
    bl_options = {'DEFAULT_CLOSED'}
    bl_ui_units_x = 13

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        mode = self.get_brush_mode(context)
        settings = self.paint_settings(context)
        brush = settings.brush

        col = layout.column()

        col.prop(brush, "stroke_method")
        col.separator()

        if brush.use_anchor:
            col.prop(brush, "use_edge_to_edge", text="Edge to Edge")

        if brush.use_airbrush:
            col.prop(brush, "rate", text="Rate", slider=True)

        if brush.use_space:
            row = col.row(align=True)
            row.prop(brush, "spacing", text="Spacing")
            row.prop(brush, "use_pressure_spacing", toggle=True, text="")

        if brush.use_line or brush.use_curve:
            row = col.row(align=True)
            row.prop(brush, "spacing", text="Spacing")

        if mode == 'SCULPT':
            col.row().prop(brush, "use_scene_spacing", text="Spacing Distance", expand=True)

        if mode in {'PAINT_TEXTURE', 'PAINT_2D', 'SCULPT'}:
            if brush.image_paint_capabilities.has_space_attenuation or brush.sculpt_capabilities.has_space_attenuation:
                col.prop(brush, "use_space_attenuation")

        if brush.use_curve:
            col.separator()
            col.template_ID(brush, "paint_curve", new="paintcurve.new")
            col.operator("paintcurve.draw")
            col.separator()

        if brush.use_space:
            col.separator()
            row = col.row(align=True)
            col.prop(brush, "dash_ratio", text="Dash Ratio")
            col.prop(brush, "dash_samples", text="Dash Length")

        if (mode == 'SCULPT' and brush.sculpt_capabilities.has_jitter) or mode != 'SCULPT':
            col.separator()
            row = col.row(align=True)
            if brush.jitter_unit == 'BRUSH':
                row.prop(brush, "jitter", slider=True)
            else:
                row.prop(brush, "jitter_absolute")
            row.prop(brush, "use_pressure_jitter", toggle=True, text="")
            col.row().prop(brush, "jitter_unit", expand=True)

        col.separator()
        col.prop(settings, "input_samples")


class SmoothStrokePanel(BrushPanel):
    bl_label = "Stabilize Stroke"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        if not super().poll(context):
            return False
        settings = cls.paint_settings(context)
        brush = settings.brush
        if brush.brush_capabilities.has_smooth_stroke:
            return True
        return False

    def draw_header(self, context):
        settings = self.paint_settings(context)
        brush = settings.brush

        self.layout.prop(brush, "use_smooth_stroke", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        settings = self.paint_settings(context)
        brush = settings.brush

        col = layout.column()
        col.active = brush.use_smooth_stroke
        col.prop(brush, "smooth_stroke_radius", text="Radius", slider=True)
        col.prop(brush, "smooth_stroke_factor", text="Factor", slider=True)


class FalloffPanel(BrushPanel):
    bl_label = "Falloff"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        if not super().poll(context):
            return False
        settings = cls.paint_settings(context)
        return (settings and settings.brush and settings.brush.curve)

    def draw(self, context):
        layout = self.layout
        settings = self.paint_settings(context)
        mode = self.get_brush_mode(context)
        brush = settings.brush

        if brush is None:
            return

        col = layout.column(align=True)
        row = col.row(align=True)
        row.prop(brush, "curve_preset", text="")

        if brush.curve_preset == 'CUSTOM':
            layout.template_curve_mapping(brush, "curve", brush=True)

            col = layout.column(align=True)
            row = col.row(align=True)
            row.operator("brush.curve_preset", icon='SMOOTHCURVE', text="").shape = 'SMOOTH'
            row.operator("brush.curve_preset", icon='SPHERECURVE', text="").shape = 'ROUND'
            row.operator("brush.curve_preset", icon='ROOTCURVE', text="").shape = 'ROOT'
            row.operator("brush.curve_preset", icon='SHARPCURVE', text="").shape = 'SHARP'
            row.operator("brush.curve_preset", icon='LINCURVE', text="").shape = 'LINE'
            row.operator("brush.curve_preset", icon='NOCURVE', text="").shape = 'MAX'

        if mode in {'SCULPT', 'PAINT_VERTEX', 'PAINT_WEIGHT'} and brush.sculpt_tool != 'POSE':
            col.separator()
            row = col.row(align=True)
            row.use_property_split = True
            row.use_property_decorate = False
            row.prop(brush, "falloff_shape", expand=True)


class DisplayPanel(BrushPanel):
    bl_label = "Brush Cursor"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_header(self, context):
        settings = self.paint_settings(context)
        if settings and not self.is_popover:
            self.layout.prop(settings, "show_brush", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        mode = self.get_brush_mode(context)
        settings = self.paint_settings(context)
        brush = settings.brush
        tex_slot = brush.texture_slot
        tex_slot_mask = brush.mask_texture_slot

        if self.is_popover:
            row = layout.row(align=True)
            row.prop(settings, "show_brush", text="")
            row.label(text="Display Cursor")

        col = layout.column()
        col.active = brush.brush_capabilities.has_overlay and settings.show_brush

        col.prop(brush, "cursor_color_add", text="Cursor Color")
        if mode == 'SCULPT' and brush.sculpt_capabilities.has_secondary_color:
            col.prop(brush, "cursor_color_subtract", text="Inverse Color")

        col.separator()

        row = col.row(align=True)
        row.prop(brush, "cursor_overlay_alpha", text="Falloff Opacity")
        row.prop(brush, "use_cursor_overlay_override", toggle=True, text="", icon='BRUSH_DATA')
        row.prop(
            brush, "use_cursor_overlay", text="", toggle=True,
            icon='HIDE_OFF' if brush.use_cursor_overlay else 'HIDE_ON',
        )

        if mode in ['PAINT_2D', 'PAINT_TEXTURE', 'PAINT_VERTEX', 'SCULPT']:
            row = col.row(align=True)
            row.prop(brush, "texture_overlay_alpha", text="Texture Opacity")
            row.prop(brush, "use_primary_overlay_override", toggle=True, text="", icon='BRUSH_DATA')
            if tex_slot.map_mode != 'STENCIL':
                row.prop(
                    brush, "use_primary_overlay", text="", toggle=True,
                    icon='HIDE_OFF' if brush.use_primary_overlay else 'HIDE_ON',
                )

        if mode in ['PAINT_TEXTURE', 'PAINT_2D']:
            row = col.row(align=True)
            row.prop(brush, "mask_overlay_alpha", text="Mask Texture Opacity")
            row.prop(brush, "use_secondary_overlay_override", toggle=True, text="", icon='BRUSH_DATA')
            if tex_slot_mask.map_mode != 'STENCIL':
                row.prop(
                    brush, "use_secondary_overlay", text="", toggle=True,
                    icon='HIDE_OFF' if brush.use_secondary_overlay else 'HIDE_ON',
                )


class VIEW3D_MT_tools_projectpaint_clone(Menu):
    bl_label = "Clone Layer"

    def draw(self, context):
        layout = self.layout

        for i, uv_layer in enumerate(context.active_object.data.uv_layers):
            props = layout.operator("wm.context_set_int", text=uv_layer.name, translate=False)
            props.data_path = "active_object.data.uv_layer_clone_index"
            props.value = i


def brush_settings(layout, context, brush, popover=False):
    """ Draw simple brush settings for Sculpt,
        Texture/Vertex/Weight Paint modes, or skip certain settings for the popover """

    mode = UnifiedPaintPanel.get_brush_mode(context)

    ### Draw simple settings unique to each paint mode. ###
    brush_shared_settings(layout, context, brush, popover)

    # Sculpt Mode #
    if mode == 'SCULPT':
        capabilities = brush.sculpt_capabilities
        sculpt_tool = brush.sculpt_tool

        # normal_radius_factor
        layout.prop(brush, "normal_radius_factor", slider=True)

        if context.preferences.experimental.use_sculpt_tools_tilt and capabilities.has_tilt:
            layout.prop(brush, "tilt_strength_factor", slider=True)

        row = layout.row(align=True)
        row.prop(brush, "hardness", slider=True)
        row.prop(brush, "invert_hardness_pressure", text="")
        row.prop(brush, "use_hardness_pressure", text="")

        # auto_smooth_factor and use_inverse_smooth_pressure
        if capabilities.has_auto_smooth:
            UnifiedPaintPanel.prop_unified(
                layout,
                context,
                brush,
                "auto_smooth_factor",
                pressure_name="use_inverse_smooth_pressure",
                slider=True,
            )

        # topology_rake_factor
        if (
                capabilities.has_topology_rake and
                context.sculpt_object.use_dynamic_topology_sculpting
        ):
            layout.prop(brush, "topology_rake_factor", slider=True)

        # normal_weight
        if capabilities.has_normal_weight:
            layout.prop(brush, "normal_weight", slider=True)

        # crease_pinch_factor
        if capabilities.has_pinch_factor:
            text = "Pinch"
            if sculpt_tool in {'BLOB', 'SNAKE_HOOK'}:
                text = "Magnify"
            layout.prop(brush, "crease_pinch_factor", slider=True, text=text)

        # rake_factor
        if capabilities.has_rake_factor:
            layout.prop(brush, "rake_factor", slider=True)

        # plane_offset, use_offset_pressure, use_plane_trim, plane_trim
        if capabilities.has_plane_offset:
            layout.separator()
            UnifiedPaintPanel.prop_unified(
                layout,
                context,
                brush,
                "plane_offset",
                pressure_name="use_offset_pressure",
                slider=True,
            )

            row = layout.row(heading="Plane Trim")
            row.prop(brush, "use_plane_trim", text="")
            sub = row.row()
            sub.active = brush.use_plane_trim
            sub.prop(brush, "plane_trim", slider=True, text="")

            layout.separator()

        # height
        if capabilities.has_height:
            layout.prop(brush, "height", slider=True, text="Height")

        # use_persistent, set_persistent_base
        if capabilities.has_persistence:
            layout.separator()
            layout.prop(brush, "use_persistent")
            layout.operator("sculpt.set_persistent_base")
            layout.separator()

        if capabilities.has_color:
            ups = context.scene.tool_settings.unified_paint_settings
            row = layout.row(align=True)
            UnifiedPaintPanel.prop_unified_color(row, context, brush, "color", text="")
            UnifiedPaintPanel.prop_unified_color(row, context, brush, "secondary_color", text="")
            row.separator()
            row.operator("paint.brush_colors_flip", icon='FILE_REFRESH', text="", emboss=False)
            row.prop(ups, "use_unified_color", text="", icon='BRUSHES_ALL')
            layout.prop(brush, "blend", text="Blend Mode")

        # Per sculpt tool options.

        if sculpt_tool == 'CLAY_STRIPS':
            row = layout.row()
            row.prop(brush, "tip_roundness")

        elif sculpt_tool == 'ELASTIC_DEFORM':
            layout.separator()
            layout.prop(brush, "elastic_deform_type")
            layout.prop(brush, "elastic_deform_volume_preservation", slider=True)
            layout.separator()

        elif sculpt_tool == 'SNAKE_HOOK':
            layout.separator()
            layout.prop(brush, "snake_hook_deform_type")
            layout.separator()

        elif sculpt_tool == 'POSE':
            layout.separator()
            layout.prop(brush, "deform_target")
            layout.separator()
            layout.prop(brush, "pose_deform_type")
            layout.prop(brush, "pose_origin_type")
            layout.prop(brush, "pose_offset")
            layout.prop(brush, "pose_smooth_iterations")
            if brush.pose_deform_type == 'ROTATE_TWIST' and brush.pose_origin_type in {'TOPOLOGY', 'FACE_SETS'}:
                layout.prop(brush, "pose_ik_segments")
            if brush.pose_deform_type == 'SCALE_TRANSLATE':
                layout.prop(brush, "use_pose_lock_rotation")
            layout.prop(brush, "use_pose_ik_anchored")
            layout.prop(brush, "use_connected_only")
            layout.prop(brush, "disconnected_distance_max")

            layout.separator()

        elif sculpt_tool == 'CLOTH':
            layout.separator()
            layout.prop(brush, "cloth_simulation_area_type")
            if brush.cloth_simulation_area_type != 'GLOBAL':
                layout.prop(brush, "cloth_sim_limit")
                layout.prop(brush, "cloth_sim_falloff")

            if brush.cloth_simulation_area_type == 'LOCAL':
                layout.prop(brush, "use_cloth_pin_simulation_boundary")

            layout.separator()
            layout.prop(brush, "cloth_deform_type")
            layout.prop(brush, "cloth_force_falloff_type")
            layout.separator()
            layout.prop(brush, "cloth_mass")
            layout.prop(brush, "cloth_damping")
            layout.prop(brush, "cloth_constraint_softbody_strength")
            layout.separator()
            layout.prop(brush, "use_cloth_collision")
            layout.separator()

        elif sculpt_tool == 'SCRAPE':
            row = layout.row(align=True)
            row.prop(brush, "area_radius_factor")
            row.prop(brush, "use_pressure_area_radius", text="")
            row = layout.row()
            row.prop(brush, "invert_to_scrape_fill", text="Invert to Fill")

        elif sculpt_tool == 'FILL':
            row = layout.row(align=True)
            row.prop(brush, "area_radius_factor")
            row.prop(brush, "use_pressure_area_radius", text="")
            row = layout.row()
            row.prop(brush, "invert_to_scrape_fill", text="Invert to Scrape")

        elif sculpt_tool == 'GRAB':
            layout.prop(brush, "use_grab_active_vertex")
            layout.prop(brush, "use_grab_silhouette")

        elif sculpt_tool == 'PAINT':
            row = layout.row(align=True)
            row.prop(brush, "flow")
            row.prop(brush, "invert_flow_pressure", text="")
            row.prop(brush, "use_flow_pressure", text="")

            row = layout.row(align=True)
            row.prop(brush, "wet_mix")
            row.prop(brush, "invert_wet_mix_pressure", text="")
            row.prop(brush, "use_wet_mix_pressure", text="")

            row = layout.row(align=True)
            row.prop(brush, "wet_persistence")
            row.prop(brush, "invert_wet_persistence_pressure", text="")
            row.prop(brush, "use_wet_persistence_pressure", text="")

            row = layout.row(align=True)
            row.prop(brush, "wet_paint_radius_factor")

            row = layout.row(align=True)
            row.prop(brush, "density")
            row.prop(brush, "invert_density_pressure", text="")
            row.prop(brush, "use_density_pressure", text="")

            row = layout.row()
            row.prop(brush, "tip_roundness")

            row = layout.row()
            row.prop(brush, "tip_scale_x")

        elif sculpt_tool == 'SMEAR':
            col = layout.column()
            col.prop(brush, "smear_deform_type")

        elif sculpt_tool == 'BOUNDARY':
            layout.prop(brush, "deform_target")
            layout.separator()
            col = layout.column()
            col.prop(brush, "boundary_deform_type")
            col.prop(brush, "boundary_falloff_type")
            col.prop(brush, "boundary_offset")

        elif sculpt_tool == 'TOPOLOGY':
            col = layout.column()
            col.prop(brush, "slide_deform_type")

        elif sculpt_tool == 'MULTIPLANE_SCRAPE':
            col = layout.column()
            col.prop(brush, "multiplane_scrape_angle")
            col.prop(brush, "use_multiplane_scrape_dynamic")
            col.prop(brush, "show_multiplane_scrape_planes_preview")

        elif sculpt_tool == 'SMOOTH':
            col = layout.column()
            col.prop(brush, "smooth_deform_type")
            if brush.smooth_deform_type == 'SURFACE':
                col.prop(brush, "surface_smooth_shape_preservation")
                col.prop(brush, "surface_smooth_current_vertex")
                col.prop(brush, "surface_smooth_iterations")

        elif sculpt_tool == 'DISPLACEMENT_SMEAR':
            col = layout.column()
            col.prop(brush, "smear_deform_type")

        elif sculpt_tool == 'MASK':
            layout.row().prop(brush, "mask_tool", expand=True)

        # End sculpt_tool interface.

    # 3D and 2D Texture Paint Mode.
    elif mode in {'PAINT_TEXTURE', 'PAINT_2D'}:
        capabilities = brush.image_paint_capabilities

        if brush.image_tool == 'FILL':
            # For some reason fill threshold only appears to be implemented in 2D paint.
            if brush.color_type == 'COLOR':
                if mode == 'PAINT_2D':
                    layout.prop(brush, "fill_threshold", text="Fill Threshold", slider=True)
            elif brush.color_type == 'GRADIENT':
                layout.row().prop(brush, "gradient_fill_mode", expand=True)


def brush_shared_settings(layout, context, brush, popover=False):
    """ Draw simple brush settings that are shared between different paint modes. """

    mode = UnifiedPaintPanel.get_brush_mode(context)

    ### Determine which settings to draw. ###
    blend_mode = False
    size = False
    size_mode = False
    strength = False
    strength_pressure = False
    weight = False
    direction = False

    # 3D and 2D Texture Paint #
    if mode in {'PAINT_TEXTURE', 'PAINT_2D'}:
        if not popover:
            blend_mode = brush.image_paint_capabilities.has_color
            size = brush.image_paint_capabilities.has_radius
            strength = strength_pressure = True

    # Sculpt #
    if mode == 'SCULPT':
        size_mode = True
        if not popover:
            size = True
            strength = True
            strength_pressure = brush.sculpt_capabilities.has_strength_pressure
            direction = not brush.sculpt_capabilities.has_direction

    # Vertex Paint #
    if mode == 'PAINT_VERTEX':
        if not popover:
            blend_mode = True
            size = True
            strength = True
            strength_pressure = True

    # Weight Paint #
    if mode == 'PAINT_WEIGHT':
        if not popover:
            size = True
            weight = brush.weight_paint_capabilities.has_weight
            strength = strength_pressure = True
        # Only draw blend mode for the Draw tool, because for other tools it is pointless. D5928#137944
        if brush.weight_tool == 'DRAW':
            blend_mode = True

    # UV Sculpt #
    if mode == 'UV_SCULPT':
        size = True
        strength = True

    ### Draw settings. ###
    ups = context.scene.tool_settings.unified_paint_settings

    if blend_mode:
        layout.prop(brush, "blend", text="Blend")
        layout.separator()

    if weight:
        UnifiedPaintPanel.prop_unified(
            layout,
            context,
            brush,
            "weight",
            unified_name="use_unified_weight",
            slider=True,
        )

    size_owner = ups if ups.use_unified_size else brush
    size_prop = "size"
    if size_mode and (size_owner.use_locked_size == 'SCENE'):
        size_prop = "unprojected_radius"
    if size or size_mode:
        if size:
            UnifiedPaintPanel.prop_unified(
                layout,
                context,
                brush,
                size_prop,
                unified_name="use_unified_size",
                pressure_name="use_pressure_size",
                text="Radius",
                slider=True,
            )
        if size_mode:
            layout.row().prop(size_owner, "use_locked_size", expand=True)
            layout.separator()

    if strength:
        pressure_name = "use_pressure_strength" if strength_pressure else None
        UnifiedPaintPanel.prop_unified(
            layout,
            context,
            brush,
            "strength",
            unified_name="use_unified_strength",
            pressure_name=pressure_name,
            slider=True,
        )
        layout.separator()

    if direction:
        layout.row().prop(brush, "direction", expand=True)


def brush_settings_advanced(layout, context, brush, popover=False):
    """Draw advanced brush settings for Sculpt, Texture/Vertex/Weight Paint modes."""

    mode = UnifiedPaintPanel.get_brush_mode(context)

    # In the popover we want to combine advanced brush settings with non-advanced brush settings.
    if popover:
        brush_settings(layout, context, brush, popover=True)
        layout.separator()
        layout.label(text="Advanced")

    # These options are shared across many modes.
    use_accumulate = False
    use_frontface = False

    if mode == 'SCULPT':
        capabilities = brush.sculpt_capabilities
        use_accumulate = capabilities.has_accumulate
        use_frontface = True

        col = layout.column(heading="Auto-Masking", align=True)

        # topology automasking
        col.prop(brush, "use_automasking_topology", text="Topology")

        # face masks automasking
        col.prop(brush, "use_automasking_face_sets", text="Face Sets")

        # boundary edges/face sets automasking
        col.prop(brush, "use_automasking_boundary_edges", text="Mesh Boundary")
        col.prop(brush, "use_automasking_boundary_face_sets", text="Face Sets Boundary")
        col.prop(brush, "automasking_boundary_edges_propagation_steps")

        layout.separator()

        # sculpt plane settings
        if capabilities.has_sculpt_plane:
            layout.prop(brush, "sculpt_plane")
            col = layout.column(heading="Use Original", align=True)
            col.prop(brush, "use_original_normal", text="Normal")
            col.prop(brush, "use_original_plane", text="Plane")
            layout.separator()

    # 3D and 2D Texture Paint.
    elif mode in {'PAINT_TEXTURE', 'PAINT_2D'}:
        capabilities = brush.image_paint_capabilities
        use_accumulate = capabilities.has_accumulate

        if mode == 'PAINT_2D':
            layout.prop(brush, "use_paint_antialiasing")
        else:
            layout.prop(brush, "use_alpha")

        # Tool specific settings
        if brush.image_tool == 'SOFTEN':
            layout.separator()
            layout.row().prop(brush, "direction", expand=True)
            layout.prop(brush, "sharp_threshold")
            if mode == 'PAINT_2D':
                layout.prop(brush, "blur_kernel_radius")
            layout.prop(brush, "blur_mode")

        elif brush.image_tool == 'MASK':
            layout.prop(brush, "weight", text="Mask Value", slider=True)

        elif brush.image_tool == 'CLONE':
            if mode == 'PAINT_2D':
                layout.prop(brush, "clone_image", text="Image")
                layout.prop(brush, "clone_alpha", text="Alpha")

    # Vertex Paint #
    elif mode == 'PAINT_VERTEX':
        layout.prop(brush, "use_alpha")
        if brush.vertex_tool != 'SMEAR':
            use_accumulate = True
        use_frontface = True

    # Weight Paint
    elif mode == 'PAINT_WEIGHT':
        if brush.weight_tool != 'SMEAR':
            use_accumulate = True
        use_frontface = True

    # Draw shared settings.
    if use_accumulate:
        layout.prop(brush, "use_accumulate")

    if use_frontface:
        layout.prop(brush, "use_frontface", text="Front Faces Only")


def draw_color_settings(context, layout, brush, color_type=False):
    """Draw color wheel and gradient settings."""
    ups = context.scene.tool_settings.unified_paint_settings

    if color_type:
        row = layout.row()
        row.use_property_split = False
        row.prop(brush, "color_type", expand=True)

    # Color wheel
    if brush.color_type == 'COLOR':
        UnifiedPaintPanel.prop_unified_color_picker(layout, context, brush, "color", value_slider=True)

        row = layout.row(align=True)
        UnifiedPaintPanel.prop_unified_color(row, context, brush, "color", text="")
        UnifiedPaintPanel.prop_unified_color(row, context, brush, "secondary_color", text="")
        row.separator()
        row.operator("paint.brush_colors_flip", icon='FILE_REFRESH', text="", emboss=False)
        row.prop(ups, "use_unified_color", text="", icon='BRUSHES_ALL')
    # Gradient
    elif brush.color_type == 'GRADIENT':
        layout.template_color_ramp(brush, "gradient", expand=True)

        layout.use_property_split = True

        col = layout.column()

        if brush.image_tool == 'DRAW':
            UnifiedPaintPanel.prop_unified(
                col,
                context,
                brush,
                "secondary_color",
                unified_name="use_unified_color",
                text="Background Color",
                header=True,
            )

            col.prop(brush, "gradient_stroke_mode", text="Gradient Mapping")
            if brush.gradient_stroke_mode in {'SPACING_REPEAT', 'SPACING_CLAMP'}:
                col.prop(brush, "grad_spacing")


# Used in both the View3D toolbar and texture properties
def brush_texture_settings(layout, brush, sculpt):
    tex_slot = brush.texture_slot

    layout.use_property_split = True
    layout.use_property_decorate = False

    # map_mode
    layout.prop(tex_slot, "map_mode", text="Mapping")

    layout.separator()

    if tex_slot.map_mode == 'STENCIL':
        if brush.texture and brush.texture.type == 'IMAGE':
            layout.operator("brush.stencil_fit_image_aspect")
        layout.operator("brush.stencil_reset_transform")

    # angle and texture_angle_source
    if tex_slot.has_texture_angle:
        col = layout.column()
        col.prop(tex_slot, "angle", text="Angle")
        if tex_slot.has_texture_angle_source:
            col.prop(tex_slot, "use_rake", text="Rake")

            if brush.brush_capabilities.has_random_texture_angle and tex_slot.has_random_texture_angle:
                if sculpt:
                    if brush.sculpt_capabilities.has_random_texture_angle:
                        col.prop(tex_slot, "use_random", text="Random")
                        if tex_slot.use_random:
                            col.prop(tex_slot, "random_angle", text="Random Angle")
                else:
                    col.prop(tex_slot, "use_random", text="Random")
                    if tex_slot.use_random:
                        col.prop(tex_slot, "random_angle", text="Random Angle")

    # scale and offset
    layout.prop(tex_slot, "offset")
    layout.prop(tex_slot, "scale")

    if sculpt:
        # texture_sample_bias
        layout.prop(brush, "texture_sample_bias", slider=True, text="Sample Bias")


def brush_mask_texture_settings(layout, brush):
    mask_tex_slot = brush.mask_texture_slot

    layout.use_property_split = True
    layout.use_property_decorate = False

    # map_mode
    layout.row().prop(mask_tex_slot, "mask_map_mode", text="Mask Mapping")

    if mask_tex_slot.map_mode == 'STENCIL':
        if brush.mask_texture and brush.mask_texture.type == 'IMAGE':
            layout.operator("brush.stencil_fit_image_aspect").mask = True
        layout.operator("brush.stencil_reset_transform").mask = True

    col = layout.column()
    col.prop(brush, "use_pressure_masking", text="Pressure Masking")
    # angle and texture_angle_source
    if mask_tex_slot.has_texture_angle:
        col = layout.column()
        col.prop(mask_tex_slot, "angle", text="Angle")
        if mask_tex_slot.has_texture_angle_source:
            col.prop(mask_tex_slot, "use_rake", text="Rake")

            if brush.brush_capabilities.has_random_texture_angle and mask_tex_slot.has_random_texture_angle:
                col.prop(mask_tex_slot, "use_random", text="Random")
                if mask_tex_slot.use_random:
                    col.prop(mask_tex_slot, "random_angle", text="Random Angle")

    # scale and offset
    col.prop(mask_tex_slot, "offset")
    col.prop(mask_tex_slot, "scale")


def brush_basic_texpaint_settings(layout, context, brush, *, compact=False):
    """Draw Tool Settings header for Vertex Paint and 2D and 3D Texture Paint modes."""
    capabilities = brush.image_paint_capabilities

    if capabilities.has_color:
        UnifiedPaintPanel.prop_unified_color(layout, context, brush, "color", text="")
        layout.prop(brush, "blend", text="" if compact else "Blend")

    UnifiedPaintPanel.prop_unified(
        layout,
        context,
        brush,
        "size",
        pressure_name="use_pressure_size",
        unified_name="use_unified_size",
        slider=True,
        text="Radius",
        header=True
    )
    UnifiedPaintPanel.prop_unified(
        layout,
        context,
        brush,
        "strength",
        pressure_name="use_pressure_strength",
        unified_name="use_unified_strength",
        header=True
    )


def brush_basic__draw_color_selector(context, layout, brush, gp_settings, props):
    tool_settings = context.scene.tool_settings
    settings = tool_settings.gpencil_paint
    ma = gp_settings.material

    row = layout.row(align=True)
    if not gp_settings.use_material_pin:
        ma = context.object.active_material
    icon_id = 0
    if ma:
        icon_id = ma.id_data.preview.icon_id
        txt_ma = ma.name
        maxw = 25
        if len(txt_ma) > maxw:
            txt_ma = txt_ma[:maxw - 5] + '..' + txt_ma[-3:]
    else:
        txt_ma = ""

    sub = row.row()
    sub.ui_units_x = 8
    sub.popover(
        panel="TOPBAR_PT_gpencil_materials",
        text=txt_ma,
        icon_value=icon_id,
    )

    row.prop(gp_settings, "use_material_pin", text="")

    if brush.gpencil_tool in {'DRAW', 'FILL'}:
        row.separator(factor=1.0)
        sub_row = row.row(align=True)
        sub_row.enabled = not gp_settings.pin_draw_mode
        if gp_settings.pin_draw_mode:
            sub_row.prop_enum(gp_settings, "brush_draw_mode", 'MATERIAL', text="", icon='MATERIAL')
            sub_row.prop_enum(gp_settings, "brush_draw_mode", 'VERTEXCOLOR', text="", icon='VPAINT_HLT')
        else:
            sub_row.prop_enum(settings, "color_mode", 'MATERIAL', text="", icon='MATERIAL')
            sub_row.prop_enum(settings, "color_mode", 'VERTEXCOLOR', text="", icon='VPAINT_HLT')

        sub_row = row.row(align=True)
        sub_row.enabled = settings.color_mode == 'VERTEXCOLOR' or gp_settings.brush_draw_mode == 'VERTEXCOLOR'
        sub_row.prop_with_popover(brush, "color", text="", panel="TOPBAR_PT_gpencil_vertexcolor")
        row.prop(gp_settings, "pin_draw_mode", text="")

    if props:
        row = layout.row(align=True)
        row.prop(props, "subdivision")


def brush_basic_gpencil_paint_settings(layout, context, brush, *, compact=False):
    tool_settings = context.tool_settings
    settings = tool_settings.gpencil_paint
    gp_settings = brush.gpencil_settings
    tool = context.workspace.tools.from_space_view3d_mode(context.mode, create=False)
    if gp_settings is None:
        return

    # Brush details
    if brush.gpencil_tool == 'ERASE':
        row = layout.row(align=True)
        row.prop(brush, "size", text="Radius")
        row.prop(gp_settings, "use_pressure", text="", icon='STYLUS_PRESSURE')
        row.prop(gp_settings, "use_occlude_eraser", text="", icon='XRAY')
        row.prop(gp_settings, "use_default_eraser", text="")

        row = layout.row(align=True)
        row.prop(gp_settings, "eraser_mode", expand=True)
        if gp_settings.eraser_mode == 'SOFT':
            row = layout.row(align=True)
            row.prop(gp_settings, "pen_strength", slider=True)
            row.prop(gp_settings, "use_strength_pressure", text="", icon='STYLUS_PRESSURE')
            row = layout.row(align=True)
            row.prop(gp_settings, "eraser_strength_factor")
            row = layout.row(align=True)
            row.prop(gp_settings, "eraser_thickness_factor")

        row = layout.row(align=True)
        row.prop(settings, "show_brush", text="Display Cursor")

    # FIXME: tools must use their own UI drawing!
    elif brush.gpencil_tool == 'FILL':
        use_property_split_prev = layout.use_property_split
        if compact:
            row = layout.row(align=True)
            row.prop(gp_settings, "fill_direction", text="", expand=True)
        else:
            layout.use_property_split = False
            row = layout.row(align=True)
            row.prop(gp_settings, "fill_direction", expand=True)

        row = layout.row(align=True)
        row.prop(gp_settings, "fill_factor")
        row = layout.row(align=True)
        row.prop(gp_settings, "fill_leak", text="Leak Size")
        row = layout.row(align=True)
        row.prop(brush, "size", text="Thickness")
        layout.use_property_split = use_property_split_prev

    else:  # brush.gpencil_tool == 'DRAW/TINT':
        row = layout.row(align=True)
        row.prop(brush, "size", text="Radius")
        row.prop(gp_settings, "use_pressure", text="", icon='STYLUS_PRESSURE')

        if gp_settings.use_pressure and context.area.type == 'PROPERTIES':
            col = layout.column()
            col.template_curve_mapping(gp_settings, "curve_sensitivity", brush=True,
                                       use_negative_slope=True)

        row = layout.row(align=True)
        row.prop(gp_settings, "pen_strength", slider=True)
        row.prop(gp_settings, "use_strength_pressure", text="", icon='STYLUS_PRESSURE')

        if gp_settings.use_strength_pressure and context.area.type == 'PROPERTIES':
            col = layout.column()
            col.template_curve_mapping(gp_settings, "curve_strength", brush=True,
                                       use_negative_slope=True)

        if brush.gpencil_tool == 'TINT':
            row = layout.row(align=True)
            row.prop(gp_settings, "vertex_mode", text="Mode")

    # FIXME: tools must use their own UI drawing!
    if tool.idname in {
            "builtin.arc",
            "builtin.curve",
            "builtin.line",
            "builtin.box",
            "builtin.circle",
            "builtin.polyline"
    }:
        settings = context.tool_settings.gpencil_sculpt
        if compact:
            row = layout.row(align=True)
            row.prop(settings, "use_thickness_curve", text="", icon='SPHERECURVE')
            sub = row.row(align=True)
            sub.active = settings.use_thickness_curve
            sub.popover(
                panel="TOPBAR_PT_gpencil_primitive",
                text="Thickness Profile",
            )
        else:
            row = layout.row(align=True)
            row.prop(settings, "use_thickness_curve", text="Use Thickness Profile")
            sub = row.row(align=True)
            if settings.use_thickness_curve:
                # Curve
                layout.template_curve_mapping(settings, "thickness_primitive_curve", brush=True)


def brush_basic_gpencil_sculpt_settings(layout, _context, brush, *, compact=False):
    gp_settings = brush.gpencil_settings
    tool = brush.gpencil_sculpt_tool

    row = layout.row(align=True)
    row.prop(brush, "size", slider=True)
    sub = row.row(align=True)
    sub.enabled = tool not in {'GRAB', 'CLONE'}
    sub.prop(gp_settings, "use_pressure", text="")

    row = layout.row(align=True)
    row.prop(brush, "strength", slider=True)
    row.prop(brush, "use_pressure_strength", text="")

    if compact:
        if tool in {'THICKNESS', 'STRENGTH', 'PINCH', 'TWIST'}:
            row.separator()
            row.prop(gp_settings, "direction", expand=True, text="")
    else:
        use_property_split_prev = layout.use_property_split
        layout.use_property_split = False
        if tool in {'THICKNESS', 'STRENGTH'}:
            layout.row().prop(gp_settings, "direction", expand=True)
        elif tool == 'PINCH':
            row = layout.row(align=True)
            row.prop_enum(gp_settings, "direction", value='ADD', text="Pinch")
            row.prop_enum(gp_settings, "direction", value='SUBTRACT', text="Inflate")
        elif tool == 'TWIST':
            row = layout.row(align=True)
            row.prop_enum(gp_settings, "direction", value='ADD', text="CCW")
            row.prop_enum(gp_settings, "direction", value='SUBTRACT', text="CW")
        layout.use_property_split = use_property_split_prev


def brush_basic_gpencil_weight_settings(layout, _context, brush, *, compact=False):
    layout.prop(brush, "size", slider=True)

    row = layout.row(align=True)
    row.prop(brush, "strength", slider=True)
    row.prop(brush, "use_pressure_strength", text="")

    layout.prop(brush, "weight", slider=True)


def brush_basic_gpencil_vertex_settings(layout, _context, brush, *, compact=False):
    gp_settings = brush.gpencil_settings

    # Brush details
    row = layout.row(align=True)
    row.prop(brush, "size", text="Radius")
    row.prop(gp_settings, "use_pressure", text="", icon='STYLUS_PRESSURE')

    if brush.gpencil_vertex_tool in {'DRAW', 'BLUR', 'SMEAR'}:
        row = layout.row(align=True)
        row.prop(gp_settings, "pen_strength", slider=True)
        row.prop(gp_settings, "use_strength_pressure", text="", icon='STYLUS_PRESSURE')

    if brush.gpencil_vertex_tool in {'DRAW', 'REPLACE'}:
        row = layout.row(align=True)
        row.prop(gp_settings, "vertex_mode", text="Mode")


classes = (
    VIEW3D_MT_tools_projectpaint_clone,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
