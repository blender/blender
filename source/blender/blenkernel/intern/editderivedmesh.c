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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 *
 * basic design:
 *
 * the bmesh derivedmesh exposes the mesh as triangles.  it stores pointers
 * to three loops per triangle.  the derivedmesh stores a cache of tessellations
 * for each face.  this cache will smartly update as needed (though at first
 * it'll simply be more brute force).  keeping track of face/edge counts may
 * be a small problem.
 *
 * this won't be the most efficient thing, considering that internal edges and
 * faces of tessellations are exposed.  looking up an edge by index in particular
 * is likely to be a little slow.
 */

#include "atomic_ops.h"

#include "BLI_math.h"
#include "BLI_jitter_2d.h"
#include "BLI_bitmap.h"
#include "BLI_task.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_deform.h"
#include "BKE_mesh.h"
#include "BKE_mesh_iterators.h"
#include "BKE_editmesh.h"
#include "BKE_editmesh_bvh.h"
#include "BKE_editmesh_cache.h"
#include "BKE_editmesh_tangent.h"

#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_mesh_types.h"

#include "MEM_guardedalloc.h"

/* -------------------------------------------------------------------- */
/* StatVis Functions */

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

static void statvis_calc_overhang(BMEditMesh *em,
                                  const float (*polyNos)[3],
                                  /* values for calculating */
                                  const float min,
                                  const float max,
                                  const char axis,
                                  /* result */
                                  unsigned char (*r_face_colors)[4])
{
  BMIter iter;
  BMesh *bm = em->bm;
  BMFace *f;
  float dir[3];
  int index;
  const float minmax_irange = 1.0f / (max - min);
  bool is_max;

  /* fallback */
  unsigned char col_fallback[4] = {64, 64, 64, 255};  /* gray */
  unsigned char col_fallback_max[4] = {0, 0, 0, 255}; /* max color */

  BLI_assert(min <= max);

  axis_from_enum_v3(dir, axis);

  if (LIKELY(em->ob)) {
    mul_transposed_mat3_m4_v3(em->ob->obmat, dir);
    normalize_v3(dir);
  }

  /* fallback max */
  {
    float fcol[3];
    BKE_defvert_weight_to_rgb(fcol, 1.0f);
    rgb_float_to_uchar(col_fallback_max, fcol);
  }

  /* now convert into global space */
  BM_ITER_MESH_INDEX (f, &iter, bm, BM_FACES_OF_MESH, index) {
    float fac = angle_normalized_v3v3(polyNos ? polyNos[index] : f->no, dir) / (float)M_PI;

    /* remap */
    if ((is_max = (fac <= max)) && (fac >= min)) {
      float fcol[3];
      fac = (fac - min) * minmax_irange;
      fac = 1.0f - fac;
      CLAMP(fac, 0.0f, 1.0f);
      BKE_defvert_weight_to_rgb(fcol, fac);
      rgb_float_to_uchar(r_face_colors[index], fcol);
    }
    else {
      const unsigned char *fallback = is_max ? col_fallback_max : col_fallback;
      copy_v4_v4_uchar(r_face_colors[index], fallback);
    }
  }
}

/* so we can use jitter values for face interpolation */
static void uv_from_jitter_v2(float uv[2])
{
  uv[0] += 0.5f;
  uv[1] += 0.5f;
  if (uv[0] + uv[1] > 1.0f) {
    uv[0] = 1.0f - uv[0];
    uv[1] = 1.0f - uv[1];
  }

  CLAMP(uv[0], 0.0f, 1.0f);
  CLAMP(uv[1], 0.0f, 1.0f);
}

static void statvis_calc_thickness(BMEditMesh *em,
                                   const float (*vertexCos)[3],
                                   /* values for calculating */
                                   const float min,
                                   const float max,
                                   const int samples,
                                   /* result */
                                   unsigned char (*r_face_colors)[4])
{
  const float eps_offset = 0.00002f;          /* values <= 0.00001 give errors */
  float *face_dists = (float *)r_face_colors; /* cheating */
  const bool use_jit = samples < 32;
  float jit_ofs[32][2];
  BMesh *bm = em->bm;
  const int tottri = em->tottri;
  const float minmax_irange = 1.0f / (max - min);
  int i;

  struct BMLoop *(*looptris)[3] = em->looptris;

  /* fallback */
  const unsigned char col_fallback[4] = {64, 64, 64, 255};

  struct BMBVHTree *bmtree;

  BLI_assert(min <= max);

  copy_vn_fl(face_dists, em->bm->totface, max);

  if (use_jit) {
    int j;
    BLI_assert(samples < 32);
    BLI_jitter_init(jit_ofs, samples);

    for (j = 0; j < samples; j++) {
      uv_from_jitter_v2(jit_ofs[j]);
    }
  }

  BM_mesh_elem_index_ensure(bm, BM_FACE);
  if (vertexCos) {
    BM_mesh_elem_index_ensure(bm, BM_VERT);
  }

  bmtree = BKE_bmbvh_new_from_editmesh(em, 0, vertexCos, false);

  for (i = 0; i < tottri; i++) {
    BMFace *f_hit;
    BMLoop **ltri = looptris[i];
    const int index = BM_elem_index_get(ltri[0]->f);
    const float *cos[3];
    float ray_co[3];
    float ray_no[3];

    if (vertexCos) {
      cos[0] = vertexCos[BM_elem_index_get(ltri[0]->v)];
      cos[1] = vertexCos[BM_elem_index_get(ltri[1]->v)];
      cos[2] = vertexCos[BM_elem_index_get(ltri[2]->v)];
    }
    else {
      cos[0] = ltri[0]->v->co;
      cos[1] = ltri[1]->v->co;
      cos[2] = ltri[2]->v->co;
    }

    normal_tri_v3(ray_no, cos[2], cos[1], cos[0]);

#define FACE_RAY_TEST_ANGLE \
  f_hit = BKE_bmbvh_ray_cast(bmtree, ray_co, ray_no, 0.0f, &dist, NULL, NULL); \
  if (f_hit && dist < face_dists[index]) { \
    float angle_fac = fabsf(dot_v3v3(ltri[0]->f->no, f_hit->no)); \
    angle_fac = 1.0f - angle_fac; \
    angle_fac = angle_fac * angle_fac * angle_fac; \
    angle_fac = 1.0f - angle_fac; \
    dist /= angle_fac; \
    if (dist < face_dists[index]) { \
      face_dists[index] = dist; \
    } \
  } \
  (void)0

    if (use_jit) {
      int j;
      for (j = 0; j < samples; j++) {
        float dist = face_dists[index];
        interp_v3_v3v3v3_uv(ray_co, cos[0], cos[1], cos[2], jit_ofs[j]);
        madd_v3_v3fl(ray_co, ray_no, eps_offset);

        FACE_RAY_TEST_ANGLE;
      }
    }
    else {
      float dist = face_dists[index];
      mid_v3_v3v3v3(ray_co, cos[0], cos[1], cos[2]);
      madd_v3_v3fl(ray_co, ray_no, eps_offset);

      FACE_RAY_TEST_ANGLE;
    }
  }

  BKE_bmbvh_free(bmtree);

  /* convert floats into color! */
  for (i = 0; i < bm->totface; i++) {
    float fac = face_dists[i];

    /* important not '<=' */
    if (fac < max) {
      float fcol[3];
      fac = (fac - min) * minmax_irange;
      fac = 1.0f - fac;
      CLAMP(fac, 0.0f, 1.0f);
      BKE_defvert_weight_to_rgb(fcol, fac);
      rgb_float_to_uchar(r_face_colors[i], fcol);
    }
    else {
      copy_v4_v4_uchar(r_face_colors[i], col_fallback);
    }
  }
}

static void statvis_calc_intersect(BMEditMesh *em,
                                   const float (*vertexCos)[3],
                                   /* result */
                                   unsigned char (*r_face_colors)[4])
{
  BMesh *bm = em->bm;
  int i;

  /* fallback */
  // const char col_fallback[4] = {64, 64, 64, 255};
  float fcol[3];
  unsigned char col[3];

  struct BMBVHTree *bmtree;
  BVHTreeOverlap *overlap;
  unsigned int overlap_len;

  memset(r_face_colors, 64, sizeof(int) * em->bm->totface);

  BM_mesh_elem_index_ensure(bm, BM_FACE);
  if (vertexCos) {
    BM_mesh_elem_index_ensure(bm, BM_VERT);
  }

  bmtree = BKE_bmbvh_new_from_editmesh(em, 0, vertexCos, false);

  overlap = BKE_bmbvh_overlap(bmtree, bmtree, &overlap_len);

  /* same for all faces */
  BKE_defvert_weight_to_rgb(fcol, 1.0f);
  rgb_float_to_uchar(col, fcol);

  if (overlap) {
    for (i = 0; i < overlap_len; i++) {
      BMFace *f_hit_pair[2] = {
          em->looptris[overlap[i].indexA][0]->f,
          em->looptris[overlap[i].indexB][0]->f,
      };
      int j;

      for (j = 0; j < 2; j++) {
        BMFace *f_hit = f_hit_pair[j];
        int index;

        index = BM_elem_index_get(f_hit);

        copy_v3_v3_uchar(r_face_colors[index], col);
      }
    }
    MEM_freeN(overlap);
  }

  BKE_bmbvh_free(bmtree);
}

static void statvis_calc_distort(BMEditMesh *em,
                                 const float (*vertexCos)[3],
                                 const float (*polyNos)[3],
                                 /* values for calculating */
                                 const float min,
                                 const float max,
                                 /* result */
                                 unsigned char (*r_face_colors)[4])
{
  BMIter iter;
  BMesh *bm = em->bm;
  BMFace *f;
  const float *f_no;
  int index;
  const float minmax_irange = 1.0f / (max - min);

  /* fallback */
  const unsigned char col_fallback[4] = {64, 64, 64, 255};

  /* now convert into global space */
  BM_ITER_MESH_INDEX (f, &iter, bm, BM_FACES_OF_MESH, index) {
    float fac;

    if (f->len == 3) {
      fac = -1.0f;
    }
    else {
      BMLoop *l_iter, *l_first;
      if (vertexCos) {
        f_no = polyNos[index];
      }
      else {
        f_no = f->no;
      }

      fac = 0.0f;
      l_iter = l_first = BM_FACE_FIRST_LOOP(f);
      do {
        float no_corner[3];
        if (vertexCos) {
          normal_tri_v3(no_corner,
                        vertexCos[BM_elem_index_get(l_iter->prev->v)],
                        vertexCos[BM_elem_index_get(l_iter->v)],
                        vertexCos[BM_elem_index_get(l_iter->next->v)]);
        }
        else {
          BM_loop_calc_face_normal_safe(l_iter, no_corner);
        }
        /* simple way to detect (what is most likely) concave */
        if (dot_v3v3(f_no, no_corner) < 0.0f) {
          negate_v3(no_corner);
        }
        fac = max_ff(fac, angle_normalized_v3v3(f_no, no_corner));
      } while ((l_iter = l_iter->next) != l_first);
      fac *= 2.0f;
    }

    /* remap */
    if (fac >= min) {
      float fcol[3];
      fac = (fac - min) * minmax_irange;
      CLAMP(fac, 0.0f, 1.0f);
      BKE_defvert_weight_to_rgb(fcol, fac);
      rgb_float_to_uchar(r_face_colors[index], fcol);
    }
    else {
      copy_v4_v4_uchar(r_face_colors[index], col_fallback);
    }
  }
}

static void statvis_calc_sharp(BMEditMesh *em,
                               const float (*vertexCos)[3],
                               /* values for calculating */
                               const float min,
                               const float max,
                               /* result */
                               unsigned char (*r_vert_colors)[4])
{
  float *vert_angles = (float *)r_vert_colors; /* cheating */
  BMIter iter;
  BMesh *bm = em->bm;
  BMEdge *e;
  // float f_no[3];
  const float minmax_irange = 1.0f / (max - min);
  int i;

  /* fallback */
  const unsigned char col_fallback[4] = {64, 64, 64, 255};

  (void)vertexCos; /* TODO */

  copy_vn_fl(vert_angles, em->bm->totvert, -M_PI);

  /* first assign float values to verts */
  BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
    float angle = BM_edge_calc_face_angle_signed(e);
    float *col1 = &vert_angles[BM_elem_index_get(e->v1)];
    float *col2 = &vert_angles[BM_elem_index_get(e->v2)];
    *col1 = max_ff(*col1, angle);
    *col2 = max_ff(*col2, angle);
  }

  /* convert floats into color! */
  for (i = 0; i < bm->totvert; i++) {
    float fac = vert_angles[i];

    /* important not '<=' */
    if (fac > min) {
      float fcol[3];
      fac = (fac - min) * minmax_irange;
      CLAMP(fac, 0.0f, 1.0f);
      BKE_defvert_weight_to_rgb(fcol, fac);
      rgb_float_to_uchar(r_vert_colors[i], fcol);
    }
    else {
      copy_v4_v4_uchar(r_vert_colors[i], col_fallback);
    }
  }
}

