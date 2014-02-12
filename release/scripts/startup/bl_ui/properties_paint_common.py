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


class UnifiedPaintPanel():
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
            return toolsettings.image_paint
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
    if brush.brush_capabilities.has_texture_angle:
        col = layout.column()
        col.label(text="Angle:")
        row = col.row(align=True)
        if brush.brush_capabilities.has_texture_angle_source:
            if brush.brush_capabilities.has_random_texture_angle:
                if sculpt:
                    if brush.sculpt_capabilities.has_random_texture_angle:
                        row.prop(brush, "texture_angle_source_random", text="")
                    else:
                        row.prop(brush, "texture_angle_source_no_random", text="")

                else:
                    row.prop(brush, "texture_angle_source_random", text="")
            else:
                row.prop(brush, "texture_angle_source_no_random", text="")

        row.prop(tex_slot, "angle", text="")

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
    col.label(text="Angle:")
    col.active = brush.brush_capabilities.has_texture_angle
    col.prop(mask_tex_slot, "angle", text="")

    # scale and offset
    split = layout.split()
    split.prop(mask_tex_slot, "offset")
    split.prop(mask_tex_slot, "scale")
