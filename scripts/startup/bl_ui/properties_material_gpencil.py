# SPDX-FileCopyrightText: 2018-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Menu, Panel, UIList
from rna_prop_ui import PropertyPanel
from bl_ui.utils import PresetPanel
from bl_ui.space_properties import PropertiesAnimationMixin

from bl_ui.properties_grease_pencil_common import (
    GreasePencilMaterialsPanel,
)


class GREASE_PENCIL_MT_material_context_menu(Menu):
    bl_label = "Material Specials"

    def draw(self, _context):
        layout = self.layout
        layout.operator("grease_pencil.material_reveal", icon='RESTRICT_VIEW_OFF', text="Show All")
        layout.operator("grease_pencil.material_hide", icon='RESTRICT_VIEW_ON', text="Hide Others").invert = True

        layout.separator()

        layout.operator("grease_pencil.material_lock_all", icon='LOCKED', text="Lock All")
        layout.operator("grease_pencil.material_unlock_all", icon='UNLOCKED', text="Unlock All")
        layout.operator("grease_pencil.material_lock_unselected", text="Lock Unselected")
        layout.operator("grease_pencil.material_lock_unused", text="Lock Unused")

        layout.separator()

        layout.operator(
            "grease_pencil.material_copy_to_object",
            text="Copy Material to Selected",
        ).only_active = True
        layout.operator(
            "grease_pencil.material_copy_to_object",
            text="Copy All Materials to Selected",
        ).only_active = False

        layout.operator("object.material_slot_remove_unused")
        layout.operator("object.material_slot_remove_all")


class GPENCIL_UL_matslots(UIList):
    def draw_item(self, _context, layout, _data, item, icon, _active_data, _active_propname, _index):
        slot = item
        ma = slot.material

        row = layout.row(align=True)
        row.label(text="", icon_value=icon)

        if ma is None:
            return

        if (gpcolor := ma.grease_pencil) is None:
            return

        row = layout.row(align=True)
        row.enabled = not gpcolor.lock
        row.prop(ma, "name", text="", emboss=False, icon='NONE')

        row = layout.row(align=True)

        if gpcolor.ghost is True:
            icon = 'ONIONSKIN_OFF'
        else:
            icon = 'ONIONSKIN_ON'
        row.prop(gpcolor, "ghost", text="", icon=icon, emboss=False)
        row.prop(gpcolor, "hide", text="", emboss=False)
        row.prop(gpcolor, "lock", text="", emboss=False)


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
        ma = context.material
        found_material = ma and ma.grease_pencil
        found_object = context.object and context.object.type == 'GREASEPENCIL'
        return found_material or found_object


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
    bl_parent_id = "MATERIAL_PT_gpencil_surface"

    def draw_header(self, context):
        ma = context.material
        if ma is not None and ma.grease_pencil is not None:
            gpcolor = ma.grease_pencil
            self.layout.prop(gpcolor, "show_stroke", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        ma = context.material
        if ma is not None and ma.grease_pencil is not None:
            gpcolor = ma.grease_pencil

            col = layout.column()

            col.prop(gpcolor, "mode")

            col.prop(gpcolor, "stroke_style", text="Style")

            col.prop(gpcolor, "color", text="Base Color")
            col.prop(gpcolor, "use_stroke_holdout")

            if gpcolor.stroke_style == 'TEXTURE':
                row = col.row()
                col = row.column(align=True)
                col.template_ID(gpcolor, "stroke_image", open="image.open")

            if gpcolor.stroke_style == 'TEXTURE':
                row = col.row()
                row.prop(gpcolor, "mix_stroke_factor", text="Blend", slider=True)
                if gpcolor.mode == 'LINE':
                    col.prop(gpcolor, "pixel_size", text="UV Factor")

            if gpcolor.mode in {'DOTS', 'BOX'}:
                col.prop(gpcolor, "alignment_mode")
                col.prop(gpcolor, "alignment_rotation")

            if gpcolor.mode == 'LINE':
                col.prop(gpcolor, "use_overlap_strokes")


class MATERIAL_PT_gpencil_fillcolor(GPMaterialButtonsPanel, Panel):
    bl_label = "Fill"
    bl_parent_id = "MATERIAL_PT_gpencil_surface"

    def draw_header(self, context):
        ma = context.material
        gpcolor = ma.grease_pencil
        self.layout.prop(gpcolor, "show_fill", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        ma = context.material
        gpcolor = ma.grease_pencil

        # color settings
        col = layout.column()
        col.prop(gpcolor, "fill_style", text="Style")

        if gpcolor.fill_style == 'SOLID':
            col.prop(gpcolor, "fill_color", text="Base Color")
            col.prop(gpcolor, "use_fill_holdout")

        elif gpcolor.fill_style == 'GRADIENT':
            col.prop(gpcolor, "gradient_type")

            col.prop(gpcolor, "fill_color", text="Base Color")
            col.prop(gpcolor, "mix_color", text="Secondary Color")
            col.prop(gpcolor, "use_fill_holdout")
            col.prop(gpcolor, "mix_factor", text="Blend", slider=True)
            col.prop(gpcolor, "flip", text="Flip Colors")

            col.prop(gpcolor, "texture_offset", text="Location")

            row = col.row()
            row.enabled = gpcolor.gradient_type == 'LINEAR'
            row.prop(gpcolor, "texture_angle", text="Rotation")

            col.prop(gpcolor, "texture_scale", text="Scale")

        elif gpcolor.fill_style == 'TEXTURE':
            col.prop(gpcolor, "fill_color", text="Base Color")
            col.prop(gpcolor, "use_fill_holdout")

            col.template_ID(gpcolor, "fill_image", open="image.open")

            col.prop(gpcolor, "mix_factor", text="Blend", slider=True)

            col.prop(gpcolor, "texture_offset", text="Location")
            col.prop(gpcolor, "texture_angle", text="Rotation")
            col.prop(gpcolor, "texture_scale", text="Scale")
            col.prop(gpcolor, "texture_clamp", text="Clip Image")


class MATERIAL_PT_gpencil_animation(GPMaterialButtonsPanel, PropertiesAnimationMixin, PropertyPanel, Panel):
    _animated_id_context_property = "material"


class MATERIAL_PT_gpencil_preview(GPMaterialButtonsPanel, Panel):
    bl_label = "Preview"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        ma = context.material
        self.layout.template_preview(ma)


class MATERIAL_PT_gpencil_custom_props(GPMaterialButtonsPanel, PropertyPanel, Panel):
    COMPAT_ENGINES = {'BLENDER_WORKBENCH'}
    _context_path = "object.active_material"
    _property_type = bpy.types.Material


class MATERIAL_PT_gpencil_settings(GPMaterialButtonsPanel, Panel):
    bl_label = "Settings"
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
    GREASE_PENCIL_MT_material_context_menu,
    MATERIAL_PT_gpencil_slots,
    MATERIAL_PT_gpencil_preview,
    MATERIAL_PT_gpencil_material_presets,
    MATERIAL_PT_gpencil_surface,
    MATERIAL_PT_gpencil_strokecolor,
    MATERIAL_PT_gpencil_fillcolor,
    MATERIAL_PT_gpencil_settings,
    MATERIAL_PT_gpencil_animation,
    MATERIAL_PT_gpencil_custom_props,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class

    for cls in classes:
        register_class(cls)
