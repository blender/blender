# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import base64

from ...io.com import gltf2_io
from ...io.exp import binary_data as gltf2_io_binary_data


class Buffer:
    """Class representing binary data for use in a glTF file as 'buffer' property."""

    def __init__(self, is_glb, meshopt_extension, buffer_index=0, initial_data=None):
        self.is_glb = is_glb
        self.meshopt_extension = meshopt_extension
        self.__data = bytearray(b"")
        if initial_data is not None:
            self.__data = bytearray(initial_data.tobytes())
        self.__buffer_index = buffer_index
        self.__fake_bytelength = 0

    def add_fake_bytelength(self, byte_length):
        """used for meshopt compression fallback"""
        self.__fake_bytelength += byte_length

    def get_fake_bytelength(self):
        """used for meshopt compression fallback"""
        return self.__fake_bytelength

    def add_and_get_view(self, binary_data: gltf2_io_binary_data.BinaryData,
                         additional_buffer=None) -> gltf2_io.BufferView:
        """Add binary data to the buffer. Return a glTF BufferView."""

        # If there is an additional buffer => We are exporting with meshopt
        # It means that compressed data will go in the additional buffer
        # And uncomppressed data will also go in the additional buffer,
        # as only fallback data must go in the main buffer (bufferView definition, but without any real data.

        # if hasattr(binary_data, 'extensions') => Compressed data,
        # So populate main bufferview with main buffer, extension buffer view with additional buffer
        # else => no compressed data, so populate main bufferview with additional (compressed) buffer, no extension

        if not additional_buffer:
            offset = len(self.__data)
            self.__data.extend(binary_data.data)

        else:
            offset = len(additional_buffer.__data)
            fake_byte_length = self.__fake_bytelength  # Calculate the offset for the fallback data
            if not hasattr(binary_data, 'extensions'):
                additional_buffer.__data.extend(binary_data.data)
            else:
                self.add_fake_bytelength(binary_data.byte_length)
                # We also need to have a padding in the main buffer for the fallback data,
                # even if it doesn't contain real data, to have the correct offset for the
                # fallback bufferview
                padding = (4 - (binary_data.byte_length % 4)) % 4
                self.add_fake_bytelength(padding)

        length = binary_data.byte_length

        # offsets should be a multiple of 4 --> therefore add padding if necessary
        padding = (4 - (length % 4)) % 4
        if not additional_buffer:
            self.__data.extend(b"\x00" * padding)
        else:
            if not hasattr(binary_data, 'extensions'):
                additional_buffer.__data.extend(b"\x00" * padding)

        if not additional_buffer:
            buffer_index = self.__buffer_index
        else:
            if not self.is_glb:
                if hasattr(binary_data, 'extensions'):
                    buffer_index = self.__buffer_index
                else:
                    buffer_index = additional_buffer.__buffer_index
            else:
                # For glb, make sure the compressed buffer is first, and the fallback second
                if hasattr(binary_data, 'extensions'):
                    buffer_index = 1  # fallback buffer
                else:
                    buffer_index = 0  # compressed / data buffer

        buffer_view = gltf2_io.BufferView(
            buffer=buffer_index,
            byte_length=length,
            byte_offset=fake_byte_length if hasattr(binary_data, 'extensions') else offset,
            byte_stride=None,
            extensions=None,
            extras=None,
            name=None,
            target=binary_data.bufferViewTarget
        )

        if additional_buffer is not None and hasattr(binary_data, 'extensions'):
            # KHR/EXT_meshopt_compression
            compressed_binary_data = binary_data.extensions[self.meshopt_extension]['buffer']
            additional_buffer.__data.extend(compressed_binary_data)
            length = len(compressed_binary_data)
            padding = (4 - (length % 4)) % 4
            additional_buffer.__data.extend(b"\x00" * padding)

            buffer_view.extensions = binary_data.extensions

            # For glb, make sure the real data (compressed or not) is in the first
            # buffer, and the fallback in the second buffer
            if not self.is_glb:
                buffer_view.extensions[self.meshopt_extension]['buffer'] = additional_buffer.__buffer_index
            else:
                buffer_view.extensions[self.meshopt_extension]['buffer'] = 0

            buffer_view.extensions[self.meshopt_extension]['byteOffset'] = offset

        return buffer_view

    @property
    def byte_length(self):
        return len(self.__data)

    def to_bytes(self):
        return self.__data

    def clear(self):
        self.__data = b""

    def to_embed_string(self):
        return 'data:application/octet-stream;base64,' + base64.b64encode(self.__data).decode('ascii')
