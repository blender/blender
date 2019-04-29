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
 */

/** \file
 * \ingroup bmesh
 *
 * Convert triangle to quads.
 *
 * TODO
 * - convert triangles to any sided faces, not just quads.
 */

#include "MEM_guardedalloc.h"

#include "DNA_meshdata_types.h"

#include "BLI_math.h"
#include "BLI_sort_utils.h"

#include "BKE_customdata.h"

#include "bmesh.h"

#include "intern/bmesh_operators_private.h" /* own include */

/* assumes edges are validated before reaching this poin */
static float quad_calc_error(const float v1[3],
                             const float v2[3],
                             const float v3[3],
                             const float v4[3])
{
  /* Gives a 'weight' to a pair of triangles that join an edge
   * to decide how good a join they would make. */
  /* Note: this is more complicated than it needs to be and should be cleaned up.. */
  float error = 0.0f;

  /* Normal difference */
  {
    float n1[3], n2[3];
    float angle_a, angle_b;
    float diff;

    normal_tri_v3(n1, v1, v2, v3);
    normal_tri_v3(n2, v1, v3, v4);
    angle_a = (compare_v3v3(n1, n2, FLT_EPSILON)) ? 0.0f : angle_normalized_v3v3(n1, n2);

    normal_tri_v3(n1, v2, v3, v4);
    normal_tri_v3(n2, v4, v1, v2);
    angle_b = (compare_v3v3(n1, n2, FLT_EPSILON)) ? 0.0f : angle_normalized_v3v3(n1, n2);

    diff = (angle_a + angle_b) / (float)(M_PI * 2);

    error += diff;
  }

  /* Colinearity */
  {
    float edge_vecs[4][3];
    float diff;

    sub_v3_v3v3(edge_vecs[0], v1, v2);
    sub_v3_v3v3(edge_vecs[1], v2, v3);
    sub_v3_v3v3(edge_vecs[2], v3, v4);
    sub_v3_v3v3(edge_vecs[3], v4, v1);

    normalize_v3(edge_vecs[0]);
    normalize_v3(edge_vecs[1]);
    normalize_v3(edge_vecs[2]);
    normalize_v3(edge_vecs[3]);

    /* a completely skinny face is 'pi' after halving */
    diff = (fabsf(angle_normalized_v3v3(edge_vecs[0], edge_vecs[1]) - (float)M_PI_2) +
            fabsf(angle_normalized_v3v3(edge_vecs[1], edge_vecs[2]) - (float)M_PI_2) +
            fabsf(angle_normalized_v3v3(edge_vecs[2], edge_vecs[3]) - (float)M_PI_2) +
            fabsf(angle_normalized_v3v3(edge_vecs[3], edge_vecs[0]) - (float)M_PI_2)) /
           (float)(M_PI * 2);

    error += diff;
  }

  /* Concavity */
  {
    float area_min, area_max, area_a, area_b;
    float diff;

    area_a = area_tri_v3(v1, v2, v3) + area_tri_v3(v1, v3, v4);
    area_b = area_tri_v3(v2, v3, v4) + area_tri_v3(v4, v1, v2);

    area_min = min_ff(area_a, area_b);
    area_max = max_ff(area_a, area_b);

    diff = area_max ? (1.0f - (area_min / area_max)) : 1.0f;

    error += diff;
  }

  return error;
}

static void bm_edge_to_quad_verts(const BMEdge *e, const BMVert *r_v_quad[4])
{
  BLI_assert(e->l->f->len == 3 && e->l->radial_next->f->len == 3);
  BLI_assert(BM_edge_is_manifold(e));
  r_v_quad[0] = e->l->v;
  r_v_quad[1] = e->l->prev->v;
  r_v_quad[2] = e->l->next->v;
  r_v_quad[3] = e->l->radial_next->prev->v;
}

/* cache customdata delimiters */
struct DelimitData_CD {
  int cd_type;
  int cd_size;
  int cd_offset;
  int cd_offset_end;
};

struct DelimitData {
  uint do_seam : 1;
  uint do_sharp : 1;
  uint do_mat : 1;
  uint do_angle_face : 1;
  uint do_angle_shape : 1;

  float angle_face;
  float angle_face__cos;

