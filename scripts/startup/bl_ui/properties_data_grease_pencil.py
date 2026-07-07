# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later
import bpy
from bpy.types import Panel, Menu, UIList
from rna_prop_ui import PropertyPanel
from bl_ui.space_properties import PropertiesAnimationMixin


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


class GREASE_PENCIL_UL_masks(UIList):
    def draw_item(self, _context, layout, _data, item, icon, _active_data, _active_propname, _index):
        mask = item
        row = layout.row(align=True)
        row.prop(mask, "name", text="", emboss=False, icon_value=icon)
        row.prop(mask, "invert", text="", emboss=False)
        row.prop(mask, "hide", text="", emboss=False)


class GreasePencil_LayerMaskPanel:
    def draw_header(self, context):
        grease_pencil = context.grease_pencil
        layer = grease_pencil.layers.active

        self.layout.prop(layer, "use_masks", text="", toggle=0)

    def draw(self, context):
        layout = self.layout
        grease_pencil = context.grease_pencil
        layer = grease_pencil.layers.active

        layout = self.layout
        layout.enabled = layer.use_masks

        if not layer:
            return

        rows = 4
        row = layout.row()
        col = row.column()
        col.template_list(
            "GREASE_PENCIL_UL_masks", "", layer, "mask_layers", layer.mask_layers,
            "active_mask_index", rows=rows, sort_lock=True,
        )

        col = row.column(align=True)
        col.menu("GREASE_PENCIL_MT_layer_mask_add", icon='ADD', text="")
        col.operator("grease_pencil.layer_mask_remove", icon='REMOVE', text="")

        col.separator()

        sub = col.column(align=True)
        sub.operator("grease_pencil.layer_mask_reorder", icon='TRIA_UP', text="").direction = 'UP'
        sub.operator("grease_pencil.layer_mask_reorder", icon='TRIA_DOWN', text="").direction = 'DOWN'


class GreasePencil_LayerTransformPanel:
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


class GreasePencil_LayerAdjustmentsPanel:
    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        grease_pencil = context.grease_pencil
        layer = grease_pencil.layers.active
        layout.active = not layer.lock

        # Layer options
        col = layout.column(align=True)

        col.prop(layer, "tint_color")
        col.prop(layer, "tint_factor", text="Factor", slider=True)

        col = layout.row(align=True)
        col.prop(layer, "radius_offset", text="Stroke Thickness")


class GreasePencil_LayerRelationsPanel:
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

        col = layout.row(align=True)
        # Only enable this property when a view layer is selected.
        col.enabled = bool(layer.viewlayer_render)
        col.prop(layer, "use_viewlayer_masks")


class GreasePencil_LayerDisplayPanel:
    bl_label = "Display"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        grease_pencil = context.grease_pencil
        layer = grease_pencil.layers.active

        layout.prop(layer, "channel_color", text="Channel Color")


