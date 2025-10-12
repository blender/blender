# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy

from bpy.types import (
    Operator,
    PropertyGroup,
)
from bpy.props import (
    StringProperty,
    IntProperty,
    EnumProperty,
    BoolProperty,
    CollectionProperty,
)

# Note: The UI classes are stored in bl_ui/properties_data_armature.py

# Data Structure ##############################################################

# Note: bones are stored by name, this means that if the bone is renamed,
# there can be problems. However, bone renaming is unlikely during animation.


class SelectionEntry(PropertyGroup):
    __slots__ = ()

    name: StringProperty(name="Bone Name", override={'LIBRARY_OVERRIDABLE'})


class SelectionSet(PropertyGroup):
    __slots__ = ()

    name: StringProperty(name="Set Name", override={'LIBRARY_OVERRIDABLE'})
    bone_ids: CollectionProperty(
        type=SelectionEntry,
        override={'LIBRARY_OVERRIDABLE', 'USE_INSERTION'}
    )
    is_selected: BoolProperty(
        name="Include this selection set when copying to the clipboard. "
        "If none are specified, all sets will be copied.",
        override={'LIBRARY_OVERRIDABLE'},
    )


# Operators ##############################################################

class _PoseModeOnlyMixin:
    """Operator only available for objects of type armature in pose mode."""
    @classmethod
    def poll(cls, context):
        return (
            context.object and
            context.object.type == 'ARMATURE' and
            context.mode == 'POSE'
        )


class _NeedSelSetMixin(_PoseModeOnlyMixin):
    """Operator only available if the armature has a selected selection set."""
    @classmethod
    def poll(cls, context):
        if not super().poll(context):
            return False
        arm = context.object
        return 0 <= arm.active_selection_set < len(arm.selection_sets)


class POSE_OT_selection_set_delete_all(_PoseModeOnlyMixin, Operator):
    bl_idname = "pose.selection_set_delete_all"
    bl_label = "Delete All Sets"
    bl_description = "Remove all Selection Sets from this Armature"
    bl_options = {'UNDO', 'REGISTER'}

    def execute(self, context):
        arm = context.object
        arm.selection_sets.clear()
        return {'FINISHED'}


class POSE_OT_selection_set_remove_bones(_PoseModeOnlyMixin, Operator):
    bl_idname = "pose.selection_set_remove_bones"
    bl_label = "Remove Selected Bones from All Sets"
    bl_description = "Remove the selected bones from all Selection Sets"
    bl_options = {'UNDO', 'REGISTER'}

    def execute(self, context):
        arm = context.object

        # Iterate only the selected bones in current pose that are not hidden.
        for bone in context.selected_pose_bones:
            for selset in arm.selection_sets:
                if bone.name in selset.bone_ids:
                    idx = selset.bone_ids.find(bone.name)
                    selset.bone_ids.remove(idx)

        return {'FINISHED'}


class POSE_OT_selection_set_move(_NeedSelSetMixin, Operator):
    bl_idname = "pose.selection_set_move"
    bl_label = "Move Selection Set in List"
    bl_description = "Move the active Selection Set up/down the list of sets"
    bl_options = {'UNDO', 'REGISTER'}

    direction: EnumProperty(
        name="Move Direction",
        description="Direction to move the active Selection Set: UP (default) or DOWN",
        items=[
            ('UP', "Up", "", -1),
            ('DOWN', "Down", "", 1),
        ],
        default='UP',
        options={'HIDDEN'},
    )

    @classmethod
    def poll(cls, context):
        if not super().poll(context):
            return False
        arm = context.object
        return len(arm.selection_sets) > 1

    def execute(self, context):
        arm = context.object

        active_idx = arm.active_selection_set
        new_idx = active_idx + (-1 if self.direction == 'UP' else 1)

        if new_idx < 0 or new_idx >= len(arm.selection_sets):
            return {'FINISHED'}

        arm.selection_sets.move(active_idx, new_idx)
        arm.active_selection_set = new_idx

        return {'FINISHED'}


class POSE_OT_selection_set_add(_PoseModeOnlyMixin, Operator):
    bl_idname = "pose.selection_set_add"
    bl_label = "Create Selection Set"
    bl_description = "Create a new empty Selection Set"
    bl_options = {'UNDO', 'REGISTER'}

    def execute(self, context):
        arm = context.object
        sel_sets = arm.selection_sets
        new_sel_set = sel_sets.add()
        new_sel_set.name = _uniqify("SelectionSet", sel_sets.keys())

        # Select newly created set.
        arm.active_selection_set = len(sel_sets) - 1

        return {'FINISHED'}