  float angle_shape;

  struct DelimitData_CD cdata[4];
  int cdata_len;
};

static bool bm_edge_is_contiguous_loop_cd_all(const BMEdge *e,
                                              const struct DelimitData_CD *delimit_data)
{
  int cd_offset;
  for (cd_offset = delimit_data->cd_offset; cd_offset < delimit_data->cd_offset_end;
       cd_offset += delimit_data->cd_size) {
    if (BM_edge_is_contiguous_loop_cd(e, delimit_data->cd_type, cd_offset) == false) {
      return false;
    }
  }

  return true;
}

static bool bm_edge_delimit_cdata(CustomData *ldata,
                                  CustomDataType type,
                                  struct DelimitData_CD *r_delim_cd)
{
  const int layer_len = CustomData_number_of_layers(ldata, type);
  r_delim_cd->cd_type = type;
  r_delim_cd->cd_size = CustomData_sizeof(r_delim_cd->cd_type);
  r_delim_cd->cd_offset = CustomData_get_n_offset(ldata, type, 0);
  r_delim_cd->cd_offset_end = r_delim_cd->cd_size * layer_len;
  return (r_delim_cd->cd_offset != -1);
}

static float bm_edge_is_delimit(const BMEdge *e, const struct DelimitData *delimit_data)
{
  BMFace *f_a = e->l->f, *f_b = e->l->radial_next->f;
#if 0
  const bool is_contig = BM_edge_is_contiguous(e);
  float angle;
#endif

  if ((delimit_data->do_seam) && (BM_elem_flag_test(e, BM_ELEM_SEAM))) {
    goto fail;
  }

  if ((delimit_data->do_sharp) && (BM_elem_flag_test(e, BM_ELEM_SMOOTH) == 0)) {
    goto fail;
  }

  if ((delimit_data->do_mat) && (f_a->mat_nr != f_b->mat_nr)) {
    goto fail;
  }

  if (delimit_data->do_angle_face) {
    if (dot_v3v3(f_a->no, f_b->no) < delimit_data->angle_face__cos) {
      goto fail;
    }
  }

  if (delimit_data->do_angle_shape) {
    const BMVert *verts[4];
    bm_edge_to_quad_verts(e, verts);

    /* if we're checking the shape at all, a flipped face is out of the question */
    if (is_quad_flip_v3(verts[0]->co, verts[1]->co, verts[2]->co, verts[3]->co)) {
      goto fail;
    }
    else {
      float edge_vecs[4][3];

      sub_v3_v3v3(edge_vecs[0], verts[0]->co, verts[1]->co);
      sub_v3_v3v3(edge_vecs[1], verts[1]->co, verts[2]->co);
      sub_v3_v3v3(edge_vecs[2], verts[2]->co, verts[3]->co);
      sub_v3_v3v3(edge_vecs[3], verts[3]->co, verts[0]->co);

      normalize_v3(edge_vecs[0]);
      normalize_v3(edge_vecs[1]);
      normalize_v3(edge_vecs[2]);
      normalize_v3(edge_vecs[3]);

      if ((fabsf(angle_normalized_v3v3(edge_vecs[0], edge_vecs[1]) - (float)M_PI_2) >
           delimit_data->angle_shape) ||
          (fabsf(angle_normalized_v3v3(edge_vecs[1], edge_vecs[2]) - (float)M_PI_2) >
           delimit_data->angle_shape) ||
          (fabsf(angle_normalized_v3v3(edge_vecs[2], edge_vecs[3]) - (float)M_PI_2) >
           delimit_data->angle_shape) ||
          (fabsf(angle_normalized_v3v3(edge_vecs[3], edge_vecs[0]) - (float)M_PI_2) >
           delimit_data->angle_shape)) {
        goto fail;
      }
    }
  }

  if (delimit_data->cdata_len) {
    int i;
    for (i = 0; i < delimit_data->cdata_len; i++) {
      if (!bm_edge_is_contiguous_loop_cd_all(e, &delimit_data->cdata[i])) {
        goto fail;
      }
    }
  }

  return false;

fail:
  return true;
}

#define EDGE_MARK (1 << 0)

#define FACE_OUT (1 << 0)
#define FACE_INPUT (1 << 2)

