/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Mesh Fairing algorithm designed by Brett Fedack, used in the addon "Mesh Fairing":
 * https://github.com/fedackb/mesh-fairing.
 */

/** \file
 * \ingroup bke
 */

#include "BLI_map.hh"
#include "BLI_math.h"
#include "BLI_vector.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_mesh.h"
#include "BKE_mesh_fair.h"
#include "BKE_mesh_mapping.h"

#include "bmesh.h"
#include "bmesh_tools.h"

#include "MEM_guardedalloc.h"
#include "eigen_capi.h"

using blender::Map;
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

  virtual float *vertex_deformation_co_get(const int v) = 0;
  virtual void vertex_deformation_co_set(const int v, const float co[3]) = 0;

  virtual int vertex_index_from_loop(const int loop) = 0;
  virtual float cotangent_loop_weight_get(const int loop) = 0;

  int vertex_count_get()
  {
    return totvert_;
  }

  int loop_count_get()
  {
    return totloop_;
  }

  MeshElemMap *vertex_loop_map_get(const int v)
  {
    return &vlmap_[v];
  }

  virtual ~FairingContext() = default;

  void fair_vertices(bool *affected,
                     const eMeshFairingDepth depth,
                     VertexWeight *vertex_weight,
                     LoopWeight *loop_weight)
  {

    fair_vertices_ex(affected, (int)depth, vertex_weight, loop_weight);
  }

 protected:
  int totvert_;
  int totloop_;

  MeshElemMap *vlmap_;
  int *vlmap_mem_;

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

      const float *co = vertex_deformation_co_get(v);
      for (int j = 0; j < 3; j++) {
        EIG_linear_solver_right_hand_side_add(solver, j, i, multiplier * co[j]);
      }
      return;
    }

    float w_ij_sum = 0;
    const float w_i = vertex_weight->weight_at_index(v);
    MeshElemMap *vlmap_elem = &vlmap_[v];
    for (int l = 0; l < vlmap_elem->count; l++) {
      const int l_index = vlmap_elem->indices[l];
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

  void fair_vertices_ex(const bool *affected,
                        const int order,
                        VertexWeight *vertex_weight,
                        LoopWeight *loop_weight)
  {
    Map<int, int> vert_col_map;
    int num_affected_vertices = 0;
    for (int i = 0; i < totvert_; i++) {
      if (!affected[i]) {
        continue;
      }
      vert_col_map.add(i, num_affected_vertices);
      num_affected_vertices++;
    }

    /* Early return, nothing to do. */
    if (ELEM(num_affected_vertices, 0, totvert_)) {
      return;
    }

    /* Setup fairing matrices */
    LinearSolver *solver = EIG_linear_solver_new(num_affected_vertices, num_affected_vertices, 3);
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
      float co[3];
      for (int j = 0; j < 3; j++) {
        co[j] = EIG_linear_solver_variable_get(solver, j, col);
      }
      vertex_deformation_co_set(v, co);
    }

    /* Free solver data */
    EIG_linear_solver_delete(solver);
  }
};

class MeshFairingContext : public FairingContext {
 public:
  MeshFairingContext(Mesh *mesh, MVert *deform_mverts)
  {
    totvert_ = mesh->totvert;
    totloop_ = mesh->totloop;

    medge_ = mesh->medge;
    mpoly_ = mesh->mpoly;
    mloop_ = mesh->mloop;
    BKE_mesh_vert_loop_map_create(&vlmap_,
                                  &vlmap_mem_,
                                  mesh->mvert,
                                  mesh->medge,
                                  mesh->mpoly,
                                  mesh->mloop,
                                  mesh->totvert,
                                  mesh->totpoly,
                                  mesh->totloop,
                                  false);

    BKE_mesh_edge_loop_map_create(&elmap_,
                                  &elmap_mem_,
                                  mesh->medge,
                                  mesh->totedge,
                                  mesh->mpoly,
                                  mesh->totpoly,
                                  mesh->mloop,
                                  mesh->totloop);

    /* Deformation coords. */
    if (deform_mverts) {
      deform_mvert_ = deform_mverts;
    }
    else {
      deform_mvert_ = mesh->mvert;
    }

    loop_to_poly_map_.resize(mesh->totloop);
    for (int i = 0; i < mesh->totpoly; i++) {
      for (int l = 0; l < mesh->mpoly[i].totloop; l++) {
        loop_to_poly_map_[l + mesh->mpoly[i].loopstart] = i;
      }
    }
  }

  ~MeshFairingContext() override
  {
    MEM_SAFE_FREE(vlmap_);
    MEM_SAFE_FREE(vlmap_mem_);
    MEM_SAFE_FREE(elmap_);
    MEM_SAFE_FREE(elmap_mem_);
  }

  float *vertex_deformation_co_get(const int v)
  {
    return deform_mvert_[v].co;
  }

