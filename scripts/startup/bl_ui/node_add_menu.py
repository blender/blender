# SPDX-FileCopyrightText: 2022-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

__all__ = (
    "set_math_node_default_props",
    "set_int_math_node_default_props",
    "set_vector_math_node_defaults"
)

import bpy
from bpy.types import Menu
from bpy.app.translations import (
    pgettext_n as n_,
    pgettext_iface as iface_,
    contexts as i18n_contexts,
)


def set_socket_default_value(settings, socket_identifier, socket_default_value):
    prop = settings.add()
    prop.name = "inputs[\"{:s}\"].default_value".format(socket_identifier)
    prop.value = socket_default_value
    return prop


def color_mix_node_defaults(enum_identifier, props):
    if enum_identifier == 'MIX':
        set_socket_default_value(props.settings, "Factor", "0.5")


def set_math_node_default_props(enum_identifier, props):

    if enum_identifier in ('MULTIPLY', 'POWER', 'MODULO', 'FLOORED_MODULO', 'ARCTAN2'):
        set_socket_default_value(props.settings, "Value", "1.0")
        set_socket_default_value(props.settings, "Value_001", "1.0")
    elif enum_identifier == 'ADD':
        set_socket_default_value(props.settings, "Value", "0.0")
        set_socket_default_value(props.settings, "Value_001", "0.0")
    elif enum_identifier == 'SUBTRACT':
        # 1 - x operations are common for subtraction.
        set_socket_default_value(props.settings, "Value", "1.0")
        set_socket_default_value(props.settings, "Value_001", "0.0")
    elif enum_identifier == 'MULTIPLY_ADD':
        set_socket_default_value(props.settings, "Value_001", "1.0")
        set_socket_default_value(props.settings, "Value_002", "0.0")


def set_int_math_node_default_props(enum_identifier, props):
    if enum_identifier in (
        'MULTIPLY',
        'DIVIDE',
        'DIVIDE_ROUND',
        'DIVIDE_FLOOR',
        'DIVIDE_CEIL',
        'FLOORED_MODULO',
            'MODULO'):
        set_socket_default_value(props.settings, "Value", "1")
        set_socket_default_value(props.settings, "Value_001", "1")

    elif enum_identifier == 'MULTIPLY_ADD':
        set_socket_default_value(props.settings, "Value", "1")
        set_socket_default_value(props.settings, "Value_001", "0")


def set_vector_math_node_defaults(enum_identifier, props):
    if enum_identifier in ('MULTIPLY', 'DIVIDE', 'POWER', 'MODULO'):
        set_socket_default_value(props.settings, "Vector", "(1.0, 1.0, 1.0)")
        set_socket_default_value(props.settings, "Vector_001", "(1.0, 1.0, 1.0)")
    elif enum_identifier == 'SUBTRACT':
        # 1 - x operations are common for subtraction.
        set_socket_default_value(props.settings, "Vector", "(1.0, 1.0, 1.0)")
        set_socket_default_value(props.settings, "Vector_001", "(0.0, 0.0, 0.0)")
    elif enum_identifier == 'MULTIPLY_ADD':
        set_socket_default_value(props.settings, "Vector_001", "(1.0, 1.0, 1.0)")
        set_socket_default_value(props.settings, "Vector_002", "(0.0, 0.0, 0.0)")


