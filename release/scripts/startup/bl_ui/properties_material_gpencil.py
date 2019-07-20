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
from bpy.types import Menu, Panel, UIList
from rna_prop_ui import PropertyPanel
from bl_ui.utils import PresetPanel

from bl_ui.properties_grease_pencil_common import (
    GreasePencilMaterialsPanel,
)


class GPENCIL_MT_color_context_menu(Menu):
    bl_label = "Layer"

    def draw(self, _context):
        layout = self.layout

        layout.operator("gpencil.color_reveal", icon='RESTRICT_VIEW_OFF', text="Show All")
        layout.operator("gpencil.color_hide", icon='RESTRICT_VIEW_ON', text="Hide Others").unselected = True

        layout.separator()

        layout.operator("gpencil.color_lock_all", icon='LOCKED', text="Lock All")
        layout.operator("gpencil.color_unlock_all", icon='UNLOCKED', text="UnLock All")

        layout.operator("gpencil.stroke_lock_color", text="Lock Unselected")
        layout.operator("gpencil.lock_layer", text="Lock Unused")


class GPENCIL_UL_matslots(UIList):
    def draw_item(self, _context, layout, _data, item, icon, _active_data, _active_propname, _index):
        slot = item
        ma = slot.material
        if (ma is not None) and (ma.grease_pencil is not None):
            gpcolor = ma.grease_pencil

            if self.layout_type in {'DEFAULT', 'COMPACT'}:
                if gpcolor.lock:
                    layout.active = False

                row = layout.row(align=True)
                row.enabled = not gpcolor.lock
                row.prop(ma, "name", text="", emboss=False, icon_value=icon)

                row = layout.row(align=True)
                row.prop(gpcolor, "lock", text="", emboss=False)
                row.prop(gpcolor, "hide", text="", emboss=False)
                if gpcolor.ghost is True:
                    icon = 'ONIONSKIN_OFF'
                else:
                    icon = 'ONIONSKIN_ON'
                row.prop(gpcolor, "ghost", text="", icon=icon, emboss=False)

            elif self.layout_type == 'GRID':
                layout.alignment = 'CENTER'
                layout.label(text="", icon_value=icon)


class GPMaterialButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "material"

    @classmethod
    def poll(cls, context):
        ma = context.material
        return ma and ma.grease_pencil


class MATERIAL_PT_gpencil_slots(GreasePencilMaterialsPanel, Panel):
    bl_label = "Grease Pencil Material Slots"
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "material"
    bl_options = {'HIDE_HEADER'}

    @classmethod
    def poll(cls, context):
        ob = context.object
        ma = context.material

        return (ma and ma.grease_pencil) or (ob and ob.type == 'GPENCIL')


# Used as parent for "Stroke" and "Fill" panels
class MATERIAL_PT_gpencil_surface(GPMaterialButtonsPanel, Panel):
    bl_label = "Surface"

    def draw_header_preset(self, _context):
        MATERIAL_PT_gpencil_material_presets.draw_panel_header(self.layout)

    def draw(self, _context):
        layout = self.layout
        layout.use_property_split = True


