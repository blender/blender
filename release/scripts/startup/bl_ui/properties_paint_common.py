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
        toolsettings = context.tool_settings

        if context.sculpt_object:
            return toolsettings.sculpt
        elif context.vertex_paint_object:
            return toolsettings.vertex_paint
        elif context.weight_paint_object:
            return toolsettings.weight_paint
        elif context.image_paint_object:
            if (toolsettings.image_paint and toolsettings.image_paint.detect_data()):
                return toolsettings.image_paint

            return None
        elif context.particle_edit_object:
            return toolsettings.particle_edit

        return None

    @staticmethod
    def unified_paint_settings(parent, context):
        ups = context.tool_settings.unified_paint_settings
        parent.label(text="Unified Settings:")
        row = parent.row()
        row.prop(ups, "use_unified_size", text="Size")
        row.prop(ups, "use_unified_strength", text="Strength")
        if context.weight_paint_object:
            parent.prop(ups, "use_unified_weight", text="Weight")
        elif context.vertex_paint_object or context.image_paint_object:
            parent.prop(ups, "use_unified_color", text="Color")
        else:
            parent.prop(ups, "use_unified_color", text="Color")

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

        for i, tex in enumerate(context.active_object.data.uv_textures):
            props = layout.operator("wm.context_set_int", text=tex.name, translate=False)
            props.data_path = "active_object.data.uv_texture_clone_index"
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
                col.label("Gradient Colors")
                col.template_color_ramp(brush, "gradient", expand=True)

                if brush.image_tool == 'DRAW':
                    col.label("Background Color")
                    row = col.row(align=True)
                    panel.prop_unified_color(row, context, brush, "secondary_color", text="")
                    col.prop(brush, "gradient_stroke_mode", text="Mode")
                    if brush.gradient_stroke_mode in {'SPACING_REPEAT', 'SPACING_CLAMP'}:
                        col.prop(brush, "grad_spacing")
                else: # if brush.image_tool == 'FILL':
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
                col.prop(settings, "use_clone_layer", text="Clone from paint slot")
            elif settings.mode == 'IMAGE':
                col.prop(settings, "use_clone_layer", text="Clone from image/UV map")

            if settings.use_clone_layer:
                ob = context.active_object
                col = layout.column()

                if settings.mode == 'MATERIAL':
                    if len(ob.material_slots) > 1:
                        col.label("Materials")
                        col.template_list("MATERIAL_UL_matslots", "",
                                          ob, "material_slots",
                                          ob, "active_material_index", rows=2)

                    mat = ob.active_material
                    if mat:
                        col.label("Source Clone Slot")
                        col.template_list("TEXTURE_UL_texpaintslots", "",
                                          mat, "texture_paint_images",
                                          mat, "paint_clone_slot", rows=2)

                elif settings.mode == 'IMAGE':
                    mesh = ob.data

                    clone_text = mesh.uv_texture_clone.name if mesh.uv_texture_clone else ""
                    col.label("Source Clone Image")
                    col.template_ID(settings, "clone_image")
                    col.label("Source Clone UV Map")
                    col.menu("VIEW3D_MT_tools_projectpaint_clone", text=clone_text, translate=False)
        else:
            col.prop(brush, "clone_image", text="Image")
            col.prop(brush, "clone_alpha", text="Alpha")

    col.separator()

    if capabilities.has_radius:
        row = col.row(align=True)
        panel.prop_unified_size(row, context, brush, "size", slider=True, text="Radius")
        panel.prop_unified_size(row, context, brush, "use_pressure_size")

    row = col.row(align=True)

    if capabilities.has_space_attenuation:
        row.prop(brush, "use_space_attenuation", toggle=True, icon_only=True)

    panel.prop_unified_strength(row, context, brush, "strength", text="Strength")
    panel.prop_unified_strength(row, context, brush, "use_pressure_strength")

    if brush.image_tool in {'DRAW', 'FILL'}:
        col.separator()
        col.prop(brush, "blend", text="Blend")

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

    layout.label(text="Brush Mapping:")

    # map_mode
    if sculpt:
        layout.row().prop(tex_slot, "map_mode", text="")
        layout.separator()
    else:
        layout.row().prop(tex_slot, "tex_paint_map_mode", text="")
        layout.separator()

    if tex_slot.map_mode == 'STENCIL':
        if brush.texture and brush.texture.type == 'IMAGE':
            layout.operator("brush.stencil_fit_image_aspect")
        layout.operator("brush.stencil_reset_transform")

    # angle and texture_angle_source
    if tex_slot.has_texture_angle:
        col = layout.column()
        col.label(text="Angle:")
        col.prop(tex_slot, "angle", text="")
        if tex_slot.has_texture_angle_source:
            col.prop(tex_slot, "use_rake", text="Rake")

            if brush.brush_capabilities.has_random_texture_angle and tex_slot.has_random_texture_angle:
                if sculpt:
                    if brush.sculpt_capabilities.has_random_texture_angle:
                        col.prop(tex_slot, "use_random", text="Random")
                        if tex_slot.use_random:
                            col.prop(tex_slot, "random_angle", text="")
                else:
                    col.prop(tex_slot, "use_random", text="Random")
                    if tex_slot.use_random:
                        col.prop(tex_slot, "random_angle", text="")

    # scale and offset
    split = layout.split()
    split.prop(tex_slot, "offset")
    split.prop(tex_slot, "scale")

    if sculpt:
        # texture_sample_bias
        col = layout.column(align=True)
        col.label(text="Sample Bias:")
        col.prop(brush, "texture_sample_bias", slider=True, text="")


def brush_mask_texture_settings(layout, brush):
    mask_tex_slot = brush.mask_texture_slot

    layout.label(text="Mask Mapping:")

    # map_mode
    layout.row().prop(mask_tex_slot, "mask_map_mode", text="")
    layout.separator()

    if mask_tex_slot.map_mode == 'STENCIL':
        if brush.mask_texture and brush.mask_texture.type == 'IMAGE':
            layout.operator("brush.stencil_fit_image_aspect").mask = True
        layout.operator("brush.stencil_reset_transform").mask = True

    col = layout.column()
    col.prop(brush, "use_pressure_masking", text="")
    # angle and texture_angle_source
    if mask_tex_slot.has_texture_angle:
        col = layout.column()
        col.label(text="Angle:")
        col.prop(mask_tex_slot, "angle", text="")
        if mask_tex_slot.has_texture_angle_source:
            col.prop(mask_tex_slot, "use_rake", text="Rake")

            if brush.brush_capabilities.has_random_texture_angle and mask_tex_slot.has_random_texture_angle:
                col.prop(mask_tex_slot, "use_random", text="Random")
                if mask_tex_slot.use_random:
                    col.prop(mask_tex_slot, "random_angle", text="")

    # scale and offset
    split = layout.split()
    split.prop(mask_tex_slot, "offset")
    split.prop(mask_tex_slot, "scale")


classes = (
    VIEW3D_MT_tools_projectpaint_clone,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
