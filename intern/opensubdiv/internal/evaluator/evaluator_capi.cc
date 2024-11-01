/* SPDX-FileCopyrightText: 2015 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Sergey Sharybin. */

#include "opensubdiv_evaluator_capi.hh"

#include <opensubdiv/osd/glslPatchShaderSource.h>

#include "MEM_guardedalloc.h"

#include "internal/evaluator/evaluator_cache_impl.h"

OpenSubdiv_EvaluatorCache *openSubdiv_createEvaluatorCache(eOpenSubdivEvaluator evaluator_type)
{
  OpenSubdiv_EvaluatorCache *evaluator_cache = MEM_new<OpenSubdiv_EvaluatorCache>(__func__);
  evaluator_cache->impl = openSubdiv_createEvaluatorCacheInternal(evaluator_type);
  return evaluator_cache;
}

void openSubdiv_deleteEvaluatorCache(OpenSubdiv_EvaluatorCache *evaluator_cache)
{
  if (!evaluator_cache) {
    return;
  }

  openSubdiv_deleteEvaluatorCacheInternal(evaluator_cache->impl);
  MEM_delete(evaluator_cache);
}

const char *openSubdiv_getGLSLPatchBasisSource(void)
{
  /* Using a global string to avoid dealing with memory allocation/ownership. */
  static std::string patch_basis_source;
  if (patch_basis_source.empty()) {
    patch_basis_source = OpenSubdiv::Osd::GLSLPatchShaderSource::GetPatchBasisShaderSource();
  }
  return patch_basis_source.c_str();
}
