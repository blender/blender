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
)


def select_bones(arm_object: Object, action: Action, *, select: bool, flipped: bool) -> None:
    pose_bone_re = re.compile(r'pose.bones\["([^"]+)"\]')
    pose = arm_object.pose

    seen_bone_names: Set[str] = set()

    for fcurve in action.fcurves:
        data_path: str = fcurve.data_path
        match = pose_bone_re.match(data_path)
        if not match:
            continue

        bone_name = match.group(1)

        if bone_name in seen_bone_names:
            continue
        seen_bone_names.add(bone_name)

        if flipped:
            bone_name = bpy.utils.flip_name(bone_name)

        try:
            pose_bone = pose.bones[bone_name]
        except KeyError:
            continue

        pose_bone.bone.select = select


if __name__ == '__main__':
    import doctest

    print(f"Test result: {doctest.testmod()}")
