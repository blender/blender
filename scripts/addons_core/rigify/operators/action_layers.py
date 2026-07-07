# SPDX-FileCopyrightText: 2022-2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
import random

from typing import Sequence, Any

from bpy.types import PropertyGroup, Action, UIList, UILayout, Context, Panel, Operator, Armature, ActionSlot
from bpy.props import (EnumProperty, IntProperty, BoolProperty, StringProperty, FloatProperty,
                       PointerProperty, CollectionProperty)
from bpy.app.translations import (
    pgettext_iface as iface_,
    pgettext_rpt as rpt_,
)

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


def get_first_compatible_action_slot(action: Action, id_type: str) -> ActionSlot | None:
    for slot in action.slots:
        if slot.target_id_type in ('UNSPECIFIED', id_type):
            return slot
    return None


class ActionSlot(PropertyGroup, ActionSlotBase):
    def on_action_update(self, context):
        if not self.action:
            return

        # We must trigger the lazy-initialization of the unique_id before
        # any UI code tries to access it, since if it tried to lazy-initialize
        # during UI drawing, that would result in an error.
        self.ensure_unique_id()

        if self.action_slot:
            # Nothing else to do here.
            return

        # Set the first compatible slot if none already set. However, be careful
        # to prevent infinite loops, as this will call this function again.
        first_slot = get_first_compatible_action_slot(self.action, 'OBJECT')
        if first_slot:
            # Only write when not None, to prevent looping infinitely.
            self.action_slot = first_slot

    action: PointerProperty(
        name="Action",
        type=Action,
        description="Action to apply to the rig via constraints",
        update=on_action_update,
    )

    def slot_name_from_handle(self, slot_handle_as_str: str, _is_set: bool) -> str:
        """This is a get_transform callback function, see Blender 5.0 PyAPI docs."""
        if not slot_handle_as_str:
            return ""
        slot_handle = int(slot_handle_as_str)
        action_slot = next((s for s in self.action.slots if s.handle == slot_handle), None)
        if not action_slot:
            return ""
        # We use the display name rather than the identifier because in Rigify's context,
        # we don't care about the datablock type prefix found in the identifier,
        # since our action slots are always for Objects.
        return action_slot.name_display

    def slot_name_to_handle(self, new_name: str, _current_name: str, _is_set: bool) -> str:
        """This is a set_transform callback function, see Blender 5.0 PyAPI docs."""
        action_slot = self.action.slots.get("OB" + new_name)
        if not action_slot:
            return ""
        return str(action_slot.handle)

    action_slot_ui: StringProperty(
        name="Action Slot",
        description="Slot of the Action to use for the Action Constraints",
        # These callbacks let us store the action slot's `handle` property
        # under the hood (which is unique and never changes), while acting
        # as a user-friendly display name in the UI.
        get_transform=slot_name_from_handle,
        set_transform=slot_name_to_handle,
        update=on_action_update,
    )

    unique_id: IntProperty(default=0)

    def ensure_unique_id(self) -> int:
        if self.unique_id:
            return self.unique_id

        # IDProperties only support signed 32-bit integers, so this is the
        # biggest pool of random numbers we can pick from.
        unique_id = random.randint(0, 2**31 - 1)
        self.unique_id = unique_id
        return unique_id

    @property
    def action_slot(self) -> ActionSlot | None:
        return self.action.slots.get("OB" + self.action_slot_ui)

    @action_slot.setter
    def action_slot(self, slot: ActionSlot):
        """For convenience, caller can assign an Action Slot,
        even though under the hood we'll actually be storing the slot handle.
        """
        # We don't actually assign the handle directly, since we have
        # the action_slot_ui wrapper property, which masks the handle for us.
        self.action_slot_ui = slot.name_display if slot else ""

    def get_name_transform(self) -> str:
        """Return a useful display name for this Rigify action set-up,
        consisting of the Action name and the slot name, with a little arrow inbetween.

        The latter is omitted when the Action has only a single slot, to be less cluttered
        for users who prefer to use separate Actions,
        and for legacy rigs where all slots are named "Legacy Slot".
        """
        if not self.action:
            return str(self.unique_id)

        if self.action_slot and len(self.action.slots) > 1:
            return f"{self.action.name} ➔ {self.action_slot.name_display}"

        return self.action.name

    name: StringProperty(get=get_name_transform)

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

    # Corrective Action properties

    is_corrective: BoolProperty(
        name="Corrective",
        description="Indicate that this is a corrective action. Corrective actions will activate "
                    "based on the activation of two other actions (using End Frame if both inputs "
                    "are at their End Frame, and Start Frame if either is at Start Frame)"
    )

    def setup_id_to_str(self, unique_id_as_str: str, _is_set: bool) -> str:
        """This is a get_transform callback function, see Blender 5.0 PyAPI docs."""
        if not unique_id_as_str:
            return ""
        unique_id = int(unique_id_as_str)
        action_setups = self.id_data.rigify_action_slots
        action_setup = next((setup for setup in action_setups if setup.unique_id == unique_id), None)
        if not action_setup:
            return ""
        return action_setup.name

    def setup_name_to_id(self, name: str, _curr_value: str, _is_set: bool) -> str:
        """This is a set_transform callback function, see Blender 5.0 PyAPI docs."""
        action_setups = self.id_data.rigify_action_slots
        action_setup = next((setup for setup in action_setups if setup.name == name), None)
        if not action_setup:
            return ""
        return str(action_setup.unique_id)

    trigger_select_a: StringProperty(
        name="Trigger A",
        description="Action Set-up whose activation will trigger this set-up as a corrective",
        get_transform=setup_id_to_str,
        set_transform=setup_name_to_id,
    )
    trigger_select_b: StringProperty(
        name="Trigger B",
        description="Action Set-up whose activation will trigger this set-up as a corrective",
        # These callbacks let us store the trigger action setups' `unique_id` property
        # under the hood (which is unique and never changes), while acting as
        # a user-friendly display name in the UI.
        get_transform=setup_id_to_str,
        set_transform=setup_name_to_id,
    )

    @property
    def trigger_a(self):
        action_setups = self.id_data.rigify_action_slots
        return next((setup for setup in action_setups if setup.name == self.trigger_select_a), None)

    @trigger_a.setter
    def trigger_a(self, action_setup):
        self.trigger_select_a = action_setup.name if action_setup else ""

    @property
    def trigger_b(self):
        action_setups = self.id_data.rigify_action_slots
        return next((setup for setup in action_setups if setup.name == self.trigger_select_b), None)

    @trigger_b.setter
    def trigger_b(self, action_setup):
        self.trigger_select_b = action_setup.name if action_setup else ""

    show_action_a: BoolProperty(name="Show Settings")
    show_action_b: BoolProperty(name="Show Settings")