void BKE_editmesh_statvis_calc(BMEditMesh *em, EditMeshData *emd, const MeshStatVis *statvis)
{
  switch (statvis->type) {
    case SCE_STATVIS_OVERHANG: {
      BKE_editmesh_color_ensure(em, BM_FACE);
      statvis_calc_overhang(em,
                            emd ? emd->polyNos : NULL,
                            statvis->overhang_min / (float)M_PI,
                            statvis->overhang_max / (float)M_PI,
                            statvis->overhang_axis,
                            em->derivedFaceColor);
      break;
    }
    case SCE_STATVIS_THICKNESS: {
      const float scale = 1.0f / mat4_to_scale(em->ob->obmat);
      BKE_editmesh_color_ensure(em, BM_FACE);
      statvis_calc_thickness(em,
                             emd ? emd->vertexCos : NULL,
                             statvis->thickness_min * scale,
                             statvis->thickness_max * scale,
                             statvis->thickness_samples,
                             em->derivedFaceColor);
      break;
    }
    case SCE_STATVIS_INTERSECT: {
      BKE_editmesh_color_ensure(em, BM_FACE);
      statvis_calc_intersect(em, emd ? emd->vertexCos : NULL, em->derivedFaceColor);
      break;
    }
    case SCE_STATVIS_DISTORT: {
      BKE_editmesh_color_ensure(em, BM_FACE);

      if (emd) {
        BKE_editmesh_cache_ensure_poly_normals(em, emd);
      }

      statvis_calc_distort(em,
                           emd ? emd->vertexCos : NULL,
                           emd ? emd->polyNos : NULL,
                           statvis->distort_min,
                           statvis->distort_max,
                           em->derivedFaceColor);
      break;
    }
    case SCE_STATVIS_SHARP: {
      BKE_editmesh_color_ensure(em, BM_VERT);
      statvis_calc_sharp(em,
                         emd ? emd->vertexCos : NULL,
                         statvis->sharp_min,
                         statvis->sharp_max,
                         /* in this case they are vertex colors */
                         em->derivedVertColor);
      break;
    }
  }
}

