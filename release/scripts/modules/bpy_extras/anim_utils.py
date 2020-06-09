# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

__all__ = (
    "bake_action",
    "bake_action_objects",

    "bake_action_iter",
    "bake_action_objects_iter",
)

import bpy


def bake_action(
        obj,
        *,
        action, frames,
        **kwargs
):
    """
    :arg obj: Object to bake.
    :type obj: :class:`bpy.types.Object`
    :arg action: An action to bake the data into, or None for a new action
       to be created.
    :type action: :class:`bpy.types.Action` or None
    :arg frames: Frames to bake.
    :type frames: iterable of int

    :return: an action or None
    :rtype: :class:`bpy.types.Action`
    """
    if not (kwargs.get("do_pose") or kwargs.get("do_object")):
        return None

    action, = bake_action_objects(
        [(obj, action)],
        frames=frames,
        **kwargs,
    )
    return action


def bake_action_objects(
        object_action_pairs,
        *,
        frames,
        **kwargs
):
    """
    A version of :func:`bake_action_objects_iter` that takes frames and returns the output.

    :arg frames: Frames to bake.
    :type frames: iterable of int

    :return: A sequence of Action or None types (aligned with `object_action_pairs`)
    :rtype: sequence of :class:`bpy.types.Action`
    """
    iter = bake_action_objects_iter(object_action_pairs, **kwargs)
    iter.send(None)
    for frame in frames:
        iter.send(frame)
    return iter.send(None)


def bake_action_objects_iter(
        object_action_pairs,
        **kwargs
):
    """
    An coroutine that bakes actions for multiple objects.

    :arg object_action_pairs: Sequence of object action tuples,
       action is the destination for the baked data. When None a new action will be created.
    :type object_action_pairs: Sequence of (:class:`bpy.types.Object`, :class:`bpy.types.Action`)
    """
    scene = bpy.context.scene
    frame_back = scene.frame_current
    iter_all = tuple(
        bake_action_iter(obj, action=action, **kwargs)
        for (obj, action) in object_action_pairs
    )
    for iter in iter_all:
        iter.send(None)
    while True:
        frame = yield None
        if frame is None:
            break
        scene.frame_set(frame)
        bpy.context.view_layer.update()
        for iter in iter_all:
            iter.send(frame)
    scene.frame_set(frame_back)
    yield tuple(iter.send(None) for iter in iter_all)