# =============================================
# Operators

# noinspection PyPep8Naming
class RIGIFY_OT_action_create(Operator):
    """Create a new action"""
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

    to_unique_id: IntProperty()

    def execute(self, context):
        armature_id_store = context.object.data
        for i, action_setup in enumerate(armature_id_store.rigify_action_slots):
            if action_setup.unique_id == self.to_unique_id:
                armature_id_store.rigify_active_action_slot = i
                break
        else:
            self.report({'ERROR'}, "Failed to find Action Slot.")
            return {'CANCELLED'}

        self.report({'INFO'}, rpt_("Set active action set-up index to {}.").format(i))
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

        if action_slot.action:
            row = layout.row()
            icon = 'ACTION'

            # Check if this action is a trigger for the active corrective action
            if active_action.is_corrective and \
                action_slot in (active_action.trigger_a,
                                active_action.trigger_b):
                icon = 'RESTRICT_INSTANCED_OFF'

            # Check if the active action is a trigger for this corrective action.
            if action_slot.is_corrective and \
                active_action in [action_slot.trigger_a,
                                  action_slot.trigger_b]:
                icon = 'RESTRICT_INSTANCED_OFF'

            row.label(text=action_slot.name, icon=icon)

            # Highlight various errors
            if action_slot.is_corrective:
                text = "Corrective"
                icon = 'RESTRICT_INSTANCED_OFF'

                for trigger in (action_slot.trigger_a, action_slot.trigger_b):
                    # No trigger action set, no slot or invalid slot
                    if not trigger or trigger.is_corrective:
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

        active_action_setup = action_slots[active_idx]

        col = layout.column(align=True)
        col.template_ID(active_action_setup, 'action', new=RIGIFY_OT_action_create.bl_idname)
        if not active_action_setup.action:
            return
        if not active_action_setup.action.slots:
            layout.alert = True
            layout.label(text="No slots in this Action.")
            return

        col.prop_search(active_action_setup, "action_slot_ui", active_action_setup.action, 'slots', text="")

        layout = layout.column()
        layout.prop(active_action_setup, 'is_corrective')

        if active_action_setup.is_corrective:
            self.draw_ui_corrective(context, active_action_setup)
        else:
            target_rig = get_rigify_target_rig(armature_id_store)
            self.draw_slot_ui(layout, active_action_setup, target_rig)
            self.draw_status(active_action_setup)

    def draw_ui_corrective(self, context: Context, slot):
        layout = self.layout

        layout.prop(slot, 'frame_start', text="Frame Start")
        layout.prop(slot, 'frame_end', text="End")
        layout.separator()

        for trigger_prop in ['trigger_select_a', 'trigger_select_b']:
            self.draw_ui_trigger(context, slot, trigger_prop)

    def draw_ui_trigger(self, context: Context, slot, trigger_prop: str):
        layout = self.layout
        metarig = context.object
        assert isinstance(metarig.data, Armature)

        trigger = getattr(slot, trigger_prop.replace("select_", ""))
        icon = 'ACTION' if trigger else 'ERROR'

        row = layout.row()
        try:
            active_slot = metarig.data.rigify_action_slots[metarig.data.rigify_active_action_slot]
        except IndexError:
            return
        row.prop_search(active_slot, trigger_prop, metarig.data, 'rigify_action_slots', icon=icon)

        if not trigger:
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
        op.to_unique_id = trigger.unique_id

        if show:
            col = layout.column(align=True)
            col.enabled = False
            target_rig = get_rigify_target_rig(metarig.data)
            self.draw_slot_ui(col, trigger, target_rig)
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
                text = rpt_("Bone not found: {:s}").format(slot.subtarget)
                row.label(text=text, translate=False, icon='ERROR')
        else:
            row.prop(slot, 'subtarget', icon=bone_icon)

        flipped_subtarget = mirror_name(slot.subtarget)

        if flipped_subtarget != slot.subtarget:
            flipped_subtarget_exists = not target_rig or flipped_subtarget in target_rig.pose.bones

            row = layout.row()
            row.use_property_split = True
            text = iface_("Symmetrical ({:s})").format(flipped_subtarget)
            row.prop(slot, 'symmetrical', text=text, translate=False)

            if slot.symmetrical and not flipped_subtarget_exists:
                row.alert = True

                row = layout.split(factor=0.4)
                row.column()
                row.alert = True
                text = rpt_("Bone not found: {:s}").format(flipped_subtarget)
                row.label(text=text, icon='ERROR', translate=False)

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
            text = rpt_("Min and max values are the same!")
            col.label(text=text, translate=False)
            text = rpt_("Will be stuck reading frame {:d}!").format(slot.frame_start)
            col.label(text=text, translate=False)
            return

        if slot.frame_start == slot.frame_end:
            col = split.column(align=True)
            col.alert = True
            text = rpt_("Start and end frame cannot be the same!")
            col.label(text=text, translate=False)

        default_frame = slot.get_default_frame()

        if slot.is_default_frame_integer():
            text = rpt_("Default Frame: {:.0f}").format(default_frame)
            split.label(text=text, translate=False)
        else:
            split.alert = True
            text = rpt_("Default Frame: {:.02f} (Should be a whole number!)").format(default_frame)
            split.label(text=text, translate=False)


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
