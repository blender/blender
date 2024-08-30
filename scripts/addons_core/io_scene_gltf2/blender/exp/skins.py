# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import mathutils
from ...io.com import gltf2_io, constants as gltf2_io_constants
from ...io.exp import binary_data as gltf2_io_binary_data
from ...io.exp.user_extensions import export_user_extensions
from . import accessors as gltf2_blender_gather_accessors
from . import joints as gltf2_blender_gather_joints
from .tree import VExportNode
from .cache import cached


@cached
def gather_skin(armature_uuid, export_settings):
    """
    Gather armatures, bones etc into a glTF2 skin object.

    :param blender_object: the object which may contain a skin
    :param export_settings:
    :return: a glTF2 skin object
    """

    if armature_uuid not in export_settings['vtree'].nodes:
        # User filtered objects to export, and keep the skined mesh, without keeping the armature
        return None

    blender_armature_object = export_settings['vtree'].nodes[armature_uuid].blender_object

    if not __filter_skin(blender_armature_object, export_settings):
        return None

    skin = gltf2_io.Skin(
        extensions=__gather_extensions(blender_armature_object, export_settings),
        extras=__gather_extras(blender_armature_object, export_settings),
        inverse_bind_matrices=__gather_inverse_bind_matrices(armature_uuid, export_settings),
        joints=__gather_joints(armature_uuid, export_settings),
        name=__gather_name(blender_armature_object, export_settings),
        skeleton=__gather_skeleton(blender_armature_object, export_settings)
    )

    # If armature is not exported, joints will be empty.
    # Do not construct skin in that case
    if len(skin.joints) == 0:
        return None

    export_user_extensions('gather_skin_hook', export_settings, skin, blender_armature_object)

    return skin


def __filter_skin(blender_armature_object, export_settings):
    if not export_settings['gltf_skins']:
        return False
    if blender_armature_object.type != 'ARMATURE' or len(blender_armature_object.pose.bones) == 0:
        return False

    return True


def __gather_extensions(blender_armature_object, export_settings):
    return None


def __gather_extras(blender_armature_object, export_settings):
    return None


def __gather_inverse_bind_matrices(armature_uuid, export_settings):

    blender_armature_object = export_settings['vtree'].nodes[armature_uuid].blender_object

    axis_basis_change = mathutils.Matrix.Identity(4)
    if export_settings['gltf_yup']:
        axis_basis_change = mathutils.Matrix(
            ((1.0, 0.0, 0.0, 0.0), (0.0, 0.0, 1.0, 0.0), (0.0, -1.0, 0.0, 0.0), (0.0, 0.0, 0.0, 1.0)))

    # store matrix_world of armature in case we need to add a neutral bone
    export_settings['vtree'].nodes[armature_uuid].matrix_world_armature = blender_armature_object.matrix_world.copy()

    bones_uuid = export_settings['vtree'].get_all_bones(armature_uuid)

    def __collect_matrices(bone):
        inverse_bind_matrix = (
            axis_basis_change @
            (
                blender_armature_object.matrix_world @
                bone.bone.matrix_local
            )
        ).inverted_safe()
        matrices.append(inverse_bind_matrix)

    matrices = []
    for b in bones_uuid:
        if export_settings['vtree'].nodes[b].leaf_reference is None:
            __collect_matrices(blender_armature_object.pose.bones[export_settings['vtree'].nodes[b].blender_bone.name])
        else:
            inverse_bind_matrix = (
                axis_basis_change @
                (
                    blender_armature_object.matrix_world @
                    export_settings['vtree'].nodes[export_settings['vtree'].nodes[b].leaf_reference].matrix_world_tail
                )
            ).inverted_safe()
            matrices.append(inverse_bind_matrix)  # Leaf bone

    # flatten the matrices
    inverse_matrices = []
    for matrix in matrices:
        for column in range(0, 4):
            for row in range(0, 4):
                inverse_matrices.append(matrix[row][column])

    binary_data = gltf2_io_binary_data.BinaryData.from_list(inverse_matrices, gltf2_io_constants.ComponentType.Float)
    return gltf2_blender_gather_accessors.gather_accessor(
        binary_data,
        gltf2_io_constants.ComponentType.Float,
        len(inverse_matrices) // gltf2_io_constants.DataType.num_elements(gltf2_io_constants.DataType.Mat4),
        None,
        None,
        gltf2_io_constants.DataType.Mat4,
        export_settings
    )


def __gather_joints(armature_uuid, export_settings):

    all_armature_children = export_settings['vtree'].nodes[armature_uuid].children
    root_bones_uuid = [
        c for c in all_armature_children if export_settings['vtree'].nodes[c].blender_type == VExportNode.BONE]

    # Create bone nodes
    for root_bone_uuid in root_bones_uuid:
        gltf2_blender_gather_joints.gather_joint_vnode(root_bone_uuid, export_settings)

    bones_uuid = export_settings['vtree'].get_all_bones(armature_uuid)
    joints = [export_settings['vtree'].nodes[b].node for b in bones_uuid]
    return joints


def __gather_name(blender_armature_object, export_settings):
    return blender_armature_object.name


def __gather_skeleton(blender_armature_object, export_settings):
    # In the future support the result of https://github.com/KhronosGroup/glTF/pull/1195
    return None
