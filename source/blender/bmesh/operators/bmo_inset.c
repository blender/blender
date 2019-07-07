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
 * Inset face regions.
 * Inset individual faces.
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_alloca.h"
#include "BLI_memarena.h"
#include "BKE_customdata.h"

#include "bmesh.h"

#include "intern/bmesh_operators_private.h" /* own include */

/* Merge loop-data that diverges, see: T41445 */
#define USE_LOOP_CUSTOMDATA_MERGE

#define ELE_NEW 1

/* -------------------------------------------------------------------- */
/* Generic Interp Face (use for both types of inset) */

/**
 * Interpolation, this is more complex for regions since we're not creating new faces
 * and throwing away old ones, so instead, store face data needed for interpolation.
 *
 * \note This uses CustomData functions in quite a low-level way which should be
 * avoided, but in this case its hard to do without storing a duplicate mesh. */

/* just enough of a face to store interpolation data we can use once the inset is done */
typedef struct InterpFace {
  BMFace *f;
  void **blocks_l;
  void **blocks_v;
  float (*cos_2d)[2];
  float axis_mat[3][3];
} InterpFace;

/* basically a clone of #BM_vert_interp_from_face */
static void bm_interp_face_store(InterpFace *iface, BMesh *bm, BMFace *f, MemArena *interp_arena)
{
  BMLoop *l_iter, *l_first;
  void **blocks_l = iface->blocks_l = BLI_memarena_alloc(interp_arena,
                                                         sizeof(*iface->blocks_l) * f->len);
  void **blocks_v = iface->blocks_v = BLI_memarena_alloc(interp_arena,
                                                         sizeof(*iface->blocks_v) * f->len);
  float(*cos_2d)[2] = iface->cos_2d = BLI_memarena_alloc(interp_arena,
                                                         sizeof(*iface->cos_2d) * f->len);
  void *axis_mat = iface->axis_mat;
  int i;

  BLI_assert(BM_face_is_normal_valid(f));

  axis_dominant_v3_to_m3(axis_mat, f->no);

  iface->f = f;

  i = 0;
  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    mul_v2_m3v3(cos_2d[i], axis_mat, l_iter->v->co);
    blocks_l[i] = NULL;
    CustomData_bmesh_copy_data(&bm->ldata, &bm->ldata, l_iter->head.data, &blocks_l[i]);
    /* if we were not modifying the loops later we would do... */
    // blocks[i] = l_iter->head.data;

    blocks_v[i] = NULL;
    CustomData_bmesh_copy_data(&bm->vdata, &bm->vdata, l_iter->v->head.data, &blocks_v[i]);

    /* use later for index lookups */
    BM_elem_index_set(l_iter, i); /* set_dirty */
  } while ((void)i++, (l_iter = l_iter->next) != l_first);
  bm->elem_index_dirty |= BM_LOOP;
}
static void bm_interp_face_free(InterpFace *iface, BMesh *bm)
{
  void **blocks_l = iface->blocks_l;
  void **blocks_v = iface->blocks_v;
  int i;

  for (i = 0; i < iface->f->len; i++) {
    CustomData_bmesh_free_block(&bm->ldata, &blocks_l[i]);
    CustomData_bmesh_free_block(&bm->vdata, &blocks_v[i]);
  }
}

#ifdef USE_LOOP_CUSTOMDATA_MERGE
/**
 * This function merges loop customdata (UV's)
 * where interpolating the values across the face causes values to diverge.
 */
