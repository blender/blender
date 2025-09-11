# SPDX-FileCopyrightText: 2018-2022 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
import typing
import mathutils
from ......io.com import gltf2_io
from ......io.exp.user_extensions import export_user_extensions
from ......io.com import constants as gltf2_io_constants
from ......io.exp import binary_data as gltf2_io_binary_data
from .....com import gltf2_blender_math
from ....accessors import gather_accessor
from ....cache import cached
from ....tree import VExportNode
from .keyframes import gather_bone_sampled_keyframes


@cached
def gather_bone_sampled_animation_sampler(
        armature_uuid: str,
        bone: str,
        channel: str,
        action_name: str,
        slot_identifier: str,
        node_channel_is_animated: bool,
        node_channel_interpolation: str,
        export_settings
):

    pose_bone = export_settings['vtree'].nodes[armature_uuid].blender_object.pose.bones[bone]

    keyframes = __gather_keyframes(
        armature_uuid,
        bone,
        channel,
        action_name,
        slot_identifier,
        node_channel_is_animated,
        export_settings)

    if keyframes is None:
        # After check, no need to animate this node for this channel
        return None

    # Now we are raw input/output, we need to convert to glTF data
    input, output = __convert_keyframes(armature_uuid, bone, channel, keyframes, action_name, export_settings)

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

    export_user_extensions('gather_animation_sampler_hook',
                           export_settings,
                           sampler,
                           export_settings['vtree'].nodes[armature_uuid].blender_object,
                           pose_bone,
                           action_name,
                           node_channel_is_animated)

    return sampler


@cached
def __gather_keyframes(
        armature_uuid: str,
        bone: str,
        channel: str,
        action_name: str,
        slot_identifier: str,
        node_channel_is_animated: bool,
        export_settings
):

    keyframes = gather_bone_sampled_keyframes(
        armature_uuid,
        bone,
        channel,
        action_name,
        slot_identifier,
        node_channel_is_animated,
        export_settings
    )

    if keyframes is None:
        # After check, no need to animation this node
        return None

    return keyframes


def __convert_keyframes(armature_uuid, bone_name, channel, keyframes, action_name, export_settings):

    # Sliding can come from:
    # - option SLIDE for negative frames
    # - option to start animation at frame 0 for looping
    if armature_uuid in export_settings['slide'].keys(
    ) and action_name in export_settings['slide'][armature_uuid].keys():
        for k in keyframes:
            k.frame += -export_settings['slide'][armature_uuid][action_name]
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

    bone = export_settings['vtree'].nodes[armature_uuid].blender_object.pose.bones[bone_name]
    target_datapath = "pose.bones['" + bone_name + "']." + channel

    if bone.parent is None:
        # bone at root of armature
        axis_basis_change = mathutils.Matrix.Identity(4)
        if is_yup:
            axis_basis_change = mathutils.Matrix(
                ((1.0, 0.0, 0.0, 0.0),
                    (0.0, 0.0, 1.0, 0.0),
                    (0.0, -1.0, 0.0, 0.0),
                    (0.0, 0.0, 0.0, 1.0)))
        correction_matrix_local = axis_basis_change @ bone.bone.matrix_local
    else:
        # Bone is not at root of armature
        # There are 2 cases :
        parent_uuid = export_settings['vtree'].nodes[export_settings['vtree']
                                                     .nodes[armature_uuid].bones[bone.name]].parent_uuid
        if parent_uuid is not None and export_settings['vtree'].nodes[parent_uuid].blender_type == VExportNode.BONE:
            # export bone is not at root of armature neither
            blender_bone_parent = export_settings['vtree'].nodes[parent_uuid].blender_bone
            correction_matrix_local = (
                blender_bone_parent.bone.matrix_local.inverted_safe() @
                bone.bone.matrix_local
            )
        else:
            # exported bone (after filter) is at root of armature
            axis_basis_change = mathutils.Matrix.Identity(4)
            if is_yup:
                axis_basis_change = mathutils.Matrix(
                    ((1.0, 0.0, 0.0, 0.0),
                     (0.0, 0.0, 1.0, 0.0),
                     (0.0, -1.0, 0.0, 0.0),
                     (0.0, 0.0, 0.0, 1.0)))
            correction_matrix_local = axis_basis_change
    transform = correction_matrix_local

    values = []
    fps = (bpy.context.scene.render.fps * bpy.context.scene.render.fps_base)
    for keyframe in keyframes:
        # Transform the data and build gltf control points
        value = gltf2_blender_math.transform(keyframe.value, target_datapath, transform, False)
        keyframe_value = gltf2_blender_math.mathutils_to_gltf(value)

        if keyframe.in_tangent is not None:
            # we can directly transform the tangent as it currently is represented by a control point
            in_tangent = gltf2_blender_math.transform(keyframe.in_tangent, target_datapath, transform, False)

            # the tangent in glTF is relative to the keyframe value and uses seconds
            if not isinstance(value, list):
                in_tangent = fps * (in_tangent - value)
            else:
                in_tangent = [fps * (in_tangent[i] - value[i]) for i in range(len(value))]
            keyframe_value = gltf2_blender_math.mathutils_to_gltf(in_tangent) + keyframe_value  # append

        if keyframe.out_tangent is not None:
            # we can directly transform the tangent as it currently is represented by a control point
            out_tangent = gltf2_blender_math.transform(keyframe.out_tangent, target_datapath, transform, False)

            # the tangent in glTF is relative to the keyframe value and uses seconds
            if not isinstance(value, list):
                out_tangent = fps * (out_tangent - value)
            else:
                out_tangent = [fps * (out_tangent[i] - value[i]) for i in range(len(value))]
            keyframe_value = keyframe_value + gltf2_blender_math.mathutils_to_gltf(out_tangent)  # append

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
        export_settings)

    return input, output


def __gather_interpolation(node_channel_is_animated, node_channel_interpolation, keyframes, export_settings):

    if len(keyframes) > 2:
        # keep STEP as STEP, other become the interpolation chosen by the user
        return {
            "STEP": "STEP"
        }.get(node_channel_interpolation, export_settings['gltf_sampling_interpolation_fallback'])
    elif len(keyframes) == 1:
        if node_channel_is_animated is False:
            return "STEP"
        elif node_channel_interpolation == "CUBICSPLINE":
            return export_settings['gltf_sampling_interpolation_fallback']  # We can't have a single keyframe with CUBICSPLINE
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