  void vertex_deformation_co_set(const int v, const float co[3])
  {
    copy_v3_v3(deform_mvert_[v].co, co);
  }

  void adjacents_coords_from_loop(const int loop,
                                  float r_adj_next[3],
                                  float r_adj_prev[3]) override
  {
    const int vert = mloop_[loop].v;
    const MPoly *p = &mpoly_[loop_to_poly_map_[loop]];
    const int corner = poly_find_loop_from_vert(p, &mloop_[p->loopstart], vert);
    copy_v3_v3(r_adj_next, deform_mvert_[ME_POLY_LOOP_NEXT(mloop_, p, corner)->v].co);
    copy_v3_v3(r_adj_prev, deform_mvert_[ME_POLY_LOOP_PREV(mloop_, p, corner)->v].co);
  }

  int other_vertex_index_from_loop(const int loop, const uint v) override
  {
    MEdge *e = &medge_[mloop_[loop].e];
    if (e->v1 == v) {
      return e->v2;
    }
    return e->v1;
  }

  int vertex_index_from_loop(const int loop) override
  {
    return mloop_[loop].v;
  }

  float cotangent_loop_weight_get(const int UNUSED(loop)) override
  {
    /* TODO: Implement cotangent loop weights for meshes. */
    return 1.0f;
  }

 protected:
  Mesh *mesh_;
  MLoop *mloop_;
  MPoly *mpoly_;
  MEdge *medge_;
  Vector<int> loop_to_poly_map_;

  MVert *deform_mvert_;

  MeshElemMap *elmap_;
  int *elmap_mem_;
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

    bmloop_.resize(bm->totloop);
    vlmap_ = (MeshElemMap *)MEM_calloc_arrayN(sizeof(MeshElemMap), bm->totvert, "bmesh loop map");
    vlmap_mem_ = (int *)MEM_malloc_arrayN(sizeof(int), bm->totloop, "bmesh loop map mempool");

    BMVert *v;
    BMLoop *l;
    BMIter iter;
    BMIter loop_iter;
    int index_iter = 0;

    /* This initializes both the bmloop and the vlmap for bmesh in a single loop. */
    BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
      int loop_count = 0;
      const int vert_index = BM_elem_index_get(v);
      vlmap_[vert_index].indices = &vlmap_mem_[index_iter];

      BM_ITER_ELEM (l, &loop_iter, v, BM_LOOPS_OF_VERT) {
        const int loop_index = BM_elem_index_get(l);
        bmloop_[loop_index] = l;
        vlmap_mem_[index_iter] = loop_index;
        index_iter++;
        loop_count++;
      }
      vlmap_[vert_index].count = loop_count;
    }
  }

  ~BMeshFairingContext() override
  {
    MEM_SAFE_FREE(vlmap_);
    MEM_SAFE_FREE(vlmap_mem_);
  }

  float *vertex_deformation_co_get(const int v)
  {
    return BM_vert_at_index(bm, v)->co;
  }

  void vertex_deformation_co_set(const int v, const float co[3])
  {
    copy_v3_v3(BM_vert_at_index(bm, v)->co, co);
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

  int vertex_index_from_loop(const int loop) override
  {
    return BM_elem_index_get(bmloop_[loop]->v);
  }

  float cotangent_loop_weight_get(const int loop) override
  {
    return 1.0f;

    /* TODO: enable this when it works. */
    BMLoop *l = bmloop_[loop];
    float *co_c[2];
    int co_c_count = 1;

    float *co_a = l->v->co;
    float *co_b = l->next->v->co;
    co_c[0] = l->prev->v->co;
    if (!BM_edge_is_boundary(l->e)) {
      co_c_count = 2;
      co_c[1] = l->radial_next->next->next->v->co;
    }

    float weight = 0.0f;
    for (int c = 0; c < co_c_count; c++) {
      float v1[3];
      float v2[3];
      sub_v3_v3v3(v1, co_a, co_c[c]);
      sub_v3_v3v3(v2, co_b, co_c[c]);
      const float angle = angle_v3v3(v1, v2);
      const float tangent = tan(angle);
      if (tangent != 0) {
        weight += 1.0f / tangent;
      }
      else {
        weight += 1e-4;
      }
    }
    weight *= 0.5f;
    return weight;
  }

 protected:
  BMesh *bm;
  Vector<BMLoop *> bmloop_;
};

class UniformVertexWeight : public VertexWeight {
 public:
  UniformVertexWeight(FairingContext *fairing_context)
  {
    const int totvert = fairing_context->vertex_count_get();
    fairing_context_ = fairing_context;
    vertex_weights_.resize(totvert);
    cached_.resize(totvert);
    for (int i = 0; i < totvert; i++) {
      cached_[i] = false;
    }
  }