static void bm_loop_customdata_merge(BMesh *bm,
                                     BMEdge *e_connect,
                                     BMLoop *l_a_outer,
                                     BMLoop *l_b_outer,
                                     BMLoop *l_a_inner,
                                     BMLoop *l_b_inner)
{
  /**
   * Check for diverged values at the vert shared by
   * \a l_a_inner & \a l_b_inner.
   *
   * <pre>
   *  -----------------------+
   *           l_a_outer--> /|<--l_b_outer
   *                       / |
   *      (face a)        /  |
   *                     / <--e_connect
   *                    /    |
   * e_a  l_a_inner--> / <--l_b_inner
   * -----------------+      |
   *                 /|      |
   * l_a/b_inner_inset| (face b)
   *               /  |      |
   *              /   |e_b   |
   *  (inset face(s)) |      |
   *            /     |      |
   * </pre>
   */

  const bool is_flip = (l_a_inner->next == l_a_outer);
  BMLoop *l_a_inner_inset, *l_b_inner_inset;
  BMEdge *e_a, *e_b;
  int layer_n;

  /* paranoid sanity checks */
  BLI_assert(l_a_outer->v == l_b_outer->v);
  BLI_assert(l_a_inner->v == l_b_inner->v);

  BLI_assert(l_b_inner->f != l_a_inner->f);

  BLI_assert(l_a_outer->f == l_a_inner->f);
  BLI_assert(l_b_outer->f == l_b_inner->f);

  (void)e_connect;
  BLI_assert(BM_edge_in_face(e_connect, l_a_inner->f));
  BLI_assert(BM_edge_in_face(e_connect, l_b_inner->f));

  if (is_flip) {
    e_a = l_a_inner->prev->e;
    e_b = l_b_inner->e;
  }
  else {
    e_a = l_a_inner->e;
    e_b = l_b_inner->prev->e;
  }

  l_a_inner_inset = BM_edge_other_loop(e_a, l_a_inner);
  l_b_inner_inset = BM_edge_other_loop(e_b, l_b_inner);
  BLI_assert(l_a_inner_inset->v == l_b_inner_inset->v);

  /* check if there is no chance of diversion */
  if (l_a_inner_inset->f == l_b_inner_inset->f) {
    return;
  }

  for (layer_n = 0; layer_n < bm->ldata.totlayer; layer_n++) {
    const int type = bm->ldata.layers[layer_n].type;
    const int offset = bm->ldata.layers[layer_n].offset;
    if (!CustomData_layer_has_math(&bm->ldata, layer_n)) {
      continue;
    }

    /* check we begin with merged data */
    if ((CustomData_data_equals(type,
                                BM_ELEM_CD_GET_VOID_P(l_a_outer, offset),
                                BM_ELEM_CD_GET_VOID_P(l_b_outer, offset)) == true)

    /* Epsilon for comparing UV's is too big, gives noticeable problems. */
#  if 0
        &&
        /* check if the data ends up diverged */
        (CustomData_data_equals(type,
                                BM_ELEM_CD_GET_VOID_P(l_a_inner, offset),
                                BM_ELEM_CD_GET_VOID_P(l_b_inner, offset)) == false)
#  endif
    ) {
      /* no need to allocate a temp block:
       * a = (a + b);
       * a *= 0.5f;
       * b = a;
       */
      const void *data_src;

      CustomData_data_mix_value(type,
                                BM_ELEM_CD_GET_VOID_P(l_a_inner_inset, offset),
                                BM_ELEM_CD_GET_VOID_P(l_b_inner_inset, offset),
                                CDT_MIX_MIX,
                                0.5f);
      CustomData_data_copy_value(type,
                                 BM_ELEM_CD_GET_VOID_P(l_a_inner_inset, offset),
                                 BM_ELEM_CD_GET_VOID_P(l_b_inner_inset, offset));

      /* use this as a reference (could be 'l_b_inner_inset' too) */
      data_src = BM_ELEM_CD_GET_VOID_P(l_a_inner_inset, offset);

      /* check if the 2 faces share an edge */
      if (is_flip ? (l_b_inner_inset->e == l_a_inner_inset->prev->e) :
                    (l_a_inner_inset->e == l_b_inner_inset->prev->e)) {
        /* simple case, we have all loops already */
      }
      else {
        /* compare with (l_a_inner / l_b_inner) and assign the blended value if they match */
        BMIter iter;
        BMLoop *l_iter;
        const void *data_cmp_a = BM_ELEM_CD_GET_VOID_P(l_b_inner, offset);
        const void *data_cmp_b = BM_ELEM_CD_GET_VOID_P(l_a_inner, offset);
        BM_ITER_ELEM (l_iter, &iter, l_a_inner_inset->v, BM_LOOPS_OF_VERT) {
          if (BM_elem_flag_test(l_iter->f, BM_ELEM_TAG)) {
            if (!ELEM(l_iter, l_a_inner, l_b_inner, l_a_inner_inset, l_b_inner_inset)) {
              void *data_dst = BM_ELEM_CD_GET_VOID_P(l_iter, offset);

              if (CustomData_data_equals(type, data_dst, data_cmp_a) ||
                  CustomData_data_equals(type, data_dst, data_cmp_b)) {
                CustomData_data_copy_value(type, data_src, data_dst);
              }
            }
          }
        }
      }

      CustomData_data_copy_value(type, data_src, BM_ELEM_CD_GET_VOID_P(l_b_inner, offset));
      CustomData_data_copy_value(type, data_src, BM_ELEM_CD_GET_VOID_P(l_a_inner, offset));
    }
  }
}
#endif /* USE_LOOP_CUSTOMDATA_MERGE */

/* -------------------------------------------------------------------- */
/* Inset Individual */

