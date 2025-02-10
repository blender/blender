# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy

from ...io.imp.user_extensions import import_user_extensions
from ...io.imp.gltf2_io_binary import BinaryData
from .animation_utils import make_fcurve, get_or_create_action_and_slot


class BlenderWeightAnim():
    """Blender ShapeKey Animation."""
    def __new__(cls, *args, **kwargs):
        raise RuntimeError("%s should not be instantiated" % cls)

    @staticmethod
    def anim(gltf, anim_idx, vnode_id):
        """Manage animation."""
        vnode = gltf.vnodes[vnode_id]

        node_idx = vnode.mesh_node_idx

        import_user_extensions('gather_import_animation_weight_before_hook',
                               gltf, vnode, gltf.data.animations[anim_idx])

        if node_idx is None:
            return

        node = gltf.data.nodes[node_idx]
        obj = vnode.blender_object
        fps = (bpy.context.scene.render.fps * bpy.context.scene.render.fps_base)

        animation = gltf.data.animations[anim_idx]

        if anim_idx not in node.animations.keys():
            return

        for channel_idx in node.animations[anim_idx]:
            channel = animation.channels[channel_idx]
            if channel.target.path == "weights":
                path = channel.target.path
                break
            if channel.target.path == "pointer":
                pointer_tab = channel.target.extensions["KHR_animation_pointer"]["pointer"].split("/")
                if len(pointer_tab) >= 4 and pointer_tab[1] in ["nodes", "meshes"] and pointer_tab[3] == "weights":
                    path = pointer_tab[3]
                    break
        else:
            return

        action, slot = get_or_create_action_and_slot(gltf, vnode_id, anim_idx, path)

        keys = BinaryData.get_data_from_accessor(gltf, animation.samplers[channel.sampler].input)
        values = BinaryData.get_data_from_accessor(gltf, animation.samplers[channel.sampler].output)

        # retrieve number of targets
        pymesh = gltf.data.meshes[gltf.data.nodes[node_idx].mesh]
        nb_targets = len(pymesh.shapekey_names)

        if animation.samplers[channel.sampler].interpolation == "CUBICSPLINE":
            offset = nb_targets
            stride = 3 * nb_targets
        else:
            offset = 0
            stride = nb_targets

        coords = [0] * (2 * len(keys))
        coords[::2] = (key[0] * fps for key in keys)

        for sk in range(nb_targets):
            if pymesh.shapekey_names[sk] is not None:  # Do not animate shapekeys not created
                coords[1::2] = (values[offset + stride * i + sk][0] for i in range(len(keys)))
                kb_name = pymesh.shapekey_names[sk]
                data_path = 'key_blocks["%s"].value' % bpy.utils.escape_identifier(kb_name)

                make_fcurve(
                    action,
                    slot,
                    coords,
                    data_path=data_path,
                    group_name="",
                    interpolation=animation.samplers[channel.sampler].interpolation,
                )

                # Expand weight range if needed
                kb = obj.data.shape_keys.key_blocks[kb_name]
                min_weight = min(coords[1:2])
                max_weight = max(coords[1:2])
                if min_weight < kb.slider_min:
                    kb.slider_min = min_weight
                if max_weight > kb.slider_max:
                    kb.slider_max = max_weight

        import_user_extensions('gather_import_animation_weight_after_hook', gltf, vnode, animation)
