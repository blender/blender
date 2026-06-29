# SPDX-FileCopyrightText: 2018-2022 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
import typing
from .....io.com import gltf2_io
from .....io.exp.user_extensions import export_user_extensions
from ....com.conversion import get_target
from ...cache import cached
from ..anim_extra_utils import gather_animated_node, get_impacted_data, get_gltf_name_from_blender_property


@cached
def gather_fcurve_channel_target(
        id_type: str,
        elem_uuid: str,
        channels: typing.Tuple[bpy.types.FCurve],
        bone: typing.Optional[str],
        export_settings
) -> gltf2_io.AnimationChannelTarget:

    animation_channel_target = gltf2_io.AnimationChannelTarget(
        extensions=None,
        extras=None,
        node=__gather_node(id_type, elem_uuid, bone, export_settings),
        path=__gather_path(id_type, elem_uuid, channels, export_settings)
    )

    # This is used by sampled animation
    # So set it here on fcurve, to avoid crash later
    animation_channel_target.tmp_alpha_cst = False

    if id_type == 'OBJECT':
        blender_object = export_settings['vtree'].nodes[elem_uuid].blender_object
        export_user_extensions('animation_gather_fcurve_channel_target', export_settings, blender_object, bone)

    return animation_channel_target


@cached
def gather_fcurve_channel_target_extras(
        id_type: str,
        elem_uuid: str,
        bone: str,
        custom_property: str,
        export_settings) -> gltf2_io.AnimationChannelTarget:

    impacted_data = get_impacted_data(id_type)

    if impacted_data is None:
        export_settings['log'].warning("Unsupported type for animation pointer extras: " + id_type)
        return None

    if custom_property.startswith('pose.bones'):
        # Extract the custom prop name from the path
        custom_property = custom_property.split('][')[1][1:-2]
    else:
        custom_property = custom_property[2:-2]

    animation_channel_target = gltf2_io.AnimationChannelTarget(
        extensions=None,
        extras=None,
        node=__gather_node(id_type, elem_uuid, bone, export_settings),
        path="/" + impacted_data + "/XXX/extras/" + custom_property
    )

    return animation_channel_target


@cached
def gather_fcurve_channel_target_data(
        id_type: str,
        elem_uuid: str,
        bone: str,
        prop: str,
        export_settings) -> gltf2_io.AnimationChannelTarget:

    impacted_data = get_impacted_data(id_type)

    gltf_prop = get_gltf_name_from_blender_property(id_type, elem_uuid, prop, export_settings)

    animation_channel_target = gltf2_io.AnimationChannelTarget(
        extensions=None,
        extras=None,
        node=__gather_node(id_type, elem_uuid, bone, export_settings),
        path="/" + impacted_data + "/XXX/" + gltf_prop
    )

    return animation_channel_target


def __gather_node(id_type: str,
                  elem_uuid: str,
                  bone: typing.Union[str, None],
                  export_settings
                  ) -> gltf2_io.Node:

    return gather_animated_node(id_type, elem_uuid, bone, export_settings)


def __gather_path(id_type: str,
                  elem_uuid: str,
                  channels: typing.Tuple[bpy.types.FCurve],
                  export_settings
                  ) -> str:

    if id_type == "NODETREE":
        for path in export_settings['KHR_animation_pointer'][None]['materials'][elem_uuid]['paths'].keys():
            if path == "node_tree." + channels[0].data_path:
                return export_settings['KHR_animation_pointer'][None]['materials'][elem_uuid]['paths'][path]['path']
        return None
    else:

        # Note: channels has some None items only for SK if some SK are not animated, so keep a not None channel item
        target = [c for c in channels if c is not None][0].data_path.split('.')[-1]

        return get_target(target)
