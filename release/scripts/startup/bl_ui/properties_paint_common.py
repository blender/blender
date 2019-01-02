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
    def paint_settings(context):
        tool_settings = context.tool_settings

        if context.sculpt_object:
            return tool_settings.sculpt
        elif context.vertex_paint_object:
            return tool_settings.vertex_paint
        elif context.weight_paint_object:
            return tool_settings.weight_paint
        elif context.image_paint_object:
            if (tool_settings.image_paint and tool_settings.image_paint.detect_data()):
                return tool_settings.image_paint

            return None
        elif context.particle_edit_object:
            return tool_settings.particle_edit

        return None

    @staticmethod
    def unified_paint_settings(parent, context):
        ups = context.tool_settings.unified_paint_settings

        flow = parent.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)

        col = flow.column()
        col.prop(ups, "use_unified_size", text="Size")
        col = flow.column()
        col.prop(ups, "use_unified_strength", text="Strength")
        if context.weight_paint_object:
            col = flow.column()
            col.prop(ups, "use_unified_weight", text="Weight")
        elif context.vertex_paint_object or context.image_paint_object:
            col = flow.column()
            col.prop(ups, "use_unified_color", text="Color")
        else:
            col = flow.column()
            col.prop(ups, "use_unified_color", text="Color")

    @staticmethod
    def prop_unified_size(parent, context, brush, prop_name, icon='NONE', text="", slider=False):
        ups = context.tool_settings.unified_paint_settings
        ptr = ups if ups.use_unified_size else brush
        parent.prop(ptr, prop_name, icon=icon, text=text, slider=slider)

    @staticmethod
    def prop_unified_strength(parent, context, brush, prop_name, icon='NONE', text="", slider=False):
        ups = context.tool_settings.unified_paint_settings
        ptr = ups if ups.use_unified_strength else brush
        parent.prop(ptr, prop_name, icon=icon, text=text, slider=slider)

    @staticmethod
    def prop_unified_weight(parent, context, brush, prop_name, icon='NONE', text="", slider=False):
        ups = context.tool_settings.unified_paint_settings
        ptr = ups if ups.use_unified_weight else brush
        parent.prop(ptr, prop_name, icon=icon, text=text, slider=slider)

    @staticmethod
    def prop_unified_color(parent, context, brush, prop_name, text=""):
        ups = context.tool_settings.unified_paint_settings
        ptr = ups if ups.use_unified_color else brush
        parent.prop(ptr, prop_name, text=text)

    @staticmethod
    def prop_unified_color_picker(parent, context, brush, prop_name, value_slider=True):
        ups = context.tool_settings.unified_paint_settings
        ptr = ups if ups.use_unified_color else brush
        parent.template_color_picker(ptr, prop_name, value_slider=value_slider)


class VIEW3D_MT_tools_projectpaint_clone(Menu):
    bl_label = "Clone Layer"

    def draw(self, context):
        layout = self.layout

        for i, uv_layer in enumerate(context.active_object.data.uv_layers):
            props = layout.operator("wm.context_set_int", text=uv_layer.name, translate=False)
            props.data_path = "active_object.data.uv_layer_clone_index"
            props.value = i


