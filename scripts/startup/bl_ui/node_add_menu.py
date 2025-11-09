# SPDX-FileCopyrightText: 2022-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

__all__ = (
    "add_closure_zone",
    "add_color_mix_node",
    "add_foreach_geometry_element_zone",
    "add_node_type",
    "add_node_type_with_outputs",
    "add_node_type_with_searchable_enum",
    "add_node_type_with_searchable_enum_socket",
    "add_repeat_zone",
    "add_simulation_zone",
    "draw_node_group_add_menu",
)

import bpy
from bpy.types import Menu
from bpy.app.translations import (
    pgettext_iface as iface_,
    contexts as i18n_contexts,
)


# NOTE: This is kept for compatibility's sake, as some scripts import node_add_menu.add_node_type.
def add_node_type(layout, node_type, *, label=None, poll=None, search_weight=0.0, translate=True):
    """Add a node type to a menu."""
    return AddNodeMenu.node_operator(
        layout,
        node_type,
        label=label,
        poll=poll,
        search_weight=search_weight,
        translate=translate,
    )


def add_node_type_with_searchable_enum(context, layout, node_idname, property_name, search_weight=0.0):
    return AddNodeMenu.node_operator_with_searchable_enum(context, layout, node_idname, property_name, search_weight)


def add_node_type_with_searchable_enum_socket(
        context,
        layout,
        node_idname,
        socket_identifier,
        enum_names,
        search_weight=0.0,
):
    return AddNodeMenu.node_operator_with_searchable_enum_socket(
        context, layout, node_idname, socket_identifier, enum_names, search_weight,
    )


def add_node_type_with_outputs(context, layout, node_type, subnames, *, label=None, search_weight=0.0):
    return AddNodeMenu.node_operator_with_outputs(
        context,
        layout,
        node_type,
        subnames,
        label=label,
        search_weight=search_weight,
    )


def add_color_mix_node(context, layout):
    return AddNodeMenu.color_mix_node(context, layout)


def add_empty_group(layout):
    return AddNodeMenu.new_empty_group(layout)


def draw_node_group_add_menu(context, layout):
    """Add items to the layout used for interacting with node groups."""
    return AddNodeMenu.draw_group_menu(context, layout)


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
        "node.add_closure_zone",
        text=label,
        text_ctxt=i18n_contexts.default,
    )
    props.use_transform = True
    return props


