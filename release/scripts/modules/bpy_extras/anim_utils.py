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
    )

import bpy


# XXX visual keying is actually always considered as True in this code...
def bake_action(frame_start,
                frame_end,
                frame_step=1,
                only_selected=False,
                do_pose=True,
                do_object=True,
                do_visual_keying=True,
                do_constraint_clear=False,
                do_parents_clear=False,
                do_clean=False,
                action=None,
                ):

    """
    Return an image from the file path with options to search multiple paths
    and return a placeholder if its not found.

    :arg frame_start: First frame to bake.
    :type frame_start: int
    :arg frame_end: Last frame to bake.
    :type frame_end: int
    :arg frame_step: Frame step.
    :type frame_step: int
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
    :arg action: An action to bake the data into, or None for a new action
       to be created.
    :type action: :class:`bpy.types.Action` or None

    :return: an action or None
    :rtype: :class:`bpy.types.Action`
    """

    # -------------------------------------------------------------------------
    # Helper Functions and vars

    def pose_frame_info(obj):
        matrix = {}
        for name, pbone in obj.pose.bones.items():
            if do_visual_keying:
                # Get the final transform of the bone in its own local space...
                matrix[name] = obj.convert_space(pbone, pbone.matrix, 'POSE', 'LOCAL')
            else:
                matrix[name] = pbone.matrix_basis.copy()
        return matrix

    if do_parents_clear:
        if do_visual_keying:
            def obj_frame_info(obj):
                return obj.matrix_world.copy()
        else:
            def obj_frame_info(obj):
                parent = obj.parent
                matrix = obj.matrix_basis
                if parent:
                    return parent.matrix_world * matrix
                else:
                    return matrix.copy()
    else:
        if do_visual_keying:
            def obj_frame_info(obj):
                parent = obj.parent
                matrix = obj.matrix_world
                if parent:
                    return parent.matrix_world.inverted_safe() * matrix
                else:
                    return matrix.copy()
        else:
            def obj_frame_info(obj):
                return obj.matrix_basis.copy()

    # -------------------------------------------------------------------------
    # Setup the Context

    # TODO, pass data rather then grabbing from the context!
    scene = bpy.context.scene
    obj = bpy.context.object
    frame_back = scene.frame_current

    if obj.pose is None:
        do_pose = False

    if not (do_pose or do_object):
        return None

    pose_info = []
    obj_info = []

    options = {'INSERTKEY_NEEDED'}

    frame_range = range(frame_start, frame_end + 1, frame_step)

    # -------------------------------------------------------------------------
    # Collect transformations

    for f in frame_range:
        scene.frame_set(f)
        scene.update()
        if do_pose:
            pose_info.append(pose_frame_info(obj))
        if do_object:
            obj_info.append(obj_frame_info(obj))

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

    # Leave tweak mode before trying to modify the action (T48397)
    if atd.use_tweak_mode:
        atd.use_tweak_mode = False

    atd.action = action

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

            # create compatible eulers
            euler_prev = None

            for (f, matrix) in zip(frame_range, pose_info):
                pbone.matrix_basis = matrix[name].copy()

                pbone.keyframe_insert("location", -1, f, name, options)

                rotation_mode = pbone.rotation_mode
                if rotation_mode == 'QUATERNION':
                    pbone.keyframe_insert("rotation_quaternion", -1, f, name, options)
                elif rotation_mode == 'AXIS_ANGLE':
                    pbone.keyframe_insert("rotation_axis_angle", -1, f, name, options)
                else:  # euler, XYZ, ZXY etc
                    if euler_prev is not None:
                        euler = pbone.rotation_euler.copy()
                        euler.make_compatible(euler_prev)
                        pbone.rotation_euler = euler
                        euler_prev = euler
                        del euler
                    else:
                        euler_prev = pbone.rotation_euler.copy()
                    pbone.keyframe_insert("rotation_euler", -1, f, name, options)

                pbone.keyframe_insert("scale", -1, f, name, options)

    # object. TODO. multiple objects
    if do_object:
        if do_constraint_clear:
            while obj.constraints:
                obj.constraints.remove(obj.constraints[0])

        # create compatible eulers
        euler_prev = None

        for (f, matrix) in zip(frame_range, obj_info):
            name = "Action Bake"  # XXX: placeholder
            obj.matrix_basis = matrix

            obj.keyframe_insert("location", -1, f, name, options)

            rotation_mode = obj.rotation_mode
            if rotation_mode == 'QUATERNION':
                obj.keyframe_insert("rotation_quaternion", -1, f, name, options)
            elif rotation_mode == 'AXIS_ANGLE':
                obj.keyframe_insert("rotation_axis_angle", -1, f, name, options)
            else:  # euler, XYZ, ZXY etc
                if euler_prev is not None:
                    euler = obj.rotation_euler.copy()
                    euler.make_compatible(euler_prev)
                    obj.rotation_euler = euler
                    euler_prev = euler
                    del euler
                else:
                    euler_prev = obj.rotation_euler.copy()
                obj.keyframe_insert("rotation_euler", -1, f, name, options)

            obj.keyframe_insert("scale", -1, f, name, options)

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

    scene.frame_set(frame_back)

    return action
