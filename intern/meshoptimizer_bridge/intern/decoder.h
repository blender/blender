/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "common.h"

#include <stddef.h>

API(int)
decodeVertexBuffer(void *destination,
                   size_t vertex_count,
                   size_t vertex_size,
                   const unsigned char *buffer,
                   size_t buffer_size);

API(int)
decodeIndexBuffer(void *destination,
                  size_t index_count,
                  size_t index_size,
                  const unsigned char *buffer,
                  size_t buffer_size);

API(int)
decodeIndexSequence(void *destination,
                    size_t index_count,
                    size_t index_size,
                    const unsigned char *buffer,
                    size_t buffer_size);

API(void)
decodeFilterOct(void *buffer, size_t count, size_t stride);

API(void)
decodeFilterQuat(void *buffer, size_t count, size_t stride);

API(void)
decodeFilterExp(void *buffer, size_t count, size_t stride);
