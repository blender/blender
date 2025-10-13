# SPDX-FileCopyrightText: 2021-2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

"""
Pose Library - usage functions.
"""

from typing import Set
import re
import bpy

from bpy.types import (
    Action,
    Object,
    ActionSlot,
)


def _find_best_slot(action: Action, object: Object) -> ActionSlot | None:
    """
    Trying to find a slot that is the best match for the given object.
    The best slot is either
    the slot of the given object if that exists in the action,
    or the first slot of type object
    """
    if not action.slots:
        return None
    # For the selection code, the object doesn't need to be animated yet, so anim_data may be None.
    anim_data = object.animation_data

    # last_slot_identifier will equal to the current slot identifier if one is assigned.
    if anim_data and anim_data.last_slot_identifier in action.slots:
        return action.slots[anim_data.last_slot_identifier]

    for slot in action.slots:
        if slot.target_id_type == 'OBJECT':
            return slot
    return None


def select_bones(arm_object: Object, action: Action, *, select: bool, flipped: bool) -> None:
    from bpy_extras import anim_utils

    pose = arm_object.pose
    if not pose:
        return

    slot = _find_best_slot(action, arm_object)
    if not slot:
        return

    seen_bone_names: Set[str] = set()
    channelbag = anim_utils.action_get_channelbag_for_slot(action, slot)
    if not channelbag:
        return

    pose_bone_re = re.compile(r'pose.bones\["([^"]+)"\]')
    for fcurve in channelbag.fcurves:
        data_path: str = fcurve.data_path
        regex_match = pose_bone_re.match(data_path)
        if not regex_match:
            continue

        bone_name = regex_match.group(1)

        if bone_name in seen_bone_names:
            continue
        seen_bone_names.add(bone_name)

        if flipped:
            bone_name = bpy.utils.flip_name(bone_name)

        try:
            pose_bone = pose.bones[bone_name]
        except KeyError:
            continue

        pose_bone.select = select


if __name__ == '__main__':
    import doctest

    print(f"Test result: {doctest.testmod()}")
