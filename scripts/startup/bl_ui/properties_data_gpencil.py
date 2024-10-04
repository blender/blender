# SPDX-FileCopyrightText: 2018-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Menu, Panel, UIList
from rna_prop_ui import PropertyPanel
from .space_properties import PropertiesAnimationMixin

###############################
# Base-Classes (for shared stuff - e.g. poll, attributes, etc.)


class DataButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        return context.gpencil


class ObjectButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        ob = context.object
        return ob and ob.type == 'GPENCIL'


class LayerDataButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        gpencil = context.gpencil
        return gpencil and gpencil.layers.active


###############################
# GP Object Properties Panels and Helper Classes

class DATA_PT_context_gpencil(DataButtonsPanel, Panel):
    bl_label = ""
    bl_options = {'HIDE_HEADER'}

    def draw(self, context):
        layout = self.layout

        ob = context.object
        space = context.space_data

        if ob:
            layout.template_ID(ob, "data")
        else:
            layout.template_ID(space, "pin_id")


class DATA_PT_gpencil_onion_skinning(DataButtonsPanel, Panel):
    bl_label = "Onion Skinning"

    def draw(self, context):
        gpd = context.gpencil

        layout = self.layout
        layout.use_property_split = True

        col = layout.column()
        col.prop(gpd, "onion_mode")
        col.prop(gpd, "onion_factor", text="Opacity", slider=True)
        col.prop(gpd, "onion_keyframe_type")

        if gpd.onion_mode == 'ABSOLUTE':
            col = layout.column(align=True)
            col.prop(gpd, "ghost_before_range", text="Frames Before")
            col.prop(gpd, "ghost_after_range", text="Frames After")
        elif gpd.onion_mode == 'RELATIVE':
            col = layout.column(align=True)
            col.prop(gpd, "ghost_before_range", text="Keyframes Before")
            col.prop(gpd, "ghost_after_range", text="Keyframes After")


class DATA_PT_gpencil_onion_skinning_custom_colors(DataButtonsPanel, Panel):
    bl_parent_id = "DATA_PT_gpencil_onion_skinning"
    bl_label = "Custom Colors"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_header(self, context):
        gpd = context.gpencil

        self.layout.prop(gpd, "use_ghost_custom_colors", text="")

    def draw(self, context):
        gpd = context.gpencil

        layout = self.layout
        layout.use_property_split = True
        layout.enabled = gpd.users <= 1 and gpd.use_ghost_custom_colors

        layout.prop(gpd, "before_color", text="Before")
        layout.prop(gpd, "after_color", text="After")


class DATA_PT_gpencil_onion_skinning_display(DataButtonsPanel, Panel):
    bl_parent_id = "DATA_PT_gpencil_onion_skinning"
    bl_label = "Display"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        gpd = context.gpencil

        layout = self.layout
        layout.use_property_split = True
        layout.enabled = gpd.users <= 1

        layout.prop(gpd, "use_ghosts_always", text="View in Render")

        col = layout.column(align=True)
        col.prop(gpd, "use_onion_fade", text="Fade")
        sub = layout.column()
        sub.active = gpd.onion_mode in {'RELATIVE', 'SELECTED'}
        sub.prop(gpd, "use_onion_loop", text="Show Start Frame")


class GPENCIL_UL_vgroups(UIList):
    def draw_item(self, _context, layout, _data, item, icon, _active_data, _active_propname, _index):
        vgroup = item
        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            layout.prop(vgroup, "name", text="", emboss=False, icon_value=icon)
            icon = 'LOCKED' if vgroup.lock_weight else 'UNLOCKED'
            layout.prop(vgroup, "lock_weight", text="", icon=icon, emboss=False)
        elif self.layout_type == 'GRID':
            layout.alignment = 'CENTER'
            layout.label(text="", icon_value=icon)


class DATA_PT_gpencil_strokes(DataButtonsPanel, Panel):
    bl_label = "Strokes"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        ob = context.object
        gpd = context.gpencil

        col = layout.column(align=True)
        col.prop(gpd, "stroke_depth_order")

        if ob:
            col.enabled = not ob.show_in_front

        col = layout.column(align=True)
        col.prop(gpd, "stroke_thickness_space")
        sub = col.column()
        sub.active = gpd.stroke_thickness_space == 'WORLDSPACE'
        sub.prop(gpd, "pixel_factor", text="Thickness Scale")

        col.prop(gpd, "edit_curve_resolution")


class DATA_PT_gpencil_display(DataButtonsPanel, Panel):
    bl_label = "Viewport Display"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        gpd = context.gpencil

        layout.prop(gpd, "edit_line_color", text="Edit Line Color")


class DATA_PT_gpencil_canvas(DataButtonsPanel, Panel):
    bl_label = "Canvas"
    bl_parent_id = "DATA_PT_gpencil_display"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False
        gpd = context.gpencil
        grid = gpd.grid

        row = layout.row(align=True)
        col = row.column()
        col.prop(grid, "color", text="Color")
        col.prop(grid, "scale", text="Scale")
        col.prop(grid, "offset")
        row = layout.row(align=True)
        col = row.column()
        col.prop(grid, "lines", text="Subdivisions")


class DATA_PT_gpencil_animation(DataButtonsPanel, PropertiesAnimationMixin, PropertyPanel, Panel):
    _animated_id_context_property = "gpencil"


class DATA_PT_custom_props_gpencil(DataButtonsPanel, PropertyPanel, Panel):
    _context_path = "object.data"
    _property_type = bpy.types.GreasePencil


###############################


classes = (
    DATA_PT_context_gpencil,
    DATA_PT_gpencil_onion_skinning,
    DATA_PT_gpencil_onion_skinning_custom_colors,
    DATA_PT_gpencil_onion_skinning_display,
    DATA_PT_gpencil_strokes,
    DATA_PT_gpencil_display,
    DATA_PT_gpencil_canvas,
    DATA_PT_gpencil_animation,
    DATA_PT_custom_props_gpencil,

    GPENCIL_UL_vgroups,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class

    for cls in classes:
        register_class(cls)