def brush_texpaint_common(panel, context, layout, brush, settings, projpaint=False):
    capabilities = brush.image_paint_capabilities

    col = layout.column()

    if brush.image_tool in {'DRAW', 'FILL'}:
        if brush.blend not in {'ERASE_ALPHA', 'ADD_ALPHA'}:
            if not brush.use_gradient:
                panel.prop_unified_color_picker(col, context, brush, "color", value_slider=True)

            if settings.palette:
                col.template_palette(settings, "palette", color=True)

            if brush.use_gradient:
                col.label(text="Gradient Colors")
                col.template_color_ramp(brush, "gradient", expand=True)

                if brush.image_tool == 'DRAW':
                    col.label(text="Background Color")
                    row = col.row(align=True)
                    panel.prop_unified_color(row, context, brush, "secondary_color", text="")
                    col.prop(brush, "gradient_stroke_mode", text="Mode")
                    if brush.gradient_stroke_mode in {'SPACING_REPEAT', 'SPACING_CLAMP'}:
                        col.prop(brush, "grad_spacing")
                else:  # if brush.image_tool == 'FILL':
                    col.prop(brush, "gradient_fill_mode")
            else:
                row = col.row(align=True)
                panel.prop_unified_color(row, context, brush, "color", text="")
                if brush.image_tool == 'FILL' and not projpaint:
                    col.prop(brush, "fill_threshold")
                else:
                    panel.prop_unified_color(row, context, brush, "secondary_color", text="")
                    row.separator()
                    row.operator("paint.brush_colors_flip", icon='FILE_REFRESH', text="")
        else:
            if brush.image_tool == 'FILL' and not projpaint:
                col.prop(brush, "fill_threshold")

    elif brush.image_tool == 'SOFTEN':
        col = layout.column(align=True)
        col.row().prop(brush, "direction", expand=True)
        col.separator()
        col.prop(brush, "sharp_threshold")
        if not projpaint:
            col.prop(brush, "blur_kernel_radius")
        col.separator()
        col.prop(brush, "blur_mode")
    elif brush.image_tool == 'MASK':
        col.prop(brush, "weight", text="Mask Value", slider=True)

    elif brush.image_tool == 'CLONE':
        col.separator()
        if projpaint:
            if settings.mode == 'MATERIAL':
                col.prop(settings, "use_clone_layer", text="Clone from Paint Slot")
            elif settings.mode == 'IMAGE':
                col.prop(settings, "use_clone_layer", text="Clone from Image/UV Map")

            if settings.use_clone_layer:
                ob = context.active_object
                col = layout.column()

                if settings.mode == 'MATERIAL':
                    if len(ob.material_slots) > 1:
                        col.label(text="Materials")
                        col.template_list("MATERIAL_UL_matslots", "",
                                          ob, "material_slots",
                                          ob, "active_material_index", rows=2)

                    mat = ob.active_material
                    if mat:
                        col.label(text="Source Clone Slot")
                        col.template_list("TEXTURE_UL_texpaintslots", "",
                                          mat, "texture_paint_images",
                                          mat, "paint_clone_slot", rows=2)

                elif settings.mode == 'IMAGE':
                    mesh = ob.data

                    clone_text = mesh.uv_layer_clone.name if mesh.uv_layer_clone else ""
                    col.label(text="Source Clone Image")
                    col.template_ID(settings, "clone_image")
                    col.label(text="Source Clone UV Map")
                    col.menu("VIEW3D_MT_tools_projectpaint_clone", text=clone_text, translate=False)
        else:
            col.prop(brush, "clone_image", text="Image")
            col.prop(brush, "clone_alpha", text="Alpha")

    col.separator()

    if not panel.is_popover:
        brush_basic_texpaint_settings(col, context, brush)

    col = layout.column()

    # use_accumulate
    if capabilities.has_accumulate:
        col = layout.column(align=True)
        col.prop(brush, "use_accumulate")

    if projpaint:
        col.prop(brush, "use_alpha")

    col.prop(brush, "use_gradient")

    col.separator()
    col.template_ID(settings, "palette", new="palette.new")


# Used in both the View3D toolbar and texture properties
def brush_texture_settings(layout, brush, sculpt):
    tex_slot = brush.texture_slot

    layout.use_property_split = True
    layout.use_property_decorate = False

    # map_mode
    if sculpt:
        layout.prop(tex_slot, "map_mode", text="Mapping")
    else:
        layout.prop(tex_slot, "tex_paint_map_mode", text="Mapping")

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

# Basic Brush Options
#
# Share between topbar and brush panel.

