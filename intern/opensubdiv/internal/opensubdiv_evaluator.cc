// Copyright 2015 Blender Foundation. All rights reserved.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
// Author: Sergey Sharybin

#include "opensubdiv_evaluator_capi.h"

#include <new>
#include "MEM_guardedalloc.h"

#include "internal/opensubdiv_evaluator_internal.h"

namespace {

void setCoarsePositions(OpenSubdiv_Evaluator* evaluator,
                        const float* positions,
                        const int start_vertex_index, const int num_vertices) {
  evaluator->internal->eval_output->setCoarsePositions(positions,
                                                       start_vertex_index,
                                                       num_vertices);
}

void setVaryingData(OpenSubdiv_Evaluator* evaluator,
                    const float* varying_data,
                    const int start_vertex_index, const int num_vertices) {
  evaluator->internal->eval_output->setVaryingData(varying_data,
                                                   start_vertex_index,
                                                   num_vertices);
}

void setFaceVaryingData(OpenSubdiv_Evaluator* evaluator,
                        const int face_varying_channel,
                        const float* face_varying_data,
                        const int start_vertex_index, const int num_vertices) {
  evaluator->internal->eval_output->setFaceVaryingData(face_varying_channel,
                                                       face_varying_data,
                                                       start_vertex_index,
                                                       num_vertices);
}

void setCoarsePositionsFromBuffer(OpenSubdiv_Evaluator* evaluator,
                                  const void* buffer,
                                  const int start_offset,
                                  const int stride,
                                  const int start_vertex_index,
                                  const int num_vertices) {
  evaluator->internal->eval_output->setCoarsePositionsFromBuffer(
          buffer,
          start_offset,
          stride,
          start_vertex_index,
          num_vertices);
}

void setVaryingDataFromBuffer(OpenSubdiv_Evaluator* evaluator,
                              const void* buffer,
                              const int start_offset,
                              const int stride,
                              const int start_vertex_index,
                              const int num_vertices) {
  evaluator->internal->eval_output->setVaryingDataFromBuffer(
          buffer,
          start_offset,
          stride,
          start_vertex_index,
          num_vertices);
}

void setFaceVaryingDataFromBuffer(OpenSubdiv_Evaluator* evaluator,
                                  const int face_varying_channel,
                                  const void* buffer,
                                  const int start_offset,
                                  const int stride,
                                  const int start_vertex_index,
                                  const int num_vertices) {
  evaluator->internal->eval_output->setFaceVaryingDataFromBuffer(
          face_varying_channel,
          buffer,
          start_offset,
          stride,
          start_vertex_index,
          num_vertices);
}

void refine(OpenSubdiv_Evaluator* evaluator) {
  evaluator->internal->eval_output->refine();
}

void evaluateLimit(OpenSubdiv_Evaluator* evaluator,
                   const int ptex_face_index,
                   const float face_u, const float face_v,
                   float P[3], float dPdu[3], float dPdv[3]) {
  evaluator->internal->eval_output->evaluateLimit(ptex_face_index,
                                                  face_u, face_v,
                                                  P, dPdu, dPdv);
}

void evaluateVarying(OpenSubdiv_Evaluator* evaluator,
                     const int ptex_face_index,
                     float face_u, float face_v,
                     float varying[3]) {
  evaluator->internal->eval_output->evaluateVarying(ptex_face_index,
                                                    face_u, face_v,
                                                    varying);
}

void evaluateFaceVarying(OpenSubdiv_Evaluator* evaluator,
                         const int face_varying_channel,
                         const int ptex_face_index,
                         float face_u, float face_v,
                         float face_varying[2]) {
  evaluator->internal->eval_output->evaluateFaceVarying(
      face_varying_channel, ptex_face_index, face_u, face_v, face_varying);
}

void assignFunctionPointers(OpenSubdiv_Evaluator* evaluator) {
  evaluator->setCoarsePositions = setCoarsePositions;
  evaluator->setVaryingData = setVaryingData;
  evaluator->setFaceVaryingData = setFaceVaryingData;

  evaluator->setCoarsePositionsFromBuffer = setCoarsePositionsFromBuffer;
  evaluator->setVaryingDataFromBuffer = setVaryingDataFromBuffer;
  evaluator->setFaceVaryingDataFromBuffer = setFaceVaryingDataFromBuffer;

  evaluator->refine = refine;

  evaluator->evaluateLimit = evaluateLimit;
  evaluator->evaluateVarying = evaluateVarying;
  evaluator->evaluateFaceVarying = evaluateFaceVarying;
}

}  // namespace

OpenSubdiv_Evaluator* openSubdiv_createEvaluatorFromTopologyRefiner(
    OpenSubdiv_TopologyRefiner* topology_refiner) {
  OpenSubdiv_Evaluator* evaluator = OBJECT_GUARDED_NEW(OpenSubdiv_Evaluator);
  assignFunctionPointers(evaluator);
  evaluator->internal = openSubdiv_createEvaluatorInternal(topology_refiner);
  return evaluator;
}

void openSubdiv_deleteEvaluator(OpenSubdiv_Evaluator* evaluator) {
  openSubdiv_deleteEvaluatorInternal(evaluator->internal);
  OBJECT_GUARDED_DELETE(evaluator, OpenSubdiv_Evaluator);
}
