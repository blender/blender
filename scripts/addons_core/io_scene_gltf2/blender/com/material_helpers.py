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

    # Oclusion (glTF Core)
    gltf_node_group.interface.new_socket("Occlusion", socket_type="NodeSocketFloat")

    # Thickness (glTF KHR_materials_volume)
    thicknessFactor = gltf_node_group.interface.new_socket("Thickness", socket_type="NodeSocketFloat", )
    thicknessFactor.default_value = 0.0

    # Dispersion (glTF KHR_materials_dispersion)
    dispersionFactor = gltf_node_group.interface.new_socket("Dispersion", socket_type="NodeSocketFloat", )
    dispersionFactor.default_value = 0.0

    # Iridescence (glTF KHR_materials_iridescence)
    iridescenceFactor = gltf_node_group.interface.new_socket("Iridescence Factor", socket_type="NodeSocketFloat", )
    iridescenceFactor.default_value = 0.0

    iridescenceTicknessMinimum = gltf_node_group.interface.new_socket(
        "Iridescence Thickness Minimum", socket_type="NodeSocketFloat", )
    iridescenceTicknessMinimum.default_value = 100.0

    gltf_node_group.nodes.new('NodeGroupOutput')
    gltf_node_group_input = gltf_node_group.nodes.new('NodeGroupInput')
    gltf_node_group_input.location = -200, 0
    return gltf_node_group
