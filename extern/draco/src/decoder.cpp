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
 * @date   2020-11-18
 */


#include "decoder.h"

#include <memory>
#include <vector>
#include <cinttypes>

#include "draco/mesh/mesh.h"
#include "draco/core/decoder_buffer.h"
#include "draco/compression/decode.h"

#define LOG_PREFIX "DracoDecoder | "

struct Decoder {
    std::unique_ptr<draco::Mesh> mesh;
    std::vector<uint8_t> indexBuffer;
    std::map<uint32_t, std::vector<uint8_t>> buffers;
    draco::DecoderBuffer decoderBuffer;
    uint32_t vertexCount;
    uint32_t indexCount;
};

Decoder *decoderCreate()
{
    return new Decoder;
}

void decoderRelease(Decoder *decoder)
{
    delete decoder;
}

bool decoderDecode(Decoder *decoder, void *data, size_t byteLength)
{
    draco::Decoder dracoDecoder;
    draco::DecoderBuffer dracoDecoderBuffer;
    dracoDecoderBuffer.Init(reinterpret_cast<char *>(data), byteLength);
    
    auto decoderStatus = dracoDecoder.DecodeMeshFromBuffer(&dracoDecoderBuffer);
    if (!decoderStatus.ok())
    {
        printf(LOG_PREFIX "Error during Draco decoding: %s\n", decoderStatus.status().error_msg());
        return false;
    }
    
    decoder->mesh = std::move(decoderStatus).value();
    decoder->vertexCount = decoder->mesh->num_points();
    decoder->indexCount = decoder->mesh->num_faces() * 3;
    
    printf(LOG_PREFIX "Decoded %" PRIu32 " vertices, %" PRIu32 " indices\n", decoder->vertexCount, decoder->indexCount);
    
    return true;
}

uint32_t decoderGetVertexCount(Decoder *decoder)
{
    return decoder->vertexCount;
}

uint32_t decoderGetIndexCount(Decoder *decoder)
{
    return decoder->indexCount;
}

bool decoderAttributeIsNormalized(Decoder *decoder, uint32_t id)
{
    const draco::PointAttribute* attribute = decoder->mesh->GetAttributeByUniqueId(id);
    return attribute != nullptr && attribute->normalized();
}

bool decoderReadAttribute(Decoder *decoder, uint32_t id, size_t componentType, char *dataType)
{
    const draco::PointAttribute* attribute = decoder->mesh->GetAttributeByUniqueId(id);
    
    if (attribute == nullptr)
    {
        printf(LOG_PREFIX "Attribute with id=%" PRIu32 " does not exist in Draco data\n", id);
        return false;
    }
    
    size_t stride = getAttributeStride(componentType, dataType);
    
    std::vector<uint8_t> decodedData;
    decodedData.resize(stride * decoder->vertexCount);
    
    for (uint32_t i = 0; i < decoder->vertexCount; ++i)
    {
        auto index = attribute->mapped_index(draco::PointIndex(i));
        uint8_t *value = decodedData.data() + i * stride;
        
        bool converted = false;
        
        switch (componentType)
        {
        case ComponentType::Byte:
            converted = attribute->ConvertValue(index, reinterpret_cast<int8_t *>(value));
            break;
        case ComponentType::UnsignedByte:
            converted = attribute->ConvertValue(index, reinterpret_cast<uint8_t *>(value));
            break;
        case ComponentType::Short:
            converted = attribute->ConvertValue(index, reinterpret_cast<int16_t *>(value));
            break;
        case ComponentType::UnsignedShort:
            converted = attribute->ConvertValue(index, reinterpret_cast<uint16_t *>(value));
            break;
        case ComponentType::UnsignedInt:
            converted = attribute->ConvertValue(index, reinterpret_cast<uint32_t *>(value));
            break;
        case ComponentType::Float:
            converted = attribute->ConvertValue(index, reinterpret_cast<float *>(value));
            break;
        default:
            break;
        }

        if (!converted)
        {
            printf(LOG_PREFIX "Failed to convert Draco attribute type to glTF accessor type for attribute with id=%" PRIu32 "\n", id);
            return false;
        }
    }
    
    decoder->buffers[id] = decodedData;
    return true;
}

size_t decoderGetAttributeByteLength(Decoder *decoder, size_t id)
{
    auto iter = decoder->buffers.find(id);
    if (iter != decoder->buffers.end())
    {
        return iter->second.size();
    }
    else
    {
        return 0;
    }
}

void decoderCopyAttribute(Decoder *decoder, size_t id, void *output)
{
    auto iter = decoder->buffers.find(id);
    if (iter != decoder->buffers.end())
    {
        memcpy(output, iter->second.data(), iter->second.size());
    }
}

template<class T>
void decodeIndices(Decoder *decoder)
{
    std::vector<uint8_t> decodedIndices;
    decodedIndices.resize(decoder->indexCount * sizeof(T));
    T *typedView = reinterpret_cast<T *>(decodedIndices.data());
    
    for (uint32_t faceIndex = 0; faceIndex < decoder->mesh->num_faces(); ++faceIndex)
    {
        const draco::Mesh::Face &face = decoder->mesh->face(draco::FaceIndex(faceIndex));
        typedView[faceIndex * 3 + 0] = face[0].value();
        typedView[faceIndex * 3 + 1] = face[1].value();
        typedView[faceIndex * 3 + 2] = face[2].value();
    }
    
    decoder->indexBuffer = decodedIndices;
}

bool decoderReadIndices(Decoder *decoder, size_t indexComponentType)
{
    switch (indexComponentType)
    {
        case ComponentType::Byte:
            decodeIndices<int8_t>(decoder);
            break;
        case ComponentType::UnsignedByte:
            decodeIndices<uint8_t>(decoder);
            break;
        case ComponentType::Short:
            decodeIndices<int16_t>(decoder);
            break;
        case ComponentType::UnsignedShort:
            decodeIndices<uint16_t>(decoder);
            break;
        case ComponentType::UnsignedInt:
            decodeIndices<uint32_t>(decoder);
            break;
        default:
            printf(LOG_PREFIX "Index component type %zu not supported\n", indexComponentType);
            return false;
    }
    
    return true;
}

size_t decoderGetIndicesByteLength(Decoder *decoder)
{
    return decoder->indexBuffer.size();
}

void decoderCopyIndices(Decoder *decoder, void *output)
{
    memcpy(output, decoder->indexBuffer.data(), decoder->indexBuffer.size());
}
