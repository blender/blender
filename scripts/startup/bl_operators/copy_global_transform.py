# SPDX-FileCopyrightText: 2021-2025 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Copy Global Transform

Simple operators for copying world-space transforms.

It's called "global" to avoid confusion with the Blender World data-block.
"""

import abc
from typing import Iterable, Optional, Any, TypeAlias

import bpy
from bpy.types import (
    Context, Object, Operator, PoseBone,
    Camera, ID, ActionChannelbag,
)
from mathutils import Matrix


_axis_enum_items = [
    ("x", "X", "", 1),
    ("y", "Y", "", 2),
    ("z", "Z", "", 3),
]

# Mapping from frame number to the dominant (in terms of genetics) key type.
# GENERATED is the only recessive key type, others are dominant.
KeyInfo: TypeAlias = dict[float, str]


def get_matrix(context: Context) -> Matrix:
    bone = context.active_pose_bone
    if bone:
        # Convert matrix to world space
        arm = context.active_object
        mat = arm.matrix_world @ bone.matrix
    else:
        mat = context.active_object.matrix_world

    return mat


def set_matrix(context: Context, mat: Matrix) -> None:
    from bpy_extras.anim_utils import AutoKeying
    bone = context.active_pose_bone
    if bone:
        # Convert matrix to local space
        arm_eval = context.active_object.evaluated_get(context.view_layer.depsgraph)
        bone.matrix = arm_eval.matrix_world.inverted() @ mat
        AutoKeying.autokey_transformation(context, bone)
    else:
        context.active_object.matrix_world = mat
        AutoKeying.autokey_transformation(context, context.active_object)


def _channelbag_for_id(animated_id: ID) -> ActionChannelbag | None:
    # This is on purpose limited to the first layer and strip. To support more
    # than 1 layer, a rewrite of the caller is needed.

    adt = animated_id.animation_data
    action = adt and adt.action
    if action is None:
        return None

    slot = adt.action_slot

    for layer in action.layers:
        for strip in layer.strips:
            assert strip.type == 'KEYFRAME'
            channelbag = strip.channelbag(slot)
            return channelbag

    return None


def _selected_keyframes(context: Context) -> list[float]:
    """Return the list of frame numbers that have a selected key.

    Only keys on the active bone/object are considered.
    """

    bone = context.active_pose_bone
    if bone:
        return _selected_keyframes_for_bone(context.active_object, bone)
    return _selected_keyframes_for_object(context.active_object)


def _selected_keyframes_for_bone(object: Object, bone: PoseBone) -> list[float]:
    """Return the list of frame numbers that have a selected key.

    Only keys on the given pose bone are considered.
    """
    name = bpy.utils.escape_identifier(bone.name)
    return _selected_keyframes_for_action_slot(object, "pose.bones[\"{:s}\"].".format(name))


def _selected_keyframes_for_object(object: Object) -> list[float]:
    """Return the list of frame numbers that have a selected key.

    Only keys on the given object are considered.
    """
    return _selected_keyframes_for_action_slot(object, "")


def _selected_keyframes_for_action_slot(object: Object, rna_path_prefix: str) -> list[float]:
    """Return the list of frame numbers that have a selected key.

    Only keys on the given object's Action Slot on FCurves starting with rna_path_prefix are considered.
    """

    cbag = _channelbag_for_id(object)
    if not cbag:
        return []

    keyframes = set()
    for fcurve in cbag.fcurves:
        if not fcurve.data_path.startswith(rna_path_prefix):
            continue

        for kp in fcurve.keyframe_points:
            if not kp.select_control_point:
                continue
            keyframes.add(kp.co.x)
    return sorted(keyframes)


def _copy_matrix_to_clipboard(window_manager: bpy.types.WindowManager, matrix: Matrix) -> None:
    rows = ["    {!r},".format(tuple(row)) for row in matrix]
    as_string = "\n".join(rows)
    window_manager.clipboard = "Matrix((\n{:s}\n))".format(as_string)


class OBJECT_OT_copy_global_transform(Operator):
    bl_idname = "object.copy_global_transform"
    bl_label = "Copy Global Transform"
    bl_description = (
        "Copies the matrix of the currently active object or pose bone to the clipboard. Uses world-space matrices"
    )
    # This operator cannot be un-done because it manipulates data outside Blender.
    bl_options = {'REGISTER'}

    @classmethod
    def poll(cls, context: Context) -> bool:
        return bool(context.active_pose_bone) or bool(context.active_object)

    def execute(self, context: Context) -> set[str]:
        mat = get_matrix(context)
        _copy_matrix_to_clipboard(context.window_manager, mat)
        return {'FINISHED'}


def get_relative_ob(context: Context) -> Optional[Object]:
    """Get the 'relative' object.

    This is the object that's configured, or if that's empty, the active scene camera.
    """
    rel_ob = context.scene.tool_settings.anim_relative_object
    return rel_ob or context.scene.camera


class OBJECT_OT_copy_relative_transform(Operator):
    bl_idname = "object.copy_relative_transform"
    bl_label = "Copy Relative Transform"
    bl_description = "Copies the matrix of the currently active object or pose bone to the clipboard. " \
        "Uses matrices relative to a specific object or the active scene camera"
    # This operator cannot be un-done because it manipulates data outside Blender.
    bl_options = {'REGISTER'}

    @classmethod
    def poll(cls, context: Context) -> bool:
        rel_ob = get_relative_ob(context)
        if not rel_ob:
            return False
        return bool(context.active_pose_bone) or bool(context.active_object)

    def execute(self, context: Context) -> set[str]:
        rel_ob = get_relative_ob(context)
        if not rel_ob:
            self.report(
                {'ERROR'},
                "No 'Relative To' object found, set one explicitly or make sure there is an active object")
            return {'CANCELLED'}
        mat = rel_ob.matrix_world.inverted() @ get_matrix(context)
        _copy_matrix_to_clipboard(context.window_manager, mat)
        return {'FINISHED'}


class UnableToMirrorError(Exception):
    """Raised when mirroring is enabled but no mirror object/bone is set."""


class OBJECT_OT_paste_transform(Operator):
    bl_idname = "object.paste_transform"
    bl_label = "Paste Global Transform"
    bl_description = (
        "Pastes the matrix from the clipboard to the currently active pose bone or object. Uses world-space matrices"
    )
    bl_options = {'REGISTER', 'UNDO'}

    _method_items = [
        (
            'CURRENT',
            "Current Transform",
            "Paste onto the current values only, only manipulating the animation data if auto-keying is enabled",
        ),
        (
            'EXISTING_KEYS',
            "Selected Keys",
            "Paste onto frames that have a selected key, potentially creating new keys on those frames",
        ),
        (
            'BAKE',
            "Bake on Key Range",
            "Paste onto all frames between the first and last selected key, creating new keyframes if necessary",
        ),
    ]
    method: bpy.props.EnumProperty(  # type: ignore
        items=_method_items,
        name="Paste Method",
        description="Update the current transform, selected keyframes, or even create new keys",
        options={'SKIP_SAVE'},
    )
    bake_step: bpy.props.IntProperty(  # type: ignore
        name="Frame Step",
        description="Only used for baking. Step=1 creates a key on every frame, step=2 bakes on 2s, etc",
        min=1,
        soft_min=1,
        soft_max=5,
        options={'SKIP_SAVE'},
    )

    use_mirror: bpy.props.BoolProperty(  # type: ignore
        name="Mirror Transform",
        description="When pasting, mirror the transform relative to a specific object or bone",
        default=False,
        options={'SKIP_SAVE'},
    )

    mirror_axis_loc: bpy.props.EnumProperty(  # type: ignore
        items=_axis_enum_items,
        name="Location Axis",
        description="Coordinate axis used to mirror the location part of the transform",
        default='x',
        options={'SKIP_SAVE'},
    )
    mirror_axis_rot: bpy.props.EnumProperty(  # type: ignore
        items=_axis_enum_items,
        name="Rotation Axis",
        description="Coordinate axis used to mirror the rotation part of the transform",
        default='z',
        options={'SKIP_SAVE'},
    )

    use_relative: bpy.props.BoolProperty(  # type: ignore
        name="Use Relative Paste",
        description="When pasting, assume the pasted matrix is relative to another object (set in the user interface)",
        default=False,
        options={'SKIP_SAVE'},
    )

    @classmethod
    def poll(cls, context: Context) -> bool:
        if not context.active_pose_bone and not context.active_object:
            cls.poll_message_set("Select an object or pose bone")
            return False
        return True

    @classmethod
    def string_to_matrix(cls, value: str) -> Matrix | None:
        if value.startswith("Matrix"):
            return cls.parse_matrix(value)
        if value.startswith("<Matrix 4x4"):
            return cls.parse_repr_m4(value[12:-1])
        if value:
            return cls.parse_print_m4(value)
        return None

    @staticmethod
    def parse_matrix(value: str) -> Matrix | None:
        import ast
        try:
            return Matrix(ast.literal_eval(value[6:]))
        except Exception:
            # ast.literal_eval() can raise a slew of exceptions, all of
            # which means that it's not a matrix on the clipboard.
            return None

    @staticmethod
    def parse_print_m4(value: str) -> Optional[Matrix]:
        """Parse output from Blender's print_m4() function.

        Expects four lines of space-separated floats.
        """

        lines = value.strip().splitlines()
        if len(lines) != 4:
            return None

        try:
            floats = tuple(tuple(float(item) for item in line.split()) for line in lines)
        except ValueError:
            # Apprently not the expected format.
            return None
        return Matrix(floats)

    @staticmethod
    def parse_repr_m4(value: str) -> Optional[Matrix]:
        """Four lines of (a, b, c, d) floats."""

        lines = value.strip().splitlines()
        if len(lines) != 4:
            return None

        try:
            floats = tuple(tuple(float(item.strip()) for item in line.strip()[1:-1].split(',')) for line in lines)
        except ValueError:
            # Apprently not the expected format.
            return None
        return Matrix(floats)

    def execute(self, context: Context) -> set[str]:
        clipboard = context.window_manager.clipboard.strip()

        mat = self.string_to_matrix(clipboard)
        if mat is None:
            self.report({'ERROR'}, "Clipboard does not contain a matrix")
            return {'CANCELLED'}

        try:
            mat = self._preprocess_matrix(context, mat)
        except UnableToMirrorError:
            self.report({'ERROR'}, "Unable to mirror, no mirror object/bone configured")
            return {'CANCELLED'}

        applicator = {
            'CURRENT': self._paste_current,
            'EXISTING_KEYS': self._paste_existing_keys,
            'BAKE': self._paste_bake,
        }[self.method]
        return applicator(context, mat)

    def _preprocess_matrix(self, context: Context, matrix: Matrix) -> Matrix:
        if self.use_relative:
            matrix = self._relative_to_world(context, matrix)

        if self.use_mirror:
            matrix = self._mirror_matrix(context, matrix)
        return matrix

    def _relative_to_world(self, context: Context, matrix: Matrix) -> Matrix:
        rel_ob = get_relative_ob(context)
        if not rel_ob:
            return matrix

        rel_ob_eval = rel_ob.evaluated_get(context.view_layer.depsgraph)
        return rel_ob_eval.matrix_world @ matrix

    def _mirror_matrix(self, context: Context, matrix: Matrix) -> Matrix:
        mirror_ob = context.scene.tool_settings.anim_mirror_object
        mirror_bone = context.scene.tool_settings.anim_mirror_bone

        # No mirror object means "current armature object".
        ctx_ob = context.object
        if not mirror_ob and mirror_bone and ctx_ob and ctx_ob.type == 'ARMATURE':
            mirror_ob = ctx_ob

        if not mirror_ob:
            raise UnableToMirrorError()

        if mirror_ob.type == 'ARMATURE' and mirror_bone:
            return self._mirror_over_bone(matrix, mirror_ob, mirror_bone)
        return self._mirror_over_ob(matrix, mirror_ob)

    def _mirror_over_ob(self, matrix: Matrix, mirror_ob: bpy.types.Object) -> Matrix:
        mirror_matrix = mirror_ob.matrix_world
        return self._mirror_over_matrix(matrix, mirror_matrix)

    def _mirror_over_bone(self, matrix: Matrix, mirror_ob: bpy.types.Object, mirror_bone_name: str) -> Matrix:
        bone = mirror_ob.pose.bones[mirror_bone_name]
        mirror_matrix = mirror_ob.matrix_world @ bone.matrix
        return self._mirror_over_matrix(matrix, mirror_matrix)

    def _mirror_over_matrix(self, matrix: Matrix, mirror_matrix: Matrix) -> Matrix:
        # Compute the matrix in the space of the mirror matrix:
        mat_local = mirror_matrix.inverted() @ matrix

        # Decompose the matrix, as we don't want to touch the scale. This
        # operator should only mirror the translation and rotation components.
        trans, rot_q, scale = mat_local.decompose()

        # Mirror the translation component:
        axis_index = ord(self.mirror_axis_loc) - ord('x')
        trans[axis_index] *= -1

        # Flip the rotation, and use a rotation order that applies the to-be-flipped axes first.
        match self.mirror_axis_rot:
            case 'x':
                rot_e = rot_q.to_euler('XYZ')
                rot_e.x *= -1  # Flip the requested rotation axis.
                rot_e.y *= -1  # Also flip the bone roll.
            case 'y':
                rot_e = rot_q.to_euler('YZX')
                rot_e.y *= -1  # Flip the requested rotation axis.
                rot_e.z *= -1  # Also flip another axis? Not sure how to handle this one.
            case 'z':
                rot_e = rot_q.to_euler('ZYX')
                rot_e.z *= -1  # Flip the requested rotation axis.
                rot_e.y *= -1  # Also flip the bone roll.

        # Recompose the local matrix:
        mat_local = Matrix.LocRotScale(trans, rot_e, scale)

        # Go back to world space:
        mirrored_world = mirror_matrix @ mat_local
        return mirrored_world

    @staticmethod
    def _paste_current(context: Context, matrix: Matrix) -> set[str]:
        set_matrix(context, matrix)
        return {'FINISHED'}

    def _paste_existing_keys(self, context: Context, matrix: Matrix) -> set[str]:
        if not context.scene.tool_settings.use_keyframe_insert_auto:
            self.report({'ERROR'}, "This mode requires auto-keying to work properly")
            return {'CANCELLED'}

        frame_numbers = _selected_keyframes(context)
        if not frame_numbers:
            self.report({'WARNING'}, "No selected frames found")
            return {'CANCELLED'}

        self._paste_on_frames(context, frame_numbers, matrix)
        return {'FINISHED'}

    def _paste_bake(self, context: Context, matrix: Matrix) -> set[str]:
        if not context.scene.tool_settings.use_keyframe_insert_auto:
            self.report({'ERROR'}, "This mode requires auto-keying to work properly")
            return {'CANCELLED'}

        bake_step = max(1, self.bake_step)
        # Put the clamped bake step back into RNA for the redo panel.
        self.bake_step = bake_step

        frame_start, frame_end = self._determine_bake_range(context)
        frame_range = range(round(frame_start), round(frame_end) + bake_step, bake_step)
        self._paste_on_frames(context, frame_range, matrix)
        return {'FINISHED'}

    def _determine_bake_range(self, context: Context) -> tuple[float, float]:
        frame_numbers = _selected_keyframes(context)
        if frame_numbers:
            # Note that these could be the same frame, if len(frame_numbers) == 1:
            return frame_numbers[0], frame_numbers[-1]

        if context.scene.use_preview_range:
            self.report({'INFO'}, "No selected keys, pasting over preview range")
            return context.scene.frame_preview_start, context.scene.frame_preview_end

        self.report({'INFO'}, "No selected keys, pasting over scene range")
        return context.scene.frame_start, context.scene.frame_end

    def _paste_on_frames(self, context: Context, frame_numbers: Iterable[float], matrix: Matrix) -> None:
        current_frame = context.scene.frame_current_final
        try:
            for frame in frame_numbers:
                context.scene.frame_set(int(frame), subframe=frame % 1.0)
                set_matrix(context, matrix)
        finally:
            context.scene.frame_set(int(current_frame), subframe=current_frame % 1.0)


class Transformable(metaclass=abc.ABCMeta):
    """Interface for a bone or an object."""

    def __init__(self) -> None:
        self._key_info_cache: Optional[KeyInfo] = None

    @abc.abstractmethod
    def matrix_world(self) -> Matrix:
        pass

    def set_matrix_world(self, context: Context, matrix: Matrix) -> None:
        """Set the world matrix, without autokeying."""
        self._set_matrix_world(context, matrix)

    def set_matrix_world_autokey(self, context: Context, matrix: Matrix) -> None:
        """Set the world matrix, and autokey the resulting transform."""
        self._set_matrix_world(context, matrix)
        self._autokey_matrix_world(context)

    @abc.abstractmethod
    def _set_matrix_world(self, context: Context, matrix: Matrix) -> None:
        pass

    @abc.abstractmethod
    def _autokey_matrix_world(self, context: Context) -> None:
        pass

    @abc.abstractmethod
    def _my_fcurves(self) -> Iterable[bpy.types.FCurve]:
        pass

    def key_info(self) -> KeyInfo:
        if self._key_info_cache is not None:
            return self._key_info_cache

        keyinfo: KeyInfo = {}
        for fcurve in self._my_fcurves():
            for kp in fcurve.keyframe_points:
                frame = kp.co.x
                if kp.type == 'GENERATED' and frame in keyinfo:
                    # Don't bother overwriting other key types.
                    continue
                keyinfo[frame] = kp.type

        self._key_info_cache = keyinfo
        return keyinfo

    def remove_keys_of_type(
            self,
            key_type: str,
            *,
            frame_start: float | int = float("-inf"),
            frame_end: float | int = float("inf")) -> None:
        self._key_info_cache = None

        for fcurve in self._my_fcurves():
            to_remove = [
                kp for kp in fcurve.keyframe_points if kp.type == key_type and (frame_start <= kp.co.x <= frame_end)
            ]
            for kp in reversed(to_remove):
                fcurve.keyframe_points.remove(kp, fast=True)
            fcurve.keyframe_points.handles_recalc()


class TransformableObject(Transformable):
    object: Object

    def __init__(self, object: Object) -> None:
        super().__init__()
        self.object = object

    def __str__(self) -> str:
        return "TransformableObject({:s})".format(self.object.name)

    def matrix_world(self) -> Matrix:
        return self.object.matrix_world

    def _set_matrix_world(self, _context: Context, matrix: Matrix) -> None:
        self.object.matrix_world = matrix

    def _autokey_matrix_world(self, context: Context) -> None:
        from bpy_extras.anim_utils import AutoKeying
        AutoKeying.autokey_transformation(context, self.object)

    def __hash__(self) -> int:
        return hash(self.object.as_pointer())

    def _my_fcurves(self) -> Iterable[bpy.types.FCurve]:
        cbag = _channelbag_for_id(self.object)
        if not cbag:
            return
        yield from cbag.fcurves


class TransformableBone(Transformable):
    arm_object: Object
    pose_bone: PoseBone

    def __init__(self, pose_bone: PoseBone) -> None:
        super().__init__()
        self.arm_object = pose_bone.id_data
        self.pose_bone = pose_bone

    def __str__(self) -> str:
        return "TransformableBone({:s}, bone={:s})".format(self.arm_object.name, self.pose_bone.name)

    def matrix_world(self) -> Matrix:
        mat = self.arm_object.matrix_world @ self.pose_bone.matrix
        return mat

    def _set_matrix_world(self, context: Context, matrix: Matrix) -> None:
        # Convert matrix to armature-local space
        arm_eval = self.arm_object.evaluated_get(context.view_layer.depsgraph)
        self.pose_bone.matrix = arm_eval.matrix_world.inverted() @ matrix

    def _autokey_matrix_world(self, context: Context) -> None:
        from bpy_extras.anim_utils import AutoKeying
        AutoKeying.autokey_transformation(context, self.pose_bone)

    def __hash__(self) -> int:
        return hash(self.pose_bone.as_pointer())

    def _my_fcurves(self) -> Iterable[bpy.types.FCurve]:
        cbag = _channelbag_for_id(self.arm_object)
        if not cbag:
            return

        rna_prefix = self.pose_bone.path_from_id() + "."
        for fcurve in cbag.fcurves:
            if fcurve.data_path.startswith(rna_prefix):
                yield fcurve


class FixToCameraCommon:
    """Common functionality for the Fix To Scene Camera operator + its 'delete' button."""

    keytype = 'GENERATED'

    # Operator method stubs to avoid PyLance/MyPy errors:
    @classmethod
    def poll_message_set(cls, message: str) -> None:
        super().poll_message_set(message)

    def report(self, level: set[str], message: str) -> None:
        super().report(level, message)

    # Implement in subclass:
    def _execute(self, context: Context, transformables: list[Transformable]) -> None:
        raise NotImplementedError()

    @classmethod
    def poll(cls, context: Context) -> bool:
        if not context.active_pose_bone and not context.active_object:
            cls.poll_message_set("Select an object or pose bone")
            return False
        if context.mode not in {'POSE', 'OBJECT'}:
            cls.poll_message_set("Switch to Pose or Object mode")
            return False
        if not context.scene.camera:
            cls.poll_message_set("The Scene needs a camera")
            return False
        return True

    def execute(self, context: Context) -> set[str]:
        match context.mode:
            case 'OBJECT':
                transformables = self._transformable_objects(context)
            case 'POSE':
                transformables = self._transformable_pbones(context)
            case mode:
                self.report({'ERROR'}, "Unsupported mode: {!r}".format(mode))
                return {'CANCELLED'}

        restore_frame = context.scene.frame_current
        restore_matrices = [(transformable, transformable.matrix_world().copy()) for transformable in transformables]

        try:
            self._execute(context, transformables)
        finally:
            # Restore the state of the scene & the transformables. This is necessary
            # as not all properties may have been auto-keyed (for example 'only
            # available' enabled, and rotation is not actually keyed yet), so we can't
            # assume that going to the original frame restores the entire matrix.
            context.scene.frame_set(restore_frame)
            for transformable, matrix in restore_matrices:
                transformable.set_matrix_world(context, matrix)

        return {'FINISHED'}

    def _transformable_objects(self, context: Context) -> list[Transformable]:
        return [TransformableObject(object=ob) for ob in context.selected_editable_objects]

    def _transformable_pbones(self, context: Context) -> list[Transformable]:
        return [TransformableBone(pose_bone=bone) for bone in context.selected_pose_bones]


class OBJECT_OT_fix_to_camera(FixToCameraCommon, Operator):
    bl_idname = "object.fix_to_camera"
    bl_label = "Fix to Scene Camera"
    bl_description = "Generate new keys to fix the selected object/bone to the camera on unkeyed frames"
    bl_options = {'REGISTER', 'UNDO'}

    use_location: bpy.props.BoolProperty(  # type: ignore
        name="Location",
        description="Create Location keys when fixing to the scene camera",
        default=True,
    )
    use_rotation: bpy.props.BoolProperty(  # type: ignore
        name="Rotation",
        description="Create Rotation keys when fixing to the scene camera",
        default=True,
    )
    use_scale: bpy.props.BoolProperty(  # type: ignore
        name="Scale",
        description="Create Scale keys when fixing to the scene camera",
        default=True,
    )

    def _get_matrices(self, camera: Camera, transformables: list[Transformable]) -> dict[Transformable, Matrix]:
        camera_mat_inv = camera.matrix_world.inverted()
        return {t: camera_mat_inv @ t.matrix_world() for t in transformables}

    def _execute(self, context: Context, transformables: list[Transformable]) -> None:
        from bpy_extras.anim_utils import AutoKeying
        from bpy_extras.wm_utils import progress_report

        depsgraph = context.view_layer.depsgraph
        scene = context.scene

        scene.frame_set(scene.frame_start)
        camera_eval = scene.camera.evaluated_get(depsgraph)
        last_camera_name = scene.camera.name
        matrices = self._get_matrices(camera_eval, transformables)

        if scene.use_preview_range:
            frame_start = scene.frame_preview_start
            frame_end = scene.frame_preview_end
        else:
            frame_start = scene.frame_start
            frame_end = scene.frame_end

        with (
            AutoKeying.options(
                keytype=self.keytype,
                use_loc=self.use_location,
                use_rot=self.use_rotation,
                use_scale=self.use_scale,
                force_autokey=True,
            ),
            progress_report.ProgressReport(context.window_manager) as progress,
        ):
            frames_to_visit = range(frame_start, frame_end + scene.frame_step, scene.frame_step)
            progress.enter_substeps(len(frames_to_visit))

            for frame in frames_to_visit:
                scene.frame_set(frame)
                progress.step()

                camera_eval = scene.camera.evaluated_get(depsgraph)
                cam_matrix_world = camera_eval.matrix_world
                camera_mat_inv = cam_matrix_world.inverted()

                if scene.camera.name != last_camera_name:
                    # The scene camera changed, so the previous
                    # relative-to-camera matrices can no longer be used.
                    matrices = self._get_matrices(camera_eval, transformables)
                    last_camera_name = scene.camera.name

                for t, camera_rel_matrix in matrices.items():
                    key_info = t.key_info()
                    key_type = key_info.get(frame, "")
                    if key_type not in {self.keytype, ""}:
                        # Manually set key, remember the current camera-relative matrix.
                        matrices[t] = camera_mat_inv @ t.matrix_world()
                        continue

                    # No key, or a generated one. Overwrite it with a new transform.
                    t.set_matrix_world_autokey(context, cam_matrix_world @ camera_rel_matrix)


class OBJECT_OT_delete_fix_to_camera_keys(Operator, FixToCameraCommon):
    bl_idname = "object.delete_fix_to_camera_keys"
    bl_label = "Delete Generated Keys"
    bl_description = "Delete all keys that were generated by the 'Fix to Scene Camera' operator"
    bl_options = {'REGISTER', 'UNDO'}

    def _execute(self, context: Context, transformables: list[Transformable]) -> None:
        scene = context.scene
        if scene.use_preview_range:
            frame_start = scene.frame_preview_start
            frame_end = scene.frame_preview_end
        else:
            frame_start = scene.frame_start
            frame_end = scene.frame_end

        for t in transformables:
            t.remove_keys_of_type(self.keytype, frame_start=frame_start, frame_end=frame_end)


# Messagebus subscription to monitor changes & refresh panels.
_msgbus_owner = object()


def _refresh_3d_panels():
    refresh_area_types = {'VIEW_3D'}
    for win in bpy.context.window_manager.windows:
        for area in win.screen.areas:
            if area.type not in refresh_area_types:
                continue
            area.tag_redraw()


classes = (
    OBJECT_OT_copy_global_transform,
    OBJECT_OT_copy_relative_transform,
    OBJECT_OT_paste_transform,
    OBJECT_OT_fix_to_camera,
    OBJECT_OT_delete_fix_to_camera_keys,
)


def _register_message_bus() -> None:
    bpy.msgbus.subscribe_rna(
        key=(bpy.types.ToolSettings, "use_keyframe_insert_auto"),
        owner=_msgbus_owner,
        args=(),
        notify=_refresh_3d_panels,
        options={'PERSISTENT'},
    )


def _unregister_message_bus() -> None:
    bpy.msgbus.clear_by_owner(_msgbus_owner)


@bpy.app.handlers.persistent  # type: ignore
def _on_blendfile_load_post(_none: Any, _other_none: Any) -> None:
    # The parameters are required, but both are None.
    _register_message_bus()


def register():
    bpy.app.handlers.load_post.append(_on_blendfile_load_post)


def unregister():
    _unregister_message_bus()
    bpy.app.handlers.load_post.remove(_on_blendfile_load_post)