class NodeMenu(Menu):
    """A baseclass defining the shared methods for AddNodeMenu and SwapNodeMenu."""
    draw_assets: bool
    use_transform: bool

    main_operator_id: str
    zone_operator_id: str
    new_empty_group_operator_id: str

    root_asset_menu: str
    pathing_dict: dict[str, str]

    @classmethod
    def poll(cls, context):
        return context.space_data.type == 'NODE_EDITOR'

    @classmethod
    def node_operator(cls, layout, node_type, *, label=None, poll=None, search_weight=0.0, translate=True):
        """The main operator defined for the node menu.
        \n(e.g. 'Add Node' for AddNodeMenu, or 'Swap Node' for SwapNodeMenu)."""

        bl_rna = bpy.types.Node.bl_rna_get_subclass(node_type)
        if not label:
            label = bl_rna.name if bl_rna else iface_("Unknown")

        if poll is True or poll is None:
            translation_context = bl_rna.translation_context if bl_rna else i18n_contexts.default
            props = layout.operator(
                cls.main_operator_id,
                text=label,
                text_ctxt=translation_context,
                translate=translate,
                search_weight=search_weight,
            )
            props.type = node_type

            if hasattr(props, "use_transform"):
                props.use_transform = cls.use_transform

            return props

        return None

    @classmethod
    def node_operator_with_searchable_enum(cls, context, layout, node_idname, property_name, search_weight=0.0):
        """Similar to `node_operator`, but with extra entries based on a enum property while in search."""
        operators = []
        operators.append(cls.node_operator(layout, node_idname, search_weight=search_weight))

        if getattr(context, "is_menu_search", False):
            node_type = getattr(bpy.types, node_idname)
            translation_context = node_type.bl_rna.properties[property_name].translation_context
            for item in node_type.bl_rna.properties[property_name].enum_items_static:
                props = cls.node_operator(
                    layout,
                    node_idname,
                    label="{:s} \u25B8 {:s}".format(
                        iface_(node_type.bl_rna.name),
                        iface_(item.name, translation_context),
                    ),
                    translate=False,
                    search_weight=search_weight,
                )
                prop = props.settings.add()
                prop.name = property_name
                prop.value = repr(item.identifier)
                operators.append(props)

        for props in operators:
            if hasattr(props, "use_transform"):
                props.use_transform = cls.use_transform

        return operators

    @classmethod
    def node_operator_with_searchable_enum_socket(
            cls,
            context,
            layout,
            node_idname,
            socket_identifier,
            enum_names,
            search_weight=0.0,
    ):
        """Similar to `node_operator`, but with extra entries based on a enum socket while in search."""
        operators = []
        operators.append(cls.node_operator(layout, node_idname, search_weight=search_weight))
        if getattr(context, "is_menu_search", False):
            node_type = getattr(bpy.types, node_idname)
            for enum_name in enum_names:
                props = cls.node_operator(
                    layout,
                    node_idname,
                    label="{:s} \u25B8 {:s}".format(iface_(node_type.bl_rna.name), iface_(enum_name)),
                    translate=False,
                    search_weight=search_weight,
                )
                prop = props.settings.add()
                prop.name = "inputs[\"{:s}\"].default_value".format(bpy.utils.escape_identifier(socket_identifier))
                prop.value = repr(enum_name)
                operators.append(props)

        for props in operators:
            if hasattr(props, "use_transform"):
                props.use_transform = cls.use_transform

        return operators

    @classmethod
    def node_operator_with_outputs(cls, context, layout, node_type, subnames, *, label=None, search_weight=0.0):
        """Similar to `node_operator`, but with extra entries based on a enum socket while in search."""
        bl_rna = bpy.types.Node.bl_rna_get_subclass(node_type)
        if not label:
            label = bl_rna.name if bl_rna else "Unknown"

        operators = []
        operators.append(cls.node_operator(layout, node_type, label=label, search_weight=search_weight))

        if getattr(context, "is_menu_search", False):
            for subname in subnames:
                item_props = cls.node_operator(layout, node_type, label="{:s} \u25B8 {:s}".format(
                    iface_(label), iface_(subname)), search_weight=search_weight, translate=False)
                item_props.visible_output = subname
                operators.append(item_props)

        for props in operators:
            if hasattr(props, "use_transform"):
                props.use_transform = cls.use_transform

        return operators

    @classmethod
    def color_mix_node(cls, context, layout):
        """The 'Mix Color' node, with its different blend modes available while in search."""
        label = iface_("Mix Color")

        operators = []
        props = cls.node_operator(layout, "ShaderNodeMix", label=label, translate=False)
        ops = props.settings.add()
        ops.name = "data_type"
        ops.value = "'RGBA'"
        operators.append(props)

        if getattr(context, "is_menu_search", False):
            translation_context = bpy.types.ShaderNodeMix.bl_rna.properties["blend_type"].translation_context
            for item in bpy.types.ShaderNodeMix.bl_rna.properties["blend_type"].enum_items_static:
                props = cls.node_operator(
                    layout,
                    "ShaderNodeMix",
                    label="{:s} \u25B8 {:s}".format(
                        label,
                        iface_(item.name, translation_context),
                    ),
                    translate=False,
                )
                prop = props.settings.add()
                prop.name = "data_type"
                prop.value = "'RGBA'"
                prop = props.settings.add()
                prop.name = "blend_type"
                prop.value = repr(item.identifier)
                operators.append(props)

        for props in operators:
            if hasattr(props, "use_transform"):
                props.use_transform = cls.use_transform

        return operators

    @classmethod
    def new_empty_group(cls, layout):
        """Group Node with a newly created empty group as its assigned nodetree."""
        props = layout.operator(
            cls.new_empty_group_operator_id,
            text="New Group",
            text_ctxt=i18n_contexts.default,
            icon='ADD',
        )
        if hasattr(props, "use_transform"):
            props.use_transform = cls.use_transform

        return props

    @classmethod
    def draw_group_menu(cls, context, layout):
        """Show operators used for interacting with node groups."""
        space_node = context.space_data
        node_tree = space_node.edit_tree
        all_node_groups = context.blend_data.node_groups

        operators = []
        operators.append(cls.new_empty_group(layout))

        if node_tree in all_node_groups.values():
            layout.separator()
            cls.node_operator(layout, "NodeGroupInput")
            cls.node_operator(layout, "NodeGroupOutput")

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
                    search_weight = -1.0 if group.is_linked_packed else 0.0
                    props = cls.node_operator(
                        layout,
                        node_tree_group_type[group.bl_idname],
                        label=group.name,
                        search_weight=search_weight,
                    )
                    ops = props.settings.add()
                    ops.name = "node_tree"
                    ops.value = "bpy.data.node_groups[{!r}]".format(group.name)
                    ops = props.settings.add()
                    ops.name = "width"
                    ops.value = repr(group.default_group_node_width)
                    ops = props.settings.add()
                    ops.name = "name"
                    ops.value = repr(group.name)
                    operators.append(props)

        for props in operators:
            if hasattr(props, "use_transform"):
                props.use_transform = cls.use_transform

        return operators

    @classmethod
    def draw_menu(cls, layout, path):
        """
        Takes the given menu path and draws the corresponding menu.
        Menu paths are either explicitly defined, or based on bl_label if not.
        """
        if cls.pathing_dict is None:
            raise ValueError("`pathing_dict` was not set for {!s}".format(cls))

        layout.menu(cls.pathing_dict[path])

    @classmethod
    def simulation_zone(cls, layout, label):
        props = layout.operator(cls.zone_operator_id, text=iface_(label), translate=False)
        props.input_node_type = "GeometryNodeSimulationInput"
        props.output_node_type = "GeometryNodeSimulationOutput"
        props.add_default_geometry_link = True

        if hasattr(props, "use_transform"):
            props.use_transform = cls.use_transform

        return props

    @classmethod
    def repeat_zone(cls, layout, label):
        props = layout.operator(cls.zone_operator_id, text=iface_(label), translate=False)
        props.input_node_type = "GeometryNodeRepeatInput"
        props.output_node_type = "GeometryNodeRepeatOutput"
        props.add_default_geometry_link = True

        if hasattr(props, "use_transform"):
            props.use_transform = cls.use_transform

        return props

    @classmethod
    def for_each_element_zone(cls, layout, label):
        props = layout.operator(cls.zone_operator_id, text=iface_(label), translate=False)
        props.input_node_type = "GeometryNodeForeachGeometryElementInput"
        props.output_node_type = "GeometryNodeForeachGeometryElementOutput"
        props.add_default_geometry_link = False

        if hasattr(props, "use_transform"):
            props.use_transform = cls.use_transform

        return props

    @classmethod
    def closure_zone(cls, layout, label):
        props = layout.operator(cls.zone_operator_id, text=iface_(label), translate=False)
        props.input_node_type = "NodeClosureInput"
        props.output_node_type = "NodeClosureOutput"
        props.add_default_geometry_link = False

        if hasattr(props, "use_transform"):
            props.use_transform = cls.use_transform

        return props

    @classmethod
    def draw_root_assets(cls, layout):
        if cls.draw_assets:
            layout.menu_contents(cls.root_asset_menu)


