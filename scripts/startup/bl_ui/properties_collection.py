# SPDX-FileCopyrightText: 2017-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from bpy.types import (
    Collection,
    Menu,
    Panel,
)

from rna_prop_ui import PropertyPanel


class CollectionButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "collection"

    @classmethod
    def poll(cls, context):
        return context.collection != context.scene.collection


def lineart_make_line_type_entry(col, line_type, text_disp, expand, search_from):
    col.prop(line_type, "use", text=text_disp)
    if line_type.use and expand:
        col.prop_search(line_type, "layer", search_from,
                        "layers", icon='GREASEPENCIL')
        col.prop_search(line_type, "material", search_from,
                        "materials", icon='SHADING_TEXTURE')


class COLLECTION_PT_collection_flags(CollectionButtonsPanel, Panel):
    bl_label = "Restrictions"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        collection = context.collection
        vl = context.view_layer
        vlc = vl.active_layer_collection

        col = layout.column(align=True)
        col.prop(collection, "hide_select", text="Selectable", toggle=False, invert_checkbox=True)
        col.prop(collection, "hide_render", toggle=False)

        col = layout.column(align=True)
        col.prop(vlc, "holdout", toggle=False)
        col.prop(vlc, "indirect_only", toggle=False)


class COLLECTION_MT_context_menu_instance_offset(Menu):
    bl_label = "Instance Offset"

    def draw(self, _context):
        layout = self.layout
        layout.operator("object.instance_offset_from_cursor")
        layout.operator("object.instance_offset_from_object")
        layout.operator("object.instance_offset_to_cursor")


class COLLECTION_PT_instancing(CollectionButtonsPanel, Panel):
    bl_label = "Instancing"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False
        collection = context.collection

        row = layout.row(align=True)
        row.prop(collection, "instance_offset")
        row.menu("COLLECTION_MT_context_menu_instance_offset", icon='DOWNARROW_HLT', text="")


class COLLECTION_PT_lineart_collection(CollectionButtonsPanel, Panel):
    bl_label = "Line Art"
    bl_order = 10

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False
        collection = context.collection

        row = layout.row()
        row.prop(collection, "lineart_usage")

        layout.prop(collection, "lineart_use_intersection_mask", text="Collection Mask")

        col = layout.column(align=True)
        col.active = collection.lineart_use_intersection_mask
        row = col.row(align=True, heading="Masks")
        for i in range(8):
            row.prop(collection, "lineart_intersection_mask", index=i, text=" ", toggle=True)
            if i == 3:
                row = col.row(align=True)

        row = layout.row(heading="Intersection Priority")
        row.prop(collection, "use_lineart_intersection_priority", text="")
        subrow = row.row()
        subrow.active = collection.use_lineart_intersection_priority
        subrow.prop(collection, "lineart_intersection_priority", text="")


class COLLECTION_PT_collection_custom_props(CollectionButtonsPanel, PropertyPanel, Panel):
    _context_path = "collection"
    _property_type = Collection


classes = (
    COLLECTION_MT_context_menu_instance_offset,
    COLLECTION_PT_collection_flags,
    COLLECTION_PT_instancing,
    COLLECTION_PT_lineart_collection,
    COLLECTION_PT_collection_custom_props,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
