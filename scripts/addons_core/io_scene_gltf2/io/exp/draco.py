# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

from ctypes import *
from pathlib import Path

from ...io.exp.binary_data import BinaryData
from ...io.com.draco import dll_path


def encode_scene_primitives(scenes, export_settings):
    """
    Handles draco compression.
    Moves position, normal and texture coordinate attributes into a Draco encoded buffer.
    """

    # Load DLL and setup function signatures.
    dll = cdll.LoadLibrary(str(dll_path().resolve()))

    dll.encoderCreate.restype = c_void_p
    dll.encoderCreate.argtypes = [c_uint32]

    dll.encoderRelease.restype = None
    dll.encoderRelease.argtypes = [c_void_p]

    dll.encoderSetCompressionLevel.restype = None
    dll.encoderSetCompressionLevel.argtypes = [c_void_p, c_uint32]

    dll.encoderSetQuantizationBits.restype = None
    dll.encoderSetQuantizationBits.argtypes = [c_void_p, c_uint32, c_uint32, c_uint32, c_uint32, c_uint32]

    dll.encoderSetIndices.restype = None
    dll.encoderSetIndices.argtypes = [c_void_p, c_size_t, c_uint32, c_void_p]

    dll.encoderSetAttribute.restype = c_uint32
    dll.encoderSetAttribute.argtypes = [c_void_p, c_char_p, c_size_t, c_char_p, c_void_p, c_bool]

    dll.encoderEncode.restype = c_bool
    dll.encoderEncode.argtypes = [c_void_p, c_uint8]

    dll.encoderGetEncodedVertexCount.restype = c_uint32
    dll.encoderGetEncodedVertexCount.argtypes = [c_void_p]

    dll.encoderGetEncodedIndexCount.restype = c_uint32
    dll.encoderGetEncodedIndexCount.argtypes = [c_void_p]

    dll.encoderGetByteLength.restype = c_uint64
    dll.encoderGetByteLength.argtypes = [c_void_p]

    dll.encoderCopy.restype = None
    dll.encoderCopy.argtypes = [c_void_p, c_void_p]

    # Don't encode the same primitive multiple times.
    encoded_primitives_cache = {}

    # Compress meshes into Draco buffers.
    for scene in scenes:
        for node in scene.nodes:
            __traverse_node(node, lambda node: __encode_node(node, dll, export_settings, encoded_primitives_cache))

    # Release uncompressed index and attribute buffers.
    # Since those buffers may be shared across nodes, this step must happen after all meshes have been compressed.
    for scene in scenes:
        for node in scene.nodes:
            __traverse_node(node, lambda node: __cleanup_node(node))


def __cleanup_node(node):
    if node.mesh is None:
        return

    for primitive in node.mesh.primitives:
        if primitive.extensions is None or primitive.extensions['KHR_draco_mesh_compression'] is None:
            continue

        primitive.indices.buffer_view = None
        for attr_name in primitive.attributes:
            attr = primitive.attributes[attr_name]
            attr.buffer_view = None


def __traverse_node(node, f):
    f(node)
    if node.children is not None:
        for child in node.children:
            __traverse_node(child, f)


def __encode_node(node, dll, export_settings, encoded_primitives_cache):
    if node.mesh is not None:
        export_settings['log'].info('Draco encoder: Encoding mesh {}.'.format(node.name))
        for primitive in node.mesh.primitives:
            __encode_primitive(primitive, dll, export_settings, encoded_primitives_cache)


def __encode_primitive(primitive, dll, export_settings, encoded_primitives_cache):
    attributes = primitive.attributes
    indices = primitive.indices

    # Check if this primitive has already been encoded.
    # This usually happens when nodes are duplicated in Blender, thus their indices/attributes are shared data.
    if primitive in encoded_primitives_cache:
        if primitive.extensions is None:
            primitive.extensions = {}
        primitive.extensions['KHR_draco_mesh_compression'] = encoded_primitives_cache[primitive]
        return

    # Only do TRIANGLES primitives
    if primitive.mode not in [None, 4]:
        return

    if 'POSITION' not in attributes:
        export_settings['log'].warning('Draco encoder: Primitive without positions encountered. Skipping.')
        return

    positions = attributes['POSITION']

    # Skip nodes without a position buffer, e.g. a primitive from a Blender shared instance.
    if attributes['POSITION'].buffer_view is None:
        return

    encoder = dll.encoderCreate(positions.count)

    draco_ids = {}
    for attr_name in attributes:
        attr = attributes[attr_name]
        draco_id = dll.encoderSetAttribute(
            encoder,
            attr_name.encode(),
            attr.component_type,
            attr.type.encode(),
            attr.buffer_view.data,
            attr.normalized)
        draco_ids[attr_name] = draco_id

    dll.encoderSetIndices(encoder, indices.component_type, indices.count, indices.buffer_view.data)

    dll.encoderSetCompressionLevel(encoder, export_settings['gltf_draco_mesh_compression_level'])
    dll.encoderSetQuantizationBits(encoder,
                                   export_settings['gltf_draco_position_quantization'],
                                   export_settings['gltf_draco_normal_quantization'],
                                   export_settings['gltf_draco_texcoord_quantization'],
                                   export_settings['gltf_draco_color_quantization'],
                                   export_settings['gltf_draco_generic_quantization'])

    preserve_triangle_order = primitive.targets is not None and len(primitive.targets) > 0
    if not dll.encoderEncode(encoder, preserve_triangle_order):
        export_settings['log'].error('Could not encode primitive. Skipping primitive.')

    byte_length = dll.encoderGetByteLength(encoder)
    encoded_data = bytes(byte_length)
    dll.encoderCopy(encoder, encoded_data)

    if primitive.extensions is None:
        primitive.extensions = {}

    extension_info = {
        'bufferView': BinaryData(encoded_data),
        'attributes': draco_ids
    }
    primitive.extensions['KHR_draco_mesh_compression'] = extension_info
    encoded_primitives_cache[primitive] = extension_info

    # Set to triangle list mode.
    primitive.mode = 4

    # Update accessors to match encoded data.
    indices.count = dll.encoderGetEncodedIndexCount(encoder)
    encoded_vertices = dll.encoderGetEncodedVertexCount(encoder)
    for attr_name in attributes:
        attributes[attr_name].count = encoded_vertices

    dll.encoderRelease(encoder)
