// Copyright 2019 Blender Foundation. All rights reserved.
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
// Author: Sebastian Parborg, Pablo Dobarro

#include <unordered_map>

#include "MEM_guardedalloc.h"

#include "quadriflow_capi.hpp"
#include "config.hpp"
#include "field-math.hpp"
#include "optimizer.hpp"
#include "parametrizer.hpp"
#include "loader.hpp"

using namespace qflow;

struct ObjVertex {
  uint32_t p = (uint32_t)-1;
  uint32_t n = (uint32_t)-1;
  uint32_t uv = (uint32_t)-1;

  ObjVertex()
  {
  }

  ObjVertex(uint32_t pi)
  {
    p = pi;
  }

  bool operator==(const ObjVertex &v) const
  {
    return v.p == p && v.n == n && v.uv == uv;
  }
};

struct ObjVertexHash {
  std::size_t operator()(const ObjVertex &v) const
  {
    size_t hash = std::hash<uint32_t>()(v.p);
    hash = hash * 37 + std::hash<uint32_t>()(v.uv);
    hash = hash * 37 + std::hash<uint32_t>()(v.n);
    return hash;
  }
};

typedef std::unordered_map<ObjVertex, uint32_t, ObjVertexHash> VertexMap;

static int check_if_canceled(float progress,
                             void (*update_cb)(void *, float progress, int *cancel),
                             void *update_cb_data)
{
  int cancel = 0;
  update_cb(update_cb_data, progress, &cancel);
  return cancel;
}

void QFLOW_quadriflow_remesh(QuadriflowRemeshData *qrd,
                             void (*update_cb)(void *, float progress, int *cancel),
                             void *update_cb_data)
{
  Parametrizer field;
  VertexMap vertexMap;

  /* Get remeshing parameters. */
  int faces = qrd->target_faces;

  if (qrd->preserve_sharp) {
    field.flag_preserve_sharp = 1;
  }
  if (qrd->preserve_boundary) {
    field.flag_preserve_boundary = 1;
  }
  if (qrd->adaptive_scale) {
    field.flag_adaptive_scale = 1;
  }
  if (qrd->minimum_cost_flow) {
    field.flag_minimum_cost_flow = 1;
  }
  if (qrd->aggresive_sat) {
    field.flag_aggresive_sat = 1;
  }
  if (qrd->rng_seed) {
    field.hierarchy.rng_seed = qrd->rng_seed;
  }

  if (check_if_canceled(0.0f, update_cb, update_cb_data) != 0) {
    return;
  }

  /* Copy mesh to quadriflow data structures. */
  std::vector<Vector3d> positions;
  std::vector<uint32_t> indices;
  std::vector<ObjVertex> vertices;

  for (int i = 0; i < qrd->totverts; i++) {
    Vector3d v(qrd->verts[i * 3], qrd->verts[i * 3 + 1], qrd->verts[i * 3 + 2]);
    positions.push_back(v);
  }

  for (int q = 0; q < qrd->totfaces; q++) {
    Vector3i f(qrd->faces[q * 3], qrd->faces[q * 3 + 1], qrd->faces[q * 3 + 2]);

    ObjVertex tri[6];
    int nVertices = 3;

    tri[0] = ObjVertex(f[0]);
    tri[1] = ObjVertex(f[1]);
    tri[2] = ObjVertex(f[2]);

    for (int i = 0; i < nVertices; ++i) {
      const ObjVertex &v = tri[i];
      VertexMap::const_iterator it = vertexMap.find(v);
      if (it == vertexMap.end()) {
        vertexMap[v] = (uint32_t)vertices.size();
        indices.push_back((uint32_t)vertices.size());
        vertices.push_back(v);
      }
      else {
        indices.push_back(it->second);
      }
    }
  }

  field.F.resize(3, indices.size() / 3);
  memcpy(field.F.data(), indices.data(), sizeof(uint32_t) * indices.size());

  field.V.resize(3, vertices.size());
  for (uint32_t i = 0; i < vertices.size(); ++i) {
    field.V.col(i) = positions.at(vertices[i].p);
  }

  if (check_if_canceled(0.1f, update_cb, update_cb_data)) {
    return;
  }

  /* Start processing the input mesh data */
  field.NormalizeMesh();
  field.Initialize(faces);

  if (check_if_canceled(0.2f, update_cb, update_cb_data)) {
    return;
  }

  /* Setup mesh boundary constraints if needed */
  if (field.flag_preserve_boundary) {
    Hierarchy &mRes = field.hierarchy;
    mRes.clearConstraints();
    for (uint32_t i = 0; i < 3 * mRes.mF.cols(); ++i) {
      if (mRes.mE2E[i] == -1) {
        uint32_t i0 = mRes.mF(i % 3, i / 3);
        uint32_t i1 = mRes.mF((i + 1) % 3, i / 3);
        Vector3d p0 = mRes.mV[0].col(i0), p1 = mRes.mV[0].col(i1);
        Vector3d edge = p1 - p0;
        if (edge.squaredNorm() > 0) {
          edge.normalize();
          mRes.mCO[0].col(i0) = p0;
          mRes.mCO[0].col(i1) = p1;
          mRes.mCQ[0].col(i0) = mRes.mCQ[0].col(i1) = edge;
          mRes.mCQw[0][i0] = mRes.mCQw[0][i1] = mRes.mCOw[0][i0] = mRes.mCOw[0][i1] = 1.0;
        }
      }
    }
    mRes.propagateConstraints();
  }

  /* Optimize the mesh field orientations (tangental field etc) */
  Optimizer::optimize_orientations(field.hierarchy);
  field.ComputeOrientationSingularities();

  if (check_if_canceled(0.3f, update_cb, update_cb_data)) {
    return;
  }

  if (field.flag_adaptive_scale == 1) {
    field.EstimateSlope();
  }

  if (check_if_canceled(0.4f, update_cb, update_cb_data)) {
    return;
  }

  Optimizer::optimize_scale(field.hierarchy, field.rho, field.flag_adaptive_scale);
  field.flag_adaptive_scale = 1;

  Optimizer::optimize_positions(field.hierarchy, field.flag_adaptive_scale);

  field.ComputePositionSingularities();

  if (check_if_canceled(0.5f, update_cb, update_cb_data)) {
    return;
  }

  /* Compute the final quad geomtry using a maxflow solver */
  field.ComputeIndexMap();

  if (check_if_canceled(0.9f, update_cb, update_cb_data)) {
    return;
  }

  /* Get the output mesh data */
  qrd->out_totverts = field.O_compact.size();
  qrd->out_totfaces = field.F_compact.size();

  qrd->out_verts = (float *)MEM_malloc_arrayN(
      qrd->out_totverts, 3 * sizeof(float), "quadriflow remesher out verts");
  qrd->out_faces = (unsigned int *)MEM_malloc_arrayN(
      qrd->out_totfaces, 4 * sizeof(unsigned int), "quadriflow remesh out quads");

  for (int i = 0; i < qrd->out_totverts; i++) {
    auto t = field.O_compact[i] * field.normalize_scale + field.normalize_offset;
    qrd->out_verts[i * 3] = t[0];
    qrd->out_verts[i * 3 + 1] = t[1];
    qrd->out_verts[i * 3 + 2] = t[2];
  }

  for (int i = 0; i < qrd->out_totfaces; i++) {
    qrd->out_faces[i * 4] = field.F_compact[i][0];
    qrd->out_faces[i * 4 + 1] = field.F_compact[i][1];
    qrd->out_faces[i * 4 + 2] = field.F_compact[i][2];
    qrd->out_faces[i * 4 + 3] = field.F_compact[i][3];
  }
}
