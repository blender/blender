# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from bpy.types import (
    Panel,
)
from bpy.app.translations import pgettext_iface as iface_


class PHYSICS_PT_geometry_nodes(Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "physics"
    bl_label = "Simulation Nodes"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def geometry_nodes_objects(cls, context):
        for ob in context.selected_editable_objects:
            if any([modifier.type == 'NODES' for modifier in ob.modifiers]):
                yield ob

    @classmethod
    def poll(cls, context):
        return any(cls.geometry_nodes_objects(context))

    def draw(self, context):
        layout = self.layout

        if len(context.selected_editable_objects) > 1:
            calc_text = iface_("Calculate Selected to Frame")
            bake_text = iface_("Bake Selected")
        else:
            calc_text = iface_("Calculate to Frame")
            bake_text = iface_("Bake")

        layout.operator("object.simulation_nodes_cache_calculate_to_frame", text=calc_text).selected = True

        row = layout.row(align=True)
        row.operator("object.simulation_nodes_cache_bake", text=bake_text).selected = True
        row.operator("object.simulation_nodes_cache_delete", text="", icon='TRASH').selected = True

        layout.use_property_split = True
        layout.use_property_decorate = False
        ob = context.object
        layout.prop(ob, "use_simulation_cache", text="Cache")


classes = (
    PHYSICS_PT_geometry_nodes,
)


if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
