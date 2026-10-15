/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "common.h"

#include <ktx.h>

#include <stddef.h>

/* Loads a KTX2 file and, if needed (e.g. Basis Universal supercompression),
 * transcodes it to raw RGBA8 so `getImageDataFloat()` always returns plain pixels. */
API(int)
decodeFromFile(const char *filename, ktxTexture2 **newTex);

API(int)
getImageInfo(ktxTexture2 *texture, unsigned int *width, unsigned int *height, size_t *dataSize);

/* Normalizes each RGBA8 byte to a [0, 1] float and flips rows so the result is bottom-to-top,
 * matching Blender's `ImBuf`/`Image.pixels` convention (KTX stores rows top-to-bottom).
 * `destinationSize` is in bytes. */
API(int)
getImageDataFloat(ktxTexture2 *texture, float *destination, size_t destinationSize);

API(void)
destroyTexture(ktxTexture2 *texture);