class POSE_OT_selection_set_remove(_NeedSelSetMixin, Operator):
    bl_idname = "pose.selection_set_remove"
    bl_label = "Delete Selection Set"
    bl_description = "Remove a Selection Set from this Armature"
    bl_options = {'UNDO', 'REGISTER'}

    def execute(self, context):
        arm = context.object

        arm.selection_sets.remove(arm.active_selection_set)

        # Change currently active selection set.
        numsets = len(arm.selection_sets)
        if (arm.active_selection_set > (numsets - 1) and numsets > 0):
            arm.active_selection_set = len(arm.selection_sets) - 1

        return {'FINISHED'}


class POSE_OT_selection_set_assign(_PoseModeOnlyMixin, Operator):
    bl_idname = "pose.selection_set_assign"
    bl_label = "Add Bones to Selection Set"
    bl_description = "Add selected bones to Selection Set"
    bl_options = {'UNDO', 'REGISTER'}

    def invoke(self, context, _event):
        arm = context.object

        if not (arm.active_selection_set < len(arm.selection_sets)):
            bpy.ops.wm.call_menu("INVOKE_DEFAULT", name="POSE_MT_selection_set_create")
        else:
            bpy.ops.pose.selection_set_assign('EXEC_DEFAULT')

        return {'FINISHED'}

    def execute(self, context):
        arm = context.object
        act_sel_set = arm.selection_sets[arm.active_selection_set]

        # Iterate only the selected bones in current pose that are not hidden.
        for bone in context.selected_pose_bones:
            if bone.name not in act_sel_set.bone_ids:
                bone_id = act_sel_set.bone_ids.add()
                bone_id.name = bone.name

        return {'FINISHED'}


class POSE_OT_selection_set_unassign(_NeedSelSetMixin, Operator):
    bl_idname = "pose.selection_set_unassign"
    bl_label = "Remove Bones from Selection Set"
    bl_description = "Remove selected bones from Selection Set"
    bl_options = {'UNDO', 'REGISTER'}

    def execute(self, context):
        arm = context.object
        act_sel_set = arm.selection_sets[arm.active_selection_set]

        # Iterate only the selected bones in current pose that are not hidden.
        for bone in context.selected_pose_bones:
            if bone.name in act_sel_set.bone_ids:
                idx = act_sel_set.bone_ids.find(bone.name)
                act_sel_set.bone_ids.remove(idx)

        return {'FINISHED'}


class POSE_OT_selection_set_select(_NeedSelSetMixin, Operator):
    bl_idname = "pose.selection_set_select"
    bl_label = "Select Selection Set"
    bl_description = "Select the bones from this Selection Set"
    bl_options = {'UNDO', 'REGISTER'}

    selection_set_index: IntProperty(
        name="Selection Set Index",
        default=-1,
        description="Which Selection Set to select; -1 uses the active Selection Set",
        options={'HIDDEN'},
    )

    def execute(self, context):
        arm = context.object

        if self.selection_set_index == -1:
            idx = arm.active_selection_set
        else:
            idx = self.selection_set_index
        sel_set = arm.selection_sets[idx]

        for bone in context.visible_pose_bones:
            if bone.name in sel_set.bone_ids:
                bone.select = True

        return {'FINISHED'}


class POSE_OT_selection_set_deselect(_NeedSelSetMixin, Operator):
    bl_idname = "pose.selection_set_deselect"
    bl_label = "Deselect Selection Set"
    bl_description = "Remove Selection Set bones from current selection"
    bl_options = {'UNDO', 'REGISTER'}

    def execute(self, context):
        arm = context.object
        act_sel_set = arm.selection_sets[arm.active_selection_set]

        for bone in context.selected_pose_bones:
            if bone.name in act_sel_set.bone_ids:
                bone.select = False

        return {'FINISHED'}


class POSE_OT_selection_set_add_and_assign(_PoseModeOnlyMixin, Operator):
    bl_idname = "pose.selection_set_add_and_assign"
    bl_label = "Create and Add Bones to Selection Set"
    bl_description = "Create a new Selection Set with the currently selected bones"
    bl_options = {'UNDO', 'REGISTER'}

    def execute(self, context):
        bpy.ops.pose.selection_set_add('EXEC_DEFAULT')
        bpy.ops.pose.selection_set_assign('EXEC_DEFAULT')
        return {'FINISHED'}


class POSE_OT_selection_set_copy(_NeedSelSetMixin, Operator):
    bl_idname = "pose.selection_set_copy"
    bl_label = "Copy Selection Set(s)"
    bl_description = "Copy the selected Selection Set(s) to the clipboard"
    bl_options = {'UNDO', 'REGISTER'}

    def execute(self, context):
        context.window_manager.clipboard = _to_json(context)
        self.report({'INFO'}, "Copied Selection Set(s) to clipboard")
        return {'FINISHED'}


