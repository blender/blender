/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 * Mesh Fairing algorithm designed by Brett Fedack, used in the addon "Mesh Fairing":
 * https://github.com/fedackb/mesh-fairing.
 */

#include "BLI_map.hh"
#include "BLI_math_geom.h"
#include "BLI_math_vector.h"
#include "BLI_vector.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_fair.hh"
#include "BKE_mesh_mapping.hh"

#include "bmesh.h"
#include "bmesh_tools.h"

#include "MEM_guardedalloc.h"
#include "eigen_capi.h"

using blender::Array;
using blender::float3;
using blender::Map;
using blender::MutableSpan;
using blender::Span;
using blender::Vector;
using std::array;

class VertexWeight {
 public:
  virtual float weight_at_index(const int index) = 0;
  virtual ~VertexWeight() = default;
};

class LoopWeight {
 public:
  virtual float weight_at_index(const int index) = 0;
  virtual ~LoopWeight() = default;
};

class FairingContext {
 public:
  /* Get coordinates of vertices which are adjacent to the loop with specified index. */
  virtual void adjacents_coords_from_loop(const int loop,
                                          float r_adj_next[3],
                                          float r_adj_prev[3]) = 0;

  /* Get the other vertex index for a loop. */
  virtual int other_vertex_index_from_loop(const int loop, const uint v) = 0;

  int vertex_count_get()
  {
    return totvert_;
  }

  int loop_count_get()
  {
    return totvert_;
  }

  Span<int> vertex_loop_map_get(const int v)
  {
    return vlmap_[v];
  }

  float *vertex_deformation_co_get(const int v)
  {
    return co_[v];
  }

  virtual ~FairingContext() = default;

  void fair_verts(bool *affected,
                  const eMeshFairingDepth depth,
                  VertexWeight *vertex_weight,
                  LoopWeight *loop_weight)
  {

    fair_verts_ex(affected, int(depth), vertex_weight, loop_weight);
  }

 protected:
  Vector<float *> co_;

  int totvert_;
  int totloop_;

  blender::GroupedSpan<int> vlmap_;

 private:
  void fair_setup_fairing(const int v,
                          const int i,
                          LinearSolver *solver,
                          float multiplier,
                          const int depth,
                          Map<int, int> &vert_col_map,
                          VertexWeight *vertex_weight,
                          LoopWeight *loop_weight)
  {
    if (depth == 0) {
      if (vert_col_map.contains(v)) {
        const int j = vert_col_map.lookup(v);
        EIG_linear_solver_matrix_add(solver, i, j, -multiplier);
        return;
      }
      for (int j = 0; j < 3; j++) {
        EIG_linear_solver_right_hand_side_add(solver, j, i, multiplier * co_[v][j]);
      }
      return;
    }

    float w_ij_sum = 0;
    const float w_i = vertex_weight->weight_at_index(v);
    const Span<int> vlmap_elem = vlmap_[v];
    for (const int l : vlmap_elem.index_range()) {
      const int l_index = vlmap_elem[l];
      const int other_vert = other_vertex_index_from_loop(l_index, v);
      const float w_ij = loop_weight->weight_at_index(l_index);
      w_ij_sum += w_ij;
      fair_setup_fairing(other_vert,
                         i,
                         solver,
                         w_i * w_ij * multiplier,
                         depth - 1,
                         vert_col_map,
                         vertex_weight,
                         loop_weight);
    }
    fair_setup_fairing(v,
                       i,
                       solver,
                       -1 * w_i * w_ij_sum * multiplier,
                       depth - 1,
                       vert_col_map,
                       vertex_weight,
                       loop_weight);
  }

  void fair_verts_ex(const bool *affected,
                     const int order,
                     VertexWeight *vertex_weight,
                     LoopWeight *loop_weight)
  {
    Map<int, int> vert_col_map;
    int affected_verts_num = 0;
    for (int i = 0; i < totvert_; i++) {
      if (!affected[i]) {
        continue;
      }
      vert_col_map.add(i, affected_verts_num);
      affected_verts_num++;
    }

    /* Early return, nothing to do. */
    if (ELEM(affected_verts_num, 0, totvert_)) {
      return;
    }

    /* Setup fairing matrices */
    LinearSolver *solver = EIG_linear_solver_new(affected_verts_num, affected_verts_num, 3);
    for (auto item : vert_col_map.items()) {
      const int v = item.key;
      const int col = item.value;
      fair_setup_fairing(v, col, solver, 1.0f, order, vert_col_map, vertex_weight, loop_weight);
    }

    /* Solve linear system */
    EIG_linear_solver_solve(solver);

    /* Copy the result back to the mesh */
    for (auto item : vert_col_map.items()) {
      const int v = item.key;
      const int col = item.value;
      for (int j = 0; j < 3; j++) {
        co_[v][j] = EIG_linear_solver_variable_get(solver, j, col);
      }
    }

    /* Free solver data */
    EIG_linear_solver_delete(solver);
  }
};