# XXX visual keying is actually always considered as True in this code...
def bake_action_iter(
        obj,
        *,
        action,
        only_selected=False,
        do_pose=True,
        do_object=True,
        do_visual_keying=True,
        do_constraint_clear=False,
        do_parents_clear=False,
        do_clean=False
):
    """
    An coroutine that bakes action for a single object.

    :arg obj: Object to bake.
    :type obj: :class:`bpy.types.Object`
    :arg action: An action to bake the data into, or None for a new action
       to be created.
    :type action: :class:`bpy.types.Action` or None
    :arg only_selected: Only bake selected bones.
    :type only_selected: bool
    :arg do_pose: Bake pose channels.
    :type do_pose: bool
    :arg do_object: Bake objects.
    :type do_object: bool
    :arg do_visual_keying: Use the final transformations for baking ('visual keying')
    :type do_visual_keying: bool
    :arg do_constraint_clear: Remove constraints after baking.
    :type do_constraint_clear: bool
    :arg do_parents_clear: Unparent after baking objects.
    :type do_parents_clear: bool
    :arg do_clean: Remove redundant keyframes after baking.
    :type do_clean: bool

    :return: an action or None
    :rtype: :class:`bpy.types.Action`
    """
    # -------------------------------------------------------------------------
    # Helper Functions and vars

    # Note: BBONE_PROPS is a list so we can preserve the ordering
    BBONE_PROPS = [
        'bbone_curveinx', 'bbone_curveoutx',
        'bbone_curveiny', 'bbone_curveouty',
        'bbone_rollin', 'bbone_rollout',
        'bbone_scaleinx', 'bbone_scaleoutx',
        'bbone_scaleiny', 'bbone_scaleouty',
        'bbone_easein', 'bbone_easeout'
    ]

    def pose_frame_info(obj):
        matrix = {}
        bbones = {}
        for name, pbone in obj.pose.bones.items():
            if do_visual_keying:
                # Get the final transform of the bone in its own local space...
                matrix[name] = obj.convert_space(pose_bone=pbone, matrix=pbone.matrix,
                                                 from_space='POSE', to_space='LOCAL')
            else:
                matrix[name] = pbone.matrix_basis.copy()

            # Bendy Bones
            if pbone.bone.bbone_segments > 1:
                bbones[name] = {bb_prop: getattr(pbone, bb_prop) for bb_prop in BBONE_PROPS}
        return matrix, bbones

    if do_parents_clear:
        if do_visual_keying:
            def obj_frame_info(obj):
                return obj.matrix_world.copy()
        else:
            def obj_frame_info(obj):
                parent = obj.parent
                matrix = obj.matrix_basis
                if parent:
                    return parent.matrix_world @ matrix
                else:
                    return matrix.copy()
    else:
        if do_visual_keying:
            def obj_frame_info(obj):
                parent = obj.parent
                matrix = obj.matrix_world
                if parent:
                    return parent.matrix_world.inverted_safe() @ matrix
                else:
                    return matrix.copy()
        else:
            def obj_frame_info(obj):
                return obj.matrix_basis.copy()

    # -------------------------------------------------------------------------
    # Setup the Context

    if obj.pose is None:
        do_pose = False

    if not (do_pose or do_object):
        raise Exception("Pose and object baking is disabled, no action needed")

    pose_info = []
    obj_info = []

    # -------------------------------------------------------------------------
    # Collect transformations

    while True:
        # Caller is responsible for setting the frame and updating the scene.
        frame = yield None

        # Signal we're done!
        if frame is None:
            break

        if do_pose:
            pose_info.append((frame, *pose_frame_info(obj)))
        if do_object:
            obj_info.append((frame, obj_frame_info(obj)))

    # -------------------------------------------------------------------------
    # Clean (store initial data)
    if do_clean and action is not None:
        clean_orig_data = {fcu: {p.co[1] for p in fcu.keyframe_points} for fcu in action.fcurves}
    else:
        clean_orig_data = {}

    # -------------------------------------------------------------------------
    # Create action

    # in case animation data hasn't been created
    atd = obj.animation_data_create()
    if action is None:
        action = bpy.data.actions.new("Action")

    # Only leave tweak mode if we actually need to modify the action (T57159)
    if action != atd.action:
        # Leave tweak mode before trying to modify the action (T48397)
        if atd.use_tweak_mode:
            atd.use_tweak_mode = False

        atd.action = action

    # Baking the action only makes sense in Replace mode, so force it (T69105)
    if not atd.use_tweak_mode:
        atd.action_blend_type = 'REPLACE'

    # -------------------------------------------------------------------------
    # Apply transformations to action

    # pose
    if do_pose:
        for name, pbone in obj.pose.bones.items():
            if only_selected and not pbone.bone.select:
                continue

            if do_constraint_clear:
                while pbone.constraints:
                    pbone.constraints.remove(pbone.constraints[0])

            # Create compatible eulers, quats.
            euler_prev = None
            quat_prev = None

            for (f, matrix, bbones) in pose_info:
                pbone.matrix_basis = matrix[name].copy()

                pbone.keyframe_insert("location", index=-1, frame=f, group=name)

                rotation_mode = pbone.rotation_mode
                if rotation_mode == 'QUATERNION':
                    if quat_prev is not None:
                        quat = pbone.rotation_quaternion.copy()
                        quat.make_compatible(quat_prev)
                        pbone.rotation_quaternion = quat
                        quat_prev = quat
                        del quat
                    else:
                        quat_prev = pbone.rotation_quaternion.copy()
                    pbone.keyframe_insert("rotation_quaternion", index=-1, frame=f, group=name)
                elif rotation_mode == 'AXIS_ANGLE':
                    pbone.keyframe_insert("rotation_axis_angle", index=-1, frame=f, group=name)
                else:  # euler, XYZ, ZXY etc
                    if euler_prev is not None:
                        euler = pbone.rotation_euler.copy()
                        euler.make_compatible(euler_prev)
                        pbone.rotation_euler = euler
                        euler_prev = euler
                        del euler
                    else:
                        euler_prev = pbone.rotation_euler.copy()
                    pbone.keyframe_insert("rotation_euler", index=-1, frame=f, group=name)

                pbone.keyframe_insert("scale", index=-1, frame=f, group=name)

                # Bendy Bones
                if pbone.bone.bbone_segments > 1:
                    bbone_shape = bbones[name]
                    for bb_prop in BBONE_PROPS:
                        # update this property with value from bbone_shape, then key it
                        setattr(pbone, bb_prop, bbone_shape[bb_prop])
                        pbone.keyframe_insert(bb_prop, index=-1, frame=f, group=name)

    # object. TODO. multiple objects
    if do_object:
        if do_constraint_clear:
            while obj.constraints:
                obj.constraints.remove(obj.constraints[0])

        # Create compatible eulers, quats.
        euler_prev = None
        quat_prev = None

        for (f, matrix) in obj_info:
            name = "Action Bake"  # XXX: placeholder
            obj.matrix_basis = matrix

            obj.keyframe_insert("location", index=-1, frame=f, group=name)

            rotation_mode = obj.rotation_mode
            if rotation_mode == 'QUATERNION':
                if quat_prev is not None:
                    quat = obj.rotation_quaternion.copy()
                    quat.make_compatible(quat_prev)
                    obj.rotation_quaternion = quat
                    quat_prev = quat
                    del quat
                else:
                    quat_prev = obj.rotation_quaternion.copy()
                obj.keyframe_insert("rotation_quaternion", index=-1, frame=f, group=name)
            elif rotation_mode == 'AXIS_ANGLE':
                obj.keyframe_insert("rotation_axis_angle", index=-1, frame=f, group=name)
            else:  # euler, XYZ, ZXY etc
                if euler_prev is not None:
                    euler = obj.rotation_euler.copy()
                    euler.make_compatible(euler_prev)
                    obj.rotation_euler = euler
                    euler_prev = euler
                    del euler
                else:
                    euler_prev = obj.rotation_euler.copy()
                obj.keyframe_insert("rotation_euler", index=-1, frame=f, group=name)

            obj.keyframe_insert("scale", index=-1, frame=f, group=name)

        if do_parents_clear:
            obj.parent = None

    # -------------------------------------------------------------------------
    # Clean

    if do_clean:
        for fcu in action.fcurves:
            fcu_orig_data = clean_orig_data.get(fcu, set())

            keyframe_points = fcu.keyframe_points
            i = 1
            while i < len(keyframe_points) - 1:
                val = keyframe_points[i].co[1]

                if val in fcu_orig_data:
                    i += 1
                    continue

                val_prev = keyframe_points[i - 1].co[1]
                val_next = keyframe_points[i + 1].co[1]

                if abs(val - val_prev) + abs(val - val_next) < 0.0001:
                    keyframe_points.remove(keyframe_points[i])
                else:
                    i += 1

    yield action