  float weight_at_index(const int index) override
  {
    if (!cached_[index]) {
      vertex_weights_[index] = uniform_weight_at_index(index);
      cached_[index] = true;
    }
    return vertex_weights_[index];
  }

 private:
  float uniform_weight_at_index(const int index)
  {
    const int tot_loop = fairing_context_->vertex_loop_map_get(index)->count;
    if (tot_loop != 0) {
      return 1.0f / tot_loop;
    }
    return FLT_MAX;
  }
  Vector<float> vertex_weights_;
  Vector<bool> cached_;
  FairingContext *fairing_context_;
};

class VoronoiVertexWeight : public VertexWeight {

 public:
  VoronoiVertexWeight(FairingContext *fairing_context)
  {
    fairing_context_ = fairing_context;

    const int totvert = fairing_context->vertex_count_get();
    vertex_weights_.resize(totvert);
    cached_.resize(totvert);
    for (int i = 0; i < totvert; i++) {
      cached_[i] = false;
    }
  }

  float weight_at_index(const int index) override
  {
    if (!cached_[index]) {
      vertex_weights_[index] = voronoi_weight_at_index(index);
      cached_[index] = true;
    }
    return vertex_weights_[index];
  }

 private:
  Vector<float> vertex_weights_;
  Vector<bool> cached_;
  FairingContext *fairing_context_;

  float voronoi_weight_at_index(const int index)
  {
    float area = 0.0f;
    float a[3];
    copy_v3_v3(a, fairing_context_->vertex_deformation_co_get(index));
    const float acute_threshold = M_PI_2;

    MeshElemMap *vlmap_elem = fairing_context_->vertex_loop_map_get(index);
    for (int l = 0; l < vlmap_elem->count; l++) {
      const int l_index = vlmap_elem->indices[l];

      float b[3], c[3], d[3];
      fairing_context_->adjacents_coords_from_loop(l_index, b, c);

      if (angle_v3v3v3(c, fairing_context_->vertex_deformation_co_get(index), b) <
          acute_threshold) {
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

    return area != 0.0f ? 1.0f / area : 1e12;
  }

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
  float weight_at_index(const int UNUSED(index)) override
  {
    return 1.0f;
  }
};

class CotangentLoopWeight : public LoopWeight {
 public:
  CotangentLoopWeight(FairingContext *fairing_context)
  {
    const int totloop = fairing_context->loop_count_get();
    fairing_context_ = fairing_context;
    loop_weights_.resize(totloop);
    cached_.resize(totloop);
    for (int i = 0; i < totloop; i++) {
      cached_[i] = false;
    }
  }
  ~CotangentLoopWeight() = default;

  float weight_at_index(const int index) override
  {
    if (!cached_[index]) {
      loop_weights_[index] = fairing_context_->cotangent_loop_weight_get(index);
      cached_[index] = true;
    }
    return loop_weights_[index];
  }

 private:
  Vector<float> loop_weights_;
  Vector<bool> cached_;
  FairingContext *fairing_context_;
};

static void prefair_and_fair_vertices(FairingContext *fairing_context,
                                      bool *affected_vertices,
                                      const eMeshFairingDepth depth)
{
  /* Prefair. */
  UniformVertexWeight *uniform_vertex_weights = new UniformVertexWeight(fairing_context);
  UniformLoopWeight *uniform_loop_weights = new UniformLoopWeight();
  fairing_context->fair_vertices(affected_vertices,
                                 MESH_FAIRING_DEPTH_POSITION,
                                 uniform_vertex_weights,
                                 uniform_loop_weights);

  delete uniform_vertex_weights;
  delete uniform_loop_weights;

  /* Fair. */
  VoronoiVertexWeight *voronoi_vertex_weights = new VoronoiVertexWeight(fairing_context);
  CotangentLoopWeight *cotangent_loop_weights = new CotangentLoopWeight(fairing_context);

  fairing_context->fair_vertices(
      affected_vertices, depth, voronoi_vertex_weights, cotangent_loop_weights);

  delete voronoi_vertex_weights;
  delete cotangent_loop_weights;
}

void BKE_mesh_prefair_and_fair_vertices(struct Mesh *mesh,
                                        struct MVert *deform_mverts,
                                        bool *affect_vertices,
                                        const eMeshFairingDepth depth)
{
  MeshFairingContext *fairing_context = new MeshFairingContext(mesh, deform_mverts);
  prefair_and_fair_vertices(fairing_context, affect_vertices, depth);
  delete fairing_context;
}

void BKE_bmesh_prefair_and_fair_vertices(struct BMesh *bm,
                                         bool *affect_vertices,
                                         const eMeshFairingDepth depth)
{
  BMeshFairingContext *fairing_context = new BMeshFairingContext(bm);
  prefair_and_fair_vertices(fairing_context, affect_vertices, depth);
  delete fairing_context;
}
