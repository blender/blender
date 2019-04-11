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
 * Implemententation for the Draco exporter from the C++ side.
 *
 * The python side uses the CTypes libary to open the DLL, load function
 * pointers add pass the data to the compressor as raw bytes.
 *
 * The compressor intercepts the regular GLTF exporter after data has been
 * gathered and right before the data is converted to a JSON representation,
 * which is going to be written out.
 *
 * The original uncompressed data is removed and replaces an extension,
 * pointing to the newly created buffer containing the compressed data.
 *
 * @author Jim Eckerlein <eckerlein@ux3d.io>
 * @date   2019-01-15
 */

#include <iostream>
#include <fstream>
#include <memory>
#include <sstream>

#include "draco/mesh/mesh.h"
#include "draco/point_cloud/point_cloud.h"
#include "draco/core/vector_d.h"
#include "draco/io/mesh_io.h"

#if defined(_MSC_VER)
#define DLL_EXPORT(retType) extern "C" __declspec(dllexport) retType __cdecl
#else
#define DLL_EXPORT(retType) extern "C" retType
#endif

const char *logTag = "DRACO-COMPRESSOR";

/**
 * This tuple is opaquely exposed to Python through a pointer.
 * It encapsulates the complete current compressor state.
 *
 * A single instance is only intended to compress a single primitive.
 */
struct DracoCompressor {

    /**
     * All positions, normals and texture coordinates are appended to this mesh.
     */
    draco::Mesh mesh;

    /**
     * One data buffer per attribute.
     */
    std::vector<std::unique_ptr<draco::DataBuffer>> buffers;

    /**
     * The buffer the mesh is compressed into.
     */
    draco::EncoderBuffer encoderBuffer;

    /**
     * The id Draco assigns to the position attribute.
     * Required to be reported in the GLTF file.
     */
    uint32_t positionAttributeId = (uint32_t) -1;

    /**
     * The id Draco assigns to the normal attribute.
     * Required to be reported in the GLTF file.
     */
    uint32_t normalAttributeId = (uint32_t) -1;

    /**
     * The ids Draco assigns to the texture coordinate attributes.
     * Required to be reported in the GLTF file.
     */
    std::vector<uint32_t> texCoordAttributeIds;

    /**
     * Level of compression [0-10].
     * Higher values mean slower encoding.
     */
    uint32_t compressionLevel = 7;

    uint32_t quantizationBitsPosition = 14;
    uint32_t quantizationBitsNormal = 10;
    uint32_t quantizationBitsTexCoord = 12;
};

draco::GeometryAttribute createAttribute(
        draco::GeometryAttribute::Type type,
        draco::DataBuffer &buffer,
        uint8_t components
) {
    draco::GeometryAttribute attribute;
    attribute.Init(
            type,
            &buffer,
            components,
            draco::DataType::DT_FLOAT32,
            false,
            sizeof(float) * components,
            0
    );
    return attribute;
}

DLL_EXPORT(DracoCompressor *) createCompressor() {
    return new DracoCompressor;
}

DLL_EXPORT(void) setCompressionLevel(
        DracoCompressor *compressor,
        uint32_t compressionLevel
) {
    compressor->compressionLevel = compressionLevel;
}

DLL_EXPORT(void) setPositionQuantizationBits(
        DracoCompressor *compressor,
        uint32_t quantizationBitsPosition
) {
    compressor->quantizationBitsPosition = quantizationBitsPosition;
}

DLL_EXPORT(void) setNormalQuantizationBits(
        DracoCompressor *compressor,
        uint32_t quantizationBitsNormal
) {
    compressor->quantizationBitsNormal = quantizationBitsNormal;
}

DLL_EXPORT(void) setTexCoordQuantizationBits(
        DracoCompressor *compressor,
        uint32_t quantizationBitsTexCoord
) {
    compressor->quantizationBitsTexCoord = quantizationBitsTexCoord;
}

DLL_EXPORT(bool) compress(
        DracoCompressor *compressor
) {
    printf("%s: Compressing primitive:\n", logTag);
    printf("%s: Compression level [0-10]:   %d\n", logTag, compressor->compressionLevel);
    printf("%s: Position quantization bits: %d\n", logTag, compressor->quantizationBitsPosition);
    printf("%s: Normal quantization bits:   %d\n", logTag, compressor->quantizationBitsNormal);
    printf("%s: Position quantization bits: %d\n", logTag, compressor->quantizationBitsTexCoord);

    draco::ExpertEncoder encoder(compressor->mesh);

    encoder.SetSpeedOptions(10 - compressor->compressionLevel, 10 - compressor->compressionLevel);
    encoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION, compressor->quantizationBitsPosition);
    encoder.SetAttributeQuantization(draco::GeometryAttribute::NORMAL, compressor->quantizationBitsNormal);
    encoder.SetAttributeQuantization(draco::GeometryAttribute::TEX_COORD, compressor->quantizationBitsTexCoord);

    encoder.SetEncodingMethod(draco::MESH_EDGEBREAKER_ENCODING);

    draco::Status result = encoder.EncodeToBuffer(&compressor->encoderBuffer);

    if(!result.ok()) {
        printf("%s: Could not compress mesh: %s\n", logTag, result.error_msg());
        return false;
    }
    else {
        return true;
    }
}

