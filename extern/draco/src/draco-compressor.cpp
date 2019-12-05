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
 * @author Jim Eckerlein <eckerlein@ux3d.io>
 * @date   2019-11-29
 */

#include "draco-compressor.h"

#include <memory>
#include <vector>

#include "draco/mesh/mesh.h"
#include "draco/core/encoder_buffer.h"
#include "draco/compression/encode.h"

/**
 * Prefix used for logging messages.
 */
const char *logTag = "DRACO-COMPRESSOR";

struct DracoCompressor {
    draco::Mesh mesh;

    // One data buffer per attribute.
    std::vector<std::unique_ptr<draco::DataBuffer>> buffers;

    // The buffer the mesh is compressed into.
    draco::EncoderBuffer encoderBuffer;

    // Level of compression [0-10].
    // Higher values mean slower encoding.
    uint32_t compressionLevel = 7;

    struct {
        uint32_t positions = 14;
        uint32_t normals = 10;
        uint32_t uvs = 12;
        uint32_t generic = 12;
    } quantization;
};

DracoCompressor *create_compressor() {
    return new DracoCompressor;
}

void set_compression_level(
    DracoCompressor *const compressor,
    uint32_t const compressionLevel
) {
    compressor->compressionLevel = compressionLevel;
}

void set_position_quantization(
    DracoCompressor *const compressor,
    uint32_t const quantizationBitsPosition
) {
    compressor->quantization.positions = quantizationBitsPosition;
}

void set_normal_quantization(
    DracoCompressor *const compressor,
    uint32_t const quantizationBitsNormal
) {
    compressor->quantization.normals = quantizationBitsNormal;
}

void set_uv_quantization(
    DracoCompressor *const compressor,
    uint32_t const quantizationBitsTexCoord
) {
    compressor->quantization.uvs = quantizationBitsTexCoord;
}

void set_generic_quantization(
    DracoCompressor *const compressor,
    uint32_t const bits
) {
    compressor->quantization.generic = bits;
}

bool compress(
    DracoCompressor *const compressor
) {
    draco::Encoder encoder;

    encoder.SetSpeedOptions(10 - (int)compressor->compressionLevel, 10 - (int)compressor->compressionLevel);
    encoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION, compressor->quantization.positions);
    encoder.SetAttributeQuantization(draco::GeometryAttribute::NORMAL, compressor->quantization.normals);
    encoder.SetAttributeQuantization(draco::GeometryAttribute::TEX_COORD, compressor->quantization.uvs);
    encoder.SetAttributeQuantization(draco::GeometryAttribute::GENERIC, compressor->quantization.generic);

    return encoder.EncodeMeshToBuffer(compressor->mesh, &compressor->encoderBuffer).ok();
}

bool compress_morphed(
    DracoCompressor *const compressor
) {
    draco::Encoder encoder;

    encoder.SetSpeedOptions(10 - (int)compressor->compressionLevel, 10 - (int)compressor->compressionLevel);

    // For some reason, `EncodeMeshToBuffer` crashes when not disabling prediction or when enabling quantization
    // for attributes other position.
    encoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION, compressor->quantization.positions);
    encoder.SetAttributePredictionScheme(draco::GeometryAttribute::POSITION, draco::PREDICTION_NONE);
    encoder.SetAttributePredictionScheme(draco::GeometryAttribute::NORMAL, draco::PREDICTION_NONE);
    encoder.SetAttributePredictionScheme(draco::GeometryAttribute::TEX_COORD, draco::PREDICTION_NONE);
    encoder.SetAttributePredictionScheme(draco::GeometryAttribute::GENERIC, draco::PREDICTION_NONE);

    // Enforce triangle order preservation.
    encoder.SetEncodingMethod(draco::MESH_SEQUENTIAL_ENCODING);

    return encoder.EncodeMeshToBuffer(compressor->mesh, &compressor->encoderBuffer).ok();
}

uint64_t get_compressed_size(
    DracoCompressor const *const compressor
) {
    return compressor->encoderBuffer.size();
}

