/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include <algorithm>

#include "MEM_guardedalloc.h"

#include "BLI_jitter_2d.h"
#include "BLI_map.hh"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_ordered_edge.hh"

#include "BKE_bvhutils.hh"
#include "BKE_editmesh.hh"
#include "BKE_editmesh_bvh.hh"
#include "BKE_editmesh_cache.hh"

#include "extract_mesh.hh"

namespace blender::draw {

static void axis_from_enum_v3(float v[3], const char axis)
{
  zero_v3(v);
  if (axis < 3) {
    v[axis] = 1.0f;
  }
  else {
    v[axis - 3] = -1.0f;
  }
}

BLI_INLINE float overhang_remap(float fac, float min, float max, float minmax_irange)
{
  if (fac < min) {
    fac = 1.0f;
  }
  else if (fac > max) {
    fac = -1.0f;
  }
  else {
    fac = (fac - min) * minmax_irange;
    fac = 1.0f - fac;
    CLAMP(fac, 0.0f, 1.0f);
  }
  return fac;
}

static void statvis_calc_overhang(const MeshRenderData &mr,
                                  const float4x4 &object_to_world,
                                  MutableSpan<float> r_overhang)
{
  const MeshStatVis *statvis = &mr.toolsettings->statvis;
  const float min = statvis->overhang_min / float(M_PI);
  const float max = statvis->overhang_max / float(M_PI);
  const char axis = statvis->overhang_axis;
  float dir[3];
  const float minmax_irange = 1.0f / (max - min);

  BLI_assert(min <= max);

  axis_from_enum_v3(dir, axis);

  /* now convert into global space */
  mul_transposed_mat3_m4_v3(object_to_world.ptr(), dir);
  normalize_v3(dir);

  if (mr.extract_type == MeshExtractType::BMesh) {
    BMEditMesh *em = mr.edit_bmesh;
    BMIter iter;
    BMesh *bm = em->bm;
    BMFace *f;
    int l_index = 0;
    BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
      float fac = angle_normalized_v3v3(bm_face_no_get(mr, f), dir) / float(M_PI);
      fac = overhang_remap(fac, min, max, minmax_irange);
      for (int i = 0; i < f->len; i++, l_index++) {
        r_overhang[l_index] = fac;
      }
    }
  }
  else {
    for (const int face_i : mr.faces.index_range()) {
      float fac = angle_normalized_v3v3(mr.face_normals[face_i], dir) / float(M_PI);
      fac = overhang_remap(fac, min, max, minmax_irange);
      for (const int loop_i : mr.faces[face_i]) {
        r_overhang[loop_i] = fac;
      }
    }
  }
}

/**
 * Needed so we can use jitter values for face interpolation.
 */
static void uv_from_jitter_v2(float uv[2])
{
  uv[0] += 0.5f;
  uv[1] += 0.5f;
  if (uv[0] + uv[1] > 1.0f) {
    uv[0] = 1.0f - uv[0];
    uv[1] = 1.0f - uv[1];
  }

  clamp_v2(uv, 0.0f, 1.0f);
}

BLI_INLINE float thickness_remap(float fac, float min, float max, float minmax_irange)
{
  /* important not '<=' */
  if (fac < max) {
    fac = (fac - min) * minmax_irange;
    fac = 1.0f - fac;
    CLAMP(fac, 0.0f, 1.0f);
  }
  else {
    fac = -1.0f;
  }
  return fac;
}

