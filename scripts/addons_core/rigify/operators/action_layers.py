# SPDX-FileCopyrightText: 2022-2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy

from typing import Tuple, Optional, Sequence, Any

from bpy.types import PropertyGroup, Action, UIList, UILayout, Context, Panel, Operator, Armature
from bpy.props import (EnumProperty, IntProperty, BoolProperty, StringProperty, FloatProperty,
                       PointerProperty, CollectionProperty)

from .generic_ui_list import draw_ui_list

from ..utils.naming import mirror_name
from ..utils.action_layers import ActionSlotBase
from ..utils.rig import get_rigify_target_rig, is_valid_metarig


def get_action_slots(arm: Armature) -> Sequence['ActionSlot']:
    return arm.rigify_action_slots  # noqa


def get_action_slots_active(arm: Armature) -> tuple[Sequence['ActionSlot'], int]:
    return arm.rigify_action_slots, arm.rigify_active_action_slot  # noqa


def poll_trigger_action(_self, action):
    """Whether an action can be used as a corrective action's trigger or not."""
    armature_id_store = bpy.context.object.data
    assert isinstance(armature_id_store, Armature)

    slots, idx = get_action_slots_active(armature_id_store)

    active_slot = slots[idx] if 0 <= idx < len(slots) else None

    # If this action is the same as the active slot's action, don't show it.
    if active_slot and action == active_slot.action:
        return False

    # If this action is used by any other action slot, show it.
    for slot in slots:
        if slot.action == action and not slot.is_corrective:
            return True

    return False


class ActionSlot(PropertyGroup, ActionSlotBase):
    action: PointerProperty(
        name="Action",
        type=Action,
        description="Action to apply to the rig via constraints"
    )

    enabled: BoolProperty(
        name="Enabled",
        description="Create constraints for this action on the generated rig",
        default=True
    )

    symmetrical: BoolProperty(
        name="Symmetrical",
        description="Apply the same setup but mirrored to the opposite side control, shown in "
                    "parentheses. Bones will only be affected by the control with the same side "
                    "(eg., .L bones will only be affected by the .L control). Bones without a "
                    "side in their name (so no .L or .R) will be affected by both controls "
                    "with 0.5 influence each",
        default=True
    )

    subtarget: StringProperty(
        name="Control Bone",
        description="Select a bone on the generated rig which will drive this action"
    )

    transform_channel: EnumProperty(name="Transform Channel",
                                    items=[("LOCATION_X", "X Location", "X Location"),
                                           ("LOCATION_Y", "Y Location", "Y Location"),
                                           ("LOCATION_Z", "Z Location", "Z Location"),
                                           ("ROTATION_X", "X Rotation", "X Rotation"),
                                           ("ROTATION_Y", "Y Rotation", "Y Rotation"),
                                           ("ROTATION_Z", "Z Rotation", "Z Rotation"),
                                           ("SCALE_X", "X Scale", "X Scale"),
                                           ("SCALE_Y", "Y Scale", "Y Scale"),
                                           ("SCALE_Z", "Z Scale", "Z Scale")],
                                    description="Transform channel",
                                    default="LOCATION_X")

    target_space: EnumProperty(
        name="Transform Space",
        items=[("WORLD", "World Space", "World Space", 0),
               # ("POSE", "Pose Space", "Pose Space", 1),
               # ("LOCAL_WITH_PARENT", "Local With Parent", "Local With Parent", 2),
               ("LOCAL", "Local Space", "Local Space", 3)],
        default="LOCAL"
    )

    def update_frame_start(self, _context):
        if self.frame_start > self.frame_end:
            self.frame_end = self.frame_start

    frame_start: IntProperty(
        name="Start Frame",
        description="First frame of the action's timeline",
        update=update_frame_start
    )

    def update_frame_end(self, _context):
        if self.frame_end < self.frame_start:
            self.frame_start = self.frame_end

    frame_end: IntProperty(
        name="End Frame",
        default=2,
        description="Last frame of the action's timeline",
        update=update_frame_end
    )

    trans_min: FloatProperty(
        name="Min",
        default=-0.05,
        description="Value that the transformation value must reach to put the action's timeline "
                    "to the first frame. Rotations are in degrees"
    )

    trans_max: FloatProperty(
        name="Max",
        default=0.05,
        description="Value that the transformation value must reach to put the action's timeline "
                    "to the last frame. Rotations are in degrees"
    )

    is_corrective: BoolProperty(
        name="Corrective",
        description="Indicate that this is a corrective action. Corrective actions will activate "
                    "based on the activation of two other actions (using End Frame if both inputs "
                    "are at their End Frame, and Start Frame if either is at Start Frame)"
    )

    trigger_action_a: PointerProperty(
        name="Trigger A",
        type=Action,
        description="Action whose activation will trigger the corrective action",
        poll=poll_trigger_action
    )

    trigger_action_b: PointerProperty(
        name="Trigger B",
        description="Action whose activation will trigger the corrective action",
        type=Action,
        poll=poll_trigger_action
    )

    show_action_a: BoolProperty(name="Show Settings")
    show_action_b: BoolProperty(name="Show Settings")


