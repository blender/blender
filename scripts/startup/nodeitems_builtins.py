# SPDX-FileCopyrightText: 2013-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from nodeitems_utils import (
    NodeCategory,
    NodeItem,
    NodeItemCustom,
)


# Subclasses for standard node types

class SortedNodeCategory(NodeCategory):
    def __init__(self, identifier, name, description="", items=None):
        # for builtin nodes the convention is to sort by name
        if isinstance(items, list):
            items = sorted(items, key=lambda item: item.label.lower())

        super().__init__(identifier, name, description=description, items=items)


class CompositorNodeCategory(SortedNodeCategory):
    @classmethod
    def poll(cls, context):
        return (
            context.space_data.type == 'NODE_EDITOR' and
            context.space_data.tree_type == 'CompositorNodeTree'
        )


class ShaderNodeCategory(SortedNodeCategory):
    @classmethod
    def poll(cls, context):
        return (
            context.space_data.type == 'NODE_EDITOR' and
            context.space_data.tree_type == 'ShaderNodeTree'
        )


# Maps node tree type to group node type.
node_tree_group_type = {
    'CompositorNodeTree': 'CompositorNodeGroup',
    'ShaderNodeTree': 'ShaderNodeGroup',
    'TextureNodeTree': 'TextureNodeGroup',
    'GeometryNodeTree': 'GeometryNodeGroup',
}


def register():
    pass


def unregister():
    pass