static void statvis_calc_thickness(const MeshRenderData &mr,
                                   const float4x4 &object_to_world,
                                   MutableSpan<float> r_thickness)
{
  const float eps_offset = 0.00002f; /* values <= 0.00001 give errors */
  /* cheating to avoid another allocation */
  float *face_dists = r_thickness.data() + (mr.corners_num - mr.faces_num);
  const float scale = 1.0f / mat4_to_scale(object_to_world.ptr());
  const MeshStatVis *statvis = &mr.toolsettings->statvis;
  const float min = statvis->thickness_min * scale;
  const float max = statvis->thickness_max * scale;
  const float minmax_irange = 1.0f / (max - min);
  const int samples = statvis->thickness_samples;
  float jit_ofs[32][2];
  BLI_assert(samples <= 32);
  BLI_assert(min <= max);

  copy_vn_fl(face_dists, mr.faces_num, max);

  BLI_jitter_init(jit_ofs, samples);
  for (int j = 0; j < samples; j++) {
    uv_from_jitter_v2(jit_ofs[j]);
  }

  if (mr.extract_type == MeshExtractType::BMesh) {
    BMEditMesh *em = mr.edit_bmesh;
    BMesh *bm = em->bm;
    BM_mesh_elem_index_ensure(bm, BM_FACE);

    BMBVHTree *bmtree = BKE_bmbvh_new_from_editmesh(em, 0, nullptr, false);
    const Span<std::array<BMLoop *, 3>> looptris = em->looptris;
    for (int i = 0; i < mr.corner_tris_num; i++) {
      const BMLoop *const *ltri = looptris[i].data();
      const int index = BM_elem_index_get(ltri[0]->f);
      const float *cos[3] = {
          bm_vert_co_get(mr, ltri[0]->v),
          bm_vert_co_get(mr, ltri[1]->v),
          bm_vert_co_get(mr, ltri[2]->v),
      };
      float ray_co[3];
      float ray_no[3];

      normal_tri_v3(ray_no, cos[2], cos[1], cos[0]);

      for (int j = 0; j < samples; j++) {
        float dist = face_dists[index];
        interp_v3_v3v3v3_uv(ray_co, cos[0], cos[1], cos[2], jit_ofs[j]);
        madd_v3_v3fl(ray_co, ray_no, eps_offset);

        BMFace *f_hit = BKE_bmbvh_ray_cast(bmtree, ray_co, ray_no, 0.0f, &dist, nullptr, nullptr);
        if (f_hit && dist < face_dists[index]) {
          float angle_fac = fabsf(
              dot_v3v3(bm_face_no_get(mr, ltri[0]->f), bm_face_no_get(mr, f_hit)));
          angle_fac = 1.0f - angle_fac;
          angle_fac = angle_fac * angle_fac * angle_fac;
          angle_fac = 1.0f - angle_fac;
          dist /= angle_fac;
          face_dists[index] = std::min(dist, face_dists[index]);
        }
      }
    }
    BKE_bmbvh_free(bmtree);

    BMIter iter;
    BMFace *f;
    int l_index = 0;
    BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
      float fac = face_dists[BM_elem_index_get(f)];
      fac = thickness_remap(fac, min, max, minmax_irange);
      for (int i = 0; i < f->len; i++, l_index++) {
        r_thickness[l_index] = fac;
      }
    }
  }
  else {
    bke::BVHTreeFromMesh treeData = mr.mesh->bvh_corner_tris();
    const BVHTree *tree = treeData.tree;
    if (tree == nullptr) {
      return;
    }

    const Span<int3> corner_tris = mr.mesh->corner_tris();
    const Span<int> tri_faces = mr.mesh->corner_tri_faces();
    for (const int i : corner_tris.index_range()) {
      const int index = tri_faces[i];
      const float *cos[3] = {mr.vert_positions[mr.corner_verts[corner_tris[i][0]]],
                             mr.vert_positions[mr.corner_verts[corner_tris[i][1]]],
                             mr.vert_positions[mr.corner_verts[corner_tris[i][2]]]};
      float ray_co[3];
      float ray_no[3];

      normal_tri_v3(ray_no, cos[2], cos[1], cos[0]);

      for (int j = 0; j < samples; j++) {
        interp_v3_v3v3v3_uv(ray_co, cos[0], cos[1], cos[2], jit_ofs[j]);
        madd_v3_v3fl(ray_co, ray_no, eps_offset);

        BVHTreeRayHit hit;
        hit.index = -1;
        hit.dist = face_dists[index];
        if ((BLI_bvhtree_ray_cast(
                 tree, ray_co, ray_no, 0.0f, &hit, treeData.raycast_callback, &treeData) != -1) &&
            hit.dist < face_dists[index])
        {
          float angle_fac = fabsf(dot_v3v3(mr.face_normals[index], hit.no));
          angle_fac = 1.0f - angle_fac;
          angle_fac = angle_fac * angle_fac * angle_fac;
          angle_fac = 1.0f - angle_fac;
          hit.dist /= angle_fac;
          face_dists[index] = std::min(hit.dist, face_dists[index]);
        }
      }
    }

    for (const int face_i : mr.faces.index_range()) {
      float fac = face_dists[face_i];
      fac = thickness_remap(fac, min, max, minmax_irange);
      for (const int loop_i : mr.faces[face_i]) {
        r_thickness[loop_i] = fac;
      }
    }
  }
}