def find_slot_by_action(metarig_data: Armature, action) -> Tuple[Optional[ActionSlot], int]:
    """Find the ActionSlot in the rig which targets this action."""
    if not action:
        return None, -1

    for i, slot in enumerate(get_action_slots(metarig_data)):
        if slot.action == action:
            return slot, i
    else:
        return None, -1


def find_duplicate_slot(metarig_data: Armature, action_slot: ActionSlot) -> Optional[ActionSlot]:
    """Find a different ActionSlot in the rig which has the same action."""

    for slot in get_action_slots(metarig_data):
        if slot.action == action_slot.action and slot != action_slot:
            return slot

    return None


# =============================================
# Operators

# noinspection PyPep8Naming
class RIGIFY_OT_action_create(Operator):
    """Create new Action"""
    # This is needed because bpy.ops.action.new() has a poll function that blocks
    # the operator unless it's drawn in an animation UI panel.

    bl_idname = "object.rigify_action_create"
    bl_label = "New"
    bl_options = {'REGISTER', 'UNDO', 'INTERNAL'}

    def execute(self, context):
        armature_id_store = context.object.data
        assert isinstance(armature_id_store, Armature)
        action_slots, action_slot_idx = get_action_slots_active(armature_id_store)
        action_slot = action_slots[action_slot_idx]
        action = bpy.data.actions.new(name="Action")
        action_slot.action = action
        return {'FINISHED'}


# noinspection PyPep8Naming
class RIGIFY_OT_jump_to_action_slot(Operator):
    """Set Active Action Slot Index"""

    bl_idname = "object.rigify_jump_to_action_slot"
    bl_label = "Jump to Action Slot"
    bl_options = {'REGISTER', 'UNDO', 'INTERNAL'}

    to_index: IntProperty()

    def execute(self, context):
        armature_id_store = context.object.data
        armature_id_store.rigify_active_action_slot = self.to_index
        return {'FINISHED'}


# =============================================
# UI Panel