class NodeMenu(Menu):
    """A base-class defining the shared methods for AddNodeMenu and SwapNodeMenu."""
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
            label = bl_rna.name if bl_rna else n_("Unknown")
            translate = True

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
    def node_operator_with_searchable_enum(
            cls,
            context,
            layout,
            node_idname,
            property_name,
            search_weight=0.0,
            defaults_callback=None):
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
                if defaults_callback is not None:
                    defaults_callback(item.identifier, props)
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
    def node_operator_with_outputs(
            cls, context, layout, node_type, subnames, *, label=None, poll=None, search_weight=0.0):
        """Similar to `node_operator`, but with extra entries based on a enum socket while in search."""
        bl_rna = bpy.types.Node.bl_rna_get_subclass(node_type)
        if not label:
            label = bl_rna.name if bl_rna else "Unknown"

        if poll is not None and poll is False:
            return None

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
    def color_mix_node(cls, context, layout, search_weight=0.0):
        """The 'Mix Color' node, with its different blend modes available while in search."""
        label = iface_("Mix Color")

        operators = []
        props = cls.node_operator(layout, "ShaderNodeMix", label=label, translate=False, search_weight=search_weight)
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
                    search_weight=search_weight,
                )
                prop = props.settings.add()
                prop.name = "data_type"
                prop.value = "'RGBA'"
                prop = props.settings.add()
                prop.name = "blend_type"
                prop.value = repr(item.identifier)
                color_mix_node_defaults(item.identifier, props)
                operators.append(props)

        for props in operators:
            if hasattr(props, "use_transform"):
                props.use_transform = cls.use_transform

        return operators

    @classmethod
    def typed_bundle(cls, layout, label):
        props = layout.operator(cls.typed_bundle_operator_id, text=label, text_ctxt=i18n_contexts.default)

        if hasattr(props, "use_transform"):
            props.use_transform = cls.use_transform

        return props

    @classmethod
    def new_empty_group(cls, layout):
        """Group Node with a newly created empty group as its assigned node-tree."""
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

        cls.new_empty_group(layout)

        if node_tree in all_node_groups.values():
            layout.separator()
            cls.node_operator(layout, "NodeGroupInput")
            cls.node_operator(layout, "NodeGroupOutput")

        if node_tree:
            prefs = bpy.context.preferences
            show_hidden = prefs.show_hidden_ids

            local_groups = []
            has_non_local_groups = False

            for group in context.blend_data.node_groups:
                if group.bl_idname != node_tree.bl_idname:
                    continue
                if group.contains_tree(node_tree):
                    continue
                if not show_hidden:
                    if group.name.startswith('.'):
                        continue
                if group.library is not None:
                    has_non_local_groups = True
                    continue
                local_groups.append(group)

            if has_non_local_groups:
                layout.separator()
                cls.draw_menu(layout, path="Group/Linked")

            if local_groups:
                layout.separator()
                for group in local_groups:
                    cls.draw_group(context, layout, group)

    @classmethod
    def draw_linked_groups(cls, context, layout):
        space_node = context.space_data
        node_tree = space_node.edit_tree
        prefs = bpy.context.preferences
        show_hidden = prefs.show_hidden_ids

        for group in context.blend_data.node_groups:
            if group.library is None:
                continue
            if group.bl_idname != node_tree.bl_idname:
                continue
            if group.is_library_indirect:
                continue
            if not show_hidden:
                if group.name.startswith('.'):
                    continue
            cls.draw_group(context, layout, group)

    @classmethod
    def draw_group(cls, context, layout, group):
        from nodeitems_builtins import node_tree_group_type
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

        if hasattr(props, "use_transform"):
            props.use_transform = cls.use_transform

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
    typed_bundle_operator_id = "node.add_typed_bundle"

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
    typed_bundle_operator_id = "node.swap_typed_bundle"

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


class NODE_MT_linked_group_base(NodeMenu):
    bl_label = "Linked"

    def draw(self, context):
        layout = self.layout
        self.draw_linked_groups(context, layout)


class NODE_MT_layout_base(NodeMenu):
    bl_label = "Layout"

    def draw(self, _context):
        layout = self.layout
        self.node_operator(layout, "NodeFrame", search_weight=-1)
        self.node_operator(layout, "NodeReroute")

        self.draw_assets_for_catalog(layout, self.bl_label)


add_base_pathing_dict = {
    "Group": "NODE_MT_group_add",
    "Group/Linked": "NODE_MT_linked_group_add",
    "Layout": "NODE_MT_category_layout",
}


swap_base_pathing_dict = {
    "Group": "NODE_MT_group_swap",
    "Group/Linked": "NODE_MT_linked_group_swap",
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
    generate_menu(
        "NODE_MT_group_add",
        template=AddNodeMenu,
        layout_base=NODE_MT_group_base,
        pathing_dict=add_base_pathing_dict),
    generate_menu(
        "NODE_MT_group_swap",
        template=SwapNodeMenu,
        layout_base=NODE_MT_group_base,
        pathing_dict=swap_base_pathing_dict),
    generate_menu(
        "NODE_MT_linked_group_add",
        template=AddNodeMenu,
        layout_base=NODE_MT_linked_group_base,
        pathing_dict=add_base_pathing_dict),
    generate_menu(
        "NODE_MT_linked_group_swap",
        template=SwapNodeMenu,
        layout_base=NODE_MT_linked_group_base,
        pathing_dict=swap_base_pathing_dict),
    generate_menu(
        "NODE_MT_category_layout",
        template=AddNodeMenu,
        layout_base=NODE_MT_layout_base,
        pathing_dict=add_base_pathing_dict),
    generate_menu(
        "NODE_MT_layout_swap",
        template=SwapNodeMenu,
        layout_base=NODE_MT_layout_base,
        pathing_dict=swap_base_pathing_dict),
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