struct BVHTree_OverlapData {
  Span<float3> positions;
  Span<int> corner_verts;
  Span<int3> corner_tris;
  Span<int> tri_faces;
  float epsilon;
};

static bool bvh_overlap_cb(void *userdata, int index_a, int index_b, int /*thread*/)
{
  BVHTree_OverlapData *data = static_cast<BVHTree_OverlapData *>(userdata);

  if (UNLIKELY(data->tri_faces[index_a] == data->tri_faces[index_b])) {
    return false;
  }

  const int3 tri_a = data->corner_tris[index_a];
  const int3 tri_b = data->corner_tris[index_b];

  const float *tri_a_co[3] = {data->positions[data->corner_verts[tri_a[0]]],
                              data->positions[data->corner_verts[tri_a[1]]],
                              data->positions[data->corner_verts[tri_a[2]]]};
  const float *tri_b_co[3] = {data->positions[data->corner_verts[tri_b[0]]],
                              data->positions[data->corner_verts[tri_b[1]]],
                              data->positions[data->corner_verts[tri_b[2]]]};
  float ix_pair[2][3];
  int verts_shared = 0;

  verts_shared = (ELEM(tri_a_co[0], UNPACK3(tri_b_co)) + ELEM(tri_a_co[1], UNPACK3(tri_b_co)) +
                  ELEM(tri_a_co[2], UNPACK3(tri_b_co)));

  /* if 2 points are shared, bail out */
  if (verts_shared >= 2) {
    return false;
  }

  return (isect_tri_tri_v3(UNPACK3(tri_a_co), UNPACK3(tri_b_co), ix_pair[0], ix_pair[1]) &&
          /* if we share a vertex, check the intersection isn't a 'point' */
          ((verts_shared == 0) || (len_squared_v3v3(ix_pair[0], ix_pair[1]) > data->epsilon)));
}

static void statvis_calc_intersect(const MeshRenderData &mr, MutableSpan<float> r_intersect)
{
  for (int l_index = 0; l_index < mr.corners_num; l_index++) {
    r_intersect[l_index] = -1.0f;
  }

  if (mr.extract_type == MeshExtractType::BMesh) {
    BMEditMesh *em = mr.edit_bmesh;
    uint overlap_len;
    BMesh *bm = em->bm;

    BM_mesh_elem_index_ensure(bm, BM_FACE);

    BMBVHTree *bmtree = BKE_bmbvh_new_from_editmesh(em, 0, nullptr, false);
    BVHTreeOverlap *overlap = BKE_bmbvh_overlap_self(bmtree, &overlap_len);

    if (overlap) {
      for (int i = 0; i < overlap_len; i++) {
        BMFace *f_hit_pair[2] = {
            em->looptris[overlap[i].indexA][0]->f,
            em->looptris[overlap[i].indexB][0]->f,
        };
        for (int j = 0; j < 2; j++) {
          BMFace *f_hit = f_hit_pair[j];
          BMLoop *l_first = BM_FACE_FIRST_LOOP(f_hit);
          int l_index = BM_elem_index_get(l_first);
          for (int k = 0; k < f_hit->len; k++, l_index++) {
            r_intersect[l_index] = 1.0f;
          }
        }
      }
      MEM_freeN(overlap);
    }

    BKE_bmbvh_free(bmtree);
  }
  else {
    uint overlap_len;
    bke::BVHTreeFromMesh treeData = mr.mesh->bvh_corner_tris();
    const BVHTree *tree = treeData.tree;
    if (tree == nullptr) {
      return;
    }

    BVHTree_OverlapData data = {};
    data.positions = mr.vert_positions;
    data.corner_verts = mr.corner_verts;
    data.corner_tris = mr.mesh->corner_tris();
    data.tri_faces = mr.mesh->corner_tri_faces();
    data.epsilon = BLI_bvhtree_get_epsilon(tree);

    BVHTreeOverlap *overlap = BLI_bvhtree_overlap_self(tree, &overlap_len, bvh_overlap_cb, &data);
    if (overlap) {
      for (int i = 0; i < overlap_len; i++) {

        for (const IndexRange f_hit : {mr.faces[data.tri_faces[overlap[i].indexA]],
                                       mr.faces[data.tri_faces[overlap[i].indexB]]})
        {
          int l_index = f_hit.start();
          for (int k = 0; k < f_hit.size(); k++, l_index++) {
            r_intersect[l_index] = 1.0f;
          }
        }
      }
      MEM_freeN(overlap);
    }
  }
}

