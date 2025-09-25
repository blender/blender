# SPDX-FileCopyrightText: 2018-2022 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
import mathutils
from ......io.com import gltf2_io
from ......io.com import constants as gltf2_io_constants
from ......io.exp import binary_data as gltf2_io_binary_data
from ......io.exp.user_extensions import export_user_extensions
from .....com.data_path import get_target_object_path
from .....com import gltf2_blender_math
from ....tree import VExportNode
from ....cache import cached
from ....accessors import gather_accessor
from .keyframes import gather_object_sampled_keyframes


@cached
def gather_object_sampled_animation_sampler(
        obj_uuid: str,
        channel: str,
        action_name: str,
        slot_identifier: str,
        node_channel_is_animated: bool,
        node_channel_interpolation: str,
        export_settings
):

    keyframes = __gather_keyframes(
        obj_uuid,
        channel,
        action_name,
        slot_identifier,
        node_channel_is_animated,
        export_settings)

    if keyframes is None:
        # After check, no need to animate this node for this channel
        return None

    # Now we are raw input/output, we need to convert to glTF data
    input, output = __convert_keyframes(obj_uuid, channel, keyframes, action_name, export_settings)

    sampler = gltf2_io.AnimationSampler(
        extensions=None,
        extras=None,
        input=input,
        interpolation=__gather_interpolation(
            node_channel_is_animated,
            node_channel_interpolation,
            keyframes,
            export_settings),
        output=output)

    blender_object = export_settings['vtree'].nodes[obj_uuid].blender_object
    export_user_extensions('animation_gather_object_sampler', export_settings, blender_object, action_name)

    return sampler


def __gather_keyframes(
        obj_uuid: str,
        channel: str,
        action_name: str,
        slot_identifier: int,
        node_channel_is_animated: bool,
        export_settings
):

    keyframes = gather_object_sampled_keyframes(
        obj_uuid,
        channel,
        action_name,
        slot_identifier,
        node_channel_is_animated,
        export_settings
    )

    return keyframes


def __convert_keyframes(obj_uuid: str, channel: str, keyframes, action_name: str, export_settings):

    # Sliding can come from:
    # - option SLIDE for negative frames
    # - option to start animation at frame 0 for looping
    if obj_uuid in export_settings['slide'].keys() and action_name in export_settings['slide'][obj_uuid].keys():
        for k in keyframes:
            k.frame += -export_settings['slide'][obj_uuid][action_name]
            k.seconds = k.frame / (bpy.context.scene.render.fps * bpy.context.scene.render.fps_base)

    times = [k.seconds for k in keyframes]
    input = gather_accessor(
        gltf2_io_binary_data.BinaryData.from_list(times, gltf2_io_constants.ComponentType.Float),
        gltf2_io_constants.ComponentType.Float,
        len(times),
        tuple([max(times)]),
        tuple([min(times)]),
        gltf2_io_constants.DataType.Scalar,
        export_settings)

    is_yup = export_settings['gltf_yup']

    object_path = get_target_object_path(channel)
    transform = mathutils.Matrix.Identity(4)

    need_rotation_correction = (
        export_settings['gltf_cameras'] and export_settings['vtree'].nodes[obj_uuid].blender_type == VExportNode.CAMERA) or (
        export_settings['gltf_lights'] and export_settings['vtree'].nodes[obj_uuid].blender_type == VExportNode.LIGHT)

    values = []
    fps = (bpy.context.scene.render.fps * bpy.context.scene.render.fps_base)
    for keyframe in keyframes:

        # Transform the data and build gltf control points
        value = gltf2_blender_math.transform(keyframe.value, channel, transform, need_rotation_correction)
        if is_yup:
            value = gltf2_blender_math.swizzle_yup(value, channel)
        keyframe_value = gltf2_blender_math.mathutils_to_gltf(value)

        # No tangents when baking, we are using LINEAR or STEP interpolation

        values += keyframe_value

    # store the keyframe data in a binary buffer
    component_type = gltf2_io_constants.ComponentType.Float
    data_type = gltf2_io_constants.DataType.vec_type_from_num(len(keyframes[0].value))


    output = gather_accessor(
        gltf2_io_binary_data.BinaryData.from_list(values, component_type),
        component_type,
        len(values) // gltf2_io_constants.DataType.num_elements(data_type),
        None,
        None,
        data_type,
        export_settings
    )

    return input, output


def __gather_interpolation(
        node_channel_is_animated: bool,
        node_channel_interpolation: str,
        keyframes,
        export_settings):

    if len(keyframes) > 2:
        # keep STEP as STEP, other become the interpolation chosen by the user
        return {
            "STEP": "STEP"
        }.get(node_channel_interpolation, export_settings['gltf_sampling_interpolation_fallback'])
    elif len(keyframes) == 1:
        if node_channel_is_animated is False:
            return "STEP"
        elif node_channel_interpolation == "CUBICSPLINE":
            return export_settings['gltf_sampling_interpolation_fallback'] # We can't have a single keyframe with CUBICSPLINE
        else:
            return node_channel_interpolation
    else:
        # If we only have 2 keyframes, set interpolation to STEP if baked
        if node_channel_is_animated is False:
            # baked => We have first and last keyframe
            return "STEP"
        else:
            if keyframes[0].value == keyframes[1].value:
                return "STEP"
            else:
                return export_settings['gltf_sampling_interpolation_fallback']
