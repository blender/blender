# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Panel
from rna_prop_ui import PropertyPanel
from .space_properties import PropertiesAnimationMixin


class DataButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        return context.meta_ball


class DATA_PT_context_metaball(DataButtonsPanel, Panel):
    bl_label = ""
    bl_options = {'HIDE_HEADER'}

    def draw(self, context):
        layout = self.layout

        ob = context.object
        mball = context.meta_ball
        space = context.space_data

        if ob:
            layout.template_ID(ob, "data")
        elif mball:
            layout.template_ID(space, "pin_id")


class DATA_PT_metaball(DataButtonsPanel, Panel):
    bl_label = "Metaball"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        mball = context.meta_ball

        col = layout.column(align=True)
        col.prop(mball, "resolution", text="Resolution Viewport")
        col.prop(mball, "render_resolution", text="Render")

        col.separator()

        col.prop(mball, "threshold", text="Influence Threshold")

        col.separator()

        col.prop(mball, "update_method", text="Update on Edit")


class DATA_PT_mball_texture_space(DataButtonsPanel, Panel):
    bl_label = "Texture Space"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        mball = context.meta_ball

        layout.prop(mball, "use_auto_texspace")

        col = layout.column()
        col.prop(mball, "texspace_location")
        col.prop(mball, "texspace_size")


class DATA_PT_metaball_element(DataButtonsPanel, Panel):
    bl_label = "Active Element"

    @classmethod
    def poll(cls, context):
        return (context.meta_ball and context.meta_ball.elements.active)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        metaelem = context.meta_ball.elements.active

        col = layout.column()

        col.prop(metaelem, "type")

        col.separator()

        col.prop(metaelem, "stiffness", text="Stiffness")
        col.prop(metaelem, "radius", text="Radius")
        col.prop(metaelem, "use_negative", text="Negative")
        col.prop(metaelem, "hide", text="Hide")

        sub = col.column(align=True)

        if metaelem.type in {'CUBE', 'ELLIPSOID'}:
            sub.prop(metaelem, "size_x", text="Size X")
            sub.prop(metaelem, "size_y", text="Y")
            sub.prop(metaelem, "size_z", text="Z")

        elif metaelem.type == 'CAPSULE':
            sub.prop(metaelem, "size_x", text="Size X")

        elif metaelem.type == 'PLANE':
            sub.prop(metaelem, "size_x", text="Size X")
            sub.prop(metaelem, "size_y", text="Y")


class DATA_PT_metaball_animation(DataButtonsPanel, PropertiesAnimationMixin, PropertyPanel, Panel):
    _animated_id_context_property = "meta_ball"


class DATA_PT_custom_props_metaball(DataButtonsPanel, PropertyPanel, Panel):
    _context_path = "object.data"
    _property_type = bpy.types.MetaBall


classes = (
    DATA_PT_context_metaball,
    DATA_PT_metaball,
    DATA_PT_mball_texture_space,
    DATA_PT_metaball_element,
    DATA_PT_metaball_animation,
    DATA_PT_custom_props_metaball,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
