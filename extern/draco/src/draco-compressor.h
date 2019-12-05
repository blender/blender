/*
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * C++ library for the Draco compression feature inside the glTF-Blender-IO project.
 *
 * The python side uses the CTypes library to open the DLL, load function
 * pointers add pass the data to the compressor as raw bytes.
 *
 * The compressor intercepts the regular glTF exporter after data has been
 * gathered and right before the data is converted to a JSON representation,
 * which is going to be written out.
 *
 * The original uncompressed data is removed and replaces an extension,
 * pointing to the newly created buffer containing the compressed data.
 *
 * @author Jim Eckerlein <eckerlein@ux3d.io>
 * @date   2019-11-29
 */

#include <cstdint>

#if defined(_MSC_VER)
#define DLL_EXPORT(retType) extern "C" __declspec(dllexport) retType __cdecl
#else
#define DLL_EXPORT(retType) extern "C" retType
#endif

/**
 * This tuple is opaquely exposed to Python through a pointer.
 * It encapsulates the complete current compressor state.
 *
 * A single instance is only intended to compress a single primitive.
 */
struct DracoCompressor;

DLL_EXPORT(DracoCompressor *)
create_compressor ();

DLL_EXPORT(void)
set_compression_level (
    DracoCompressor *compressor,
    uint32_t compressionLevel
);

DLL_EXPORT(void)
set_position_quantization (
    DracoCompressor *compressor,
    uint32_t quantizationBitsPosition
);

DLL_EXPORT(void)
set_normal_quantization (
    DracoCompressor *compressor,
    uint32_t quantizationBitsNormal
);

DLL_EXPORT(void)
set_uv_quantization (
    DracoCompressor *compressor,
    uint32_t quantizationBitsTexCoord
);

DLL_EXPORT(void)
set_generic_quantization (
    DracoCompressor *compressor,
    uint32_t bits
);

/// Compresses a mesh.
/// Use `compress_morphed` when compressing primitives which have morph targets.
DLL_EXPORT(bool)
compress (
    DracoCompressor *compressor
);

/// Compresses the mesh.
/// Use this instead of `compress`, because this procedure takes into account that mesh triangles must not be reordered.
DLL_EXPORT(bool)
compress_morphed (
    DracoCompressor *compressor
);

/**
 * Returns the size of the compressed data in bytes.
 */
DLL_EXPORT(uint64_t)
get_compressed_size (
    DracoCompressor const *compressor
);

/**
 * Copies the compressed mesh into the given byte buffer.
 *
 * @param[o_data] A Python `bytes` object.
 */
DLL_EXPORT(void)
copy_to_bytes (
    DracoCompressor const *compressor,
    uint8_t *o_data
);

/**
 * Releases all memory allocated by the compressor.
 */
DLL_EXPORT(void)
destroy_compressor (
    DracoCompressor *compressor
);

DLL_EXPORT(void)
set_faces (
    DracoCompressor *compressor,
    uint32_t index_count,
    uint32_t index_byte_length,
    uint8_t const *indices
);

/// Adds a `float` position attribute to the current mesh.
/// Returns the id Draco has assigned to this attribute.
DLL_EXPORT(uint32_t)
add_positions_f32 (
    DracoCompressor *compressor,
    uint32_t count,
    uint8_t const *data
);

/// Adds a `float` normal attribute to the current mesh.
/// Returns the id Draco has assigned to this attribute.
DLL_EXPORT(uint32_t)
add_normals_f32 (
    DracoCompressor *compressor,
    uint32_t count,
    uint8_t const *data
);

/// Adds a `float` texture coordinate attribute to the current mesh.
/// Returns the id Draco has assigned to this attribute.
DLL_EXPORT(uint32_t)
add_uvs_f32 (
    DracoCompressor *compressor,
    uint32_t count,
    uint8_t const *data
);

/// Adds a `unsigned short` joint attribute to the current mesh.
/// Returns the id Draco has assigned to this attribute.
DLL_EXPORT(uint32_t)
add_joints_u16 (
    DracoCompressor *compressor,
    uint32_t count,
    uint8_t const *data
);

/// Adds a `float` weight attribute to the current mesh.
/// Returns the id Draco has assigned to this attribute.
DLL_EXPORT(uint32_t)
add_weights_f32 (
    DracoCompressor *compressor,
    uint32_t count,
    uint8_t const *data
);
