# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Panel
from rna_prop_ui import PropertyPanel


class DataButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        return context.lattice


class DATA_PT_context_lattice(DataButtonsPanel, Panel):
    bl_label = ""
    bl_options = {'HIDE_HEADER'}

    def draw(self, context):
        layout = self.layout

        ob = context.object
        lat = context.lattice
        space = context.space_data

        if ob:
            layout.template_ID(ob, "data")
        elif lat:
            layout.template_ID(space, "pin_id")


class DATA_PT_lattice(DataButtonsPanel, Panel):
    bl_label = "Lattice"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        lat = context.lattice

        col = layout.column()

        sub = col.column(align=True)
        sub.prop(lat, "points_u", text="Resolution U")
        sub.prop(lat, "points_v", text="V")
        sub.prop(lat, "points_w", text="W")

        col.separator()

        sub = col.column(align=True)
        sub.prop(lat, "interpolation_type_u", text="Interpolation U")
        sub.prop(lat, "interpolation_type_v", text="V")
        sub.prop(lat, "interpolation_type_w", text="W")

        col.separator()

        col.prop(lat, "use_outside")

        col.separator()

        col.prop_search(lat, "vertex_group", context.object, "vertex_groups")


class DATA_PT_custom_props_lattice(DataButtonsPanel, PropertyPanel, Panel):
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_EEVEE_NEXT',
        'BLENDER_WORKBENCH',
        'BLENDER_WORKBENCH_NEXT'}
    _context_path = "object.data"
    _property_type = bpy.types.Lattice


classes = (
    DATA_PT_context_lattice,
    DATA_PT_lattice,
    DATA_PT_custom_props_lattice,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
