/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "decoder.h"

#include <meshoptimizer.h>

int decodeVertexBuffer(void *destination,
                       size_t vertex_count,
                       size_t vertex_size,
                       const unsigned char *buffer,
                       size_t buffer_size)
{
  return meshopt_decodeVertexBuffer(destination, vertex_count, vertex_size, buffer, buffer_size);
}

int decodeIndexBuffer(void *destination,
                      size_t index_count,
                      size_t index_size,
                      const unsigned char *buffer,
                      size_t buffer_size)
{
  return meshopt_decodeIndexBuffer(destination, index_count, index_size, buffer, buffer_size);
}

int decodeIndexSequence(void *destination,
                        size_t index_count,
                        size_t index_size,
                        const unsigned char *buffer,
                        size_t buffer_size)
{
  return meshopt_decodeIndexSequence(destination, index_count, index_size, buffer, buffer_size);
}

void decodeFilterOct(void *buffer, size_t count, size_t stride)
{
  meshopt_decodeFilterOct(buffer, count, stride);
}

void decodeFilterQuat(void *buffer, size_t count, size_t stride)
{
  meshopt_decodeFilterQuat(buffer, count, stride);
}

void decodeFilterExp(void *buffer, size_t count, size_t stride)
{
  meshopt_decodeFilterExp(buffer, count, stride);
}