class MeshFairingContext : public FairingContext {
 public:
  MeshFairingContext(Mesh *mesh, MutableSpan<float3> deform_positions)
  {
    totvert_ = mesh->totvert;
    totloop_ = mesh->totloop;

    MutableSpan<float3> positions = mesh->vert_positions_for_write();
    edges_ = mesh->edges();
    faces = mesh->faces();
    corner_verts_ = mesh->corner_verts();
    corner_edges_ = mesh->corner_edges();
    vlmap_ = blender::bke::mesh::build_vert_to_loop_map(
        corner_verts_, positions.size(), vert_to_face_offsets_, vert_to_face_indices_);

    /* Deformation coords. */
    co_.reserve(mesh->totvert);
    if (!deform_positions.is_empty()) {
      for (int i = 0; i < mesh->totvert; i++) {
        co_[i] = deform_positions[i];
      }
    }
    else {
      for (int i = 0; i < mesh->totvert; i++) {
        co_[i] = positions[i];
      }
    }

    loop_to_face_map_ = blender::bke::mesh::build_loop_to_face_map(faces);
  }

  void adjacents_coords_from_loop(const int loop,
                                  float r_adj_next[3],
                                  float r_adj_prev[3]) override
  {
    using namespace blender;
    const int vert = corner_verts_[loop];
    const blender::IndexRange face = faces[loop_to_face_map_[loop]];
    const int2 adjecent_verts = bke::mesh::face_find_adjecent_verts(face, corner_verts_, vert);
    copy_v3_v3(r_adj_next, co_[adjecent_verts[0]]);
    copy_v3_v3(r_adj_prev, co_[adjecent_verts[1]]);
  }

  int other_vertex_index_from_loop(const int loop, const uint v) override
  {
    const blender::int2 &edge = edges_[corner_edges_[loop]];
    return blender::bke::mesh::edge_other_vert(edge, v);
  }

 protected:
  Mesh *mesh_;
  Span<int> corner_verts_;
  Span<int> corner_edges_;
  blender::OffsetIndices<int> faces;
  Span<blender::int2> edges_;
  Array<int> loop_to_face_map_;
  Array<int> vert_to_face_offsets_;
  Array<int> vert_to_face_indices_;
};

class BMeshFairingContext : public FairingContext {
 public:
  BMeshFairingContext(BMesh *bm)
  {
    this->bm = bm;
    totvert_ = bm->totvert;
    totloop_ = bm->totloop;

    BM_mesh_elem_table_ensure(bm, BM_VERT);
    BM_mesh_elem_index_ensure(bm, BM_LOOP);

    /* Deformation coords. */
    co_.reserve(bm->totvert);
    for (int i = 0; i < bm->totvert; i++) {
      BMVert *v = BM_vert_at_index(bm, i);
      co_[i] = v->co;
    }

    bmloop_.reinitialize(bm->totloop);
    vert_to_loop_offsets_ = Array<int>(bm->totvert, 0);
    vert_to_loop_indices_.reinitialize(bm->totloop);

    BMVert *v;
    BMLoop *l;
    BMIter iter;
    BMIter loop_iter;
    int index_iter = 0;

    /* This initializes both the bmloop and the vlmap for bmesh in a single loop. */
    BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
      const int vert_index = BM_elem_index_get(v);
      vert_to_loop_offsets_[vert_index] = index_iter;
      BM_ITER_ELEM (l, &loop_iter, v, BM_LOOPS_OF_VERT) {
        const int loop_index = BM_elem_index_get(l);
        bmloop_[loop_index] = l;
        vert_to_loop_indices_[index_iter] = loop_index;
        index_iter++;
      }
    }
    vert_to_loop_offsets_.last() = index_iter;
  }

  void adjacents_coords_from_loop(const int loop,
                                  float r_adj_next[3],
                                  float r_adj_prev[3]) override
  {
    copy_v3_v3(r_adj_next, bmloop_[loop]->next->v->co);
    copy_v3_v3(r_adj_prev, bmloop_[loop]->prev->v->co);
  }

  int other_vertex_index_from_loop(const int loop, const uint v) override
  {
    BMLoop *l = bmloop_[loop];
    BMVert *bmvert = BM_vert_at_index(bm, v);
    BMVert *bm_other_vert = BM_edge_other_vert(l->e, bmvert);
    return BM_elem_index_get(bm_other_vert);
  }

 protected:
  BMesh *bm;
  Array<BMLoop *> bmloop_;
  Array<int> vert_to_loop_offsets_;
  Array<int> vert_to_loop_indices_;
};

class UniformVertexWeight : public VertexWeight {
 public:
  UniformVertexWeight(FairingContext *fairing_context)
  {
    const int totvert = fairing_context->vertex_count_get();
    vertex_weights_.reserve(totvert);
    for (int i = 0; i < totvert; i++) {
      const int tot_loop = fairing_context->vertex_loop_map_get(i).size();
      if (tot_loop != 0) {
        vertex_weights_[i] = 1.0f / tot_loop;
      }
      else {
        vertex_weights_[i] = FLT_MAX;
      }
    }
  }

