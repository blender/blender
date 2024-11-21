/* SPDX-FileCopyrightText: 2013 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <cstdint>  // for uint64_t

#include "opensubdiv_capi_type.hh"

struct OpenSubdiv_EvaluatorCacheImpl;
struct OpenSubdiv_PatchCoord;
namespace blender::opensubdiv {
class TopologyRefinerImpl;
}

struct OpenSubdiv_EvaluatorSettings {
  // Number of smoothly interpolated vertex data channels.
  int num_vertex_data;
};

// Callback type for doing input/output operations on buffers.
// Useful to abstract GPU buffers.
struct OpenSubdiv_Buffer {
  // Bind the buffer to the GPU.
  void (*bind_gpu)(const OpenSubdiv_Buffer *buffer);

  // Allocate the buffer directly on the host for the given size in bytes. This has to return
  // a pointer to the newly allocated memory.
  void *(*alloc)(const OpenSubdiv_Buffer *buffer, const unsigned int size);

  // Allocate the buffer directly on the device for the given size in bytes.
  void (*device_alloc)(const OpenSubdiv_Buffer *buffer, const unsigned int size);

  // Update the given range of the buffer with new data.
  void (*device_update)(const OpenSubdiv_Buffer *buffer,
                        unsigned int start,
                        unsigned int len,
                        const void *data);

  // Wrap an existing GPU buffer, given its device handle, into the client's buffer type for
  // read-only use.
  void (*wrap_device_handle)(const OpenSubdiv_Buffer *buffer, uint64_t device_ptr);

  // Offset in the buffer where the data starts, if a single buffer is used for multiple data
  // channels.
  int buffer_offset;

  // Pointer to the client buffer data, which is modified or initialized through the various
  // callbacks.
  void *data;
};

struct OpenSubdiv_EvaluatorCache {
  // Implementation of the evaluator cache.
  OpenSubdiv_EvaluatorCacheImpl *impl;
};

OpenSubdiv_EvaluatorCache *openSubdiv_createEvaluatorCache(eOpenSubdivEvaluator evaluator_type);

void openSubdiv_deleteEvaluatorCache(OpenSubdiv_EvaluatorCache *evaluator_cache);

// Return the GLSL source code from the OpenSubDiv library used for patch evaluation.
// This function is not thread-safe.
const char *openSubdiv_getGLSLPatchBasisSource();