BLI_INLINE float distort_remap(float fac, float min, float /*max*/, float minmax_irange)
{
  if (fac >= min) {
    fac = (fac - min) * minmax_irange;
    CLAMP(fac, 0.0f, 1.0f);
  }
  else {
    /* fallback */
    fac = -1.0f;
  }
  return fac;
}

static void statvis_calc_distort(const MeshRenderData &mr, MutableSpan<float> r_distort)
{
  const MeshStatVis *statvis = &mr.toolsettings->statvis;
  const float min = statvis->distort_min;
  const float max = statvis->distort_max;
  const float minmax_irange = 1.0f / (max - min);

  if (mr.extract_type == MeshExtractType::BMesh) {
    BMEditMesh *em = mr.edit_bmesh;
    BMIter iter;
    BMesh *bm = em->bm;
    BMFace *f;

    int l_index = 0;
    int f_index = 0;
    BM_ITER_MESH_INDEX (f, &iter, bm, BM_FACES_OF_MESH, f_index) {
      float fac = -1.0f;

      if (f->len > 3) {
        BMLoop *l_iter, *l_first;

        fac = 0.0f;
        l_iter = l_first = BM_FACE_FIRST_LOOP(f);
        do {
          const float *no_face;
          float no_corner[3];
          if (!mr.bm_vert_coords.is_empty()) {
            no_face = mr.bm_face_normals[f_index];
            BM_loop_calc_face_normal_safe_vcos(
                l_iter,
                no_face,
                reinterpret_cast<const float (*)[3]>(mr.bm_vert_coords.data()),
                no_corner);
          }
          else {
            no_face = f->no;
            BM_loop_calc_face_normal_safe(l_iter, no_corner);
          }

          /* simple way to detect (what is most likely) concave */
          if (dot_v3v3(no_face, no_corner) < 0.0f) {
            negate_v3(no_corner);
          }
          fac = max_ff(fac, angle_normalized_v3v3(no_face, no_corner));

        } while ((l_iter = l_iter->next) != l_first);
        fac *= 2.0f;
      }

      fac = distort_remap(fac, min, max, minmax_irange);
      for (int i = 0; i < f->len; i++, l_index++) {
        r_distort[l_index] = fac;
      }
    }
  }
  else {
    for (const int face_index : mr.faces.index_range()) {
      const IndexRange face = mr.faces[face_index];
      float fac = -1.0f;

      if (face.size() > 3) {
        const float *f_no = mr.face_normals[face_index];
        fac = 0.0f;

        for (const int corner : face.drop_front(1)) {
          const int corner_prev = bke::mesh::face_corner_prev(face, corner);
          const int corner_next = bke::mesh::face_corner_next(face, corner);
          float no_corner[3];
          normal_tri_v3(no_corner,
                        mr.vert_positions[mr.corner_verts[corner_prev]],
                        mr.vert_positions[mr.corner_verts[corner]],
                        mr.vert_positions[mr.corner_verts[corner_next]]);
          /* simple way to detect (what is most likely) concave */
          if (dot_v3v3(f_no, no_corner) < 0.0f) {
            negate_v3(no_corner);
          }
          fac = max_ff(fac, angle_normalized_v3v3(f_no, no_corner));
        }
        fac *= 2.0f;
      }

      fac = distort_remap(fac, min, max, minmax_irange);
      for (const int corner : face) {
        r_distort[corner] = fac;
      }
    }
  }
}

BLI_INLINE float sharp_remap(float fac, float min, float /*max*/, float minmax_irange)
{
  /* important not '>=' */
  if (fac > min) {
    fac = (fac - min) * minmax_irange;
    CLAMP(fac, 0.0f, 1.0f);
  }
  else {
    /* fallback */
    fac = -1.0f;
  }
  return fac;
}