static void bmo_face_inset_individual(BMesh *bm,
                                      BMFace *f,
                                      MemArena *interp_arena,
                                      const float thickness,
                                      const float depth,
                                      const bool use_even_offset,
                                      const bool use_relative_offset,
                                      const bool use_interpolate)
{
  InterpFace *iface = NULL;

  /* stores verts split away from the face (aligned with face verts) */
  BMVert **verts = BLI_array_alloca(verts, f->len);
  /* store edge normals (aligned with face-loop-edges) */
  float(*edge_nors)[3] = BLI_array_alloca(edge_nors, f->len);
  float(*coords)[3] = BLI_array_alloca(coords, f->len);

  BMLoop *l_iter, *l_first;
  BMLoop *l_other;
  uint i;
  float e_length_prev;

  l_first = BM_FACE_FIRST_LOOP(f);

  /* split off all loops */
  l_iter = l_first;
  i = 0;
  do {
    BMVert *v_other = l_iter->v;
    BMVert *v_sep = BM_face_loop_separate(bm, l_iter);
    if (v_sep == v_other) {
      v_other = BM_vert_create(bm, l_iter->v->co, l_iter->v, BM_CREATE_NOP);
    }
    verts[i] = v_other;

    /* unrelated to splitting, but calc here */
    BM_edge_calc_face_tangent(l_iter->e, l_iter, edge_nors[i]);
  } while ((void)i++, ((l_iter = l_iter->next) != l_first));

  /* build rim faces */
  l_iter = l_first;
  i = 0;
  do {
    BMFace *f_new_outer;
    BMVert *v_other = verts[i];
    BMVert *v_other_next = verts[(i + 1) % f->len];

    BMEdge *e_other = BM_edge_create(bm, v_other, v_other_next, l_iter->e, BM_CREATE_NO_DOUBLE);
    (void)e_other;

    f_new_outer = BM_face_create_quad_tri(
        bm, v_other, v_other_next, l_iter->next->v, l_iter->v, f, BM_CREATE_NOP);
    BMO_face_flag_enable(bm, f_new_outer, ELE_NEW);

    /* copy loop data */
    l_other = l_iter->radial_next;
    BM_elem_attrs_copy(bm, bm, l_iter->next, l_other->prev);
    BM_elem_attrs_copy(bm, bm, l_iter, l_other->next->next);

    if (use_interpolate == false) {
      BM_elem_attrs_copy(bm, bm, l_iter->next, l_other);
      BM_elem_attrs_copy(bm, bm, l_iter, l_other->next);
    }
  } while ((void)i++, ((l_iter = l_iter->next) != l_first));

  /* hold interpolation values */
  if (use_interpolate) {
    iface = BLI_memarena_alloc(interp_arena, sizeof(*iface));
    bm_interp_face_store(iface, bm, f, interp_arena);
  }

  /* Calculate translation vector for new */
  l_iter = l_first;
  i = 0;

  if (depth != 0.0f) {
    e_length_prev = BM_edge_calc_length(l_iter->prev->e);
  }

  do {
    const float *eno_prev = edge_nors[(i ? i : f->len) - 1];
    const float *eno_next = edge_nors[i];
    float tvec[3];
    float v_new_co[3];

    add_v3_v3v3(tvec, eno_prev, eno_next);
    normalize_v3(tvec);

    copy_v3_v3(v_new_co, l_iter->v->co);

    if (use_even_offset) {
      mul_v3_fl(tvec, shell_v3v3_mid_normalized_to_dist(eno_prev, eno_next));
    }

    /* Modify vertices and their normals */
    if (use_relative_offset) {
      mul_v3_fl(tvec,
                (BM_edge_calc_length(l_iter->e) + BM_edge_calc_length(l_iter->prev->e)) / 2.0f);
    }

    madd_v3_v3fl(v_new_co, tvec, thickness);

    /* Set normal, add depth and write new vertex position*/
    copy_v3_v3(l_iter->v->no, f->no);

    if (depth != 0.0f) {
      const float e_length = BM_edge_calc_length(l_iter->e);
      const float fac = depth * (use_relative_offset ? ((e_length_prev + e_length) * 0.5f) : 1.0f);
      e_length_prev = e_length;

      madd_v3_v3fl(v_new_co, f->no, fac);
    }

    copy_v3_v3(coords[i], v_new_co);
  } while ((void)i++, ((l_iter = l_iter->next) != l_first));

  /* update the coords */
  l_iter = l_first;
  i = 0;
  do {
    copy_v3_v3(l_iter->v->co, coords[i]);
  } while ((void)i++, ((l_iter = l_iter->next) != l_first));

  if (use_interpolate) {
    BM_face_interp_from_face_ex(bm,
                                iface->f,
                                iface->f,
                                true,
                                (const void **)iface->blocks_l,
                                (const void **)iface->blocks_v,
                                iface->cos_2d,
                                iface->axis_mat);

    /* build rim faces */
    l_iter = l_first;
    do {
      /* copy loop data */
      l_other = l_iter->radial_next;

      BM_elem_attrs_copy(bm, bm, l_iter->next, l_other);
      BM_elem_attrs_copy(bm, bm, l_iter, l_other->next);
    } while ((l_iter = l_iter->next) != l_first);

    bm_interp_face_free(iface, bm);
  }
}

/**
 * Individual Face Inset.
 * Find all tagged faces (f), duplicate edges around faces, inset verts of
 * created edges, create new faces between old and new edges, fill face
 * between connected new edges, kill old face (f).
 */
void bmo_inset_individual_exec(BMesh *bm, BMOperator *op)
{
  BMFace *f;

  BMOIter oiter;
  MemArena *interp_arena = NULL;

  const float thickness = BMO_slot_float_get(op->slots_in, "thickness");
  const float depth = BMO_slot_float_get(op->slots_in, "depth");
  const bool use_even_offset = BMO_slot_bool_get(op->slots_in, "use_even_offset");
  const bool use_relative_offset = BMO_slot_bool_get(op->slots_in, "use_relative_offset");
  const bool use_interpolate = BMO_slot_bool_get(op->slots_in, "use_interpolate");

  /* Only tag faces in slot */
  BM_mesh_elem_hflag_disable_all(bm, BM_FACE, BM_ELEM_TAG, false);

  BMO_slot_buffer_hflag_enable(bm, op->slots_in, "faces", BM_FACE, BM_ELEM_TAG, false);

  if (use_interpolate) {
    interp_arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);
  }

  BMO_ITER (f, &oiter, op->slots_in, "faces", BM_FACE) {
    bmo_face_inset_individual(bm,
                              f,
                              interp_arena,
                              thickness,
                              depth,
                              use_even_offset,
                              use_relative_offset,
                              use_interpolate);

    if (use_interpolate) {
      BLI_memarena_clear(interp_arena);
    }
  }

  /* we could flag new edges/verts too, is it useful? */
  BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "faces.out", BM_FACE, ELE_NEW);

  if (use_interpolate) {
    BLI_memarena_free(interp_arena);
  }
}

/* -------------------------------------------------------------------- */
/* Inset Region */

typedef struct SplitEdgeInfo {
  float no[3];
  float length;
  BMEdge *e_old;
  BMEdge *e_new;
  BMLoop *l;
} SplitEdgeInfo;

/**
 * return the tag loop where there is...
 * - only 1 tagged face attached to this edge.
 * - 1 or more untagged faces.
 *
 * \note this function looks to be expensive
 * but in most cases it will only do 2 iterations.
 */
