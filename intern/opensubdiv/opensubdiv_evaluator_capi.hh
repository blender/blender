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

struct OpenSubdiv_EvaluatorCache {
  // Implementation of the evaluator cache.
  OpenSubdiv_EvaluatorCacheImpl *impl;
};

OpenSubdiv_EvaluatorCache *openSubdiv_createEvaluatorCache(eOpenSubdivEvaluator evaluator_type);

void openSubdiv_deleteEvaluatorCache(OpenSubdiv_EvaluatorCache *evaluator_cache);

// Return the GLSL source code from the OpenSubDiv library used for patch evaluation.
// This function is not thread-safe.
const char *openSubdiv_getGLSLPatchBasisSource();