class MATERIAL_PT_gpencil_strokecolor(GPMaterialButtonsPanel, Panel):
    bl_label = "Stroke"
    bl_parent_id = 'MATERIAL_PT_gpencil_surface'

    def draw_header(self, context):
        ma = context.material
        if ma is not None and ma.grease_pencil is not None:
            gpcolor = ma.grease_pencil
            self.layout.enabled = not gpcolor.lock
            self.layout.prop(gpcolor, "show_stroke", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        ma = context.material
        if ma is not None and ma.grease_pencil is not None:
            gpcolor = ma.grease_pencil

            col = layout.column()
            col.enabled = not gpcolor.lock

            col.prop(gpcolor, "mode")

            col.prop(gpcolor, "stroke_style", text="Style")

            if gpcolor.stroke_style == 'TEXTURE':
                row = col.row()
                row.enabled = not gpcolor.lock
                col = row.column(align=True)
                col.template_ID(gpcolor, "stroke_image", open="image.open")
                if gpcolor.mode == 'LINE':
                    col.prop(gpcolor, "pixel_size", text="UV Factor")

                col.prop(gpcolor, "use_stroke_pattern", text="Use As Stencil Mask")
                if gpcolor.use_stroke_pattern is False:
                    col.prop(gpcolor, "use_stroke_texture_mix", text="Mix Color")
                    if gpcolor.use_stroke_texture_mix is True:
                        col.prop(gpcolor, "mix_stroke_factor", text="Factor")

            if (
                    gpcolor.stroke_style == 'SOLID' or
                    gpcolor.use_stroke_pattern or
                    gpcolor.use_stroke_texture_mix
            ):
                col.prop(gpcolor, "color", text="Color")

            if gpcolor.mode in {'DOTS', 'BOX'}:
                col.prop(gpcolor, "alignment_mode")


class MATERIAL_PT_gpencil_fillcolor(GPMaterialButtonsPanel, Panel):
    bl_label = "Fill"
    bl_parent_id = 'MATERIAL_PT_gpencil_surface'

    def draw_header(self, context):
        ma = context.material
        gpcolor = ma.grease_pencil
        self.layout.enabled = not gpcolor.lock
        self.layout.prop(gpcolor, "show_fill", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        ma = context.material
        gpcolor = ma.grease_pencil

        # color settings
        col = layout.column()
        col.enabled = not gpcolor.lock
        col.prop(gpcolor, "fill_style", text="Style")

        if gpcolor.fill_style == 'GRADIENT':
            col.prop(gpcolor, "gradient_type")

        if gpcolor.fill_style != 'TEXTURE':
            col.prop(gpcolor, "fill_color", text="Color")

            if gpcolor.fill_style in {'GRADIENT', 'CHECKER'}:
                col.prop(gpcolor, "mix_color", text="Secondary Color")

            if gpcolor.fill_style == 'GRADIENT':
                col.prop(gpcolor, "mix_factor", text="Mix Factor", slider=True)

            if gpcolor.fill_style in {'GRADIENT', 'CHECKER'}:
                col.prop(gpcolor, "flip", text="Flip Colors")

                col.prop(gpcolor, "pattern_shift", text="Location")
                col.prop(gpcolor, "pattern_scale", text="Scale")

            if gpcolor.gradient_type == 'RADIAL' and gpcolor.fill_style not in {'SOLID', 'CHECKER'}:
                col.prop(gpcolor, "pattern_radius", text="Radius")
            else:
                if gpcolor.fill_style != 'SOLID':
                    col.prop(gpcolor, "pattern_angle", text="Angle")

            if gpcolor.fill_style == 'CHECKER':
                col.prop(gpcolor, "pattern_gridsize", text="Box Size")

        # Texture
        if gpcolor.fill_style == 'TEXTURE' or (gpcolor.use_fill_texture_mix is True and gpcolor.fill_style == 'SOLID'):
            col.template_ID(gpcolor, "fill_image", open="image.open")

            if gpcolor.fill_style == 'TEXTURE':
                col.prop(gpcolor, "use_fill_pattern", text="Use As Stencil Mask")
                if gpcolor.use_fill_pattern is True:
                    col.prop(gpcolor, "fill_color", text="Color")

            col.prop(gpcolor, "texture_offset", text="Offset")
            col.prop(gpcolor, "texture_scale", text="Scale")
            col.prop(gpcolor, "texture_angle")
            col.prop(gpcolor, "texture_opacity")
            col.prop(gpcolor, "texture_clamp", text="Clip Image")

            if gpcolor.use_fill_pattern is False:
                col.prop(gpcolor, "use_fill_texture_mix", text="Mix With Color")

                if gpcolor.use_fill_texture_mix is True:
                    col.prop(gpcolor, "fill_color", text="Mix Color")
                    col.prop(gpcolor, "mix_factor", text="Mix Factor", slider=True)


class MATERIAL_PT_gpencil_preview(GPMaterialButtonsPanel, Panel):
    bl_label = "Preview"
    COMPAT_ENGINES = {'BLENDER_EEVEE'}
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        ma = context.material
        self.layout.label(text=ma.name)
        self.layout.template_preview(ma)


class MATERIAL_PT_gpencil_custom_props(GPMaterialButtonsPanel, PropertyPanel, Panel):
    COMPAT_ENGINES = {'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}
    _context_path = "object.active_material"
    _property_type = bpy.types.Material


class MATERIAL_PT_gpencil_options(GPMaterialButtonsPanel, Panel):
    bl_label = "Options"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        ma = context.material
        gpcolor = ma.grease_pencil
        layout.prop(gpcolor, "pass_index")


class MATERIAL_PT_gpencil_material_presets(PresetPanel, Panel):
    """Material settings"""
    bl_label = "Material Presets"
    preset_subdir = "gpencil_material"
    preset_operator = "script.execute_preset"
    preset_add_operator = "scene.gpencil_material_preset_add"


classes = (
    GPENCIL_UL_matslots,
    GPENCIL_MT_color_context_menu,
    MATERIAL_PT_gpencil_slots,
    MATERIAL_PT_gpencil_preview,
    MATERIAL_PT_gpencil_material_presets,
    MATERIAL_PT_gpencil_surface,
    MATERIAL_PT_gpencil_strokecolor,
    MATERIAL_PT_gpencil_fillcolor,
    MATERIAL_PT_gpencil_options,
    MATERIAL_PT_gpencil_custom_props,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
