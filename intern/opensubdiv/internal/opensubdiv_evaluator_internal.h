// Copyright 2018 Blender Foundation. All rights reserved.
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

#ifndef OPENSUBDIV_EVALUATOR_INTERNAL_H_
#define OPENSUBDIV_EVALUATOR_INTERNAL_H_

#ifdef _MSC_VER
#  include <iso646.h>
#endif

#include <opensubdiv/far/patchMap.h>
#include <opensubdiv/far/patchTable.h>

struct OpenSubdiv_TopologyRefiner;

namespace opensubdiv_capi {

// Anonymous forward declaration of actual evaluator implementation.
class CpuEvalOutput;

// Wrapper around implementaiton, which defines API which we are capable to
// provide over the implementation.
//
// TODO(sergey):  It is almost the same as C-API object, so ideally need to
// merge them somehow, but how to do this and keep files with all the templates
// and such separate?
class CpuEvalOutputAPI {
 public:
  // NOTE: API object becomes an owner of evaluator. Patch we are referencing.
  CpuEvalOutputAPI(CpuEvalOutput* implementation,
                   OpenSubdiv::Far::PatchMap* patch_map);
  ~CpuEvalOutputAPI();

  // Set coarse positions from a continuous array of coordinates.
  void setCoarsePositions(const float* positions,
                          const int start_vertex_index,
                          const int num_vertices);
  // Set varying data from a continuous array of data.
  void setVaryingData(const float* varying_data,
                      const int start_vertex_index, const int num_vertices);
  // Set face varying data from a continuous array of data.
  //
  // TODO(sergey): Find a better name for vertex here. It is not the vertex of
  // geometry, but a vertex of UV map.
  void setFaceVaryingData(const float* varying_data,
                          const int start_vertex_index, const int num_vertices);

  // Set coarse vertex position from a continuous memory buffer where
  // first coordinate starts at offset of `start_offset` and there is `stride`
  // bytes between adjacent vertex coordinates.
  void setCoarsePositionsFromBuffer(const void* buffer,
                                    const int start_offset,
                                    const int stride,
                                    const int start_vertex_index,
                                    const int num_vertices);
  // Set varying data from a continuous memory buffer where
  // first coordinate starts at offset of `start_offset` and there is `stride`
  // bytes between adjacent vertex coordinates.
  void setVaryingDataFromBuffer(const void* buffer,
                                const int start_offset,
                                const int stride,
                                const int start_vertex_index,
                                const int num_vertices);
  // Set face varying data from a continuous memory buffer where
  // first coordinate starts at offset of `start_offset` and there is `stride`
  // bytes between adjacent vertex coordinates.
  //
  // TODO(sergey): Find a better name for vertex here. It is not the vertex of
  // geometry, but a vertex of UV map.
  void setFaceVaryingDataFromBuffer(const void* buffer,
                                    const int start_offset,
                                    const int stride,
                                    const int start_vertex_index,
                                    const int num_vertices);

  // Refine after coarse positions update.
  void refine();

  // Evaluate given ptex face at given bilinear coordinate.
  // If derivatives are NULL, they will not be evaluated.
  void evaluateLimit(const int ptex_face_index,
                     float face_u, float face_v,
                     float P[3], float dPdu[3], float dPdv[3]);

  // Evaluate varying data at a given bilinear coordinate of given ptex face.
  void evaluateVarying(const int ptes_face_index,
                       float face_u, float face_v,
                       float varying[3]);

  // Evaluate facee-varying data at a given bilinear coordinate of given
  // ptex face.
  void evaluateFaceVarying(const int ptes_face_index,
                           float face_u, float face_v,
                           float face_varying[2]);

 protected:
  CpuEvalOutput* implementation_;
  OpenSubdiv::Far::PatchMap* patch_map_;
};

}  // namespace opensubdiv_capi

struct OpenSubdiv_EvaluatorInternal {
 public:
  OpenSubdiv_EvaluatorInternal();
  ~OpenSubdiv_EvaluatorInternal();

  opensubdiv_capi::CpuEvalOutputAPI* eval_output;
  const OpenSubdiv::Far::PatchMap* patch_map;
  const OpenSubdiv::Far::PatchTable* patch_table;
};

OpenSubdiv_EvaluatorInternal* openSubdiv_createEvaluatorInternal(
    struct OpenSubdiv_TopologyRefiner* topology_refiner);

void openSubdiv_deleteEvaluatorInternal(
    OpenSubdiv_EvaluatorInternal* evaluator);

#endif  // OPENSUBDIV_EVALUATOR_INTERNAL_H_
