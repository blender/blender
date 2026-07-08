/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

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

API(Decoder *)
decoderCreate();

API(void)
decoderRelease(Decoder *decoder);

API(bool)
decoderDecode(Decoder *decoder, void *data, size_t byteLength);

API(uint32_t)
decoderGetVertexCount(Decoder *decoder);

API(uint32_t)
decoderGetIndexCount(Decoder *decoder);

API(bool)
decoderAttributeIsNormalized(Decoder *decoder, uint32_t id);

API(bool)
decoderReadAttribute(Decoder *decoder, uint32_t id, size_t componentType, char *dataType);

API(size_t)
decoderGetAttributeByteLength(Decoder *decoder, size_t id);

API(void)
decoderCopyAttribute(Decoder *decoder, size_t id, void *output);

API(bool)
decoderReadIndices(Decoder *decoder, size_t indexComponentType);

API(size_t)
decoderGetIndicesByteLength(Decoder *decoder);

API(void)
decoderCopyIndices(Decoder *decoder, void *output);
