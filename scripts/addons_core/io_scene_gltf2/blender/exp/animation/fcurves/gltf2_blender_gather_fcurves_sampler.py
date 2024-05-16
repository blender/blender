# SPDX-FileCopyrightText: 2018-2022 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
import typing
import mathutils
from .....io.com import gltf2_io
from .....io.com import gltf2_io_constants
from .....blender.com.gltf2_blender_conversion import get_gltf_interpolation
from .....io.exp import gltf2_io_binary_data
from .....io.exp.gltf2_io_user_extensions import export_user_extensions
from ....com.gltf2_blender_data_path import get_target_property_name
from ....com import gltf2_blender_math
from ...gltf2_blender_gather_cache import cached
from ...gltf2_blender_gather_accessors import gather_accessor
from ...gltf2_blender_gather_tree import VExportNode
from .gltf2_blender_gather_fcurves_keyframes import gather_fcurve_keyframes


@cached
def gather_animation_fcurves_sampler(
        obj_uuid: str,
        channel_group: typing.Tuple[bpy.types.FCurve],
        bone: typing.Optional[str],
        custom_range: typing.Optional[set],
        extra_mode: bool,
        export_settings
) -> gltf2_io.AnimationSampler:

    # matrix_parent_inverse needed for fcurves?

    keyframes = __gather_keyframes(
        obj_uuid,
        channel_group,
        bone,
        custom_range,
        extra_mode,
        export_settings)

    if keyframes is None:
        # After check, no need to animate this node for this channel
        return None

    # Now we are raw input/output, we need to convert to glTF data
    input, output = __convert_keyframes(obj_uuid, channel_group, bone, keyframes, extra_mode, export_settings)

    sampler = gltf2_io.AnimationSampler(
        extensions=None,
        extras=None,
        input=input,
        interpolation=__gather_interpolation(channel_group, export_settings),
        output=output
    )

    blender_object = export_settings['vtree'].nodes[obj_uuid].blender_object
    export_user_extensions('animation_gather_fcurve_channel_sampler', export_settings, blender_object, bone)

    return sampler


@cached
def __gather_keyframes(
        obj_uuid: str,
        channel_group: typing.Tuple[bpy.types.FCurve],
        bone: typing.Optional[str],
        custom_range: typing.Optional[set],
        extra_mode: bool,
        export_settings
):

    return gather_fcurve_keyframes(obj_uuid, channel_group, bone, custom_range, extra_mode, export_settings)


def __convert_keyframes(
        obj_uuid: str,
        channel_group: typing.Tuple[bpy.types.FCurve],
        bone_name: typing.Optional[str],
        keyframes,
        extra_mode: bool,
        export_settings):

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

    need_rotation_correction = (
        export_settings['gltf_cameras'] and export_settings['vtree'].nodes[obj_uuid].blender_type == VExportNode.CAMERA) or (
        export_settings['gltf_lights'] and export_settings['vtree'].nodes[obj_uuid].blender_type == VExportNode.LIGHT)

    target_datapath = [c for c in channel_group if c is not None][0].data_path

    if bone_name is not None:
        bone = export_settings['vtree'].nodes[obj_uuid].blender_object.pose.bones[bone_name]
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
                                                         .nodes[obj_uuid].bones[bone_name]].parent_uuid
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

    else:
        if export_settings['vtree'].nodes[obj_uuid].blender_object.parent is not None:
            matrix_parent_inverse = export_settings['vtree'].nodes[obj_uuid].blender_object.matrix_parent_inverse.copy(
            ).freeze()
        else:
            matrix_parent_inverse = mathutils.Matrix.Identity(4).freeze()
        transform = matrix_parent_inverse

    values = []
    fps = (bpy.context.scene.render.fps * bpy.context.scene.render.fps_base)
    for keyframe in keyframes:

        if extra_mode is True:
            # Export as is, without trying to convert
            keyframe_value = keyframe.value
            if keyframe.in_tangent is not None:
                keyframe_value = keyframe.in_tangent + keyframe_value
            if keyframe.out_tangent is not None:
                keyframe_value = keyframe_value + keyframe.out_tangent
            values += keyframe_value
            continue

        # Transform the data and build gltf control points
        value = gltf2_blender_math.transform(keyframe.value, target_datapath, transform, need_rotation_correction)
        if is_yup and bone_name is None:
            value = gltf2_blender_math.swizzle_yup(value, target_datapath)
        keyframe_value = gltf2_blender_math.mathutils_to_gltf(value)

        if keyframe.in_tangent is not None:
            # we can directly transform the tangent as it currently is represented by a control point
            in_tangent = gltf2_blender_math.transform(
                keyframe.in_tangent, target_datapath, transform, need_rotation_correction)
            if is_yup and bone_name is None:
                in_tangent = gltf2_blender_math.swizzle_yup(in_tangent, target_datapath)
            # the tangent in glTF is relative to the keyframe value and uses seconds
            if not isinstance(value, list):
                in_tangent = fps * (in_tangent - value)
            else:
                in_tangent = [fps * (in_tangent[i] - value[i]) for i in range(len(value))]
            keyframe_value = gltf2_blender_math.mathutils_to_gltf(in_tangent) + keyframe_value  # append

        if keyframe.out_tangent is not None:
            # we can directly transform the tangent as it currently is represented by a control point
            out_tangent = gltf2_blender_math.transform(
                keyframe.out_tangent, target_datapath, transform, need_rotation_correction)
            if is_yup and bone_name is None:
                out_tangent = gltf2_blender_math.swizzle_yup(out_tangent, target_datapath)
            # the tangent in glTF is relative to the keyframe value and uses seconds
            if not isinstance(value, list):
                out_tangent = fps * (out_tangent - value)
            else:
                out_tangent = [fps * (out_tangent[i] - value[i]) for i in range(len(value))]
            keyframe_value = keyframe_value + gltf2_blender_math.mathutils_to_gltf(out_tangent)  # append

        values += keyframe_value

    # store the keyframe data in a binary buffer
    component_type = gltf2_io_constants.ComponentType.Float
    if get_target_property_name(target_datapath) == "value":
        # channels with 'weight' targets must have scalar accessors
        data_type = gltf2_io_constants.DataType.Scalar
    else:
        data_type = gltf2_io_constants.DataType.vec_type_from_num(len(keyframes[0].value))

    output = gltf2_io.Accessor(
        buffer_view=gltf2_io_binary_data.BinaryData.from_list(values, component_type),
        byte_offset=None,
        component_type=component_type,
        count=len(values) // gltf2_io_constants.DataType.num_elements(data_type),
        extensions=None,
        extras=None,
        max=None,
        min=None,
        name=None,
        normalized=None,
        sparse=None,
        type=data_type
    )

    return input, output


def __gather_interpolation(
        channel_group: typing.Tuple[bpy.types.FCurve],
        export_settings,
) -> str:

    # Note: channels has some None items only for SK if some SK are not animated
    # Non-sampled keyframes implies that all keys are of the same type, and that the
    # type is supported by glTF (because we checked in needs_baking).
    blender_keyframe = [c for c in channel_group if c is not None][0].keyframe_points[0]

    # Select the interpolation method.
    return get_gltf_interpolation(blender_keyframe.interpolation)