# noinspection PyPep8Naming
class RIGIFY_UL_action_slots(UIList):
    def draw_item(self, context: Context, layout: UILayout, data: Armature,
                  action_slot: ActionSlot, icon, active_data, active_propname: str,
                  slot_index: int = 0, flt_flag: int = 0):
        action_slots, action_slot_idx = get_action_slots_active(data)
        active_action = action_slots[action_slot_idx]

        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            if action_slot.action:
                row = layout.row()
                icon = 'ACTION'

                # Check if this action is a trigger for the active corrective action
                if active_action.is_corrective and \
                    action_slot.action in [active_action.trigger_action_a,
                                           active_action.trigger_action_b]:
                    icon = 'RESTRICT_INSTANCED_OFF'

                # Check if the active action is a trigger for this corrective action.
                if action_slot.is_corrective and \
                    active_action.action in [action_slot.trigger_action_a,
                                             action_slot.trigger_action_b]:
                    icon = 'RESTRICT_INSTANCED_OFF'

                row.prop(action_slot.action, 'name', text="", emboss=False, icon=icon)

                # Highlight various errors

                if find_duplicate_slot(data, action_slot):
                    # Multiple entries for the same action
                    row.alert = True
                    row.label(text="Duplicate", icon='ERROR')

                elif action_slot.is_corrective:
                    text = "Corrective"
                    icon = 'RESTRICT_INSTANCED_OFF'

                    for trigger in [action_slot.trigger_action_a,
                                    action_slot.trigger_action_b]:
                        trigger_slot, trigger_idx = find_slot_by_action(data, trigger)

                        # No trigger action set, no slot or invalid slot
                        if not trigger_slot or trigger_slot.is_corrective:
                            row.alert = True
                            text = "No Trigger Action"
                            icon = 'ERROR'
                            break

                    row.label(text=text, icon=icon)

                else:
                    text = action_slot.subtarget
                    icon = 'BONE_DATA'

                    target_rig = get_rigify_target_rig(data)

                    if not action_slot.subtarget:
                        row.alert = True
                        text = 'No Control Bone'
                        icon = 'ERROR'

                    elif target_rig:
                        # Check for bones not actually present in the generated rig
                        bones = target_rig.pose.bones

                        if action_slot.subtarget not in bones:
                            row.alert = True
                            text = 'Bad Control Bone'
                            icon = 'ERROR'
                        elif (action_slot.symmetrical
                              and mirror_name(action_slot.subtarget) not in bones):
                            row.alert = True
                            text = 'Bad Control Bone'
                            icon = 'ERROR'

                    row.label(text=text, icon=icon)

                icon = 'CHECKBOX_HLT' if action_slot.enabled else 'CHECKBOX_DEHLT'
                row.enabled = action_slot.enabled

                layout.prop(action_slot, 'enabled', text="", icon=icon, emboss=False)

            # No action
            else:
                layout.label(text="", translate=False, icon='ACTION')

        elif self.layout_type in {'GRID'}:
            layout.alignment = 'CENTER'
            layout.label(text="", icon_value=icon)


