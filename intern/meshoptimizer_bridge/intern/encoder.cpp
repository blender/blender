/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "encoder.h"

#include <meshoptimizer.h>

void encodeIndexVersion(int version)
{
  meshopt_encodeIndexVersion(version);
}

void encodeVertexVersion(int version)
{
  meshopt_encodeVertexVersion(version);
}

size_t encodeIndexBufferBound(size_t index_count, size_t vertex_count)
{
  return meshopt_encodeIndexBufferBound(index_count, vertex_count);
}

size_t encodeIndexBuffer(unsigned char *buffer,
                         size_t buffer_size,
                         const unsigned int *indices,
                         size_t index_count)
{
  return meshopt_encodeIndexBuffer(buffer, buffer_size, indices, index_count);
}

size_t encodeVertexBufferBound(size_t vertex_count, size_t vertex_size)
{
  return meshopt_encodeVertexBufferBound(vertex_count, vertex_size);
}

size_t encodeVertexBuffer(unsigned char *buffer,
                          size_t buffer_size,
                          const void *vertices,
                          size_t vertex_count,
                          size_t vertex_size)
{
  return meshopt_encodeVertexBuffer(buffer, buffer_size, vertices, vertex_count, vertex_size);
}

size_t encodeIndexSequenceBound(size_t index_count, size_t vertex_count)
{
  return meshopt_encodeIndexSequenceBound(index_count, vertex_count);
}

size_t encodeIndexSequence(unsigned char *buffer,
                           size_t buffer_size,
                           const unsigned int *indices,
                           size_t index_count)
{
  return meshopt_encodeIndexSequence(buffer, buffer_size, indices, index_count);
}

void encodeFilterOct(void *destination, size_t count, size_t stride, int bits, const float *data)
{
  meshopt_encodeFilterOct(destination, count, stride, bits, data);
}

void encodeFilterQuat(void *destination, size_t count, size_t stride, int bits, const float *data)
{
  meshopt_encodeFilterQuat(destination, count, stride, bits, data);
}

void encodeFilterExp(void *destination,
                     size_t count,
                     size_t stride,
                     int bits,
                     const float *data,
                     enum encodeExpMode mode)
{
  meshopt_encodeFilterExp(
      destination, count, stride, bits, data, (enum meshopt_EncodeExpMode)mode);
}