static BMLoop *bm_edge_is_mixed_face_tag(BMLoop *l)
{
  if (LIKELY(l != NULL)) {
    int tot_tag = 0;
    int tot_untag = 0;
    BMLoop *l_iter;
    BMLoop *l_tag = NULL;
    l_iter = l;
    do {
      if (BM_elem_flag_test(l_iter->f, BM_ELEM_TAG)) {
        /* more than one tagged face - bail out early! */
        if (tot_tag == 1) {
          return NULL;
        }
        l_tag = l_iter;
        tot_tag++;
      }
      else {
        tot_untag++;
      }

    } while ((l_iter = l_iter->radial_next) != l);

    return ((tot_tag == 1) && (tot_untag >= 1)) ? l_tag : NULL;
  }
  else {
    return NULL;
  }
}

static float bm_edge_info_average_length(BMVert *v, SplitEdgeInfo *edge_info)
{
  BMIter iter;
  BMEdge *e;

  float len = 0.0f;
  int tot = 0;

  BM_ITER_ELEM (e, &iter, v, BM_EDGES_OF_VERT) {
    const int i = BM_elem_index_get(e);
    if (i != -1) {
      len += edge_info[i].length;
      tot++;
    }
  }

  BLI_assert(tot != 0);
  return len / (float)tot;
}

/**
 * implementation is as follows...
 *
 * - set all faces as tagged/untagged based on selection.
 * - find all edges that have 1 tagged, 1 untagged face.
 * - separate these edges and tag vertices, set their index to point to the original edge.
 * - build faces between old/new edges.
 * - inset the new edges into their faces.
 */

void bmo_inset_region_exec(BMesh *bm, BMOperator *op)
{
  const bool use_outset = BMO_slot_bool_get(op->slots_in, "use_outset");
  const bool use_boundary = BMO_slot_bool_get(op->slots_in, "use_boundary") &&
                            (use_outset == false);
  const bool use_even_offset = BMO_slot_bool_get(op->slots_in, "use_even_offset");
  const bool use_even_boundary = use_even_offset; /* could make own option */
  const bool use_relative_offset = BMO_slot_bool_get(op->slots_in, "use_relative_offset");
  const bool use_edge_rail = BMO_slot_bool_get(op->slots_in, "use_edge_rail");
  const bool use_interpolate = BMO_slot_bool_get(op->slots_in, "use_interpolate");
  const float thickness = BMO_slot_float_get(op->slots_in, "thickness");
  const float depth = BMO_slot_float_get(op->slots_in, "depth");
#ifdef USE_LOOP_CUSTOMDATA_MERGE
  const bool has_math_ldata = (use_interpolate && CustomData_has_math(&bm->ldata));
#endif

  int edge_info_len = 0;

  BMIter iter;
  SplitEdgeInfo *edge_info;
  SplitEdgeInfo *es;

  /* Interpolation Vars */
  /* an array alligned with faces but only fill items which are used. */
  InterpFace **iface_array = NULL;
  int iface_array_len;
  MemArena *interp_arena = NULL;

  /* BMVert original location storage */
  const bool use_vert_coords_orig = use_edge_rail;
  MemArena *vert_coords_orig = NULL;
  GHash *vert_coords = NULL;

  BMVert *v;
  BMEdge *e;
  BMFace *f;
  int i, k;

  if (use_interpolate) {
    interp_arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);
    /* warning, we could be more clever here and not over alloc */
    iface_array = MEM_callocN(sizeof(*iface_array) * bm->totface, __func__);
    iface_array_len = bm->totface;
  }

  if (use_outset == false) {
    BM_mesh_elem_hflag_disable_all(bm, BM_FACE, BM_ELEM_TAG, false);
    BMO_slot_buffer_hflag_enable(bm, op->slots_in, "faces", BM_FACE, BM_ELEM_TAG, false);
  }
  else {
    BM_mesh_elem_hflag_enable_all(bm, BM_FACE, BM_ELEM_TAG, false);
    BMO_slot_buffer_hflag_disable(bm, op->slots_in, "faces", BM_FACE, BM_ELEM_TAG, false);
    BMO_slot_buffer_hflag_disable(bm, op->slots_in, "faces_exclude", BM_FACE, BM_ELEM_TAG, false);
  }

  /* first count all inset edges we will split */
  /* fill in array and initialize tagging */
  BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
    if (
        /* tag if boundary is enabled */
        (use_boundary && BM_edge_is_boundary(e) && BM_elem_flag_test(e->l->f, BM_ELEM_TAG)) ||

        /* tag if edge is an interior edge inbetween a tagged and untagged face */
        (bm_edge_is_mixed_face_tag(e->l))) {
      /* tag */
      BM_elem_flag_enable(e->v1, BM_ELEM_TAG);
      BM_elem_flag_enable(e->v2, BM_ELEM_TAG);
      BM_elem_flag_enable(e, BM_ELEM_TAG);

      BM_elem_index_set(e, edge_info_len); /* set_dirty! */
      edge_info_len++;
    }
    else {
      BM_elem_flag_disable(e->v1, BM_ELEM_TAG);
      BM_elem_flag_disable(e->v2, BM_ELEM_TAG);
      BM_elem_flag_disable(e, BM_ELEM_TAG);

      BM_elem_index_set(e, -1); /* set_dirty! */
    }
  }
  bm->elem_index_dirty |= BM_EDGE;

  edge_info = MEM_mallocN(edge_info_len * sizeof(SplitEdgeInfo), __func__);

  /* fill in array and initialize tagging */
  es = edge_info;
  BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
    i = BM_elem_index_get(e);
    if (i != -1) {
      /* calc edge-split info */
      es->length = BM_edge_calc_length(e);
      es->e_old = e;
      es++;
      /* initialize no and e_new after */
    }
  }

  if (use_vert_coords_orig) {
    vert_coords_orig = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);
    vert_coords = BLI_ghash_ptr_new(__func__);
  }

  /* util macros */