/**
 * Returns the size of the compressed data in bytes.
 */
DLL_EXPORT(uint64_t) compressedSize(
        DracoCompressor *compressor
) {
    return compressor->encoderBuffer.size();
}

/**
 * Copies the compressed mesh into the given byte buffer.
 * @param[o_data] A Python `bytes` object.
 *
 */
DLL_EXPORT(void) copyToBytes(
        DracoCompressor *compressor,
        uint8_t *o_data
) {
    memcpy(o_data, compressor->encoderBuffer.data(), compressedSize(compressor));
}

DLL_EXPORT(uint32_t) getPositionAttributeId(
        DracoCompressor *compressor
) {
    return compressor->positionAttributeId;
}

DLL_EXPORT(uint32_t) getNormalAttributeId(
        DracoCompressor *compressor
) {
    return compressor->normalAttributeId;
}

DLL_EXPORT(uint32_t) getTexCoordAttributeIdCount(
        DracoCompressor *compressor
) {
    return (uint32_t) compressor->texCoordAttributeIds.size();
}

DLL_EXPORT(uint32_t) getTexCoordAttributeId(
        DracoCompressor *compressor,
        uint32_t index
) {
    return compressor->texCoordAttributeIds[index];
}

/**
 * Releases all memory allocated by the compressor.
 */
DLL_EXPORT(void) disposeCompressor(
        DracoCompressor *compressor
) {
    delete compressor;
}

template<class T>
void setFaces(
        draco::Mesh &mesh,
        int numIndices,
        T *indices
) {
    mesh.SetNumFaces((size_t) numIndices / 3);

    for (int i = 0; i < numIndices; i += 3)
    {
        const auto a = draco::PointIndex(indices[i]);
        const auto b = draco::PointIndex(indices[i + 1]);
        const auto c = draco::PointIndex(indices[i + 2]);
        mesh.SetFace(draco::FaceIndex((uint32_t) i), {a, b, c});
    }
}

DLL_EXPORT(void) setFaces(
        DracoCompressor *compressor,
        uint32_t numIndices,
        uint32_t bytesPerIndex,
        void *indices
) {
    switch (bytesPerIndex)
    {
        case 1:
        {
            setFaces(compressor->mesh, numIndices, (uint8_t *) indices);
            break;
        }
        case 2:
        {
            setFaces(compressor->mesh, numIndices, (uint16_t *) indices);
            break;
        }
        case 4:
        {
            setFaces(compressor->mesh, numIndices, (uint32_t *) indices);
            break;
        }
        default:
        {
            printf("%s: Unsupported index size %d\n", logTag, bytesPerIndex);
            break;
        }
    }
}

void addFloatAttribute(
        DracoCompressor *compressor,
        draco::GeometryAttribute::Type type,
        uint32_t count,
        uint8_t componentCount,
        float *source
) {
    auto buffer = std::make_unique<draco::DataBuffer>();

    const auto attribute = createAttribute(type, *buffer, componentCount);

    const auto id = (const uint32_t) compressor->mesh.AddAttribute(attribute, false, count);
    compressor->mesh.attribute(id)->SetIdentityMapping();

    switch (type)
    {
        case draco::GeometryAttribute::POSITION:
            compressor->positionAttributeId = id;
            break;
        case draco::GeometryAttribute::NORMAL:
            compressor->normalAttributeId = id;
            break;
        case draco::GeometryAttribute::TEX_COORD:
            compressor->texCoordAttributeIds.push_back(id);
            break;
        default:
            break;
    }

    for (uint32_t i = 0; i < count; i++)
    {
        compressor->mesh.attribute(id)->SetAttributeValue(
                draco::AttributeValueIndex(i),
                source + i * componentCount
        );
    }

    compressor->buffers.emplace_back(std::move(buffer));
}

DLL_EXPORT(void) addPositionAttribute(
        DracoCompressor *compressor,
        uint32_t count,
        float *source
) {
    addFloatAttribute(compressor, draco::GeometryAttribute::POSITION, count, 3, source);
}

DLL_EXPORT(void) addNormalAttribute(
        DracoCompressor *compressor,
        uint32_t count,
        float *source
) {
    addFloatAttribute(compressor, draco::GeometryAttribute::NORMAL, count, 3, source);
}

DLL_EXPORT(void) addTexCoordAttribute(
        DracoCompressor *compressor,
        uint32_t count,
        float *source
) {
    addFloatAttribute(compressor, draco::GeometryAttribute::TEX_COORD, count, 2, source);
}
