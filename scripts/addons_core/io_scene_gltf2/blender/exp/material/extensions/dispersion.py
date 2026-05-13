# SPDX-FileCopyrightText: 2018-2026 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0


from .....io.com.gltf2_io_extensions import Extension
from ..search_node_tree import get_socket_from_gltf_material_node, get_const_from_default_value_socket
import bpy


def export_dispersion(bmat, extensions, export_settings):

    # If no volume extension, no dispersion extension export
    if "KHR_materials_volume" not in extensions:
        return None

    dispersion_socket = get_socket_from_gltf_material_node(
        bmat.get_used_material().node_tree, 'Dispersion')
    if dispersion_socket.socket is None:
        # If no dispersion (here because there is no glTF Material Output node), no dispersion extension export
        return None

    dispersion_extension = {}
    if isinstance(dispersion_socket.socket, bpy.types.NodeSocket):
        dispersion, path = get_const_from_default_value_socket(dispersion_socket, kind='VALUE')
        if dispersion != 0.0:
            dispersion_extension['dispersion'] = dispersion

        # Storing path for KHR_animation_pointer
        if path is not None:
            path_ = {}
            path_['length'] = 1
            path_['path'] = "/materials/XXX/extensions/KHR_materials_dispersion/dispersion"
            export_settings['current_paths'][path] = path_

    return Extension('KHR_materials_dispersion', dispersion_extension, False)
