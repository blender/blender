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
 * @date   2019-11-18
 */

#include "encoder.h"

#include <memory>
#include <vector>

#include "draco/mesh/mesh.h"
#include "draco/core/encoder_buffer.h"
#include "draco/compression/encode.h"

#define LOG_PREFIX "DracoEncoder | "

struct Encoder
{
    draco::Mesh mesh;
    uint32_t encodedVertices;
    uint32_t encodedIndices;
    std::vector<std::unique_ptr<draco::DataBuffer>> buffers;
    draco::EncoderBuffer encoderBuffer;
    uint32_t compressionLevel = 7;
    size_t rawSize = 0;
    struct
    {
        uint32_t position = 14;
        uint32_t normal = 10;
        uint32_t uv = 12;
        uint32_t color = 10;
        uint32_t generic = 12;
    } quantization;
};

Encoder *encoderCreate(uint32_t vertexCount)
{
    Encoder *encoder = new Encoder;
    encoder->mesh.set_num_points(vertexCount);
    return encoder;
}

void encoderRelease(Encoder *encoder)
{
    delete encoder;
}

void encoderSetCompressionLevel(Encoder *encoder, uint32_t compressionLevel) {
    encoder->compressionLevel = compressionLevel;
}

void encoderSetQuantizationBits(Encoder *encoder, uint32_t position, uint32_t normal, uint32_t uv, uint32_t color, uint32_t generic)
{
    encoder->quantization.position = position;
    encoder->quantization.normal = normal;
    encoder->quantization.uv = uv;
    encoder->quantization.color = color;
    encoder->quantization.generic = generic;
}

bool encoderEncode(Encoder *encoder, uint8_t preserveTriangleOrder)
{
    printf(LOG_PREFIX "Preserve triangle order: %s\n", preserveTriangleOrder ? "yes" : "no");
    
    draco::Encoder dracoEncoder;

    int speed = 10 - static_cast<int>(encoder->compressionLevel);
    dracoEncoder.SetSpeedOptions(speed, speed);

    dracoEncoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION, encoder->quantization.position);
    dracoEncoder.SetAttributeQuantization(draco::GeometryAttribute::NORMAL, encoder->quantization.normal);
    dracoEncoder.SetAttributeQuantization(draco::GeometryAttribute::TEX_COORD, encoder->quantization.uv);
    dracoEncoder.SetAttributeQuantization(draco::GeometryAttribute::COLOR, encoder->quantization.color);
    dracoEncoder.SetAttributeQuantization(draco::GeometryAttribute::GENERIC, encoder->quantization.generic);
    dracoEncoder.SetTrackEncodedProperties(true);
    
    if (preserveTriangleOrder)
    {
        dracoEncoder.SetEncodingMethod(draco::MESH_SEQUENTIAL_ENCODING);
    }
    
    auto encoderStatus = dracoEncoder.EncodeMeshToBuffer(encoder->mesh, &encoder->encoderBuffer);
    if (encoderStatus.ok())
    {
        encoder->encodedVertices = static_cast<uint32_t>(dracoEncoder.num_encoded_points());
        encoder->encodedIndices = static_cast<uint32_t>(dracoEncoder.num_encoded_faces() * 3);
        size_t encodedSize = encoder->encoderBuffer.size();
        float compressionRatio = static_cast<float>(encoder->rawSize) / static_cast<float>(encodedSize);
        printf(LOG_PREFIX "Encoded %" PRIu32 " vertices, %" PRIu32 " indices, raw size: %zu, encoded size: %zu, compression ratio: %.2f\n", encoder->encodedVertices, encoder->encodedIndices, encoder->rawSize, encodedSize, compressionRatio);
        return true;
    }
    else
    {
        printf(LOG_PREFIX "Error during Draco encoding: %s\n", encoderStatus.error_msg());
        return false;
    }
}

uint32_t encoderGetEncodedVertexCount(Encoder *encoder)
{
    return encoder->encodedVertices;
}

uint32_t encoderGetEncodedIndexCount(Encoder *encoder)
{
    return encoder->encodedIndices;
}

uint64_t encoderGetByteLength(Encoder *encoder)
{
    return encoder->encoderBuffer.size();
}

void encoderCopy(Encoder *encoder, uint8_t *data)
{
    memcpy(data, encoder->encoderBuffer.data(), encoder->encoderBuffer.size());
}

