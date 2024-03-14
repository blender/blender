# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later
import bpy
from bpy.types import Panel, Menu
from rna_prop_ui import PropertyPanel


class DataButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        return hasattr(context, "grease_pencil") and context.grease_pencil


class LayerDataButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        grease_pencil = context.grease_pencil
        return grease_pencil and grease_pencil.layers.active


class DATA_PT_context_grease_pencil(DataButtonsPanel, Panel):
    bl_label = ""
    bl_options = {'HIDE_HEADER'}

    def draw(self, context):
        layout = self.layout

        ob = context.object
        grease_pencil = context.grease_pencil
        space = context.space_data

        if ob:
            layout.template_ID(ob, "data")
        elif grease_pencil:
            layout.template_ID(space, "pin_id")


class GREASE_PENCIL_MT_grease_pencil_add_layer_extra(Menu):
    bl_label = "Add Extra"

    def draw(self, context):
        layout = self.layout
        grease_pencil = context.object.data
        space = context.space_data

        if space.type == 'PROPERTIES':
            layout.operator("grease_pencil.layer_group_add", text="Add Group")

        layout.separator()
        layout.operator("grease_pencil.layer_duplicate", text="Duplicate", icon='DUPLICATE')
        layout.operator("grease_pencil.layer_duplicate", text="Duplicate Empty Keyframes").empty_keyframes = True

        layout.separator()
        layout.operator("grease_pencil.layer_reveal", icon='RESTRICT_VIEW_OFF', text="Show All")
        layout.operator("grease_pencil.layer_hide", icon='RESTRICT_VIEW_ON', text="Hide Others").unselected = True

        layout.separator()
        layout.operator("grease_pencil.layer_lock_all", icon='LOCKED', text="Lock All")
        layout.operator("grease_pencil.layer_lock_all", icon='UNLOCKED', text="Unlock All").lock = False

        layout.separator()
        layout.prop(grease_pencil, "use_autolock_layers", text="Autolock Inactive Layers")


class DATA_PT_grease_pencil_layers(DataButtonsPanel, Panel):
    bl_label = "Layers"

    def draw(self, context):
        layout = self.layout

        grease_pencil = context.grease_pencil
        layer = grease_pencil.layers.active

        row = layout.row()
        row.template_grease_pencil_layer_tree()

        col = row.column()
        sub = col.column(align=True)
        sub.operator_context = 'EXEC_DEFAULT'
        sub.operator("grease_pencil.layer_add", icon='ADD', text="")
        sub.menu("GREASE_PENCIL_MT_grease_pencil_add_layer_extra", icon='DOWNARROW_HLT', text="")

        col.operator("grease_pencil.layer_remove", icon='REMOVE', text="")

        if not layer:
            return

        layout.use_property_split = True
        layout.use_property_decorate = True
        col = layout.column(align=True)

        # Layer main properties
        row = layout.row(align=True)
        row.prop(layer, "blend_mode", text="Blend Mode")

        row = layout.row(align=True)
        row.prop(layer, "opacity", text="Opacity", slider=True)


class DATA_PT_grease_pencil_layer_transform(LayerDataButtonsPanel, Panel):
    bl_label = "Transform"
    bl_parent_id = "DATA_PT_grease_pencil_layers"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        grease_pencil = context.grease_pencil
        layer = grease_pencil.layers.active
        layout.active = not layer.lock

        row = layout.row(align=True)
        row.prop(layer, "translation")

        row = layout.row(align=True)
        row.prop(layer, "rotation")

        row = layout.row(align=True)
        row.prop(layer, "scale")


class DATA_PT_grease_pencil_layer_relations(LayerDataButtonsPanel, Panel):
    bl_label = "Relations"
    bl_parent_id = "DATA_PT_grease_pencil_layers"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        grease_pencil = context.grease_pencil
        layer = grease_pencil.layers.active
        layout.active = not layer.lock

        row = layout.row(align=True)
        row.prop(layer, "parent", text="Parent")

        if layer.parent and layer.parent.type == 'ARMATURE':
            row = layout.row(align=True)
            row.prop_search(layer, "parent_bone", layer.parent.data, "bones", text="Bone")

        layout.separator()

        col = layout.row(align=True)
        col.prop(layer, "pass_index")

        col = layout.row(align=True)
        col.prop_search(layer, "viewlayer_render", context.scene, "view_layers", text="View Layer")


class DATA_PT_grease_pencil_custom_props(DataButtonsPanel, PropertyPanel, Panel):
    _context_path = "object.data"
    _property_type = bpy.types.GreasePencilv3


classes = (
    DATA_PT_context_grease_pencil,
    DATA_PT_grease_pencil_layers,
    DATA_PT_grease_pencil_layer_transform,
    DATA_PT_grease_pencil_layer_relations,
    DATA_PT_grease_pencil_custom_props,
    GREASE_PENCIL_MT_grease_pencil_add_layer_extra,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
