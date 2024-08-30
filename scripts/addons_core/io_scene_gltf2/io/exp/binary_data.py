# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import typing
import array
from ...io.com import constants as gltf2_io_constants


class BinaryData:
    """Store for gltf binary data that can later be stored in a buffer."""

    def __init__(self, data: bytes, bufferViewTarget=None):
        if not isinstance(data, bytes):
            raise TypeError("Data is not a bytes array")
        self.data = data
        self.bufferViewTarget = bufferViewTarget

    def __eq__(self, other):
        return self.data == other.data

    def __hash__(self):
        return hash(self.data)

    @classmethod
    def from_list(cls,
                  lst: typing.List[typing.Any],
                  gltf_component_type: gltf2_io_constants.ComponentType,
                  bufferViewTarget=None):
        format_char = gltf2_io_constants.ComponentType.to_type_code(gltf_component_type)
        return BinaryData(array.array(format_char, lst).tobytes(), bufferViewTarget)

    @property
    def byte_length(self):
        return len(self.data)
