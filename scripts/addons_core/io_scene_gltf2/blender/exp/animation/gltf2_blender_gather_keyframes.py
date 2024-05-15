# SPDX-FileCopyrightText: 2018-2022 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import typing
import bpy
import mathutils
from ...com import gltf2_blender_math


class Keyframe:
    def __init__(self, channels: typing.Tuple[bpy.types.FCurve], frame: float, bake_channel: typing.Union[str, None]):
        self.seconds = frame / (bpy.context.scene.render.fps * bpy.context.scene.render.fps_base)
        self.frame = frame
        self.fps = (bpy.context.scene.render.fps * bpy.context.scene.render.fps_base)
        self.__length_morph = 0
        # Note: channels has some None items only for SK if some SK are not animated
        if bake_channel is None:
            if not all([c is None for c in channels]):
                self.target = [c for c in channels if c is not None][0].data_path.split('.')[-1]
                if self.target != "value":
                    self.__indices = [c.array_index for c in channels]
                else:
                    self.__indices = [i for i, c in enumerate(channels) if c is not None]
                    self.__length_morph = len(channels)
            else:
                # If all channels are None (baking evaluate SK case)
                self.target = "value"
                self.__indices = []
                self.__length_morph = len(channels)
                for i in range(self.get_target_len()):
                    self.__indices.append(i)

        else:
            if bake_channel == "value":
                self.__length_morph = len(channels)
            self.target = bake_channel
            self.__indices = []
            for i in range(self.get_target_len()):
                self.__indices.append(i)

        # Data holders for virtual properties
        self.__value = None
        self.__in_tangent = None
        self.__out_tangent = None

    def get_target_len(self):
        length = {
            "delta_location": 3,
            "delta_rotation_euler": 3,
            "delta_rotation_quaternion": 4,
            "delta_scale": 3,
            "location": 3,
            "rotation_axis_angle": 4,
            "rotation_euler": 3,
            "rotation_quaternion": 4,
            "scale": 3,
            "value": self.__length_morph
        }.get(self.target, 1)

        return length

    def __set_indexed(self, value):
        # Sometimes blender animations only reference a subset of components of a data target. Keyframe should always
        # contain a complete Vector/ Quaternion --> use the array_index value of the keyframe to set components in such
        # structures
        # For SK, must contains all SK values
        result = [0.0] * self.get_target_len()
        for i, v in zip(self.__indices, value):
            result[i] = v
        return result

    def get_indices(self):
        return self.__indices

    def set_value_index(self, idx, val):
        self.__value[idx] = val

    def set_value_index_in(self, idx, val):
        self.__in_tangent[idx] = val

    def set_value_index_out(self, idx, val):
        self.__out_tangent[idx] = val

    def set_first_tangent(self):
        self.__in_tangent = self.__value

    def set_last_tangent(self):
        self.__out_tangent = self.__value

    @property
    def value(self) -> typing.Union[mathutils.Vector, mathutils.Euler, mathutils.Quaternion, typing.List[float]]:
        if self.target == "value":
            return self.__value
        return gltf2_blender_math.list_to_mathutils(self.__value, self.target)

    @value.setter
    def value(self, value: typing.List[float]):
        self.__value = self.__set_indexed(value)

    @value.setter
    def value_total(self, value: typing.List[float]):
        self.__value = value

    @property
    def in_tangent(self) -> typing.Union[mathutils.Vector, mathutils.Euler, mathutils.Quaternion, typing.List[float]]:
        if self.__in_tangent is None:
            return None
        if self.target == "value":
            return self.__in_tangent
        return gltf2_blender_math.list_to_mathutils(self.__in_tangent, self.target)

    @in_tangent.setter
    def in_tangent(self, value: typing.List[float]):
        self.__in_tangent = self.__set_indexed(value)

    @property
    def out_tangent(self) -> typing.Union[mathutils.Vector, mathutils.Euler, mathutils.Quaternion, typing.List[float]]:
        if self.__out_tangent is None:
            return None
        if self.target == "value":
            return self.__out_tangent
        return gltf2_blender_math.list_to_mathutils(self.__out_tangent, self.target)

    @out_tangent.setter
    def out_tangent(self, value: typing.List[float]):
        self.__out_tangent = self.__set_indexed(value)
