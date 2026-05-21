/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "common.h"

#include <stddef.h>

enum encodeExpMode {
  /** When encoding exponents, use separate values for each component (maximum quality) */
  EncodeExpSeparate,
  /**
   * When encoding exponents, use shared value for all components of each vector
   * (better compression).
   */
  EncodeExpSharedVector,
  /**
   * When encoding exponents, use shared value for each component of all vectors
   * (best compression).
   */
  EncodeExpSharedComponent,
  /**
   * When encoding exponents, use separate values for each component, but clamp to 0 (good quality
   * if very small values are not important).
   */
  EncodeExpClamped,
};

API(void)
encodeIndexVersion(int version);

API(void)
encodeVertexVersion(int version);

API(size_t)
encodeIndexBufferBound(size_t index_count, size_t vertex_count);

API(size_t)
encodeIndexBuffer(unsigned char *buffer,
                  size_t buffer_size,
                  const unsigned int *indices,
                  size_t index_count);

API(size_t)
encodeVertexBufferBound(size_t vertex_count, size_t vertex_size);

API(size_t)
encodeVertexBuffer(unsigned char *buffer,
                   size_t buffer_size,
                   const void *vertices,
                   size_t vertex_count,
                   size_t vertex_size);

API(size_t)
encodeIndexSequenceBound(size_t index_count, size_t vertex_count);

API(size_t)
encodeIndexSequence(unsigned char *buffer,
                    size_t buffer_size,
                    const unsigned int *indices,
                    size_t index_count);

API(void)
encodeFilterOct(void *destination, size_t count, size_t stride, int bits, const float *data);

API(void)
encodeFilterQuat(void *destination, size_t count, size_t stride, int bits, const float *data);

API(void)
encodeFilterExp(void *destination,
                size_t count,
                size_t stride,
                int bits,
                const float *data,
                enum encodeExpMode mode);