class GREASE_PENCIL_MT_layer_mask_add(Menu):
    bl_label = "Add Mask"

    def draw(self, context):
        layout = self.layout

        grease_pencil = context.grease_pencil
        active_layer = grease_pencil.layers.active
        found = False
        for layer in grease_pencil.layers:
            if layer == active_layer or layer.name in active_layer.mask_layers:
                continue

            found = True
            layout.operator("grease_pencil.layer_mask_add", text=layer.name).name = layer.name

        if not found:
            layout.label(text="No layers to add")


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
        grease_pencil = context.grease_pencil
        layer = grease_pencil.layers.active

        layout.separator()
        layout.operator("grease_pencil.layer_duplicate", text="Duplicate", icon='DUPLICATE').empty_keyframes = False
        layout.operator("grease_pencil.layer_duplicate", text="Duplicate Empty Keyframes").empty_keyframes = True

        layout.separator()
        layout.operator("grease_pencil.layer_reveal", icon='RESTRICT_VIEW_OFF', text="Show All")
        layout.operator("grease_pencil.layer_hide", icon='RESTRICT_VIEW_ON', text="Hide Others").unselected = True

        layout.separator()
        layout.operator("grease_pencil.layer_lock_all", icon='LOCKED', text="Lock All").lock = True
        layout.operator("grease_pencil.layer_lock_all", icon='UNLOCKED', text="Unlock All").lock = False

        layout.separator()
        layout.prop(grease_pencil, "use_autolock_layers", text="Autolock Inactive Layers")

        if layer:
            layout.prop(layer, "ignore_locked_materials")

        layout.separator()
        layout.operator("grease_pencil.layer_merge", text="Merge Down").mode = 'ACTIVE'
        layout.operator("grease_pencil.layer_merge", text="Merge Group").mode = 'GROUP'
        layout.operator("grease_pencil.layer_merge", text="Merge All").mode = 'ALL'

        layout.separator()
        layout.operator("grease_pencil.relative_layer_mask_add", text="Mask with Layer Above").mode = 'ABOVE'
        layout.operator("grease_pencil.relative_layer_mask_add", text="Mask with Layer Below").mode = 'BELOW'

        layout.separator()
        layout.operator("grease_pencil.layer_duplicate_object", text="Copy Layer to Selected").only_active = True
        layout.operator("grease_pencil.layer_duplicate_object", text="Copy All Layers to Selected").only_active = False


class GREASE_PENCIL_MT_group_context_menu(Menu):
    bl_label = "Layer Group"

    def draw(self, context):
        layout = self.layout
        layout.operator("grease_pencil.layer_group_remove", text="Delete Group").keep_children = False
        layout.operator("grease_pencil.layer_group_remove", text="Ungroup").keep_children = True
        layout.operator("grease_pencil.layer_merge", text="Merge Group").mode = 'GROUP'

        layout.separator()
        row = layout.row(align=True)
        row.operator_enum("grease_pencil.layer_group_color_tag", "color_tag", icon_only=True)


class DATA_PT_grease_pencil_layers(DataButtonsPanel, Panel):
    bl_label = "Layers"

    @classmethod
    def draw_settings(cls, layout, grease_pencil):
        layer = grease_pencil.layers.active
        is_layer_active = layer is not None
        is_group_active = grease_pencil.layer_groups.active is not None

        row = layout.row()
        row.template_grease_pencil_layer_tree()

        col = row.column()
        sub = col.column(align=True)
        sub.operator_context = 'EXEC_DEFAULT'
        sub.operator("grease_pencil.layer_add", icon='ADD', text="")
        sub.operator("grease_pencil.layer_group_add", icon='NEWFOLDER', text="")
        sub.separator()

        if is_layer_active:
            sub.operator("grease_pencil.layer_remove", icon='REMOVE', text="")
        if is_group_active:
            sub.operator("grease_pencil.layer_group_remove", icon='REMOVE', text="").keep_children = True

        sub.separator()

        sub.menu("GREASE_PENCIL_MT_grease_pencil_add_layer_extra", icon='DOWNARROW_HLT', text="")

        col.separator()

        sub = col.column(align=True)
        sub.operator("grease_pencil.layer_move", icon='TRIA_UP', text="").direction = 'UP'
        sub.operator("grease_pencil.layer_move", icon='TRIA_DOWN', text="").direction = 'DOWN'

        if not is_layer_active:
            return

        layout.use_property_split = True
        layout.use_property_decorate = True
        col = layout.column(align=True)

        # Layer main properties
        row = layout.row(align=True)
        row.prop(layer, "blend_mode", text="Blend Mode")

        row = layout.row(align=True)
        row.prop(layer, "opacity", text="Opacity", slider=True)

        row = layout.row(align=True)
        row.prop(layer, "use_lights", text="Lights")

    def draw(self, context):
        layout = self.layout
        grease_pencil = context.grease_pencil

        self.draw_settings(layout, grease_pencil)


class DATA_PT_grease_pencil_layer_masks(LayerDataButtonsPanel, GreasePencil_LayerMaskPanel, Panel):
    bl_label = "Masks"
    bl_parent_id = "DATA_PT_grease_pencil_layers"
    bl_options = {'DEFAULT_CLOSED'}