void bmo_join_triangles_exec(BMesh *bm, BMOperator *op)
{
  float angle_face, angle_shape;

  BMIter iter;
  BMOIter siter;
  BMFace *f;
  BMEdge *e;
  /* data: edge-to-join, sort_value: error weight */
  struct SortPtrByFloat *jedges;
  unsigned i, totedge;
  uint totedge_tag = 0;

  struct DelimitData delimit_data = {0};

  delimit_data.do_seam = BMO_slot_bool_get(op->slots_in, "cmp_seam");
  delimit_data.do_sharp = BMO_slot_bool_get(op->slots_in, "cmp_sharp");
  delimit_data.do_mat = BMO_slot_bool_get(op->slots_in, "cmp_materials");

  angle_face = BMO_slot_float_get(op->slots_in, "angle_face_threshold");
  if (angle_face < DEG2RADF(180.0f)) {
    delimit_data.angle_face = angle_face;
    delimit_data.angle_face__cos = cosf(angle_face);
    delimit_data.do_angle_face = true;
  }
  else {
    delimit_data.do_angle_face = false;
  }

  angle_shape = BMO_slot_float_get(op->slots_in, "angle_shape_threshold");
  if (angle_shape < DEG2RADF(180.0f)) {
    delimit_data.angle_shape = angle_shape;
    delimit_data.do_angle_shape = true;
  }
  else {
    delimit_data.do_angle_shape = false;
  }

  if (BMO_slot_bool_get(op->slots_in, "cmp_uvs") &&
      bm_edge_delimit_cdata(&bm->ldata, CD_MLOOPUV, &delimit_data.cdata[delimit_data.cdata_len])) {
    delimit_data.cdata_len += 1;
  }

  delimit_data.cdata[delimit_data.cdata_len].cd_offset = -1;
  if (BMO_slot_bool_get(op->slots_in, "cmp_vcols") &&
      bm_edge_delimit_cdata(
          &bm->ldata, CD_MLOOPCOL, &delimit_data.cdata[delimit_data.cdata_len])) {
    delimit_data.cdata_len += 1;
  }

  /* flag all edges of all input face */
  BMO_ITER (f, &siter, op->slots_in, "faces", BM_FACE) {
    if (f->len == 3) {
      BMO_face_flag_enable(bm, f, FACE_INPUT);
    }
  }

  /* flag edges surrounded by 2 flagged triangles */
  BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
    BMFace *f_a, *f_b;
    if (BM_edge_face_pair(e, &f_a, &f_b) &&
        (BMO_face_flag_test(bm, f_a, FACE_INPUT) && BMO_face_flag_test(bm, f_b, FACE_INPUT))) {
      if (!bm_edge_is_delimit(e, &delimit_data)) {
        BMO_edge_flag_enable(bm, e, EDGE_MARK);
        totedge_tag++;
      }
    }
  }

  if (totedge_tag == 0) {
    return;
  }

  /* over alloc, some of the edges will be delimited */
  jedges = MEM_mallocN(sizeof(*jedges) * totedge_tag, __func__);

  i = 0;
  BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
    const BMVert *verts[4];
    float error;

    if (!BMO_edge_flag_test(bm, e, EDGE_MARK)) {
      continue;
    }

    bm_edge_to_quad_verts(e, verts);

    error = quad_calc_error(verts[0]->co, verts[1]->co, verts[2]->co, verts[3]->co);

    jedges[i].data = e;
    jedges[i].sort_value = error;
    i++;
  }

  totedge = i;
  qsort(jedges, totedge, sizeof(*jedges), BLI_sortutil_cmp_float);

  for (i = 0; i < totedge; i++) {
    BMLoop *l_a, *l_b;

    e = jedges[i].data;
    l_a = e->l;
    l_b = e->l->radial_next;

    /* check if another edge already claimed this face */
    if ((l_a->f->len == 3) && (l_b->f->len == 3)) {
      BMFace *f_new;
      f_new = BM_faces_join_pair(bm, l_a, l_b, true);
      if (f_new) {
        BMO_face_flag_enable(bm, f_new, FACE_OUT);
      }
    }
  }

  MEM_freeN(jedges);

  BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "faces.out", BM_FACE, FACE_OUT);
}
