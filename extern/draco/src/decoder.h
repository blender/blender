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
 * Library for the Draco encoding/decoding feature inside the glTF-Blender-IO project.
 *
 * The python script within glTF-Blender-IO uses the CTypes library to open the DLL,
 * load function pointers add pass the raw data to the encoder.
 *
 * @author Jim Eckerlein <eckerlein@ux3d.io>
 * @date   2020-11-18
 */

#pragma once

#include "common.h"

struct Decoder;

API(Decoder *) decoderCreate();

API(void) decoderRelease(Decoder *decoder);

API(bool) decoderDecode(Decoder *decoder, void *data, size_t byteLength);

API(uint32_t) decoderGetVertexCount(Decoder *decoder);

API(uint32_t) decoderGetIndexCount(Decoder *decoder);

API(bool) decoderAttributeIsNormalized(Decoder *decoder, uint32_t id);

API(bool) decoderReadAttribute(Decoder *decoder, uint32_t id, size_t componentType, char *dataType);

API(size_t) decoderGetAttributeByteLength(Decoder *decoder, size_t id);

API(void) decoderCopyAttribute(Decoder *decoder, size_t id, void *output);

API(bool) decoderReadIndices(Decoder *decoder, size_t indexComponentType);

API(size_t) decoderGetIndicesByteLength(Decoder *decoder);

API(void) decoderCopyIndices(Decoder *decoder, void *output);
