# SPDX-FileCopyrightText: 2022-2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.app.translations import (
    pgettext_iface as iface_,
    contexts as i18n_contexts,
)


def add_node_type(layout, node_type, *, label=None):
    """Add a node type to a menu."""
    bl_rna = bpy.types.Node.bl_rna_get_subclass(node_type)
    if not label:
        label = bl_rna.name if bl_rna else iface_("Unknown")
    translation_context = bl_rna.translation_context if bl_rna else i18n_contexts.default
    props = layout.operator("node.add_node", text=label, text_ctxt=translation_context)
    props.type = node_type
    props.use_transform = True
    return props


def draw_node_group_add_menu(context, layout):
    """Add items to the layout used for interacting with node groups."""
    space_node = context.space_data
    node_tree = space_node.edit_tree
    all_node_groups = context.blend_data.node_groups

    layout.operator("node.group_make")
    layout.operator("node.group_ungroup")
    if node_tree in all_node_groups.values():
        layout.separator()
        add_node_type(layout, "NodeGroupInput")
        add_node_type(layout, "NodeGroupOutput")

    if node_tree:
        from nodeitems_builtins import node_tree_group_type

        groups = [
            group for group in context.blend_data.node_groups
            if (group.bl_idname == node_tree.bl_idname and
                not group.contains_tree(node_tree) and
                not group.name.startswith('.'))
        ]
        if groups:
            layout.separator()
            for group in groups:
                props = add_node_type(layout, node_tree_group_type[group.bl_idname], label=group.name)
                ops = props.settings.add()
                ops.name = "node_tree"
                ops.value = "bpy.data.node_groups[%r]" % group.name


def draw_assets_for_catalog(layout, catalog_path):
    layout.template_node_asset_menu_items(catalog_path=catalog_path)


def draw_root_assets(layout):
    layout.menu_contents("NODE_MT_node_add_root_catalogs")


def add_simulation_zone(layout, label):
    """Add simulation zone to a menu."""
    target_bl_rna = bpy.types.Node.bl_rna_get_subclass("GeometryNodeSimulationOutput")
    if target_bl_rna:
        translation_context = target_bl_rna.translation_context
    else:
        translation_context = i18n_contexts.default
    props = layout.operator("node.add_simulation_zone", text=label, text_ctxt=translation_context)
    props.use_transform = True
    return props


def add_repeat_zone(layout, label):
    props = layout.operator("node.add_repeat_zone", text=label)
    props.use_transform = True
    return props


classes = (
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
