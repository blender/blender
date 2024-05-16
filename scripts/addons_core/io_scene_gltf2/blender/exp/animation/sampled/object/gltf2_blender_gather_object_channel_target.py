# SPDX-FileCopyrightText: 2018-2022 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

from ......io.exp.gltf2_io_user_extensions import export_user_extensions
from ......io.com import gltf2_io
from ....gltf2_blender_gather_cache import cached


@cached
def gather_object_sampled_channel_target(
        obj_uuid: str,
        channel: str,
        export_settings
) -> gltf2_io.AnimationChannelTarget:

    blender_object = export_settings['vtree'].nodes[obj_uuid].blender_object

    animation_channel_target = gltf2_io.AnimationChannelTarget(
        extensions=__gather_extensions(obj_uuid, channel, export_settings),
        extras=__gather_extras(obj_uuid, channel, export_settings),
        node=__gather_node(obj_uuid, export_settings),
        path=__gather_path(channel, export_settings)
    )

    export_user_extensions('gather_animation_object_sampled_channel_target_hook',
                           export_settings,
                           blender_object,
                           channel)

    return animation_channel_target


def __gather_extensions(armature_uuid, channel, export_settings):
    return None


def __gather_extras(armature_uuid, channel, export_settings):
    return None


def __gather_node(obj_uuid: str, export_settings):
    return export_settings['vtree'].nodes[obj_uuid].node


def __gather_path(channel, export_settings):
    return {
        "location": "translation",
        "rotation_quaternion": "rotation",
        "scale": "scale"
    }.get(channel)