class DATA_PT_grease_pencil_layer_transform(LayerDataButtonsPanel, GreasePencil_LayerTransformPanel, Panel):
    bl_label = "Transform"
    bl_parent_id = "DATA_PT_grease_pencil_layers"
    bl_options = {'DEFAULT_CLOSED'}


class DATA_PT_grease_pencil_layer_adjustments(LayerDataButtonsPanel, GreasePencil_LayerAdjustmentsPanel, Panel):
    bl_label = "Adjustments"
    bl_parent_id = "DATA_PT_grease_pencil_layers"
    bl_options = {'DEFAULT_CLOSED'}


class DATA_PT_grease_pencil_layer_relations(LayerDataButtonsPanel, GreasePencil_LayerRelationsPanel, Panel):
    bl_label = "Relations"
    bl_parent_id = "DATA_PT_grease_pencil_layers"
    bl_options = {'DEFAULT_CLOSED'}


class DATA_PT_grease_pencil_layer_display(LayerDataButtonsPanel, GreasePencil_LayerDisplayPanel, Panel):
    bl_label = "Display"
    bl_parent_id = "DATA_PT_grease_pencil_layers"
    bl_options = {'DEFAULT_CLOSED'}


class DATA_PT_grease_pencil_layer_group_display(Panel):
    bl_label = "Display"
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        grease_pencil = context.grease_pencil
        return grease_pencil and grease_pencil.layer_groups.active

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        grease_pencil = context.grease_pencil
        group = grease_pencil.layer_groups.active

        layout.prop(group, "channel_color", text="Channel Color")


class DATA_PT_grease_pencil_onion_skinning(DataButtonsPanel, Panel):
    bl_label = "Onion Skinning"

    def draw(self, context):
        grease_pencil = context.grease_pencil

        layout = self.layout
        layout.use_property_split = True

        col = layout.column()
        col.prop(grease_pencil, "onion_mode")
        col.prop(grease_pencil, "onion_factor", text="Opacity", slider=True)
        col.prop(grease_pencil, "onion_keyframe_type")

        if grease_pencil.onion_mode == 'ABSOLUTE':
            col = layout.column(align=True)
            col.prop(grease_pencil, "ghost_before_range", text="Frames Before")
            col.prop(grease_pencil, "ghost_after_range", text="Frames After")
        elif grease_pencil.onion_mode == 'RELATIVE':
            col = layout.column(align=True)
            col.prop(grease_pencil, "ghost_before_range", text="Keyframes Before")
            col.prop(grease_pencil, "ghost_after_range", text="Keyframes After")


class DATA_PT_grease_pencil_onion_skinning_custom_colors(DataButtonsPanel, Panel):
    bl_parent_id = "DATA_PT_grease_pencil_onion_skinning"
    bl_label = "Custom Colors"
    bl_options = {'DEFAULT_CLOSED'}

    def draw_header(self, context):
        grease_pencil = context.grease_pencil
        self.layout.prop(grease_pencil, "use_ghost_custom_colors", text="")

    def draw(self, context):
        grease_pencil = context.grease_pencil

        layout = self.layout
        layout.use_property_split = True
        layout.enabled = grease_pencil.users <= 1 and grease_pencil.use_ghost_custom_colors

        layout.prop(grease_pencil, "before_color", text="Before")
        layout.prop(grease_pencil, "after_color", text="After")


class DATA_PT_grease_pencil_onion_skinning_display(DataButtonsPanel, Panel):
    bl_parent_id = "DATA_PT_grease_pencil_onion_skinning"
    bl_label = "Display"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        grease_pencil = context.grease_pencil

        layout = self.layout
        layout.use_property_split = True
        # This was done in GPv2 but it's not entirely clear why. Presumably it was
        # to indicate that the settings will affect the onion skinning of the
        # other users.
        layout.enabled = grease_pencil.users <= 1

        col = layout.column(align=True)
        col.prop(grease_pencil, "use_onion_fade", text="Fade")
        sub = layout.column()
        sub.active = grease_pencil.onion_mode in {'RELATIVE', 'SELECTED'}
        sub.prop(grease_pencil, "use_onion_loop", text="Show Start Frame")