template<class T>
void encodeIndices(Encoder *encoder, uint32_t indexCount, T *indices)
{
    int face_count = indexCount / 3;
    encoder->mesh.SetNumFaces(static_cast<size_t>(face_count));
    encoder->rawSize += indexCount * sizeof(T);
    
    for (int i = 0; i < face_count; ++i)
    {
        draco::Mesh::Face face = {
            draco::PointIndex(indices[3 * i + 0]),
            draco::PointIndex(indices[3 * i + 1]),
            draco::PointIndex(indices[3 * i + 2])
        };
        encoder->mesh.SetFace(draco::FaceIndex(static_cast<uint32_t>(i)), face);
    }
}

void encoderSetIndices(Encoder *encoder, size_t indexComponentType, uint32_t indexCount, void *indices)
{
    switch (indexComponentType)
    {
        case ComponentType::Byte:
            encodeIndices(encoder, indexCount, reinterpret_cast<int8_t *>(indices));
            break;
        case ComponentType::UnsignedByte:
            encodeIndices(encoder, indexCount, reinterpret_cast<uint8_t *>(indices));
            break;
        case ComponentType::Short:
            encodeIndices(encoder, indexCount, reinterpret_cast<int16_t *>(indices));
            break;
        case ComponentType::UnsignedShort:
            encodeIndices(encoder, indexCount, reinterpret_cast<uint16_t *>(indices));
            break;
        case ComponentType::UnsignedInt:
            encodeIndices(encoder, indexCount, reinterpret_cast<uint32_t *>(indices));
            break;
        default:
            printf(LOG_PREFIX "Index component type %zu not supported\n", indexComponentType);
    }
}

draco::GeometryAttribute::Type getAttributeSemantics(char *attribute)
{
    if (!strcmp(attribute, "POSITION"))
    {
        return draco::GeometryAttribute::POSITION;
    }
    if (!strcmp(attribute, "NORMAL"))
    {
        return draco::GeometryAttribute::NORMAL;
    }
    if (!strncmp(attribute, "TEXCOORD", strlen("TEXCOORD")))
    {
        return draco::GeometryAttribute::TEX_COORD;
    }
    if (!strncmp(attribute, "COLOR", strlen("COLOR")))
    {
        return draco::GeometryAttribute::COLOR;
    }
    
    return draco::GeometryAttribute::GENERIC;
}

draco::DataType getDataType(size_t componentType)
{
    switch (componentType)
    {
        case ComponentType::Byte:
            return draco::DataType::DT_INT8;
            
        case ComponentType::UnsignedByte:
            return draco::DataType::DT_UINT8;
            
        case ComponentType::Short:
            return draco::DataType::DT_INT16;
            
        case ComponentType::UnsignedShort:
            return draco::DataType::DT_UINT16;
            
        case ComponentType::UnsignedInt:
            return draco::DataType::DT_UINT32;
            
        case ComponentType::Float:
            return draco::DataType::DT_FLOAT32;
            
        default:
            return draco::DataType::DT_INVALID;
    }
}

API(uint32_t) encoderSetAttribute(Encoder *encoder, char *attributeName, size_t componentType, char *dataType, void *data)
{
    auto buffer = std::make_unique<draco::DataBuffer>();
    uint32_t count = encoder->mesh.num_points();
    size_t componentCount = getNumberOfComponents(dataType);
    size_t stride = getAttributeStride(componentType, dataType);
    draco::DataType dracoDataType = getDataType(componentType);
    
    draco::GeometryAttribute::Type semantics = getAttributeSemantics(attributeName);
    draco::GeometryAttribute attribute;
    attribute.Init(semantics, &*buffer, componentCount, getDataType(componentType), false, stride, 0);

    auto id = static_cast<uint32_t>(encoder->mesh.AddAttribute(attribute, true, count));
    auto dataBytes = reinterpret_cast<uint8_t *>(data);

    for (uint32_t i = 0; i < count; i++)
    {
        encoder->mesh.attribute(id)->SetAttributeValue(draco::AttributeValueIndex(i), dataBytes + i * stride);
    }

    encoder->buffers.emplace_back(std::move(buffer));
    encoder->rawSize += count * stride;
    return id;
}