class POSE_OT_selection_set_paste(_PoseModeOnlyMixin, Operator):
    bl_idname = "pose.selection_set_paste"
    bl_label = "Paste Selection Set(s)"
    bl_description = "Add new Selection Set(s) from the clipboard"
    bl_options = {'UNDO', 'REGISTER'}

    def execute(self, context):
        import json

        try:
            _from_json(context, context.window_manager.clipboard)
        except (json.JSONDecodeError, KeyError):
            self.report({'ERROR'}, "The clipboard does not contain a Selection Set")
        else:
            # Select the pasted Selection Set.
            context.object.active_selection_set = len(context.object.selection_sets) - 1

        return {'FINISHED'}


def _uniqify(name, other_names):
    # :arg name: The name to make unique.
    # :type name: str
    # :arg other_names: The name to make unique.
    # :type other_names: str
    # :return: Return a unique name with ``.xxx`` suffix if necessary.
    # :rtype: str
    #
    # Example usage:
    #
    # >>> _uniqify('hey', ['there'])
    # 'hey'
    # >>> _uniqify('hey', ['hey.001', 'hey.005'])
    # 'hey'
    # >>> _uniqify('hey', ['hey', 'hey.001', 'hey.005'])
    # 'hey.002'
    # >>> _uniqify('hey', ['hey', 'hey.005', 'hey.001'])
    # 'hey.002'
    # >>> _uniqify('hey', ['hey', 'hey.005', 'hey.001', 'hey.left'])
    # 'hey.002'
    # >>> _uniqify('hey', ['hey', 'hey.001', 'hey.002'])
    # 'hey.003'
    #
    # It also works with a dict_keys object:
    # >>> _uniqify('hey', {'hey': 1, 'hey.005': 1, 'hey.001': 1}.keys())
    # 'hey.002'

    if name not in other_names:
        return name

    # Construct the list of numbers already in use.
    offset = len(name) + 1
    others = (
        n[offset:] for n in other_names
        if n.startswith(name + '.')
    )
    numbers = sorted(
        int(suffix) for suffix in others
        if suffix.isdigit()
    )

    # Find the first unused number.
    min_index = 1
    for num in numbers:
        if min_index < num:
            break
        min_index = num + 1
    return "{:s}.{:03d}".format(name, min_index)


def _to_json(context):
    # Convert the selected Selection Sets of the current rig to JSON.
    #
    # Selected Sets are the active_selection_set determined by the UIList
    # plus any with the is_selected checkbox on.
    #
    # :return: The selection as JSON data.
    # :rtype: str
    import json

    arm = context.object
    active_idx = arm.active_selection_set

    json_obj = {}
    for idx, sel_set in enumerate(context.object.selection_sets):
        if idx == active_idx or sel_set.is_selected:
            bones = [bone_id.name for bone_id in sel_set.bone_ids]
            json_obj[sel_set.name] = bones

    return json.dumps(json_obj)


def _from_json(context, as_json):
    # Add the selection sets (one or more) from JSON to the current rig.
    #
    # :arg as_json: The JSON contents to load.
    # :type as_json: str
    import json

    json_obj = json.loads(as_json)
    arm_sel_sets = context.object.selection_sets

    for name, bones in json_obj.items():
        new_sel_set = arm_sel_sets.add()
        new_sel_set.name = _uniqify(name, arm_sel_sets.keys())
        for bone_name in bones:
            bone_id = new_sel_set.bone_ids.add()
            bone_id.name = bone_name


# Registry ####################################################################

classes = (
    SelectionEntry,
    SelectionSet,
    POSE_OT_selection_set_delete_all,
    POSE_OT_selection_set_remove_bones,
    POSE_OT_selection_set_move,
    POSE_OT_selection_set_add,
    POSE_OT_selection_set_remove,
    POSE_OT_selection_set_assign,
    POSE_OT_selection_set_unassign,
    POSE_OT_selection_set_select,
    POSE_OT_selection_set_deselect,
    POSE_OT_selection_set_add_and_assign,
    POSE_OT_selection_set_copy,
    POSE_OT_selection_set_paste,
)


def register():
    bpy.types.Object.selection_sets = CollectionProperty(
        type=SelectionSet,
        name="Selection Sets",
        description="List of groups of bones for easy selection",
        override={'LIBRARY_OVERRIDABLE', 'USE_INSERTION'}
    )
    bpy.types.Object.active_selection_set = IntProperty(
        name="Active Selection Set",
        description="Index of the currently active selection set",
        default=0,
        override={'LIBRARY_OVERRIDABLE'}
    )


def unregister():
    del bpy.types.Object.selection_sets
    del bpy.types.Object.active_selection_set
