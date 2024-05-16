# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import struct
import numpy as np

from ..com.gltf2_io import Accessor
from ..com.gltf2_io_constants import ComponentType, DataType


class BinaryData():
    """Binary reader."""
    def __new__(cls, *args, **kwargs):
        raise RuntimeError("%s should not be instantiated" % cls)

    # Note that this function is not used in Blender importer, but is kept in
    # Source code to be used in any pipeline that want to manage gltf/glb file in python
    @staticmethod
    def get_binary_from_accessor(gltf, accessor_idx):
        """Get binary from accessor."""
        accessor = gltf.data.accessors[accessor_idx]
        if accessor.buffer_view is None:
            return None

        data = BinaryData.get_buffer_view(gltf, accessor.buffer_view)

        accessor_offset = accessor.byte_offset
        if accessor_offset is None:
            accessor_offset = 0

        return data[accessor_offset:]

    @staticmethod
    def get_buffer_view(gltf, buffer_view_idx):
        """Get binary data for buffer view."""
        buffer_view = gltf.data.buffer_views[buffer_view_idx]

        if buffer_view.buffer in gltf.buffers.keys():
            buffer = gltf.buffers[buffer_view.buffer]
        else:
            # load buffer
            gltf.load_buffer(buffer_view.buffer)
            buffer = gltf.buffers[buffer_view.buffer]

        byte_offset = buffer_view.byte_offset
        if byte_offset is None:
            byte_offset = 0

        return buffer[byte_offset:byte_offset + buffer_view.byte_length]

    @staticmethod
    def get_data_from_accessor(gltf, accessor_idx, cache=False):
        """Get data from accessor."""
        if accessor_idx in gltf.accessor_cache:
            return gltf.accessor_cache[accessor_idx]

        data = BinaryData.decode_accessor(gltf, accessor_idx).tolist()

        if cache:
            gltf.accessor_cache[accessor_idx] = data

        return data

    @staticmethod
    def decode_accessor(gltf, accessor_idx, cache=False):
        """Decodes accessor to 2D numpy array (count x num_components)."""
        if accessor_idx in gltf.decode_accessor_cache:
            return gltf.accessor_cache[accessor_idx]

        accessor = gltf.data.accessors[accessor_idx]
        array = BinaryData.decode_accessor_obj(gltf, accessor)

        if cache:
            gltf.accessor_cache[accessor_idx] = array
            # Prevent accidentally modifying cached arrays
            array.flags.writeable = False

        return array

    @staticmethod
    def decode_accessor_internal(accessor):
        # Is use internally when accessor binary data is not yet in a glTF buffer_view
        # MAT2/3 have special alignment requirements that aren't handled. But it
        # doesn't matter because nothing uses them.
        assert accessor.type not in ['MAT2', 'MAT3']

        dtype = ComponentType.to_numpy_dtype(accessor.component_type)
        component_nb = DataType.num_elements(accessor.type)

        buffer_data = accessor.buffer_view.data

        accessor_offset = accessor.byte_offset or 0
        buffer_data = buffer_data[accessor_offset:]

        bytes_per_elem = dtype(1).nbytes
        default_stride = bytes_per_elem * component_nb
        stride = default_stride

        array = np.frombuffer(
            buffer_data,
            dtype=np.dtype(dtype).newbyteorder('<'),
            count=accessor.count * component_nb,
        )
        array = array.reshape(accessor.count, component_nb)

        return array

    @staticmethod
    def decode_accessor_obj(gltf, accessor):
        # MAT2/3 have special alignment requirements that aren't handled. But it
        # doesn't matter because nothing uses them.
        assert accessor.type not in ['MAT2', 'MAT3']

        dtype = ComponentType.to_numpy_dtype(accessor.component_type)
        component_nb = DataType.num_elements(accessor.type)

        if accessor.buffer_view is not None:
            bufferView = gltf.data.buffer_views[accessor.buffer_view]
            buffer_data = BinaryData.get_buffer_view(gltf, accessor.buffer_view)

            accessor_offset = accessor.byte_offset or 0
            buffer_data = buffer_data[accessor_offset:]

            bytes_per_elem = dtype(1).nbytes
            default_stride = bytes_per_elem * component_nb
            stride = bufferView.byte_stride or default_stride

            if stride == default_stride:
                array = np.frombuffer(
                    buffer_data,
                    dtype=np.dtype(dtype).newbyteorder('<'),
                    count=accessor.count * component_nb,
                )
                array = array.reshape(accessor.count, component_nb)

            else:
                # The data looks like
                #   XXXppXXXppXXXppXXX
                # where X are the components and p are padding.
                # One XXXpp group is one stride's worth of data.
                assert stride % bytes_per_elem == 0
                elems_per_stride = stride // bytes_per_elem
                num_elems = (accessor.count - 1) * elems_per_stride + component_nb

                array = np.frombuffer(
                    buffer_data,
                    dtype=np.dtype(dtype).newbyteorder('<'),
                    count=num_elems,
                )
                assert array.strides[0] == bytes_per_elem
                array = np.lib.stride_tricks.as_strided(
                    array,
                    shape=(accessor.count, component_nb),
                    strides=(stride, bytes_per_elem),
                )

        else:
            # No buffer view; initialize to zeros
            array = np.zeros((accessor.count, component_nb), dtype=dtype)

        if accessor.sparse:
            sparse_indices_obj = Accessor.from_dict({
                'count': accessor.sparse.count,
                'bufferView': accessor.sparse.indices.buffer_view,
                'byteOffset': accessor.sparse.indices.byte_offset or 0,
                'componentType': accessor.sparse.indices.component_type,
                'type': 'SCALAR',
            })
            sparse_indices = BinaryData.decode_accessor_obj(gltf, sparse_indices_obj)
            sparse_indices = sparse_indices.reshape(len(sparse_indices))

            sparse_values_obj = Accessor.from_dict({
                'count': accessor.sparse.count,
                'bufferView': accessor.sparse.values.buffer_view,
                'byteOffset': accessor.sparse.values.byte_offset or 0,
                'componentType': accessor.component_type,
                'type': accessor.type,
            })
            sparse_values = BinaryData.decode_accessor_obj(gltf, sparse_values_obj)

            if not array.flags.writeable:
                array = array.copy()
            array[sparse_indices] = sparse_values

        # Normalization
        if accessor.normalized:
            if accessor.component_type == 5120:  # int8
                array = np.maximum(-1.0, array / 127.0)
            elif accessor.component_type == 5121:  # uint8
                array = array / 255.0
            elif accessor.component_type == 5122:  # int16
                array = np.maximum(-1.0, array / 32767.0)
            elif accessor.component_type == 5123:  # uint16
                array = array / 65535.0

            array = array.astype(np.float32, copy=False)

        return array

    @staticmethod
    def get_image_data(gltf, img_idx):
        """Get data from image."""
        pyimage = gltf.data.images[img_idx]

        assert not (
            pyimage.uri is not None and
            pyimage.buffer_view is not None
        )

        if pyimage.uri is not None:
            return gltf.load_uri(pyimage.uri)
        if pyimage.buffer_view is not None:
            return BinaryData.get_buffer_view(gltf, pyimage.buffer_view)
        return None
