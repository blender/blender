# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
from .vnode import VNode
from ..com.data_path import get_channelbag_for_slot

# Here data can be object, object.data, material, material.node_tree, camera, light


def simulate_stash(data, track_name, action, action_slot, start_frame=None):
    # Simulate stash :
    # * add a track
    # * add an action on track
    # * lock & mute the track
    if not data.animation_data:
        data.animation_data_create()
    tracks = data.animation_data.nla_tracks
    new_track = tracks.new(prev=None)
    new_track.name = track_name
    if start_frame is None:
        start_frame = bpy.context.scene.frame_start
    _strip = new_track.strips.new(action.name, start_frame, action)
    _strip.action_slot = action_slot
    _strip.action_frame_start = action.frame_range[0]
    _strip.action_frame_end = action.frame_range[1]
    new_track.lock = True
    new_track.mute = True


def restore_animation_on_object(data, anim_name):
    """ here, data can be an object, shapekeys, camera or light data """
    if not getattr(data, 'animation_data', None):
        return

    for track in data.animation_data.nla_tracks:
        if track.name != anim_name:
            continue
        if not track.strips:
            continue

        data.animation_data.action = track.strips[0].action
        data.animation_data.action_slot = track.strips[0].action_slot
        return

    if data.animation_data.action is not None:
        data.animation_data.action_slot = None
    data.animation_data.action = None


def make_fcurve(action, slot, co, data_path, index=0, group_name='', interpolation=None):
    channelbag = get_channelbag_for_slot(action, slot)
    try:
        fcurve = channelbag.fcurves.new(data_path=data_path, index=index)

        # Add the fcurve to the group
        if group_name:
            if group_name not in channelbag.groups:
                channelbag.groups.new(group_name)
            fcurve.group = channelbag.groups[group_name]
    except:
        # Some non valid files can have multiple target path
        return None

    fcurve.keyframe_points.add(len(co) // 2)
    fcurve.keyframe_points.foreach_set('co', co)

    # Setting interpolation
    ipo = {
        'CUBICSPLINE': 'BEZIER',
        'LINEAR': 'LINEAR',
        'STEP': 'CONSTANT',
    }[interpolation or 'LINEAR']
    ipo = bpy.types.Keyframe.bl_rna.properties['interpolation'].enum_items[ipo].value
    fcurve.keyframe_points.foreach_set('interpolation', [ipo] * len(fcurve.keyframe_points))

    # For CUBICSPLINE, also set the handle types to AUTO
    if interpolation == 'CUBICSPLINE':
        ty = bpy.types.Keyframe.bl_rna.properties['handle_left_type'].enum_items['AUTO'].value
        fcurve.keyframe_points.foreach_set('handle_left_type', [ty] * len(fcurve.keyframe_points))
        fcurve.keyframe_points.foreach_set('handle_right_type', [ty] * len(fcurve.keyframe_points))

    fcurve.update()  # force updating tangents (this may change when tangent will be managed)

    return fcurve

# This is use for TRS & weights animations
# For pointers, see the same function in animation_pointer.py


def get_or_create_action_and_slot(gltf, vnode_idx, anim_idx, path):
    animation = gltf.data.animations[anim_idx]
    vnode = gltf.vnodes[vnode_idx]

    if vnode.type == VNode.Bone:
        # For bones, the action goes on the armature.
        vnode = gltf.vnodes[vnode.bone_arma]

    obj = vnode.blender_object

    use_id = __get_id_from_path(path)

    objects = gltf.action_cache.get(anim_idx)
    if not objects:
        # Nothing exists yet for this glTF animation
        gltf.action_cache[anim_idx] = {}

        # So create a new action
        action = bpy.data.actions.new(animation.track_name)
        action.layers.new('layer0')
        action.layers[0].strips.new(type='KEYFRAME')

        gltf.action_cache[anim_idx]['action'] = action
        gltf.action_cache[anim_idx]['object_slots'] = {}

    # We now have an action for the animation, check if we have slots for this object
    slots = gltf.action_cache[anim_idx]['object_slots'].get(obj.name)
    if not slots:

        # We have no slots, create one

        action = gltf.action_cache[anim_idx]['action']
        if use_id == "OBJECT":
            slot = action.slots.new(obj.id_type, obj.name)
            gltf.needs_stash.append((obj, action, slot))
        elif use_id == "KEY":
            slot = action.slots.new(obj.data.shape_keys.id_type, obj.name)
            # Do not change the display name of the shape key slot
            # It helps to automatically assign the right slot, and it will get range correctly without setting it by hand
            gltf.needs_stash.append((obj.data.shape_keys, action, slot))
        else:
            pass  # This should not happen, as we only support TRS and weights animations here
            # animation pointer is managed in another place

        action.layers[0].strips[0].channelbags.new(slot)

        gltf.action_cache[anim_idx]['object_slots'][obj.name] = {}
        gltf.action_cache[anim_idx]['object_slots'][obj.name][slot.target_id_type] = (action, slot)
    else:
        # We have slots, check if we have the right slot (based on target_id_type)
        ac_sl = slots.get(use_id)
        if not ac_sl:
            action = gltf.action_cache[anim_idx]['action']
            if use_id == "OBJECT":
                slot = action.slots.new(obj.id_type, obj.name)
                gltf.needs_stash.append((obj, action, slot))
            elif use_id == "KEY":
                slot = action.slots.new(obj.data.shape_keys.id_type, obj.name)
                # Do not change the display name of the shape key slot
                # It helps to automatically assign the right slot, and it will get range correctly without setting it by hand
                gltf.needs_stash.append((obj.data.shape_keys, action, slot))
            else:
                pass  # This should not happen, as we only support TRS and weights animations here
                # animation pointer is managed in another place

            action.layers[0].strips[0].channelbags.new(slot)

            gltf.action_cache[anim_idx]['object_slots'][obj.name][slot.target_id_type] = (action, slot)
        else:
            action, slot = ac_sl

    # We now have action and slot, we can return the right slot
    return action, slot


def __get_id_from_path(path):
    if path in ["translation", "rotation", "scale"]:
        return "OBJECT"
    elif path == "weights":
        return "KEY"
    else:
        pass  # This should not happen, as we only support TRS and weights animations here
        # animation pointer is managed in another place