#define VERT_ORIG_STORE(_v) \
  { \
    float *_co = BLI_memarena_alloc(vert_coords_orig, sizeof(float[3])); \
    copy_v3_v3(_co, (_v)->co); \
    BLI_ghash_insert(vert_coords, _v, _co); \
  } \
  (void)0
#define VERT_ORIG_GET(_v) (const float *)BLI_ghash_lookup_default(vert_coords, (_v), (_v)->co)
  /* memory for the coords isn't given back to the arena,
   * acceptable in this case since it runs a fixed number of times. */
#define VERT_ORIG_REMOVE(_v) BLI_ghash_remove(vert_coords, (_v), NULL, NULL)

  for (i = 0, es = edge_info; i < edge_info_len; i++, es++) {
    if ((es->l = bm_edge_is_mixed_face_tag(es->e_old->l))) {
      /* do nothing */
    }
    else {
      es->l = es->e_old->l; /* must be a boundary */
    }

    /* run the separate arg */
    if (!BM_edge_is_boundary(es->e_old)) {
      bmesh_kernel_edge_separate(bm, es->e_old, es->l, false);
    }

    /* calc edge-split info */
    es->e_new = es->l->e;
    BM_edge_calc_face_tangent(es->e_new, es->l, es->no);

    if (es->e_new == es->e_old) { /* happens on boundary edges */
      /* Take care here, we're creating this double edge which _must_
       * have its verts replaced later on. */
      es->e_old = BM_edge_create(bm, es->e_new->v1, es->e_new->v2, es->e_new, BM_CREATE_NOP);
    }

    /* store index back to original in 'edge_info' */
    BM_elem_index_set(es->e_new, i);
    BM_elem_flag_enable(es->e_new, BM_ELEM_TAG);

    /* important to tag again here */
    BM_elem_flag_enable(es->e_new->v1, BM_ELEM_TAG);
    BM_elem_flag_enable(es->e_new->v2, BM_ELEM_TAG);

    /* initialize interpolation vars */
    /* this could go in its own loop,
     * only use the 'es->l->f' so we don't store loops for faces which have no mixed selection
     *
     * note: faces on the other side of the inset will be interpolated too since this is hard to
     * detect, just allow it even though it will cause some redundant interpolation */
    if (use_interpolate) {
      BMIter viter;
      BM_ITER_ELEM (v, &viter, es->l->e, BM_VERTS_OF_EDGE) {
        BMIter fiter;
        BM_ITER_ELEM (f, &fiter, v, BM_FACES_OF_VERT) {
          const int j = BM_elem_index_get(f);
          if (iface_array[j] == NULL) {
            InterpFace *iface = BLI_memarena_alloc(interp_arena, sizeof(*iface));
            bm_interp_face_store(iface, bm, f, interp_arena);
            iface_array[j] = iface;
          }
        }
      }
    }
    /* done interpolation */
  }

  /* show edge normals for debugging */
#if 0
  for (i = 0, es = edge_info; i < edge_info_len; i++, es++) {
    float tvec[3];
    BMVert *v1, *v2;

    mid_v3_v3v3(tvec, es->e_new->v1->co, es->e_new->v2->co);

    v1 = BM_vert_create(bm, tvec, NULL, BM_CREATE_NOP);
    v2 = BM_vert_create(bm, tvec, NULL, BM_CREATE_NOP);
    madd_v3_v3fl(v2->co, es->no, 0.1f);
    BM_edge_create(bm, v1, v2, NULL, 0);
  }