void copy_to_bytes(
    DracoCompressor const *const compressor,
    uint8_t *const o_data
) {
    memcpy(o_data, compressor->encoderBuffer.data(), compressor->encoderBuffer.size());
}

void destroy_compressor(
    DracoCompressor *const compressor
) {
    delete compressor;
}

template<class T>
void set_faces_impl(
    draco::Mesh &mesh,
    int const index_count,
    T const *const indices
) {
    mesh.SetNumFaces((size_t) index_count / 3);

    for (int i = 0; i < index_count; i += 3)
    {
        const auto a = draco::PointIndex(indices[i]);
        const auto b = draco::PointIndex(indices[i + 1]);
        const auto c = draco::PointIndex(indices[i + 2]);
        mesh.SetFace(draco::FaceIndex((uint32_t) i), {a, b, c});
    }
}

void set_faces(
    DracoCompressor *const compressor,
    uint32_t const index_count,
    uint32_t const index_byte_length,
    uint8_t const *const indices
) {
    switch (index_byte_length)
    {
        case 1:
        {
            set_faces_impl(compressor->mesh, index_count, (uint8_t *) indices);
            break;
        }
        case 2:
        {
            set_faces_impl(compressor->mesh, index_count, (uint16_t *) indices);
            break;
        }
        case 4:
        {
            set_faces_impl(compressor->mesh, index_count, (uint32_t *) indices);
            break;
        }
        default:
        {
            printf("%s: Unsupported index size %d\n", logTag, index_byte_length);
            break;
        }
    }
}

uint32_t add_attribute_to_mesh(
    DracoCompressor *const compressor,
    draco::GeometryAttribute::Type const semantics,
    draco::DataType const data_type,
    uint32_t const count,
    uint8_t const component_count,
    uint8_t const component_size,
    uint8_t const *const data
) {
    auto buffer = std::make_unique<draco::DataBuffer>();

    draco::GeometryAttribute attribute;

    attribute.Init(
        semantics,
        &*buffer,
        component_count,
        data_type,
        false,
        component_size * component_count,
        0
    );

    auto const id = (uint32_t)compressor->mesh.AddAttribute(attribute, true, count);

    for (uint32_t i = 0; i < count; i++)
    {
        compressor->mesh.attribute(id)->SetAttributeValue(
            draco::AttributeValueIndex(i),
            data + i * component_count * component_size
        );
    }

    compressor->buffers.emplace_back(std::move(buffer));

    return id;
}

uint32_t add_positions_f32(
    DracoCompressor *const compressor,
    uint32_t const count,
    uint8_t const *const data
) {
    return add_attribute_to_mesh(compressor, draco::GeometryAttribute::POSITION,
        draco::DT_FLOAT32, count, 3, sizeof(float), data);
}

uint32_t add_normals_f32(
    DracoCompressor *const compressor,
    uint32_t const count,
    uint8_t const *const data
) {
    return add_attribute_to_mesh(compressor, draco::GeometryAttribute::NORMAL,
        draco::DT_FLOAT32, count, 3, sizeof(float), data);
}

uint32_t add_uvs_f32(
    DracoCompressor *const compressor,
    uint32_t const count,
    uint8_t const *const data
) {
    return add_attribute_to_mesh(compressor, draco::GeometryAttribute::TEX_COORD,
        draco::DT_FLOAT32, count, 2, sizeof(float), data);
}

uint32_t add_joints_u16(
    DracoCompressor *compressor,
    uint32_t const count,
    uint8_t const *const data
) {
    return add_attribute_to_mesh(compressor, draco::GeometryAttribute::GENERIC,
        draco::DT_UINT16, count, 4, sizeof(uint16_t), data);
}

uint32_t add_weights_f32(
    DracoCompressor *compressor,
    uint32_t const count,
    uint8_t const *const data
) {
    return add_attribute_to_mesh(compressor, draco::GeometryAttribute::GENERIC,
        draco::DT_FLOAT32, count, 4, sizeof(float), data);
}
