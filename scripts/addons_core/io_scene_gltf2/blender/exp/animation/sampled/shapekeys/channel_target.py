# SPDX-FileCopyrightText: 2018-2022 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

from ......io.com import gltf2_io
from ......io.exp.user_extensions import export_user_extensions
from ....cache import cached


@cached
def gather_sk_sampled_channel_target(
        obj_uuid: str,
        export_settings
) -> gltf2_io.AnimationChannelTarget:

    animation_channel_target = gltf2_io.AnimationChannelTarget(
        extensions=__gather_extensions(obj_uuid, export_settings),
        extras=__gather_extras(obj_uuid, export_settings),
        node=__gather_node(obj_uuid, export_settings),
        path='weights'
    )

    blender_object = export_settings['vtree'].nodes[obj_uuid].blender_object
    export_user_extensions('animation_action_sk_sampled_target', export_settings, blender_object)

    return animation_channel_target


def __gather_extensions(armature_uuid, export_settings):
    return None


def __gather_extras(armature_uuid, export_settings):
    return None


def __gather_node(obj_uuid: str, export_settings):
    return export_settings['vtree'].nodes[obj_uuid].node