def brush_basic_wpaint_settings(layout, context, brush, *, compact=False):
    row = layout.row(align=True)
    UnifiedPaintPanel.prop_unified_weight(row, context, brush, "weight", slider=True, text="Weight")

    row = layout.row(align=True)
    UnifiedPaintPanel.prop_unified_size(row, context, brush, "size", slider=True, text="Radius")
    UnifiedPaintPanel.prop_unified_size(row, context, brush, "use_pressure_size")

    row = layout.row(align=True)
    UnifiedPaintPanel.prop_unified_strength(row, context, brush, "strength", text="Strength")
    UnifiedPaintPanel.prop_unified_strength(row, context, brush, "use_pressure_strength")

    layout.separator()
    layout.prop(brush, "blend", text="" if compact else "Blend")


def brush_basic_vpaint_settings(layout, context, brush, *, compact=False):
    row = layout.row(align=True)
    UnifiedPaintPanel.prop_unified_size(row, context, brush, "size", slider=True, text="Radius")
    UnifiedPaintPanel.prop_unified_size(row, context, brush, "use_pressure_size")

    row = layout.row(align=True)
    UnifiedPaintPanel.prop_unified_strength(row, context, brush, "strength", text="Strength")
    UnifiedPaintPanel.prop_unified_strength(row, context, brush, "use_pressure_strength")

    layout.separator()
    layout.prop(brush, "blend", text="" if compact else "Blend")


def brush_basic_texpaint_settings(layout, context, brush, *, compact=False):
    capabilities = brush.image_paint_capabilities

    if capabilities.has_radius:
        row = layout.row(align=True)
        UnifiedPaintPanel.prop_unified_size(row, context, brush, "size", slider=True, text="Radius")
        UnifiedPaintPanel.prop_unified_size(row, context, brush, "use_pressure_size")

    row = layout.row(align=True)

    if capabilities.has_space_attenuation:
        row.prop(brush, "use_space_attenuation", toggle=True, icon_only=True)

    UnifiedPaintPanel.prop_unified_strength(row, context, brush, "strength", text="Strength")
    UnifiedPaintPanel.prop_unified_strength(row, context, brush, "use_pressure_strength")

    if brush.image_tool in {'DRAW', 'FILL'}:
        layout.separator()
        layout.prop(brush, "blend", text="" if compact else "Blend")


def brush_basic_sculpt_settings(layout, context, brush, *, compact=False):
    tool_settings = context.tool_settings
    capabilities = brush.sculpt_capabilities

    row = layout.row(align=True)
    UnifiedPaintPanel.prop_unified_size(row, context, brush, "use_locked_size")

    ups = tool_settings.unified_paint_settings
    if (
            (ups.use_unified_size and ups.use_locked_size) or
            ((not ups.use_unified_size) and brush.use_locked_size)
    ):
        UnifiedPaintPanel.prop_unified_size(row, context, brush, "unprojected_radius", slider=True, text="Radius")
    else:
        UnifiedPaintPanel.prop_unified_size(row, context, brush, "size", slider=True, text="Radius")

    UnifiedPaintPanel.prop_unified_size(row, context, brush, "use_pressure_size")

    # strength, use_strength_pressure, and use_strength_attenuation
    layout.separator()
    row = layout.row(align=True)

    if capabilities.has_space_attenuation:
        row.prop(brush, "use_space_attenuation", toggle=True, icon_only=True)

    UnifiedPaintPanel.prop_unified_strength(row, context, brush, "strength", text="Strength")

    if capabilities.has_strength_pressure:
        UnifiedPaintPanel.prop_unified_strength(row, context, brush, "use_pressure_strength")

    # direction
    layout.separator()
    layout.row().prop(brush, "direction", expand=True, **({"text": ""} if compact else {}))


