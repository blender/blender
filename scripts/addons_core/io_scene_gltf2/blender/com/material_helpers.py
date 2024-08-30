# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy

# Get compatibility at export with old files


def get_gltf_node_old_name():
    return "glTF Settings"

# Old group name


def get_gltf_old_group_node_name():
    return "glTF Metallic Roughness"


def get_gltf_node_name():
    return "glTF Material Output"


def create_settings_group(name):
    gltf_node_group = bpy.data.node_groups.new(name, 'ShaderNodeTree')
    gltf_node_group.interface.new_socket("Occlusion", socket_type="NodeSocketFloat")
    thicknessFactor = gltf_node_group.interface.new_socket("Thickness", socket_type="NodeSocketFloat", )
    thicknessFactor.default_value = 0.0
    gltf_node_group.nodes.new('NodeGroupOutput')
    gltf_node_group_input = gltf_node_group.nodes.new('NodeGroupInput')
    gltf_node_group_input.location = -200, 0
    return gltf_node_group
