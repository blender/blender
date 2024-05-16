# SPDX-FileCopyrightText: 2018-2022 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

from ...io.com.gltf2_io import from_dict, from_union, from_none, from_float, from_str, from_list
from ...io.com.gltf2_io import to_float, to_class


class Variant:
    """defines variant for use with glTF 2.0."""

    def __init__(self, name, extensions, extras):
        self.name = name
        self.extensions = extensions
        self.extras = extras

    @staticmethod
    def from_dict(obj):
        assert isinstance(obj, dict)
        name = from_union([from_str, from_none], obj.get("name"))
        extensions = from_union([lambda x: from_dict(lambda x: from_dict(lambda x: x, x), x), from_none],
                                obj.get("extensions"))
        extras = obj.get("extras")
        return Variant(name, extensions, extras)

    def to_dict(self):
        result = {}
        result["name"] = from_union([from_str, from_none], self.name)
        result["extensions"] = from_union([lambda x: from_dict(lambda x: from_dict(lambda x: x, x), x), from_none],
                                          self.extensions)
        result["extras"] = self.extras
        return result
