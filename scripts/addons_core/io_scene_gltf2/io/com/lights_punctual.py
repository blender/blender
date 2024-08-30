# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

from ...io.com.gltf2_io import from_dict, from_union, from_none, from_float, from_str, from_list
from ...io.com.gltf2_io import to_float, to_class


class LightSpot:
    """light/spot"""

    def __init__(self, inner_cone_angle, outer_cone_angle):
        self.inner_cone_angle = inner_cone_angle
        self.outer_cone_angle = outer_cone_angle

    @staticmethod
    def from_dict(obj):
        assert isinstance(obj, dict)
        inner_cone_angle = from_union([from_float, from_none], obj.get("innerConeAngle"))
        outer_cone_angle = from_union([from_float, from_none], obj.get("outerConeAngle"))
        return LightSpot(inner_cone_angle, outer_cone_angle)

    def to_dict(self):
        result = {}
        result["innerConeAngle"] = from_union([from_float, from_none], self.inner_cone_angle)
        result["outerConeAngle"] = from_union([from_float, from_none], self.outer_cone_angle)
        return result


class Light:
    """defines a set of lights for use with glTF 2.0. Lights define light sources within a scene"""

    def __init__(self, color, intensity, spot, type, range, name, extensions, extras):
        self.color = color
        self.intensity = intensity
        self.spot = spot
        self.type = type
        self.range = range
        self.name = name
        self.extensions = extensions
        self.extras = extras

    @staticmethod
    def from_dict(obj):
        assert isinstance(obj, dict)
        color = from_union([lambda x: from_list(from_float, x), from_none], obj.get("color"))
        intensity = from_union([from_float, from_none], obj.get("intensity"))
        spot = LightSpot.from_dict(obj.get("spot"))
        type = from_str(obj.get("type"))
        range = from_union([from_float, from_none], obj.get("range"))
        name = from_union([from_str, from_none], obj.get("name"))
        extensions = from_union([lambda x: from_dict(lambda x: from_dict(lambda x: x, x), x), from_none],
                                obj.get("extensions"))
        extras = obj.get("extras")
        return Light(color, intensity, spot, type, range, name, extensions, extras)

    def to_dict(self):
        result = {}
        result["color"] = from_union([lambda x: from_list(to_float, x), from_none], self.color)
        result["intensity"] = from_union([from_float, from_none], self.intensity)
        result["spot"] = from_union([lambda x: to_class(LightSpot, x), from_none], self.spot)
        result["type"] = from_str(self.type)
        result["range"] = from_union([from_float, from_none], self.range)
        result["name"] = from_union([from_str, from_none], self.name)
        result["extensions"] = from_union([lambda x: from_dict(lambda x: from_dict(lambda x: x, x), x), from_none],
                                          self.extensions)
        result["extras"] = self.extras
        return result