class DATA_PT_grease_pencil_settings(DataButtonsPanel, Panel):
    bl_label = "Settings"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        grease_pencil = context.grease_pencil
        col = layout.column(align=True)
        col.prop(grease_pencil, "stroke_depth_order", text="Stroke Depth Order")


class DATA_PT_grease_pencil_animation(DataButtonsPanel, PropertiesAnimationMixin, PropertyPanel, Panel):
    _animated_id_context_property = "grease_pencil"


class DATA_PT_grease_pencil_custom_props(DataButtonsPanel, PropertyPanel, Panel):
    _context_path = "object.data"
    _property_type = bpy.types.GreasePencil


class GREASE_PENCIL_UL_attributes(UIList):
    def filter_items(self, _context, data, property):
        attributes = getattr(data, property)
        flags = []
        indices = [i for i in range(len(attributes))]

        # Filtering by name
        if self.filter_name:
            flags = bpy.types.UI_UL_list.filter_items_by_name(
                self.filter_name, self.bitflag_filter_item, attributes, "name", reverse=self.use_filter_invert,
            )
        if not flags:
            flags = [self.bitflag_filter_item] * len(attributes)

        # Filtering internal attributes
        for idx, item in enumerate(attributes):
            flags[idx] = 0 if item.is_internal else flags[idx]

        # Reorder by name.
        if self.use_filter_sort_alpha:
            indices = bpy.types.UI_UL_list.sort_items_by_name(attributes, "name")

        return flags, indices

    def draw_item(self, _context, layout, _data, attribute, _icon, _active_data, _active_propname, _index):
        data_type = attribute.bl_rna.properties["data_type"].enum_items[attribute.data_type]

        split = layout.split(factor=0.50)
        split.emboss = 'NONE'
        split.prop(attribute, "name", text="")
        sub = split.row()
        sub.alignment = 'RIGHT'
        sub.active = False
        sub.label(text=data_type.name)


class DATA_PT_grease_pencil_attributes(DataButtonsPanel, Panel):
    bl_label = "Attributes"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        grease_pencil = context.grease_pencil

        layout = self.layout
        row = layout.row()

        col = row.column()
        col.template_list(
            "GREASE_PENCIL_UL_attributes",
            "attributes",
            grease_pencil,
            "attributes",
            grease_pencil.attributes,
            "active_index",
            rows=3,
        )

        col = row.column(align=True)
        col.operator("geometry.attribute_add", icon='ADD', text="")
        col.operator("geometry.attribute_remove", icon='REMOVE', text="")


classes = (
    GREASE_PENCIL_UL_masks,
    GREASE_PENCIL_MT_layer_mask_add,
    DATA_PT_context_grease_pencil,
    DATA_PT_grease_pencil_layers,
    DATA_PT_grease_pencil_layer_masks,
    DATA_PT_grease_pencil_layer_transform,
    DATA_PT_grease_pencil_layer_adjustments,
    DATA_PT_grease_pencil_layer_relations,
    DATA_PT_grease_pencil_layer_display,
    DATA_PT_grease_pencil_layer_group_display,
    DATA_PT_grease_pencil_onion_skinning,
    DATA_PT_grease_pencil_onion_skinning_custom_colors,
    DATA_PT_grease_pencil_onion_skinning_display,
    DATA_PT_grease_pencil_settings,
    DATA_PT_grease_pencil_custom_props,
    GREASE_PENCIL_MT_grease_pencil_add_layer_extra,
    GREASE_PENCIL_MT_group_context_menu,
    DATA_PT_grease_pencil_animation,
    GREASE_PENCIL_UL_attributes,
    DATA_PT_grease_pencil_attributes,
)


if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
