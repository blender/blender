# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

from ctypes import *

from ...io.com.gltf2_io import BufferView
from ...io.imp.gltf2_io_binary import BinaryData
from ...io.com.gltf2_io_draco_compression_extension import dll_path


def decode_primitive(gltf, prim):
    """
    Handles draco compression.
    Moves decoded data into new buffers and buffer views held by the accessors of the given primitive.
    """

    # Load DLL and setup function signatures.
    dll = cdll.LoadLibrary(str(dll_path().resolve()))

    dll.decoderCreate.restype = c_void_p
    dll.decoderCreate.argtypes = []

    dll.decoderRelease.restype = None
    dll.decoderRelease.argtypes = [c_void_p]

    dll.decoderDecode.restype = c_bool
    dll.decoderDecode.argtypes = [c_void_p, c_void_p, c_size_t]

    dll.decoderReadAttribute.restype = c_bool
    dll.decoderReadAttribute.argtypes = [c_void_p, c_uint32, c_size_t, c_char_p]

    dll.decoderGetVertexCount.restype = c_uint32
    dll.decoderGetVertexCount.argtypes = [c_void_p]

    dll.decoderGetIndexCount.restype = c_uint32
    dll.decoderGetIndexCount.argtypes = [c_void_p]

    dll.decoderAttributeIsNormalized.restype = c_bool
    dll.decoderAttributeIsNormalized.argtypes = [c_void_p, c_uint32]

    dll.decoderGetAttributeByteLength.restype = c_size_t
    dll.decoderGetAttributeByteLength.argtypes = [c_void_p, c_uint32]

    dll.decoderCopyAttribute.restype = None
    dll.decoderCopyAttribute.argtypes = [c_void_p, c_uint32, c_void_p]

    dll.decoderReadIndices.restype = c_bool
    dll.decoderReadIndices.argtypes = [c_void_p, c_size_t]

    dll.decoderGetIndicesByteLength.restype = c_size_t
    dll.decoderGetIndicesByteLength.argtypes = [c_void_p]

    dll.decoderCopyIndices.restype = None
    dll.decoderCopyIndices.argtypes = [c_void_p, c_void_p]

    decoder = dll.decoderCreate()
    extension = prim.extensions['KHR_draco_mesh_compression']

    name = prim.name if hasattr(prim, 'name') else '[unnamed]'

    # Create Draco decoder.
    draco_buffer = bytes(BinaryData.get_buffer_view(gltf, extension['bufferView']))
    if not dll.decoderDecode(decoder, draco_buffer, len(draco_buffer)):
        gltf.log.error('Draco Decoder: Unable to decode. Skipping primitive {}.'.format(name))
        return

    # Choose a buffer index which does not yet exist, skipping over existing glTF buffers yet to be loaded
    # and buffers which were generated and did not exist in the initial glTF file, like this decoder does.
    base_buffer_idx = len(gltf.data.buffers)
    for existing_buffer_idx in gltf.buffers:
        if base_buffer_idx <= existing_buffer_idx:
            base_buffer_idx = existing_buffer_idx + 1

    # Read indices.
    index_accessor = gltf.data.accessors[prim.indices]
    if dll.decoderGetIndexCount(decoder) != index_accessor.count:
        gltf.log.warning(
            'Draco Decoder: Index count of accessor and decoded index count does not match. Updating accessor.')
        index_accessor.count = dll.decoderGetIndexCount(decoder)
    if not dll.decoderReadIndices(decoder, index_accessor.component_type):
        gltf.log.error('Draco Decoder: Unable to decode indices. Skipping primitive {}.'.format(name))
        return

    indices_byte_length = dll.decoderGetIndicesByteLength(decoder)
    decoded_data = bytes(indices_byte_length)
    dll.decoderCopyIndices(decoder, decoded_data)

    # Generate a new buffer holding the decoded indices.
    gltf.buffers[base_buffer_idx] = decoded_data

    # Create a buffer view referencing the new buffer.
    gltf.data.buffer_views.append(BufferView.from_dict({
        'buffer': base_buffer_idx,
        'byteLength': indices_byte_length
    }))

    # Update accessor to point to the new buffer view.
    index_accessor.buffer_view = len(gltf.data.buffer_views) - 1

    # Read each attribute.
    for attr_idx, attr in enumerate(extension['attributes']):
        dracoId = extension['attributes'][attr]
        if attr not in prim.attributes:
            gltf.log.error(
                'Draco Decoder: Draco attribute {} not in primitive attributes. Skipping primitive {}.'.format(
                    attr, name))
            return

        accessor = gltf.data.accessors[prim.attributes[attr]]
        if dll.decoderGetVertexCount(decoder) != accessor.count:
            gltf.log.warning(
                'Draco Decoder: Vertex count of accessor and decoded vertex count does not match for attribute {}. Updating accessor.'.format(
                    attr,
                    name))
            accessor.count = dll.decoderGetVertexCount(decoder)
        if not dll.decoderReadAttribute(decoder, dracoId, accessor.component_type, accessor.type.encode()):
            gltf.log.error('Draco Decoder: Could not decode attribute {}. Skipping primitive {}.'.format(attr, name))
            return

        byte_length = dll.decoderGetAttributeByteLength(decoder, dracoId)
        decoded_data = bytes(byte_length)
        dll.decoderCopyAttribute(decoder, dracoId, decoded_data)

        # Generate a new buffer holding the decoded vertex data.
        buffer_idx = base_buffer_idx + 1 + attr_idx
        gltf.buffers[buffer_idx] = decoded_data

        # Create a buffer view referencing the new buffer.
        gltf.data.buffer_views.append(BufferView.from_dict({
            'buffer': buffer_idx,
            'byteLength': byte_length
        }))

        # Update accessor to point to the new buffer view.
        accessor.buffer_view = len(gltf.data.buffer_views) - 1

    dll.decoderRelease(decoder)
