# SPDX-FileCopyrightText: 2022-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Menu
from bl_ui import node_add_menu
from bpy.app.translations import (
    pgettext_iface as iface_,
    contexts as i18n_contexts,
)


def add_node_type(layout, node_type, *, label=None, poll=None, search_weight=0.0, translate=True):
    """Add a node type to a menu."""
    bl_rna = bpy.types.Node.bl_rna_get_subclass(node_type)
    if not label:
        label = bl_rna.name if bl_rna else iface_("Unknown")

    if poll is True or poll is None:
        translation_context = bl_rna.translation_context if bl_rna else i18n_contexts.default
        props = layout.operator(
            "node.add_node",
            text=label,
            text_ctxt=translation_context,
            translate=translate,
            search_weight=search_weight)
        props.type = node_type
        props.use_transform = True
        return props

    return None


def add_node_type_with_outputs(context, layout, node_type, subnames, *, label=None, search_weight=0.0):
    bl_rna = bpy.types.Node.bl_rna_get_subclass(node_type)
    if not label:
        label = bl_rna.name if bl_rna else "Unknown"

    props = []
    props.append(add_node_type(layout, node_type, label=label, search_weight=search_weight))
    if getattr(context, "is_menu_search", False):
        for subname in subnames:
            sublabel = "{} ▸ {}".format(iface_(label), iface_(subname))
            item_props = add_node_type(layout, node_type, label=sublabel, search_weight=search_weight, translate=False)
            item_props.visible_output = subname
            props.append(item_props)
    return props


def draw_node_group_add_menu(context, layout):
    """Add items to the layout used for interacting with node groups."""
    space_node = context.space_data
    node_tree = space_node.edit_tree
    all_node_groups = context.blend_data.node_groups

    if node_tree in all_node_groups.values():
        layout.separator()
        add_node_type(layout, "NodeGroupInput")
        add_node_type(layout, "NodeGroupOutput")

    add_empty_group(layout)

    if node_tree:
        from nodeitems_builtins import node_tree_group_type

        prefs = bpy.context.preferences
        show_hidden = prefs.filepaths.show_hidden_files_datablocks

        groups = [
            group for group in context.blend_data.node_groups
            if (group.bl_idname == node_tree.bl_idname and
                not group.contains_tree(node_tree) and
                (show_hidden or not group.name.startswith('.')))
        ]
        if groups:
            layout.separator()
            for group in groups:
                props = add_node_type(layout, node_tree_group_type[group.bl_idname], label=group.name)
                ops = props.settings.add()
                ops.name = "node_tree"
                ops.value = "bpy.data.node_groups[{!r}]".format(group.name)
                ops = props.settings.add()
                ops.name = "width"
                ops.value = repr(group.default_group_node_width)
                ops = props.settings.add()
                ops.name = "name"
                ops.value = repr(group.name)


def draw_assets_for_catalog(layout, catalog_path):
    layout.template_node_asset_menu_items(catalog_path=catalog_path)


def draw_root_assets(layout):
    layout.menu_contents("NODE_MT_node_add_root_catalogs")


def add_node_type_with_searchable_enum(context, layout, node_idname, property_name, search_weight=0.0):
    add_node_type(layout, node_idname, search_weight=search_weight)
    if getattr(context, "is_menu_search", False):
        node_type = getattr(bpy.types, node_idname)
        translation_context = node_type.bl_rna.properties[property_name].translation_context
        for item in node_type.bl_rna.properties[property_name].enum_items_static:
            label = "{} ▸ {}".format(iface_(node_type.bl_rna.name), iface_(item.name, translation_context))
            props = add_node_type(
                layout,
                node_idname,
                label=label,
                translate=False,
                search_weight=search_weight)
            prop = props.settings.add()
            prop.name = property_name
            prop.value = repr(item.identifier)


def add_color_mix_node(context, layout):
    label = iface_("Mix Color")
    props = node_add_menu.add_node_type(layout, "ShaderNodeMix", label=label, translate=False)
    ops = props.settings.add()
    ops.name = "data_type"
    ops.value = "'RGBA'"

    if getattr(context, "is_menu_search", False):
        translation_context = bpy.types.ShaderNodeMix.bl_rna.properties["blend_type"].translation_context
        for item in bpy.types.ShaderNodeMix.bl_rna.properties["blend_type"].enum_items_static:
            sublabel = "{} ▸ {}".format(label, iface_(item.name, translation_context))
            props = node_add_menu.add_node_type(layout, "ShaderNodeMix", label=sublabel, translate=False)
            prop = props.settings.add()
            prop.name = "data_type"
            prop.value = "'RGBA'"
            prop = props.settings.add()
            prop.name = "blend_type"
            prop.value = repr(item.identifier)


def add_simulation_zone(layout, label):
    """Add simulation zone to a menu."""
    props = layout.operator("node.add_simulation_zone", text=label, text_ctxt=i18n_contexts.default)
    props.use_transform = True
    return props


def add_repeat_zone(layout, label):
    props = layout.operator("node.add_repeat_zone", text=label, text_ctxt=i18n_contexts.default)
    props.use_transform = True
    return props


def add_foreach_geometry_element_zone(layout, label):
    props = layout.operator(
        "node.add_foreach_geometry_element_zone",
        text=label,
        text_ctxt=i18n_contexts.default,
    )
    props.use_transform = True
    return props


def add_closure_zone(layout, label):
    props = layout.operator(
        "node.add_closure_zone", text=label, text_ctxt=i18n_contexts.default)
    props.use_transform = True
    return props


def add_empty_group(layout):
    props = layout.operator("node.add_empty_group", text="New Group", text_ctxt=i18n_contexts.default)
    props.use_transform = True
    return props


class NODE_MT_category_layout(Menu):
    bl_idname = "NODE_MT_category_layout"
    bl_label = "Layout"

    def draw(self, _context):
        layout = self.layout
        node_add_menu.add_node_type(layout, "NodeFrame", search_weight=-1)
        node_add_menu.add_node_type(layout, "NodeReroute")

        node_add_menu.draw_assets_for_catalog(layout, self.bl_label)


classes = (
    NODE_MT_category_layout,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
