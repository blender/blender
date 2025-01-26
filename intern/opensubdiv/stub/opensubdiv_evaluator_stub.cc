/* SPDX-FileCopyrightText: 2018 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Sergey Sharybin. */

#include "opensubdiv_evaluator_capi.hh"

#include <cstddef>

OpenSubdiv_EvaluatorCache *openSubdiv_createEvaluatorCache(eOpenSubdivEvaluator /*evaluator_type*/)
{
  return nullptr;
}

void openSubdiv_deleteEvaluatorCache(OpenSubdiv_EvaluatorCache * /*evaluator_cache*/) {}

const char *openSubdiv_getGLSLPatchBasisSource()
{
  return nullptr;
}
