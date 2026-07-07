# SPDX-FileCopyrightText: 2018-2022 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

from .....io.com.gltf2_io_extensions import Extension
from .....io.com.constants import GLTF_IOR
from ..search_node_tree import get_socket


def export_ior(blender_material, extensions, export_settings):
    ior_socket = get_socket(blender_material.node_tree, 'IOR')

    if not ior_socket.socket:
        return None

    # We don't manage case where socket is linked, always check default value
    if ior_socket.socket.is_linked:
        # TODOExt: add warning?
        return None

    # Exporting IOR even if it is the default value
    # It will be removed by the exporter if it is not animated
    # (In case the first key is the default value, we need to keep the extension)

    # Export only if the following extensions are exported:
    need_to_export_ior = [
        'KHR_materials_transmission',
        'KHR_materials_volume',
        'KHR_materials_specular'
    ]

    if not any([e in extensions.keys() for e in need_to_export_ior]):
        return None

    ior_extension = {}
    if ior_socket.socket.default_value != GLTF_IOR:
        ior_extension['ior'] = ior_socket.socket.default_value

    # Storing path for KHR_animation_pointer
    path_ = {}
    path_['length'] = 1
    path_['path'] = "/materials/XXX/extensions/KHR_materials_ior/ior"
    export_settings['current_paths']["node_tree." + ior_socket.socket.path_from_id() + ".default_value"] = path_

    return Extension('KHR_materials_ior', ior_extension, False)
