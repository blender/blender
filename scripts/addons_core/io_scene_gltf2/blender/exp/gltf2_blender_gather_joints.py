# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

from mathutils import Quaternion, Vector
from ...io.com import gltf2_io
from ...io.exp.gltf2_io_user_extensions import export_user_extensions
from ..com.gltf2_blender_extras import generate_extras
from .gltf2_blender_gather_cache import cached
from . import gltf2_blender_gather_tree


# TODO these 3 functions move to shared file
def __convert_swizzle_location(loc, export_settings):
    """Convert a location from Blender coordinate system to glTF coordinate system."""
    if export_settings['gltf_yup']:
        return Vector((loc[0], loc[2], -loc[1]))
    else:
        return Vector((loc[0], loc[1], loc[2]))


def __convert_swizzle_rotation(rot, export_settings):
    """
    Convert a quaternion rotation from Blender coordinate system to glTF coordinate system.

    'w' is still at first position.
    """
    if export_settings['gltf_yup']:
        return Quaternion((rot[0], rot[1], rot[3], -rot[2]))
    else:
        return Quaternion((rot[0], rot[1], rot[2], rot[3]))


def __convert_swizzle_scale(scale, export_settings):
    """Convert a scale from Blender coordinate system to glTF coordinate system."""
    if export_settings['gltf_yup']:
        return Vector((scale[0], scale[2], scale[1]))
    else:
        return Vector((scale[0], scale[1], scale[2]))


@cached
def gather_joint_vnode(vnode, export_settings):
    """
    Generate a glTF2 node from a blender bone, as joints in glTF2 are simply nodes.

    :param blender_bone: a blender PoseBone
    :param export_settings: the settings for this export
    :return: a glTF2 node (acting as a joint)
    """
    vtree = export_settings['vtree']
    blender_bone = vtree.nodes[vnode].blender_bone

    if export_settings['gltf_armature_object_remove'] is True:
        if vtree.nodes[vnode].parent_uuid is not None:
            mat = vtree.nodes[vtree.nodes[vnode].parent_uuid].matrix_world.inverted_safe(
            ) @ vtree.nodes[vnode].matrix_world
        else:
            mat = vtree.nodes[vnode].matrix_world
    else:
        mat = vtree.nodes[vtree.nodes[vnode].parent_uuid].matrix_world.inverted_safe() @ vtree.nodes[vnode].matrix_world

    trans, rot, sca = mat.decompose()

    trans = __convert_swizzle_location(trans, export_settings)
    rot = __convert_swizzle_rotation(rot, export_settings)
    sca = __convert_swizzle_scale(sca, export_settings)

    translation, rotation, scale = (None, None, None)
    if trans[0] != 0.0 or trans[1] != 0.0 or trans[2] != 0.0:
        translation = [trans[0], trans[1], trans[2]]
    if rot[0] != 1.0 or rot[1] != 0.0 or rot[2] != 0.0 or rot[3] != 0.0:
        rotation = [rot[1], rot[2], rot[3], rot[0]]
    if sca[0] != 1.0 or sca[1] != 1.0 or sca[2] != 1.0:
        scale = [sca[0], sca[1], sca[2]]

    # traverse into children
    children = []

    for bone_uuid in [c for c in vtree.nodes[vnode].children if vtree.nodes[c].blender_type ==
                      gltf2_blender_gather_tree.VExportNode.BONE]:
        children.append(gather_joint_vnode(bone_uuid, export_settings))

    # finally add to the joints array containing all the joints in the hierarchy
    node = gltf2_io.Node(
        camera=None,
        children=children,
        extensions=None,
        extras=__gather_extras(blender_bone, export_settings),
        matrix=None,
        mesh=None,
        name=blender_bone.name if vtree.nodes[vnode].leaf_reference is None else vtree.nodes[vtree.nodes[vnode].leaf_reference].blender_bone.name + '_leaf',
        rotation=rotation,
        scale=scale,
        skin=None,
        translation=translation,
        weights=None
    )

    export_user_extensions('gather_joint_hook', export_settings, node, blender_bone)

    vtree.nodes[vnode].node = node

    return node


def __gather_extras(blender_bone, export_settings):
    if export_settings['gltf_extras']:
        return generate_extras(blender_bone.bone)
    return None
