# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0


def get_target_property_name(data_path: str) -> str:
    """Retrieve target property."""

    if data_path.endswith("]"):
        return None
    else:
        return data_path.rsplit('.', 1)[-1]


def get_target_object_path(data_path: str) -> str:
    """Retrieve target object data path without property"""
    if data_path.endswith("]"):
        return data_path.rsplit('[', 1)[0]
    elif data_path.startswith("pose.bones["):
        return data_path[:data_path.find('"]')] + '"]'
    path_split = data_path.rsplit('.', 1)
    self_targeting = len(path_split) < 2
    if self_targeting:
        return ""
    return path_split[0]


def get_rotation_modes(target_property: str):
    """Retrieve rotation modes based on target_property"""
    if target_property in ["rotation_euler", "delta_rotation_euler"]:
        return True, ["XYZ", "XZY", "YXZ", "YZX", "ZXY", "ZYX"]
    elif target_property in ["rotation_quaternion", "delta_rotation_quaternion"]:
        return True, ["QUATERNION"]
    elif target_property in ["rotation_axis_angle"]:
        return True, ["AXIS_ANGLE"]
    else:
        return False, []


def is_location(target_property):
    return "location" in target_property


def is_rotation(target_property):
    return "rotation" in target_property


def is_scale(target_property):
    return "scale" in target_property


def get_delta_modes(target_property: str) -> str:
    """Retrieve location based on target_property"""
    return target_property.startswith("delta_")


def is_bone_anim_channel(data_path: str) -> bool:
    return data_path[:10] == "pose.bones"


def get_sk_exported(key_blocks):
    return [
        k
        for k in key_blocks
        if not skip_sk(key_blocks, k)
    ]


def skip_sk(key_blocks, k):
    # Do not export:
    # - if muted
    # - if relative key is SK itself (this avoid exporting Basis too if user didn't change order)
    # - the Basis (the first SK of the list)
    return k == k.relative_key \
        or k.mute \
        or is_first_index(key_blocks, k) is True


def is_first_index(key_blocks, k):
    return key_blocks[0].name == k.name