  float weight_at_index(const int index) override
  {
    return vertex_weights_[index];
  }

 private:
  Vector<float> vertex_weights_;
};

class VoronoiVertexWeight : public VertexWeight {

 public:
  VoronoiVertexWeight(FairingContext *fairing_context)
  {

    const int totvert = fairing_context->vertex_count_get();
    vertex_weights_.reserve(totvert);
    for (int i = 0; i < totvert; i++) {

      float area = 0.0f;
      float a[3];
      copy_v3_v3(a, fairing_context->vertex_deformation_co_get(i));
      const float acute_threshold = M_PI_2;

      const Span<int> vlmap_elem = fairing_context->vertex_loop_map_get(i);
      for (const int l : vlmap_elem.index_range()) {
        const int l_index = vlmap_elem[l];

        float b[3], c[3], d[3];
        fairing_context->adjacents_coords_from_loop(l_index, b, c);

        if (angle_v3v3v3(c, fairing_context->vertex_deformation_co_get(i), b) < acute_threshold) {
          calc_circumcenter(d, a, b, c);
        }
        else {
          add_v3_v3v3(d, b, c);
          mul_v3_fl(d, 0.5f);
        }

        float t[3];
        add_v3_v3v3(t, a, b);
        mul_v3_fl(t, 0.5f);
        area += area_tri_v3(a, t, d);

        add_v3_v3v3(t, a, c);
        mul_v3_fl(t, 0.5f);
        area += area_tri_v3(a, d, t);
      }

      vertex_weights_[i] = area != 0.0f ? 1.0f / area : 1e12;
    }
  }

  float weight_at_index(const int index) override
  {
    return vertex_weights_[index];
  }

 private:
  Vector<float> vertex_weights_;

  void calc_circumcenter(float r[3], const float a[3], const float b[3], const float c[3])
  {
    float ab[3];
    sub_v3_v3v3(ab, b, a);

    float ac[3];
    sub_v3_v3v3(ac, c, a);

    float ab_cross_ac[3];
    cross_v3_v3v3(ab_cross_ac, ab, ac);

    if (len_squared_v3(ab_cross_ac) > 0.0f) {
      float d[3];
      cross_v3_v3v3(d, ab_cross_ac, ab);
      mul_v3_fl(d, len_squared_v3(ac));

      float t[3];
      cross_v3_v3v3(t, ac, ab_cross_ac);
      mul_v3_fl(t, len_squared_v3(ab));

      add_v3_v3(d, t);

      mul_v3_fl(d, 1.0f / (2.0f * len_squared_v3(ab_cross_ac)));

      add_v3_v3v3(r, a, d);
      return;
    }
    copy_v3_v3(r, a);
  }
};

class UniformLoopWeight : public LoopWeight {
 public:
  float weight_at_index(const int /*index*/) override
  {
    return 1.0f;
  }
};

static void prefair_and_fair_verts(FairingContext *fairing_context,
                                   bool *affected_verts,
                                   const eMeshFairingDepth depth)
{
  /* Pre-fair. */
  UniformVertexWeight *uniform_vertex_weights = new UniformVertexWeight(fairing_context);
  UniformLoopWeight *uniform_loop_weights = new UniformLoopWeight();
  fairing_context->fair_verts(affected_verts, depth, uniform_vertex_weights, uniform_loop_weights);
  delete uniform_vertex_weights;

  /* Fair. */
  VoronoiVertexWeight *voronoi_vertex_weights = new VoronoiVertexWeight(fairing_context);
  /* TODO: Implement cotangent loop weights. */
  fairing_context->fair_verts(affected_verts, depth, voronoi_vertex_weights, uniform_loop_weights);

  delete uniform_loop_weights;
  delete voronoi_vertex_weights;
}

void BKE_mesh_prefair_and_fair_verts(Mesh *mesh,
                                     float (*deform_vert_positions)[3],
                                     bool *affect_verts,
                                     const eMeshFairingDepth depth)
{
  MutableSpan<float3> deform_positions_span;
  if (deform_vert_positions) {
    deform_positions_span = {reinterpret_cast<float3 *>(deform_vert_positions), mesh->totvert};
  }
  MeshFairingContext *fairing_context = new MeshFairingContext(mesh, deform_positions_span);
  prefair_and_fair_verts(fairing_context, affect_verts, depth);
  delete fairing_context;
}

void BKE_bmesh_prefair_and_fair_verts(BMesh *bm, bool *affect_verts, const eMeshFairingDepth depth)
{
  BMeshFairingContext *fairing_context = new BMeshFairingContext(bm);
  prefair_and_fair_verts(fairing_context, affect_verts, depth);
  delete fairing_context;
}
