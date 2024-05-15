# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
from mathutils import Vector

from ...io.imp.gltf2_io_user_extensions import import_user_extensions
from ...io.imp.gltf2_io_binary import BinaryData
from .gltf2_blender_animation_utils import make_fcurve
from .gltf2_blender_vnode import VNode


class BlenderNodeAnim():
    """Blender Object Animation."""
    def __new__(cls, *args, **kwargs):
        raise RuntimeError("%s should not be instantiated" % cls)

    @staticmethod
    def anim(gltf, anim_idx, node_idx):
        """Manage animation targeting a node's TRS."""
        animation = gltf.data.animations[anim_idx]
        node = gltf.data.nodes[node_idx]

        if anim_idx not in node.animations.keys():
            return

        for channel_idx in node.animations[anim_idx]:
            channel = animation.channels[channel_idx]
            if channel.target.path not in ['translation', 'rotation', 'scale', 'pointer']:
                continue

            if channel.target.path == "pointer" and channel.target.extensions is None:
                continue

            if channel.target.path == "pointer" and (
                    "KHR_animation_pointer" not in channel.target.extensions or "pointer" not in channel.target.extensions["KHR_animation_pointer"]):
                continue

            if channel.target.path == "pointer":
                pointer_tab = channel.target.extensions["KHR_animation_pointer"]["pointer"].split("/")
                if not(
                    len(pointer_tab) >= 4 and pointer_tab[1] == "nodes" and pointer_tab[3] in [
                        "translation",
                        "rotation",
                        "scale"]):
                    continue

            BlenderNodeAnim.do_channel(gltf, anim_idx, node_idx, channel)

    @staticmethod
    def do_channel(gltf, anim_idx, node_idx, channel):
        animation = gltf.data.animations[anim_idx]
        vnode = gltf.vnodes[node_idx]
        path = channel.target.path

        if path == "pointer":
            path = channel.target.extensions["KHR_animation_pointer"]["pointer"].split("/")[3]

        import_user_extensions('gather_import_animation_channel_before_hook', gltf, animation, vnode, path, channel)

        action = BlenderNodeAnim.get_or_create_action(gltf, node_idx, animation.track_name)

        keys = BinaryData.get_data_from_accessor(gltf, animation.samplers[channel.sampler].input)
        values = BinaryData.get_data_from_accessor(gltf, animation.samplers[channel.sampler].output)

        if animation.samplers[channel.sampler].interpolation == "CUBICSPLINE":
            # TODO manage tangent?
            values = values[1::3]

        # Convert the curve from glTF to Blender.

        if path == "translation":
            blender_path = "location"
            group_name = "Location"
            num_components = 3
            values = [gltf.loc_gltf_to_blender(vals) for vals in values]
            values = vnode.base_locs_to_final_locs(values)

        elif path == "rotation":
            blender_path = "rotation_quaternion"
            group_name = "Rotation"
            num_components = 4
            values = [gltf.quaternion_gltf_to_blender(vals) for vals in values]
            values = vnode.base_rots_to_final_rots(values)

        elif path == "scale":
            blender_path = "scale"
            group_name = "Scale"
            num_components = 3
            values = [gltf.scale_gltf_to_blender(vals) for vals in values]
            values = vnode.base_scales_to_final_scales(values)

        # Objects parented to a bone are translated to the bone tip by default.
        # Correct for this by translating backwards from the tip to the root.
        if vnode.type == VNode.Object and path == "translation":
            if vnode.parent is not None and gltf.vnodes[vnode.parent].type == VNode.Bone:
                bone_length = gltf.vnodes[vnode.parent].bone_length
                off = Vector((0, -bone_length, 0))
                values = [vals + off for vals in values]

        if vnode.type == VNode.Bone:
            # Need to animate the pose bone when the node is a bone.
            group_name = vnode.blender_bone_name
            blender_path = 'pose.bones["%s"].%s' % (
                bpy.utils.escape_identifier(vnode.blender_bone_name),
                blender_path
            )

            # We have the final TRS of the bone in values. We need to give
            # the TRS of the pose bone though, which is relative to the edit
            # bone.
            #
            #     Final = EditBone * PoseBone
            #   where
            #     Final =    Trans[ft] Rot[fr] Scale[fs]
            #     EditBone = Trans[et] Rot[er]
            #     PoseBone = Trans[pt] Rot[pr] Scale[ps]
            #
            # Solving for PoseBone gives
            #
            #     pt = Rot[er^{-1}] (ft - et)
            #     pr = er^{-1} fr
            #     ps = fs

            if path == 'translation':
                edit_trans, edit_rot = vnode.editbone_trans, vnode.editbone_rot
                edit_rot_inv = edit_rot.conjugated()
                values = [
                    edit_rot_inv @ (trans - edit_trans)
                    for trans in values
                ]

            elif path == 'rotation':
                edit_rot = vnode.editbone_rot
                edit_rot_inv = edit_rot.conjugated()
                values = [
                    edit_rot_inv @ rot
                    for rot in values
                ]

            elif path == 'scale':
                pass  # no change needed

        # To ensure rotations always take the shortest path, we flip
        # adjacent antipodal quaternions.
        if path == 'rotation':
            for i in range(1, len(values)):
                if values[i].dot(values[i - 1]) < 0:
                    values[i] = -values[i]

        fps = (bpy.context.scene.render.fps * bpy.context.scene.render.fps_base)

        coords = [0] * (2 * len(keys))
        coords[::2] = (key[0] * fps for key in keys)

        for i in range(0, num_components):
            coords[1::2] = (vals[i] for vals in values)
            make_fcurve(
                action,
                coords,
                data_path=blender_path,
                index=i,
                group_name=group_name,
                interpolation=animation.samplers[channel.sampler].interpolation,
            )

        import_user_extensions('gather_import_animation_channel_after_hook',
                               gltf, animation, vnode, path, channel, action)

    @staticmethod
    def get_or_create_action(gltf, node_idx, anim_name):
        vnode = gltf.vnodes[node_idx]

        if vnode.type == VNode.Bone:
            # For bones, the action goes on the armature.
            vnode = gltf.vnodes[vnode.bone_arma]

        obj = vnode.blender_object

        action = gltf.action_cache.get(obj.name)
        if not action:
            name = anim_name + "_" + obj.name
            action = bpy.data.actions.new(name)
            action.id_root = 'OBJECT'
            gltf.needs_stash.append((obj, action))
            gltf.action_cache[obj.name] = action

        return action