class AddNodeMenu(NodeMenu):
    draw_assets = True
    use_transform = True

    main_operator_id = "node.add_node"
    zone_operator_id = "node.add_zone"
    new_empty_group_operator_id = "node.add_empty_group"

    root_asset_menu = "NODE_MT_node_add_root_catalogs"

    @classmethod
    def draw_assets_for_catalog(cls, layout, catalog_path):
        if cls.draw_assets:
            layout.template_node_asset_menu_items(catalog_path=catalog_path, operator='ADD')


class SwapNodeMenu(NodeMenu):
    draw_assets = True
    # NOTE: Swap operators don't have a `use_transform` property, so defining it here has no effect.

    main_operator_id = "node.swap_node"
    zone_operator_id = "node.swap_zone"
    new_empty_group_operator_id = "node.swap_empty_group"

    root_asset_menu = "NODE_MT_node_swap_root_catalogs"

    @classmethod
    def draw_assets_for_catalog(cls, layout, catalog_path):
        if cls.draw_assets:
            layout.template_node_asset_menu_items(catalog_path=catalog_path, operator='SWAP')


class NODE_MT_group_base(NodeMenu):
    bl_label = "Group"

    def draw(self, context):
        layout = self.layout
        self.draw_group_menu(context, layout)

        self.draw_assets_for_catalog(layout, self.bl_label)


class NODE_MT_layout_base(NodeMenu):
    bl_label = "Layout"

    def draw(self, _context):
        layout = self.layout
        self.node_operator(layout, "NodeFrame", search_weight=-1)
        self.node_operator(layout, "NodeReroute")

        self.draw_assets_for_catalog(layout, self.bl_label)


add_base_pathing_dict = {
    "Group": "NODE_MT_group_add",
    "Layout": "NODE_MT_category_layout",
}


swap_base_pathing_dict = {
    "Group": "NODE_MT_group_swap",
    "Layout": "NODE_MT_layout_swap",
}


def generate_menu(bl_idname: str, template: Menu, layout_base: Menu, pathing_dict: dict = None):
    return type(bl_idname, (template, layout_base), {"bl_idname": bl_idname, "pathing_dict": pathing_dict})


def generate_menus(menus: dict, template: Menu, base_dict: dict):
    import copy
    pathing_dict = copy.copy(base_dict)
    menus = tuple(
        generate_menu(bl_idname, template, layout_base, pathing_dict)
        for bl_idname, layout_base in menus.items()
    )
    generate_pathing_dict(pathing_dict, menus)
    return menus


def generate_pathing_dict(pathing_dict, menus):
    for menu in menus:
        if hasattr(menu, "menu_path"):
            menu_path = menu.menu_path
        else:
            menu_path = menu.bl_label

        pathing_dict[menu_path] = menu.bl_idname


classes = (
    generate_menu("NODE_MT_group_add", template=AddNodeMenu, layout_base=NODE_MT_group_base),
    generate_menu("NODE_MT_group_swap", template=SwapNodeMenu, layout_base=NODE_MT_group_base),
    generate_menu("NODE_MT_category_layout", template=AddNodeMenu, layout_base=NODE_MT_layout_base),
    generate_menu("NODE_MT_layout_swap", template=SwapNodeMenu, layout_base=NODE_MT_layout_base),
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