#endif

  /* Execute the split and position verts, it would be most obvious to loop
   * over verts here but don't do this since we will be splitting them off
   * (iterating stuff you modify is bad juju)
   * instead loop over edges then their verts. */
  for (i = 0, es = edge_info; i < edge_info_len; i++, es++) {
    for (int j = 0; j < 2; j++) {
      v = (j == 0) ? es->e_new->v1 : es->e_new->v2;

      /* end confusing part - just pretend this is a typical loop on verts */

      /* only split of tagged verts - used by separated edges */

      /* comment the first part because we know this verts in a tagged face */
      if (/* v->e && */ BM_elem_flag_test(v, BM_ELEM_TAG)) {
        BMVert **vout;
        int r_vout_len;
        BMVert *v_glue = NULL;

        /* disable touching twice, this _will_ happen if the flags not disabled */
        BM_elem_flag_disable(v, BM_ELEM_TAG);

        bmesh_kernel_vert_separate(bm, v, &vout, &r_vout_len, false);
        v = NULL; /* don't use again */

        /* in some cases the edge doesn't split off */
        if (r_vout_len == 1) {
          if (use_vert_coords_orig) {
            VERT_ORIG_STORE(vout[0]);
          }
          MEM_freeN(vout);
          continue;
        }

        for (k = 0; k < r_vout_len; k++) {
          BMVert *v_split = vout[k]; /* only to avoid vout[k] all over */

          /* need to check if this vertex is from a */
          int vert_edge_tag_tot = 0;
          int vecpair[2];

          if (use_vert_coords_orig) {
            VERT_ORIG_STORE(v_split);
          }

          /* find adjacent */
          BM_ITER_ELEM (e, &iter, v_split, BM_EDGES_OF_VERT) {
            if (BM_elem_flag_test(e, BM_ELEM_TAG) && e->l &&
                BM_elem_flag_test(e->l->f, BM_ELEM_TAG)) {
              if (vert_edge_tag_tot < 2) {
                vecpair[vert_edge_tag_tot] = BM_elem_index_get(e);
                BLI_assert(vecpair[vert_edge_tag_tot] != -1);
              }

              vert_edge_tag_tot++;
            }
          }

          if (vert_edge_tag_tot != 0) {
            float tvec[3];

            if (vert_edge_tag_tot >= 2) { /* 2 edge users - common case */
              /* now there are 2 cases to check for,
               *
               * if both edges use the same face OR both faces have the same normal,
               * ...then we can calculate an edge that fits nicely between the 2 edge normals.
               *
               * Otherwise use the shared edge OR the corner defined by these 2 face normals,
               * when both edges faces are adjacent this works best but even when this vertex
               * fans out faces it should work ok.
               */

              SplitEdgeInfo *e_info_a = &edge_info[vecpair[0]];
              SplitEdgeInfo *e_info_b = &edge_info[vecpair[1]];

              BMFace *f_a = e_info_a->l->f;
              BMFace *f_b = e_info_b->l->f;

              /* set to true when we're not in-between (e_info_a->no, e_info_b->no) exactly
               * in this case use a check the angle of the tvec when calculating shell thickness */
              bool is_mid = true;

              /* we use this as either the normal OR to find the right direction for the
               * cross product between both face normals */
              add_v3_v3v3(tvec, e_info_a->no, e_info_b->no);

              if (use_edge_rail == false) {
                /* pass */
              }
              else if (f_a != f_b) {
                /* these lookups are very quick */
                BMLoop *l_other_a = BM_loop_other_vert_loop(e_info_a->l, v_split);
                BMLoop *l_other_b = BM_loop_other_vert_loop(e_info_b->l, v_split);

                if (l_other_a->v == l_other_b->v) {
                  /* both edges faces are adjacent, but we don't need to know the shared edge
                   * having both verts is enough. */
                  const float *co_other;

                  /* note that we can't use 'l_other_a->v' directly since it
                   * may be inset and give a feedback loop. */
                  if (use_vert_coords_orig) {
                    co_other = VERT_ORIG_GET(l_other_a->v);
                  }
                  else {
                    co_other = l_other_a->v->co;
                  }

                  sub_v3_v3v3(tvec, co_other, v_split->co);
                  is_mid = false;
                }

                /* distable gives odd results at times, see [#39288] */
#if 0
                else if (compare_v3v3(f_a->no, f_b->no, 0.001f) == false) {
                  /* epsilon increased to fix [#32329] */

                  /* faces don't touch,
                   * just get cross product of their normals, its *good enough*
                   */
                  float tno[3];
                  cross_v3_v3v3(tno, f_a->no, f_b->no);
                  if (dot_v3v3(tvec, tno) < 0.0f) {
                    negate_v3(tno);
                  }
                  copy_v3_v3(tvec, tno);
                  is_mid = false;
                }
#endif
              }
              normalize_v3(tvec);

              /* scale by edge angle */
              if (use_even_offset) {
                if (is_mid) {
                  mul_v3_fl(tvec, shell_v3v3_mid_normalized_to_dist(e_info_a->no, e_info_b->no));
                }
                else {
                  /* use the largest angle */
                  mul_v3_fl(
                      tvec,
                      shell_v3v3_normalized_to_dist(tvec,
                                                    len_squared_v3v3(tvec, e_info_a->no) >
                                                            len_squared_v3v3(tvec, e_info_b->no) ?
                                                        e_info_a->no :
                                                        e_info_b->no));
                }
              }

              /* scale relative to edge lengths */
              if (use_relative_offset) {
                mul_v3_fl(tvec,
                          (edge_info[vecpair[0]].length + edge_info[vecpair[1]].length) / 2.0f);
              }
            }
            else if (vert_edge_tag_tot == 1) { /* 1 edge user - boundary vert, not so common */
              const float *e_no_a = edge_info[vecpair[0]].no;

              if (use_even_boundary) {

                /* This case where only one edge attached to v_split
                 * is used - ei - the face to inset is on a boundary.
                 *
                 *                  We want the inset to align flush with the
                 *                  boundary edge, not the normal of the interior
                 *             <--- edge which would give an unsightly bump.
                 * --+-------------------------+---------------+--
                 *   |^v_other    ^e_other    /^v_split        |
                 *   |                       /                 |
                 *   |                      /                  |
                 *   |                     / <- tag split edge |
                 *   |                    /                    |
                 *   |                   /                     |
                 *   |                  /                      |
                 * --+-----------------+-----------------------+--
                 *   |                                         |
                 *   |                                         |
                 *
                 * note, the fact we are doing location comparisons on verts that are moved about
                 * doesn't matter because the direction will remain the same in this case.
                 */

                BMEdge *e_other;
                BMVert *v_other;
                /* loop will always be either next of prev */
                BMLoop *l = v_split->e->l;
                if (l->prev->v == v_split) {
                  l = l->prev;
                }
                else if (l->next->v == v_split) {
                  l = l->next;
                }
                else if (l->v == v_split) {
                  /* pass */
                }
                else {
                  /* should never happen */
                  BLI_assert(0);
                }

                /* find the edge which is _not_ being split here */
                if (!BM_elem_flag_test(l->e, BM_ELEM_TAG)) {
                  e_other = l->e;
                }
                else if (!BM_elem_flag_test(l->prev->e, BM_ELEM_TAG)) {
                  e_other = l->prev->e;
                }
                else {
                  BLI_assert(0);
                  e_other = NULL;
                }

                v_other = BM_edge_other_vert(e_other, v_split);
                sub_v3_v3v3(tvec, v_other->co, v_split->co);
                normalize_v3(tvec);

                if (use_even_offset) {
                  mul_v3_fl(tvec, shell_v3v3_normalized_to_dist(e_no_a, tvec));
                }
              }
              else {
                copy_v3_v3(tvec, e_no_a);
              }

              /* use_even_offset - doesn't apply here */

              /* scale relative to edge length */
              if (use_relative_offset) {
                mul_v3_fl(tvec, edge_info[vecpair[0]].length);
              }
            }
            else {
              /* should never happen */
              BLI_assert(0);
              zero_v3(tvec);
            }

            /* apply the offset */
            madd_v3_v3fl(v_split->co, tvec, thickness);
          }

          /* this saves expensive/slow glue check for common cases */
          if (r_vout_len > 2) {
            bool ok = true;
            /* last step, NULL this vertex if has a tagged face */
            BM_ITER_ELEM (f, &iter, v_split, BM_FACES_OF_VERT) {
              if (BM_elem_flag_test(f, BM_ELEM_TAG)) {
                ok = false;
                break;
              }
            }

            if (ok) {
              if (v_glue == NULL) {
                v_glue = v_split;
              }
              else {
                if (BM_vert_splice(bm, v_glue, v_split)) {
                  if (use_vert_coords_orig) {
                    VERT_ORIG_REMOVE(v_split);
                  }
                }
              }
            }
          }
          /* end glue */
        }
        MEM_freeN(vout);
      }
    }
  }

  if (use_vert_coords_orig) {
    BLI_memarena_free(vert_coords_orig);
    BLI_ghash_free(vert_coords, NULL, NULL);
  }

  if (use_interpolate) {
    for (i = 0; i < iface_array_len; i++) {
      if (iface_array[i]) {
        InterpFace *iface = iface_array[i];
        BM_face_interp_from_face_ex(bm,
                                    iface->f,
                                    iface->f,
                                    true,
                                    (const void **)iface->blocks_l,
                                    (const void **)iface->blocks_v,
                                    iface->cos_2d,
                                    iface->axis_mat);
      }
    }
  }

  /* create faces */
  for (i = 0, es = edge_info; i < edge_info_len; i++, es++) {
    BMVert *varr[4] = {NULL};
    int j;
    /* get the verts in the correct order */
    BM_edge_ordered_verts_ex(es->e_new, &varr[1], &varr[0], es->l);
#if 0
    if (varr[0] == es->e_new->v1) {
      varr[2] = es->e_old->v2;
      varr[3] = es->e_old->v1;
    }
    else {
      varr[2] = es->e_old->v1;
      varr[3] = es->e_old->v2;
    }
    j = 4;
#else
    /* slightly trickier check - since we can't assume the verts are split */
    j = 2; /* 2 edges are set */
    if (varr[0] == es->e_new->v1) {
      if (es->e_old->v2 != es->e_new->v2) {
        varr[j++] = es->e_old->v2;
      }
      if (es->e_old->v1 != es->e_new->v1) {
        varr[j++] = es->e_old->v1;
      }
    }
    else {
      if (es->e_old->v1 != es->e_new->v1) {
        varr[j++] = es->e_old->v1;
      }
      if (es->e_old->v2 != es->e_new->v2) {
        varr[j++] = es->e_old->v2;
      }
    }

    if (j == 2) {
      /* can't make face! */
      continue;
    }
#endif
    /* no need to check doubles, we KNOW there won't be any */
    /* yes - reverse face is correct in this case */
    f = BM_face_create_verts(bm, varr, j, es->l->f, BM_CREATE_NOP, true);
    BMO_face_flag_enable(bm, f, ELE_NEW);

    /* Copy for loop data, otherwise UV's and vcols are no good.
     * tiny speedup here we could be more clever and copy from known adjacent data
     * also - we could attempt to interpolate the loop data,
     * this would be much slower but more useful too. */
    if (0) {
      /* Don't use this because face boundaries have no adjacent loops and won't be filled in.
       * instead copy from the opposite side with the code below */
      BM_face_copy_shared(bm, f, NULL, NULL);
    }
    else {
      /* 2 inner loops on the edge between the new face and the original */
      BMLoop *l_a;
      BMLoop *l_b;
      BMLoop *l_a_other;
      BMLoop *l_b_other;

      l_a = BM_FACE_FIRST_LOOP(f);
      l_b = l_a->next;

      /* we know this side has a radial_next because of the order of created verts in the quad */
      l_a_other = BM_edge_other_loop(l_a->e, l_a);
      l_b_other = BM_edge_other_loop(l_a->e, l_b);
      BM_elem_attrs_copy(bm, bm, l_a_other, l_a);
      BM_elem_attrs_copy(bm, bm, l_b_other, l_b);

      BLI_assert(l_a->f != l_a_other->f);
      BLI_assert(l_b->f != l_b_other->f);

      /* step around to the opposite side of the quad - warning, this may have no other edges! */
      l_a = l_a->next->next;
      l_b = l_a->next;

      /**
       * Loops vars from newly created face (face_a/b)
       * <pre>
       *              l_a->e & l_b->prev->e
       * +------------------------------------+
       * |\ l_a                          l_b /|
       * | \ l_a->prev->e            l_b->e / |
       * |  \ l_a->prev          l_b->next /  |
       * |   +----------------------------+   |
       * |   |l_a_other    ^     l_b_other|   |
       * |   |        l_b->next->e &...   |   |
       * |   |        l_a->prev->prev->e  |   |
       * |   |        (inset face)        |   |
       * |   +----------------------------+   |
       * |  /                              \  |
       * | /                                \ |
       * |/                                  \|
       * +------------------------------------+
       * </pre>
       */

      /* swap a<->b intentionally */
      if (use_interpolate) {
        InterpFace *iface = iface_array[BM_elem_index_get(es->l->f)];
        const int i_a = BM_elem_index_get(l_a_other);
        const int i_b = BM_elem_index_get(l_b_other);
        CustomData_bmesh_free_block_data(&bm->ldata, l_b->head.data);
        CustomData_bmesh_free_block_data(&bm->ldata, l_a->head.data);
        CustomData_bmesh_copy_data(&bm->ldata, &bm->ldata, iface->blocks_l[i_a], &l_b->head.data);
        CustomData_bmesh_copy_data(&bm->ldata, &bm->ldata, iface->blocks_l[i_b], &l_a->head.data);

#ifdef USE_LOOP_CUSTOMDATA_MERGE
        if (has_math_ldata) {
          BMEdge *e_connect;

          /* connecting edge 'a' */
          e_connect = l_a->prev->e;
          if (BM_edge_is_manifold(e_connect)) {
            bm_loop_customdata_merge(bm,
                                     e_connect,
                                     l_a,
                                     BM_edge_other_loop(e_connect, l_a),
                                     l_a->prev,
                                     BM_edge_other_loop(e_connect, l_a->prev));
          }

          /* connecting edge 'b' */
          e_connect = l_b->e;
          if (BM_edge_is_manifold(e_connect)) {
            /* swap arg order to maintain winding */
            bm_loop_customdata_merge(bm,
                                     e_connect,
                                     l_b,
                                     BM_edge_other_loop(e_connect, l_b),
                                     l_b->next,
                                     BM_edge_other_loop(e_connect, l_b->next));
          }
        }
#endif /* USE_LOOP_CUSTOMDATA_MERGE */
      }
      else {
        BM_elem_attrs_copy(bm, bm, l_a_other, l_b);
        BM_elem_attrs_copy(bm, bm, l_b_other, l_a);
      }
    }
  }

  if (use_interpolate) {
    for (i = 0; i < iface_array_len; i++) {
      if (iface_array[i]) {
        bm_interp_face_free(iface_array[i], bm);
      }
    }

    BLI_memarena_free(interp_arena);
    MEM_freeN(iface_array);
  }

  /* we could flag new edges/verts too, is it useful? */
  BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "faces.out", BM_FACE, ELE_NEW);

  /* cheap feature to add depth to the inset */
  if (depth != 0.0f) {
    float(*varr_co)[3];
    BMOIter oiter;

    /* We need to re-calculate tagged normals,
     * but for this purpose we can copy tagged verts from the faces they inset from. */
    for (i = 0, es = edge_info; i < edge_info_len; i++, es++) {
      zero_v3(es->e_new->v1->no);
      zero_v3(es->e_new->v2->no);
    }
    for (i = 0, es = edge_info; i < edge_info_len; i++, es++) {
      const float *no = es->l->f->no;
      add_v3_v3(es->e_new->v1->no, no);
      add_v3_v3(es->e_new->v2->no, no);
    }
    for (i = 0, es = edge_info; i < edge_info_len; i++, es++) {
      /* annoying, avoid normalizing twice */
      if (len_squared_v3(es->e_new->v1->no) != 1.0f) {
        normalize_v3(es->e_new->v1->no);
      }
      if (len_squared_v3(es->e_new->v2->no) != 1.0f) {
        normalize_v3(es->e_new->v2->no);
      }
    }
    /* done correcting edge verts normals */

    /* untag verts */
    BM_mesh_elem_hflag_disable_all(bm, BM_VERT, BM_ELEM_TAG, false);

    /* tag face verts */
    BMO_ITER (f, &oiter, op->slots_in, "faces", BM_FACE) {
      BM_ITER_ELEM (v, &iter, f, BM_VERTS_OF_FACE) {
        BM_elem_flag_enable(v, BM_ELEM_TAG);
      }
    }

    /* do in 2 passes so moving the verts doesn't feed back into face angle checks
     * which BM_vert_calc_shell_factor uses. */

    /* over allocate */
    varr_co = MEM_callocN(sizeof(*varr_co) * bm->totvert, __func__);

    BM_ITER_MESH_INDEX (v, &iter, bm, BM_VERTS_OF_MESH, i) {
      if (BM_elem_flag_test(v, BM_ELEM_TAG)) {
        const float fac = (depth *
                           (use_relative_offset ? bm_edge_info_average_length(v, edge_info) :
                                                  1.0f) *
                           (use_even_boundary ? BM_vert_calc_shell_factor(v) : 1.0f));
        madd_v3_v3v3fl(varr_co[i], v->co, v->no, fac);
      }
    }

    BM_ITER_MESH_INDEX (v, &iter, bm, BM_VERTS_OF_MESH, i) {
      if (BM_elem_flag_test(v, BM_ELEM_TAG)) {
        copy_v3_v3(v->co, varr_co[i]);
      }
    }
    MEM_freeN(varr_co);
  }

  MEM_freeN(edge_info);
}