# noinspection PyPep8Naming
class DATA_PT_rigify_actions(Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = 'data'
    bl_label = "Actions"
    bl_parent_id = "DATA_PT_rigify"
    bl_options = {'DEFAULT_CLOSED'}

    @classmethod
    def poll(cls, context):
        return context.object.mode in ('POSE', 'OBJECT') and is_valid_metarig(context)

    def draw(self, context: Context):
        armature_id_store = context.object.data
        assert isinstance(armature_id_store, Armature)
        action_slots, active_idx = get_action_slots_active(armature_id_store)

        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        draw_ui_list(
            layout, context,
            class_name='RIGIFY_UL_action_slots',
            list_context_path='object.data.rigify_action_slots',
            active_index_context_path='object.data.rigify_active_action_slot',
        )

        if len(action_slots) == 0:
            return

        active_slot = action_slots[active_idx]

        layout.template_ID(active_slot, 'action', new=RIGIFY_OT_action_create.bl_idname)

        if not active_slot.action:
            return

        layout = layout.column()
        layout.prop(active_slot, 'is_corrective')

        if active_slot.is_corrective:
            self.draw_ui_corrective(context, active_slot)
        else:
            target_rig = get_rigify_target_rig(armature_id_store)
            self.draw_slot_ui(layout, active_slot, target_rig)
            self.draw_status(active_slot)

    def draw_ui_corrective(self, context: Context, slot):
        layout = self.layout

        layout.prop(slot, 'frame_start', text="Frame Start")
        layout.prop(slot, 'frame_end', text="End")
        layout.separator()

        for trigger_prop in ['trigger_action_a', 'trigger_action_b']:
            self.draw_ui_trigger(context, slot, trigger_prop)

    def draw_ui_trigger(self, context: Context, slot, trigger_prop: str):
        layout = self.layout
        metarig = context.object
        assert isinstance(metarig.data, Armature)

        trigger = getattr(slot, trigger_prop)
        icon = 'ACTION' if trigger else 'ERROR'

        row = layout.row()
        row.prop(slot, trigger_prop, icon=icon)

        if not trigger:
            return

        trigger_slot, slot_index = find_slot_by_action(metarig.data, trigger)

        if not trigger_slot:
            row = layout.split(factor=0.4)
            row.separator()
            row.alert = True
            row.label(text="Action not in list", icon='ERROR')
            return

        show_prop_name = 'show_action_' + trigger_prop[-1]
        show = getattr(slot, show_prop_name)
        icon = 'HIDE_OFF' if show else 'HIDE_ON'

        row.prop(slot, show_prop_name, icon=icon, text="")

        op = row.operator(RIGIFY_OT_jump_to_action_slot.bl_idname, text="", icon='LOOP_FORWARDS')
        op.to_index = slot_index

        if show:
            col = layout.column(align=True)
            col.enabled = False
            target_rig = get_rigify_target_rig(metarig.data)
            self.draw_slot_ui(col, trigger_slot, target_rig)
            col.separator()

    @staticmethod
    def draw_slot_ui(layout, slot, target_rig):
        if not target_rig:
            row = layout.row()
            row.alert = True
            row.label(text="Cannot verify bone name without a generated rig", icon='ERROR')

        row = layout.row()

        bone_icon = 'BONE_DATA' if slot.subtarget else 'ERROR'

        if target_rig:
            subtarget_exists = slot.subtarget in target_rig.pose.bones
            row.prop_search(slot, 'subtarget', target_rig.pose, 'bones', icon=bone_icon)
            row.alert = not subtarget_exists

            if slot.subtarget and not subtarget_exists:
                row = layout.split(factor=0.4)
                row.column()
                row.alert = True
                row.label(text=f"Bone not found: {slot.subtarget}", icon='ERROR')
        else:
            row.prop(slot, 'subtarget', icon=bone_icon)

        flipped_subtarget = mirror_name(slot.subtarget)

        if flipped_subtarget != slot.subtarget:
            flipped_subtarget_exists = not target_rig or flipped_subtarget in target_rig.pose.bones

            row = layout.row()
            row.use_property_split = True
            row.prop(slot, 'symmetrical', text=f"Symmetrical ({flipped_subtarget})")

            if slot.symmetrical and not flipped_subtarget_exists:
                row.alert = True

                row = layout.split(factor=0.4)
                row.column()
                row.alert = True
                row.label(text=f"Bone not found: {flipped_subtarget}", icon='ERROR')

        layout.prop(slot, 'frame_start', text="Frame Start")
        layout.prop(slot, 'frame_end', text="End")

        layout.prop(slot, 'target_space', text="Target Space")
        layout.prop(slot, 'transform_channel', text="Transform Channel")

        layout.prop(slot, 'trans_min')
        layout.prop(slot, 'trans_max')

    def draw_status(self, slot):
        """
        There are a lot of ways to create incorrect Action setups, so give
        the user a warning in those cases.
        """
        layout = self.layout

        split = layout.split(factor=0.4)
        heading = split.row()
        heading.alignment = 'RIGHT'
        heading.label(text="Status:")

        if slot.trans_min == slot.trans_max:
            col = split.column(align=True)
            col.alert = True
            col.label(text="Min and max value are the same!")
            col.label(text=f"Will be stuck reading frame {slot.frame_start}!")
            return

        if slot.frame_start == slot.frame_end:
            col = split.column(align=True)
            col.alert = True
            col.label(text="Start and end frame cannot be the same!")

        default_frame = slot.get_default_frame()

        if slot.is_default_frame_integer():
            split.label(text=f"Default Frame: {round(default_frame)}")
        else:
            split.alert = True
            split.label(text=f"Default Frame: {round(default_frame, 2)} "
                             "(Should be a whole number!)")


# =============================================
# Registration

classes = (
    ActionSlot,
    RIGIFY_OT_action_create,
    RIGIFY_OT_jump_to_action_slot,
    RIGIFY_UL_action_slots,
    DATA_PT_rigify_actions,
)


def register():
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)

    bpy.types.Armature.rigify_action_slots = CollectionProperty(type=ActionSlot)
    bpy.types.Armature.rigify_active_action_slot = IntProperty(min=0, default=0)


def unregister():
    from bpy.utils import unregister_class
    for cls in classes:
        unregister_class(cls)

    arm_data: Any = bpy.types.Armature

    del arm_data.rigify_action_slots
    del arm_data.rigify_active_action_slot