def brush_basic_gpencil_paint_settings(layout, context, brush, *, compact=False):
    gp_settings = brush.gpencil_settings

    # Brush details
    if brush.gpencil_tool == 'ERASE':
        row = layout.row(align=True)
        row.prop(brush, "size", text="Radius")
        row.prop(gp_settings, "use_pressure", text="", icon='STYLUS_PRESSURE')
        row.prop(gp_settings, "use_occlude_eraser", text="", icon='XRAY')

        if gp_settings.eraser_mode == 'SOFT':
            row = layout.row(align=True)
            row.prop(gp_settings, "pen_strength", slider=True)
            row.prop(gp_settings, "use_strength_pressure", text="", icon='STYLUS_PRESSURE')
            row = layout.row(align=True)
            row.prop(gp_settings, "eraser_strength_factor")
            row = layout.row(align=True)
            row.prop(gp_settings, "eraser_thickness_factor")
    elif brush.gpencil_tool == 'FILL':
        col = layout.column(align=True)
        col.prop(gp_settings, "fill_leak", text="Leak Size")
        col.separator()
        col.prop(brush, "size", text="Thickness")
        col.prop(gp_settings, "fill_simplify_level", text="Simplify")

        row = layout.row(align=True)
        row.prop(gp_settings, "fill_draw_mode", text="Boundary Draw Mode")
        row.prop(gp_settings, "show_fill_boundary", text="", icon='GRID')

        col = layout.column(align=True)
        col.enabled = gp_settings.fill_draw_mode != 'STROKE'
        col.prop(gp_settings, "show_fill", text="Ignore Transparent Strokes")
        sub = col.row(align=True)
        sub.enabled = not gp_settings.show_fill
        sub.prop(gp_settings, "fill_threshold", text="Threshold")
    else:  # brush.gpencil_tool == 'DRAW':
        row = layout.row(align=True)
        row.prop(brush, "size", text="Radius")
        row.prop(gp_settings, "use_pressure", text="", icon='STYLUS_PRESSURE')
        row = layout.row(align=True)
        row.prop(gp_settings, "pen_strength", slider=True)
        row.prop(gp_settings, "use_strength_pressure", text="", icon='STYLUS_PRESSURE')


def brush_basic_gpencil_sculpt_settings(layout, context, brush, *, compact=False):
    tool_settings = context.tool_settings
    settings = tool_settings.gpencil_sculpt
    tool = settings.sculpt_tool

    row = layout.row(align=True)
    row.prop(brush, "size", slider=True)
    sub = row.row(align=True)
    sub.enabled = tool not in {'GRAB', 'CLONE'}
    sub.prop(brush, "use_pressure_radius", text="")

    row = layout.row(align=True)
    row.prop(brush, "strength", slider=True)
    row.prop(brush, "use_pressure_strength", text="")

    layout.prop(brush, "use_falloff")

    if compact:
        if tool in {'THICKNESS', 'STRENGTH', 'PINCH', 'TWIST'}:
            row.separator()
            row.prop(brush, "direction", expand=True, text="")
    else:
        use_property_split_prev = layout.use_property_split
        layout.use_property_split = False
        if tool in {'THICKNESS', 'STRENGTH'}:
            layout.row().prop(brush, "direction", expand=True)
        elif tool == 'PINCH':
            row = layout.row(align=True)
            row.prop_enum(brush, "direction", value='ADD', text="Pinch")
            row.prop_enum(brush, "direction", value='SUBTRACT', text="Inflate")
        elif tool == 'TWIST':
            row = layout.row(align=True)
            row.prop_enum(brush, "direction", value='ADD', text="CCW")
            row.prop_enum(brush, "direction", value='SUBTRACT', text="CW")
        layout.use_property_split = use_property_split_prev


def brush_basic_gpencil_weight_settings(layout, context, brush, *, compact=False):
    layout.prop(brush, "size", slider=True)

    row = layout.row(align=True)
    row.prop(brush, "strength", slider=True)
    row.prop(brush, "use_pressure_strength", text="")

    layout.prop(brush, "use_falloff")

    layout.prop(brush, "weight", slider=True)


classes = (
    VIEW3D_MT_tools_projectpaint_clone,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