static void statvis_calc_sharp(const MeshRenderData &mr, MutableSpan<float> r_sharp)
{
  const MeshStatVis *statvis = &mr.toolsettings->statvis;
  const float min = statvis->sharp_min;
  const float max = statvis->sharp_max;
  const float minmax_irange = 1.0f / (max - min);

  /* Can we avoid this extra allocation? */
  float *vert_angles = MEM_malloc_arrayN<float>(mr.verts_num, __func__);
  copy_vn_fl(vert_angles, mr.verts_num, -M_PI);

  if (mr.extract_type == MeshExtractType::BMesh) {
    BMEditMesh *em = mr.edit_bmesh;
    BMIter iter;
    BMesh *bm = em->bm;
    BMFace *efa;
    BMEdge *e;
    /* first assign float values to verts */
    BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
      float angle = BM_edge_calc_face_angle_signed(e);
      float *col1 = &vert_angles[BM_elem_index_get(e->v1)];
      float *col2 = &vert_angles[BM_elem_index_get(e->v2)];
      *col1 = max_ff(*col1, angle);
      *col2 = max_ff(*col2, angle);
    }
    /* Copy vert value to loops. */
    BM_ITER_MESH (efa, &iter, bm, BM_FACES_OF_MESH) {
      BMLoop *l_iter, *l_first;
      l_iter = l_first = BM_FACE_FIRST_LOOP(efa);
      do {
        int l_index = BM_elem_index_get(l_iter);
        int v_index = BM_elem_index_get(l_iter->v);
        r_sharp[l_index] = sharp_remap(vert_angles[v_index], min, max, minmax_irange);
      } while ((l_iter = l_iter->next) != l_first);
    }
  }
  else {
    /* first assign float values to verts */

    Map<OrderedEdge, int> eh;
    eh.reserve(mr.edges_num);

    for (int face_index = 0; face_index < mr.faces_num; face_index++) {
      const IndexRange face = mr.faces[face_index];
      for (const int corner : face) {
        const int vert_curr = mr.corner_verts[corner];
        const int vert_next = mr.corner_verts[bke::mesh::face_corner_next(face, corner)];
        float angle;
        eh.add_or_modify(
            {vert_curr, vert_next},
            [&](int *value) { *value = face_index; },
            [&](int *value) {
              const int other_face_index = *value;
              if (other_face_index == -1) {
                /* non-manifold edge */
                angle = DEG2RADF(90.0f);
                return;
              }
              const float *f1_no = mr.face_normals[face_index];
              const float *f2_no = mr.face_normals[other_face_index];
              angle = angle_normalized_v3v3(f1_no, f2_no);
              angle = is_edge_convex_v3(mr.vert_positions[vert_curr],
                                        mr.vert_positions[vert_next],
                                        f1_no,
                                        f2_no) ?
                          angle :
                          -angle;
              /* Tag as manifold. */
              *value = -1;
            });
        float *col1 = &vert_angles[vert_curr];
        float *col2 = &vert_angles[vert_next];
        *col1 = max_ff(*col1, angle);
        *col2 = max_ff(*col2, angle);
      }
    }
    /* Remaining non manifold edges. */
    for (const OrderedEdge &edge : eh.keys()) {
      const float angle = DEG2RADF(90.0f);
      float *col1 = &vert_angles[edge.v_low];
      float *col2 = &vert_angles[edge.v_high];
      *col1 = max_ff(*col1, angle);
      *col2 = max_ff(*col2, angle);
    }

    for (int l_index = 0; l_index < mr.corners_num; l_index++) {
      const int vert = mr.corner_verts[l_index];
      r_sharp[l_index] = sharp_remap(vert_angles[vert], min, max, minmax_irange);
    }
  }

  MEM_freeN(vert_angles);
}

gpu::VertBufPtr extract_mesh_analysis(const MeshRenderData &mr, const float4x4 &object_to_world)
{
  static const GPUVertFormat format = GPU_vertformat_from_attribute("weight",
                                                                    gpu::VertAttrType::SFLOAT_32);

  gpu::VertBufPtr vbo = gpu::VertBufPtr(GPU_vertbuf_create_with_format(format));
  GPU_vertbuf_data_alloc(*vbo, mr.corners_num);
  MutableSpan<float> vbo_data = vbo->data<float>();

  switch (mr.toolsettings->statvis.type) {
    case SCE_STATVIS_OVERHANG:
      statvis_calc_overhang(mr, object_to_world, vbo_data);
      break;
    case SCE_STATVIS_THICKNESS:
      statvis_calc_thickness(mr, object_to_world, vbo_data);
      break;
    case SCE_STATVIS_INTERSECT:
      statvis_calc_intersect(mr, vbo_data);
      break;
    case SCE_STATVIS_DISTORT:
      statvis_calc_distort(mr, vbo_data);
      break;
    case SCE_STATVIS_SHARP:
      statvis_calc_sharp(mr, vbo_data);
      break;
  }
  return vbo;
}

}  // namespace blender::draw