/* -------------------------------------------------------------------- */
/* Editmesh Vert Coords */

struct CageUserData {
  int totvert;
  float (*cos_cage)[3];
  BLI_bitmap *visit_bitmap;
};

static void cage_mapped_verts_callback(void *userData,
                                       int index,
                                       const float co[3],
                                       const float UNUSED(no_f[3]),
                                       const short UNUSED(no_s[3]))
{
  struct CageUserData *data = userData;

  if ((index >= 0 && index < data->totvert) && (!BLI_BITMAP_TEST(data->visit_bitmap, index))) {
    BLI_BITMAP_ENABLE(data->visit_bitmap, index);
    copy_v3_v3(data->cos_cage[index], co);
  }
}

float (*BKE_editmesh_vertexCos_get(
    struct Depsgraph *depsgraph, BMEditMesh *em, Scene *scene, int *r_numVerts))[3]
{
  Mesh *cage;
  BLI_bitmap *visit_bitmap;
  struct CageUserData data;
  float(*cos_cage)[3];

  cage = editbmesh_get_eval_cage(depsgraph, scene, em->ob, em, &CD_MASK_BAREMESH);
  cos_cage = MEM_callocN(sizeof(*cos_cage) * em->bm->totvert, "bmbvh cos_cage");

  /* when initializing cage verts, we only want the first cage coordinate for each vertex,
   * so that e.g. mirror or array use original vertex coordinates and not mirrored or duplicate */
  visit_bitmap = BLI_BITMAP_NEW(em->bm->totvert, __func__);

  data.totvert = em->bm->totvert;
  data.cos_cage = cos_cage;
  data.visit_bitmap = visit_bitmap;

  BKE_mesh_foreach_mapped_vert(cage, cage_mapped_verts_callback, &data, MESH_FOREACH_NOP);

  MEM_freeN(visit_bitmap);

  if (r_numVerts) {
    *r_numVerts = em->bm->totvert;
  }

  return cos_cage;
}
