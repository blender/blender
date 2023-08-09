/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_alloca.h"
#include "BLI_bitmap.h"
#include "BLI_linklist_stack.h"
#include "BLI_math.h"
#include "BLI_memarena.h"

#include "BKE_context.h"
#include "BKE_crazyspace.h"
#include "BKE_editmesh.h"
#include "BKE_mesh.hh"
#include "BKE_modifier.h"
#include "BKE_scene.h"

#include "ED_mesh.hh"

#include "DEG_depsgraph_query.h"

#include "transform.hh"
#include "transform_orientations.hh"
#include "transform_snap.hh"

#include "transform_convert.hh"

/* -------------------------------------------------------------------- */
/** \name Container TransCustomData Creation
 * \{ */

static void mesh_customdata_free_fn(TransInfo *t,
                                    TransDataContainer *tc,
                                    TransCustomData *custom_data);

struct TransCustomDataLayer;
static void mesh_customdatacorrect_free(TransCustomDataLayer *tcld);

struct TransCustomData_PartialUpdate {
  BMPartialUpdate *cache;

  /** The size of proportional editing used for #BMPartialUpdate. */
  float prop_size;
  /** The size of proportional editing for the last update. */
  float prop_size_prev;
};

/**
 * \note It's important to order from least to greatest (which updates more data),
 * since the larger values are used when values change between updates
 * (which can happen when rotation is enabled with snapping).
 */
enum ePartialType {
  PARTIAL_NONE =
      -1,                 /**
                           * Update only faces between tagged and non-tagged faces (affine transformations).
                           * Use when transforming is guaranteed not to change the relative locations of vertices.
                           *
                           * This has the advantage that selecting the entire mesh or only isolated elements,
                           * can skip normal/tessellation updates entirely, so it's worth using when possible.
                           */
  PARTIAL_TYPE_GROUP = 0, /**
                           * Update for all tagged vertices (any kind of deformation).
                           * Use as a default since it can be used with any kind of deformation.
                           */
  PARTIAL_TYPE_ALL = 1,
};

#define PARTIAL_TYPE_MAX 2

/**
 * Settings used for a single update,
 * use for comparison with previous updates.
 */
struct PartialTypeState {
  ePartialType for_looptri;
  ePartialType for_normals;
};

struct TransCustomDataMesh {
  TransCustomDataLayer *cd_layer_correct;
  TransCustomData_PartialUpdate partial_update[PARTIAL_TYPE_MAX];
  PartialTypeState partial_update_state_prev;
};

static TransCustomDataMesh *mesh_customdata_ensure(TransDataContainer *tc)
{
  TransCustomDataMesh *tcmd = static_cast<TransCustomDataMesh *>(tc->custom.type.data);
  BLI_assert(tc->custom.type.data == nullptr ||
             tc->custom.type.free_cb == mesh_customdata_free_fn);
  if (tc->custom.type.data == nullptr) {
    tc->custom.type.data = MEM_callocN(sizeof(TransCustomDataMesh), __func__);
    tc->custom.type.free_cb = mesh_customdata_free_fn;
    tcmd = static_cast<TransCustomDataMesh *>(tc->custom.type.data);
    tcmd->partial_update_state_prev.for_looptri = PARTIAL_NONE;
    tcmd->partial_update_state_prev.for_normals = PARTIAL_NONE;
  }
  return tcmd;
}

static void mesh_customdata_free(TransCustomDataMesh *tcmd)
{
  if (tcmd->cd_layer_correct != nullptr) {
    mesh_customdatacorrect_free(tcmd->cd_layer_correct);
  }

  for (int i = 0; i < ARRAY_SIZE(tcmd->partial_update); i++) {
    if (tcmd->partial_update[i].cache != nullptr) {
      BM_mesh_partial_destroy(tcmd->partial_update[i].cache);
    }
  }

  MEM_freeN(tcmd);
}

static void mesh_customdata_free_fn(TransInfo * /*t*/,
                                    TransDataContainer * /*tc*/,
                                    TransCustomData *custom_data)
{
  TransCustomDataMesh *tcmd = static_cast<TransCustomDataMesh *>(custom_data->data);
  mesh_customdata_free(tcmd);
  custom_data->data = nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name CustomData TransCustomDataLayer Creation
 * \{ */

struct TransCustomDataMergeGroup {
  /** map {BMVert: TransCustomDataLayerVert} */
  LinkNode **cd_loop_groups;
};

struct TransCustomDataLayer {
  BMesh *bm;
  MemArena *arena;

  GHash *origfaces;
  BMesh *bm_origfaces;

  /* Special handle for multi-resolution. */
  int cd_loop_mdisp_offset;

  /* Optionally merge custom-data groups (this keeps UVs connected for example). */
  struct {
    /** map {BMVert: TransDataBasic} */
    GHash *origverts;
    TransCustomDataMergeGroup *data;
    int data_len;
    /** Array size of 'layer_math_map_len'
     * maps #TransCustomDataLayerVert.cd_group index to absolute #CustomData layer index */
    int *customdatalayer_map;
    /** Number of math BMLoop layers. */
    int customdatalayer_map_len;
  } merge_group;

  bool use_merge_group;
};

#define USE_FACE_SUBSTITUTE
#ifdef USE_FACE_SUBSTITUTE
#  define FACE_SUBSTITUTE_INDEX INT_MIN

/**
 * Search for a neighboring face with area and preferably without selected vertex.
 * Used to replace area-less faces in custom-data correction.
 */
static BMFace *mesh_customdatacorrect_find_best_face_substitute(BMFace *f)
{
  BMFace *best_face = nullptr;
  BMLoop *l;
  BMIter liter;
  BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
    BMLoop *l_radial_next = l->radial_next;
    BMFace *f_test = l_radial_next->f;
    if (f_test == f) {
      continue;
    }
    if (is_zero_v3(f_test->no)) {
      continue;
    }

    /* Check the loops edge isn't selected. */
    if (!BM_elem_flag_test(l_radial_next->v, BM_ELEM_SELECT) &&
        !BM_elem_flag_test(l_radial_next->next->v, BM_ELEM_SELECT))
    {
      /* Prefer edges with unselected vertices.
       * Useful for extrude. */
      best_face = f_test;
      break;
    }
    if (best_face == nullptr) {
      best_face = f_test;
    }
  }
  return best_face;
}

static void mesh_customdatacorrect_face_substitute_set(TransCustomDataLayer *tcld,
                                                       BMFace *f,
                                                       BMFace *f_copy)
{
  BLI_assert(is_zero_v3(f->no));
  BMesh *bm = tcld->bm;
  /* It is impossible to calculate the loops weights of a face without area.
   * Find a substitute. */
  BMFace *f_substitute = mesh_customdatacorrect_find_best_face_substitute(f);
  if (f_substitute) {
    /* Copy the custom-data from the substitute face. */
    BMLoop *l_iter, *l_first;
    l_iter = l_first = BM_FACE_FIRST_LOOP(f);
    do {
      BM_loop_interp_from_face(bm, l_iter, f_substitute, false, false);
    } while ((l_iter = l_iter->next) != l_first);

    /* Use the substitute face as the reference during the transformation. */
    BMFace *f_substitute_copy = BM_face_copy(tcld->bm_origfaces, bm, f_substitute, true, true);

    /* Hack: reference substitute face in `f_copy->no`.
     * `tcld->origfaces` is already used to restore the initial value. */
    BM_elem_index_set(f_copy, FACE_SUBSTITUTE_INDEX);
    *((BMFace **)&f_copy->no[0]) = f_substitute_copy;
  }
}

static BMFace *mesh_customdatacorrect_face_substitute_get(BMFace *f_copy)
{
  BLI_assert(BM_elem_index_get(f_copy) == FACE_SUBSTITUTE_INDEX);
  return *((BMFace **)&f_copy->no[0]);
}

#endif /* USE_FACE_SUBSTITUTE */

static void mesh_customdatacorrect_init_vert(TransCustomDataLayer *tcld,
                                             TransDataBasic *td,
                                             const int index)
{
  BMesh *bm = tcld->bm;
  BMVert *v = static_cast<BMVert *>(td->extra);
  BMIter liter;
  int j, l_num;
  float *loop_weights;

  // BM_ITER_ELEM (l, &liter, sv->v, BM_LOOPS_OF_VERT) {
  BM_iter_init(&liter, bm, BM_LOOPS_OF_VERT, v);
  l_num = liter.count;
  loop_weights = tcld->use_merge_group ?
                     static_cast<float *>(BLI_array_alloca(loop_weights, l_num)) :
                     nullptr;
  for (j = 0; j < l_num; j++) {
    BMLoop *l = static_cast<BMLoop *>(BM_iter_step(&liter));
    BMLoop *l_prev, *l_next;

    /* Generic custom-data correction. Copy face data. */
    void **val_p;
    if (!BLI_ghash_ensure_p(tcld->origfaces, l->f, &val_p)) {
      BMFace *f_copy = BM_face_copy(tcld->bm_origfaces, bm, l->f, true, true);
      *val_p = f_copy;
#ifdef USE_FACE_SUBSTITUTE
      if (is_zero_v3(l->f->no)) {
        mesh_customdatacorrect_face_substitute_set(tcld, l->f, f_copy);
      }
#endif
    }

    if (tcld->use_merge_group) {
      if ((l_prev = BM_loop_find_prev_nodouble(l, l->next, FLT_EPSILON)) &&
          (l_next = BM_loop_find_next_nodouble(l, l_prev, FLT_EPSILON)))
      {
        loop_weights[j] = angle_v3v3v3(l_prev->v->co, l->v->co, l_next->v->co);
      }
      else {
        loop_weights[j] = 0.0f;
      }
    }
  }

  if (tcld->use_merge_group) {
    /* Store cd_loop_groups. */
    TransCustomDataMergeGroup *merge_data = &tcld->merge_group.data[index];
    if (l_num != 0) {
      merge_data->cd_loop_groups = static_cast<LinkNode **>(BLI_memarena_alloc(
          tcld->arena, tcld->merge_group.customdatalayer_map_len * sizeof(void *)));
      for (j = 0; j < tcld->merge_group.customdatalayer_map_len; j++) {
        const int layer_nr = tcld->merge_group.customdatalayer_map[j];
        merge_data->cd_loop_groups[j] = BM_vert_loop_groups_data_layer_create(
            bm, v, layer_nr, loop_weights, tcld->arena);
      }
    }
    else {
      merge_data->cd_loop_groups = nullptr;
    }

    BLI_ghash_insert(tcld->merge_group.origverts, v, td);
  }
}

static void mesh_customdatacorrect_init_container_generic(TransDataContainer * /*tc*/,
                                                          TransCustomDataLayer *tcld)
{
  BMesh *bm = tcld->bm;

  GHash *origfaces = BLI_ghash_ptr_new(__func__);
  BMeshCreateParams params{};
  params.use_toolflags = false;
  BMesh *bm_origfaces = BM_mesh_create(&bm_mesh_allocsize_default, &params);

  /* We need to have matching loop custom-data. */
  BM_mesh_copy_init_customdata_all_layers(bm_origfaces, bm, BM_LOOP, nullptr);

  tcld->origfaces = origfaces;
  tcld->bm_origfaces = bm_origfaces;

  bmesh_edit_begin(bm, BMO_OPTYPE_FLAG_UNTAN_MULTIRES);
  tcld->cd_loop_mdisp_offset = CustomData_get_offset(&bm->ldata, CD_MDISPS);
}

static void mesh_customdatacorrect_init_container_merge_group(TransDataContainer *tc,
                                                              TransCustomDataLayer *tcld)
{
  BMesh *bm = tcld->bm;
  BLI_assert(CustomData_has_math(&bm->ldata));

  /* TODO: We don't need `layer_math_map` when there are no loops linked
   * to one of the sliding vertices. */

  /* Over allocate, only 'math' layers are indexed. */
  int *customdatalayer_map = static_cast<int *>(
      MEM_mallocN(sizeof(int) * bm->ldata.totlayer, __func__));
  int layer_math_map_len = 0;
  for (int i = 0; i < bm->ldata.totlayer; i++) {
    if (CustomData_layer_has_math(&bm->ldata, i)) {
      customdatalayer_map[layer_math_map_len++] = i;
    }
  }
  BLI_assert(layer_math_map_len != 0);

  tcld->merge_group.data_len = tc->data_len + tc->data_mirror_len;
  tcld->merge_group.customdatalayer_map = customdatalayer_map;
  tcld->merge_group.customdatalayer_map_len = layer_math_map_len;
  tcld->merge_group.origverts = BLI_ghash_ptr_new_ex(__func__, tcld->merge_group.data_len);
  tcld->merge_group.data = static_cast<TransCustomDataMergeGroup *>(BLI_memarena_alloc(
      tcld->arena, tcld->merge_group.data_len * sizeof(*tcld->merge_group.data)));
}

static TransCustomDataLayer *mesh_customdatacorrect_create_impl(TransDataContainer *tc,
                                                                const bool use_merge_group)
{
  BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);
  BMesh *bm = em->bm;

  if (bm->shapenr > 1) {
    /* Don't do this at all for non-basis shape keys, too easy to
     * accidentally break uv maps or vertex colors then */
    /* create copies of faces for custom-data projection. */
    return nullptr;
  }
  if (!CustomData_has_math(&bm->ldata) && !CustomData_has_layer(&bm->ldata, CD_MDISPS)) {
    /* There is no custom-data to correct. */
    return nullptr;
  }

  TransCustomDataLayer *tcld = static_cast<TransCustomDataLayer *>(
      MEM_callocN(sizeof(*tcld), __func__));
  tcld->bm = bm;
  tcld->arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);

  /* Init `cd_loop_mdisp_offset` to -1 to avoid problems with a valid index. */
  tcld->cd_loop_mdisp_offset = -1;
  tcld->use_merge_group = use_merge_group;

  mesh_customdatacorrect_init_container_generic(tc, tcld);

  if (tcld->use_merge_group) {
    mesh_customdatacorrect_init_container_merge_group(tc, tcld);
  }

  {
    /* Setup Verts. */
    int i = 0;

    TransData *tob = tc->data;
    for (int j = tc->data_len; j--; tob++, i++) {
      mesh_customdatacorrect_init_vert(tcld, (TransDataBasic *)tob, i);
    }

    TransDataMirror *td_mirror = tc->data_mirror;
    for (int j = tc->data_mirror_len; j--; td_mirror++, i++) {
      mesh_customdatacorrect_init_vert(tcld, (TransDataBasic *)td_mirror, i);
    }
  }

  return tcld;
}

static void mesh_customdatacorrect_create(TransDataContainer *tc, const bool use_merge_group)
{
  TransCustomDataLayer *customdatacorrect;
  customdatacorrect = mesh_customdatacorrect_create_impl(tc, use_merge_group);

  if (!customdatacorrect) {
    return;
  }

  TransCustomDataMesh *tcmd = mesh_customdata_ensure(tc);
  BLI_assert(tcmd->cd_layer_correct == nullptr);
  tcmd->cd_layer_correct = customdatacorrect;
}

static void mesh_customdatacorrect_free(TransCustomDataLayer *tcld)
{
  bmesh_edit_end(tcld->bm, BMO_OPTYPE_FLAG_UNTAN_MULTIRES);

  if (tcld->bm_origfaces) {
    BM_mesh_free(tcld->bm_origfaces);
  }
  if (tcld->origfaces) {
    BLI_ghash_free(tcld->origfaces, nullptr, nullptr);
  }
  if (tcld->merge_group.origverts) {
    BLI_ghash_free(tcld->merge_group.origverts, nullptr, nullptr);
  }
  if (tcld->arena) {
    BLI_memarena_free(tcld->arena);
  }
  if (tcld->merge_group.customdatalayer_map) {
    MEM_freeN(tcld->merge_group.customdatalayer_map);
  }

  MEM_freeN(tcld);
}

void transform_convert_mesh_customdatacorrect_init(TransInfo *t)
{
  bool use_merge_group = false;
  if (ELEM(t->mode, TFM_EDGE_SLIDE, TFM_VERT_SLIDE)) {
    if (!(t->settings->uvcalc_flag & UVCALC_TRANSFORM_CORRECT_SLIDE)) {
      /* No custom-data correction. */
      return;
    }
    use_merge_group = true;
  }
  else if (ELEM(t->mode,
                TFM_TRANSLATION,
                TFM_ROTATION,
                TFM_RESIZE,
                TFM_TOSPHERE,
                TFM_SHEAR,
                TFM_BEND,
                TFM_SHRINKFATTEN,
                TFM_TRACKBALL,
                TFM_PUSHPULL,
                TFM_ALIGN))
  {
    {
      if (!(t->settings->uvcalc_flag & UVCALC_TRANSFORM_CORRECT)) {
        /* No custom-data correction. */
        return;
      }
      use_merge_group = (t->settings->uvcalc_flag & UVCALC_TRANSFORM_CORRECT_KEEP_CONNECTED) != 0;
    }
  }
  else {
    return;
  }

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    if (tc->custom.type.data != nullptr) {
      TransCustomDataMesh *tcmd = static_cast<TransCustomDataMesh *>(tc->custom.type.data);
      if (tcmd && tcmd->cd_layer_correct) {
        mesh_customdatacorrect_free(tcmd->cd_layer_correct);
        tcmd->cd_layer_correct = nullptr;
      }
    }

    mesh_customdatacorrect_create(tc, use_merge_group);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name CustomData Layer Correction Apply
 * \{ */

/**
 * If we're sliding the vert, return its original location, if not, the current location is good.
 */
static const float *mesh_vert_orig_co_get(TransCustomDataLayer *tcld, BMVert *v)
{
  TransDataBasic *td = static_cast<TransDataBasic *>(
      BLI_ghash_lookup(tcld->merge_group.origverts, v));
  return td ? td->iloc : v->co;
}

static void mesh_customdatacorrect_apply_vert(TransCustomDataLayer *tcld,
                                              TransDataBasic *td,
                                              TransCustomDataMergeGroup *merge_data,
                                              bool do_loop_mdisps)
{
  BMesh *bm = tcld->bm;
  BMVert *v = static_cast<BMVert *>(td->extra);
  const float *co_orig_3d = td->iloc;

  BMIter liter;
  int j, l_num;
  float *loop_weights;
  const bool is_moved = (len_squared_v3v3(v->co, co_orig_3d) > FLT_EPSILON);
  const bool do_loop_weight = is_moved && tcld->merge_group.customdatalayer_map_len;
  const float *v_proj_axis = v->no;
  /* original (l->prev, l, l->next) projections for each loop ('l' remains unchanged) */
  float v_proj[3][3];

  if (do_loop_weight) {
    project_plane_normalized_v3_v3v3(v_proj[1], co_orig_3d, v_proj_axis);
  }

  // BM_ITER_ELEM (l, &liter, sv->v, BM_LOOPS_OF_VERT)
  BM_iter_init(&liter, bm, BM_LOOPS_OF_VERT, v);
  l_num = liter.count;
  loop_weights = do_loop_weight ? static_cast<float *>(BLI_array_alloca(loop_weights, l_num)) :
                                  nullptr;
  for (j = 0; j < l_num; j++) {
    BMFace *f_copy; /* the copy of 'f' */
    BMLoop *l = static_cast<BMLoop *>(BM_iter_step(&liter));

    f_copy = static_cast<BMFace *>(BLI_ghash_lookup(tcld->origfaces, l->f));

#ifdef USE_FACE_SUBSTITUTE
    /* In some faces it is not possible to calculate interpolation,
     * so we use a substitute. */
    if (BM_elem_index_get(f_copy) == FACE_SUBSTITUTE_INDEX) {
      f_copy = mesh_customdatacorrect_face_substitute_get(f_copy);
    }
#endif

    /* only loop data, no vertex data since that contains shape keys,
     * and we do not want to mess up other shape keys */
    BM_loop_interp_from_face(bm, l, f_copy, false, false);

    /* weight the loop */
    if (do_loop_weight) {
      const float eps = 1.0e-8f;
      const BMLoop *l_prev = l->prev;
      const BMLoop *l_next = l->next;
      const float *co_prev = mesh_vert_orig_co_get(tcld, l_prev->v);
      const float *co_next = mesh_vert_orig_co_get(tcld, l_next->v);
      bool co_prev_ok;
      bool co_next_ok;

      /* In the unlikely case that we're next to a zero length edge -
       * walk around the to the next.
       *
       * Since we only need to check if the vertex is in this corner,
       * its not important _which_ loop - as long as its not overlapping
       * 'sv->co_orig_3d', see: #45096. */
      project_plane_normalized_v3_v3v3(v_proj[0], co_prev, v_proj_axis);
      while (UNLIKELY(((co_prev_ok = (len_squared_v3v3(v_proj[1], v_proj[0]) > eps)) == false) &&
                      ((l_prev = l_prev->prev) != l->next)))
      {
        co_prev = mesh_vert_orig_co_get(tcld, l_prev->v);
        project_plane_normalized_v3_v3v3(v_proj[0], co_prev, v_proj_axis);
      }
      project_plane_normalized_v3_v3v3(v_proj[2], co_next, v_proj_axis);
      while (UNLIKELY(((co_next_ok = (len_squared_v3v3(v_proj[1], v_proj[2]) > eps)) == false) &&
                      ((l_next = l_next->next) != l->prev)))
      {
        co_next = mesh_vert_orig_co_get(tcld, l_next->v);
        project_plane_normalized_v3_v3v3(v_proj[2], co_next, v_proj_axis);
      }

      if (co_prev_ok && co_next_ok) {
        const float dist = dist_signed_squared_to_corner_v3v3v3(
            v->co, UNPACK3(v_proj), v_proj_axis);

        loop_weights[j] = (dist >= 0.0f) ? 1.0f : ((dist <= -eps) ? 0.0f : (1.0f + (dist / eps)));
        if (UNLIKELY(!isfinite(loop_weights[j]))) {
          loop_weights[j] = 0.0f;
        }
      }
      else {
        loop_weights[j] = 0.0f;
      }
    }
  }

  if (tcld->use_merge_group) {
    LinkNode **cd_loop_groups = merge_data->cd_loop_groups;
    if (tcld->merge_group.customdatalayer_map_len && cd_loop_groups) {
      if (do_loop_weight) {
        for (j = 0; j < tcld->merge_group.customdatalayer_map_len; j++) {
          BM_vert_loop_groups_data_layer_merge_weights(
              bm, cd_loop_groups[j], tcld->merge_group.customdatalayer_map[j], loop_weights);
        }
      }
      else {
        for (j = 0; j < tcld->merge_group.customdatalayer_map_len; j++) {
          BM_vert_loop_groups_data_layer_merge(
              bm, cd_loop_groups[j], tcld->merge_group.customdatalayer_map[j]);
        }
      }
    }
  }

  /* Special handling for multires
   *
   * Interpolate from every other loop (not ideal)
   * However values will only be taken from loops which overlap other mdisps.
   */
  const bool update_loop_mdisps = is_moved && do_loop_mdisps && (tcld->cd_loop_mdisp_offset != -1);
  if (update_loop_mdisps) {
    float(*faces_center)[3] = static_cast<float(*)[3]>(BLI_array_alloca(faces_center, l_num));
    BMLoop *l;

    BM_ITER_ELEM_INDEX (l, &liter, v, BM_LOOPS_OF_VERT, j) {
      BM_face_calc_center_median(l->f, faces_center[j]);
    }

    BM_ITER_ELEM_INDEX (l, &liter, v, BM_LOOPS_OF_VERT, j) {
      BMFace *f_copy = static_cast<BMFace *>(BLI_ghash_lookup(tcld->origfaces, l->f));
      float f_copy_center[3];
      BMIter liter_other;
      BMLoop *l_other;
      int j_other;

      BM_face_calc_center_median(f_copy, f_copy_center);

      BM_ITER_ELEM_INDEX (l_other, &liter_other, v, BM_LOOPS_OF_VERT, j_other) {
        BM_face_interp_multires_ex(bm,
                                   l_other->f,
                                   f_copy,
                                   faces_center[j_other],
                                   f_copy_center,
                                   tcld->cd_loop_mdisp_offset);
      }
    }
  }
}

static void mesh_customdatacorrect_apply(TransDataContainer *tc, bool is_final)
{
  TransCustomDataMesh *tcmd = static_cast<TransCustomDataMesh *>(tc->custom.type.data);
  TransCustomDataLayer *tcld = tcmd ? tcmd->cd_layer_correct : nullptr;
  if (tcld == nullptr) {
    return;
  }
  const bool use_merge_group = tcld->use_merge_group;

  TransCustomDataMergeGroup *merge_data = tcld->merge_group.data;
  TransData *tob = tc->data;
  for (int i = tc->data_len; i--; tob++) {
    mesh_customdatacorrect_apply_vert(tcld, (TransDataBasic *)tob, merge_data, is_final);

    if (use_merge_group) {
      merge_data++;
    }
  }

  TransDataMirror *td_mirror = tc->data_mirror;
  for (int i = tc->data_mirror_len; i--; td_mirror++) {
    mesh_customdatacorrect_apply_vert(tcld, (TransDataBasic *)td_mirror, merge_data, is_final);

    if (use_merge_group) {
      merge_data++;
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name CustomData Layer Correction Restore
 * \{ */

static void mesh_customdatacorrect_restore(TransInfo *t)
{
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransCustomDataMesh *tcmd = static_cast<TransCustomDataMesh *>(tc->custom.type.data);
    TransCustomDataLayer *tcld = tcmd ? tcmd->cd_layer_correct : nullptr;
    if (!tcld) {
      continue;
    }

    BMesh *bm = tcld->bm;
    BMesh *bm_copy = tcld->bm_origfaces;

    GHashIterator gh_iter;
    GHASH_ITER (gh_iter, tcld->origfaces) {
      BMFace *f = static_cast<BMFace *>(BLI_ghashIterator_getKey(&gh_iter));
      BMFace *f_copy = static_cast<BMFace *>(BLI_ghashIterator_getValue(&gh_iter));
      BLI_assert(f->len == f_copy->len);

      BMLoop *l_iter, *l_first, *l_copy;
      l_iter = l_first = BM_FACE_FIRST_LOOP(f);
      l_copy = BM_FACE_FIRST_LOOP(f_copy);
      do {
        /* TODO: Restore only the elements that transform. */
        BM_elem_attrs_copy(bm_copy, bm, l_copy, l_iter);
        l_copy = l_copy->next;
      } while ((l_iter = l_iter->next) != l_first);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Island Creation
 * \{ */

void transform_convert_mesh_islands_calc(BMEditMesh *em,
                                         const bool calc_single_islands,
                                         const bool calc_island_center,
                                         const bool calc_island_axismtx,
                                         TransIslandData *r_island_data)
{
  TransIslandData data = {nullptr};

  BMesh *bm = em->bm;
  char htype;
  char itype;
  int i;

  /* group vars */
  int *groups_array = nullptr;
  int(*group_index)[2] = nullptr;

  bool has_only_single_islands = bm->totedgesel == 0 && bm->totfacesel == 0;
  if (has_only_single_islands && !calc_single_islands) {
    return;
  }

  data.island_vert_map = static_cast<int *>(
      MEM_mallocN(sizeof(*data.island_vert_map) * bm->totvert, __func__));
  /* we shouldn't need this, but with incorrect selection flushing
   * its possible we have a selected vertex that's not in a face,
   * for now best not crash in that case. */
  copy_vn_i(data.island_vert_map, bm->totvert, -1);

  if (!has_only_single_islands) {
    if (em->selectmode & (SCE_SELECT_VERTEX | SCE_SELECT_EDGE)) {
      groups_array = static_cast<int *>(
          MEM_mallocN(sizeof(*groups_array) * bm->totedgesel, __func__));
      data.island_tot = BM_mesh_calc_edge_groups(
          bm, groups_array, &group_index, nullptr, nullptr, BM_ELEM_SELECT);

      htype = BM_EDGE;
      itype = BM_VERTS_OF_EDGE;
    }
    else { /* (bm->selectmode & SCE_SELECT_FACE) */
      groups_array = static_cast<int *>(
          MEM_mallocN(sizeof(*groups_array) * bm->totfacesel, __func__));
      data.island_tot = BM_mesh_calc_face_groups(
          bm, groups_array, &group_index, nullptr, nullptr, nullptr, BM_ELEM_SELECT, BM_VERT);

      htype = BM_FACE;
      itype = BM_VERTS_OF_FACE;
    }

    BLI_assert(data.island_tot);
    if (calc_island_center) {
      data.center = static_cast<float(*)[3]>(
          MEM_mallocN(sizeof(*data.center) * data.island_tot, __func__));
    }

    if (calc_island_axismtx) {
      data.axismtx = static_cast<float(*)[3][3]>(
          MEM_mallocN(sizeof(*data.axismtx) * data.island_tot, __func__));
    }

    BM_mesh_elem_table_ensure(bm, htype);

    void **ele_array;
    ele_array = (htype == BM_FACE) ? (void **)bm->ftable : (void **)bm->etable;

    BM_mesh_elem_index_ensure(bm, BM_VERT);

    /* may be an edge OR a face array */
    for (i = 0; i < data.island_tot; i++) {
      BMEditSelection ese = {nullptr};

      const int fg_sta = group_index[i][0];
      const int fg_len = group_index[i][1];
      float co[3], no[3], tangent[3];
      int j;

      zero_v3(co);
      zero_v3(no);
      zero_v3(tangent);

      ese.htype = htype;

      /* loop on each face or edge in this group:
       * - assign r_vert_map
       * - calculate (co, no)
       */
      for (j = 0; j < fg_len; j++) {
        ese.ele = static_cast<BMElem *>(ele_array[groups_array[fg_sta + j]]);

        if (data.center) {
          float tmp_co[3];
          BM_editselection_center(&ese, tmp_co);
          add_v3_v3(co, tmp_co);
        }

        if (data.axismtx) {
          float tmp_no[3], tmp_tangent[3];
          BM_editselection_normal(&ese, tmp_no);
          BM_editselection_plane(&ese, tmp_tangent);
          add_v3_v3(no, tmp_no);
          add_v3_v3(tangent, tmp_tangent);
        }

        {
          /* setup vertex map */
          BMIter iter;
          BMVert *v;

          /* connected edge-verts */
          BM_ITER_ELEM (v, &iter, ese.ele, itype) {
            data.island_vert_map[BM_elem_index_get(v)] = i;
          }
        }
      }

      if (data.center) {
        mul_v3_v3fl(data.center[i], co, 1.0f / float(fg_len));
      }

      if (data.axismtx) {
        if (createSpaceNormalTangent(data.axismtx[i], no, tangent)) {
          /* pass */
        }
        else {
          if (normalize_v3(no) != 0.0f) {
            axis_dominant_v3_to_m3(data.axismtx[i], no);
            invert_m3(data.axismtx[i]);
          }
          else {
            unit_m3(data.axismtx[i]);
          }
        }
      }
    }

    MEM_freeN(groups_array);
    MEM_freeN(group_index);
  }

  /* for proportional editing we need islands of 1 so connected vertices can use it with
   * V3D_AROUND_LOCAL_ORIGINS */
  if (calc_single_islands) {
    BMIter viter;
    BMVert *v;
    int group_tot_single = 0;

    BM_ITER_MESH_INDEX (v, &viter, bm, BM_VERTS_OF_MESH, i) {
      if (BM_elem_flag_test(v, BM_ELEM_SELECT) && (data.island_vert_map[i] == -1)) {
        group_tot_single += 1;
      }
    }

    if (group_tot_single != 0) {
      if (calc_island_center) {
        data.center = static_cast<float(*)[3]>(MEM_reallocN(
            data.center, sizeof(*data.center) * (data.island_tot + group_tot_single)));
      }
      if (calc_island_axismtx) {
        data.axismtx = static_cast<float(*)[3][3]>(MEM_reallocN(
            data.axismtx, sizeof(*data.axismtx) * (data.island_tot + group_tot_single)));
      }

      BM_ITER_MESH_INDEX (v, &viter, bm, BM_VERTS_OF_MESH, i) {
        if (BM_elem_flag_test(v, BM_ELEM_SELECT) && (data.island_vert_map[i] == -1)) {
          data.island_vert_map[i] = data.island_tot;
          if (data.center) {
            copy_v3_v3(data.center[data.island_tot], v->co);
          }
          if (data.axismtx) {
            if (is_zero_v3(v->no) == false) {
              axis_dominant_v3_to_m3(data.axismtx[data.island_tot], v->no);
              invert_m3(data.axismtx[data.island_tot]);
            }
            else {
              unit_m3(data.axismtx[data.island_tot]);
            }
          }

          data.island_tot += 1;
        }
      }
    }
  }

  *r_island_data = data;
}

void transform_convert_mesh_islanddata_free(TransIslandData *island_data)
{
  if (island_data->center) {
    MEM_freeN(island_data->center);
  }
  if (island_data->axismtx) {
    MEM_freeN(island_data->axismtx);
  }
  if (island_data->island_vert_map) {
    MEM_freeN(island_data->island_vert_map);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Connectivity Distance for Proportional Editing
 * \{ */

/* Propagate distance from v1 and v2 to v0. */
static bool bmesh_test_dist_add(BMVert *v0,
                                BMVert *v1,
                                BMVert *v2,
                                float *dists, /* optionally track original index */
                                int *index,
                                const float mtx[3][3])
{
  if ((BM_elem_flag_test(v0, BM_ELEM_SELECT) == 0) && (BM_elem_flag_test(v0, BM_ELEM_HIDDEN) == 0))
  {
    const int i0 = BM_elem_index_get(v0);
    const int i1 = BM_elem_index_get(v1);

    BLI_assert(dists[i1] != FLT_MAX);
    if (dists[i0] <= dists[i1]) {
      return false;
    }

    float dist0;

    if (v2) {
      /* Distance across triangle. */
      const int i2 = BM_elem_index_get(v2);
      BLI_assert(dists[i2] != FLT_MAX);
      if (dists[i0] <= dists[i2]) {
        return false;
      }

      float vm0[3], vm1[3], vm2[3];
      mul_v3_m3v3(vm0, mtx, v0->co);
      mul_v3_m3v3(vm1, mtx, v1->co);
      mul_v3_m3v3(vm2, mtx, v2->co);

      dist0 = geodesic_distance_propagate_across_triangle(vm0, vm1, vm2, dists[i1], dists[i2]);
    }
    else {
      /* Distance along edge. */
      float vec[3];
      sub_v3_v3v3(vec, v1->co, v0->co);
      mul_m3_v3(mtx, vec);

      dist0 = dists[i1] + len_v3(vec);
    }

    if (dist0 < dists[i0]) {
      dists[i0] = dist0;
      if (index != nullptr) {
        index[i0] = index[i1];
      }
      return true;
    }
  }

  return false;
}

static bool bmesh_test_loose_edge(BMEdge *edge)
{
  /* Actual loose edge. */
  if (edge->l == nullptr) {
    return true;
  }

  /* Loose edge due to hidden adjacent faces. */
  BMIter iter;
  BMFace *face;
  BM_ITER_ELEM (face, &iter, edge, BM_FACES_OF_EDGE) {
    if (BM_elem_flag_test(face, BM_ELEM_HIDDEN) == 0) {
      return false;
    }
  }
  return true;
}

void transform_convert_mesh_connectivity_distance(BMesh *bm,
                                                  const float mtx[3][3],
                                                  float *dists,
                                                  int *index)
{
  BLI_LINKSTACK_DECLARE(queue, BMEdge *);

  /* any BM_ELEM_TAG'd edge is in 'queue_next', so we don't add in twice */
  const int tag_queued = BM_ELEM_TAG;
  const int tag_loose = BM_ELEM_TAG_ALT;

  BLI_LINKSTACK_DECLARE(queue_next, BMEdge *);

  BLI_LINKSTACK_INIT(queue);
  BLI_LINKSTACK_INIT(queue_next);

  {
    /* Set indexes and initial distances for selected vertices. */
    BMIter viter;
    BMVert *v;
    int i;

    BM_ITER_MESH_INDEX (v, &viter, bm, BM_VERTS_OF_MESH, i) {
      float dist;
      BM_elem_index_set(v, i); /* set_inline */

      if (BM_elem_flag_test(v, BM_ELEM_SELECT) == 0 || BM_elem_flag_test(v, BM_ELEM_HIDDEN)) {
        dist = FLT_MAX;
        if (index != nullptr) {
          index[i] = i;
        }
      }
      else {
        dist = 0.0f;
        if (index != nullptr) {
          index[i] = i;
        }
      }

      dists[i] = dist;
    }
    bm->elem_index_dirty &= ~BM_VERT;
  }

  {
    /* Add edges with at least one selected vertex to the queue. */
    BMIter eiter;
    BMEdge *e;

    BM_ITER_MESH (e, &eiter, bm, BM_EDGES_OF_MESH) {

      /* Always clear to satisfy the assert, also predictable to leave in cleared state. */
      BM_elem_flag_disable(e, tag_queued);

      if (BM_elem_flag_test(e, BM_ELEM_HIDDEN)) {
        continue;
      }

      BMVert *v1 = e->v1;
      BMVert *v2 = e->v2;
      int i1 = BM_elem_index_get(v1);
      int i2 = BM_elem_index_get(v2);

      if (dists[i1] != FLT_MAX || dists[i2] != FLT_MAX) {
        BLI_LINKSTACK_PUSH(queue, e);
      }
      BM_elem_flag_set(e, tag_loose, bmesh_test_loose_edge(e));
    }
  }

  do {
    BMEdge *e;

    while ((e = BLI_LINKSTACK_POP(queue))) {
      BMVert *v1 = e->v1;
      BMVert *v2 = e->v2;
      int i1 = BM_elem_index_get(v1);
      int i2 = BM_elem_index_get(v2);

      if (BM_elem_flag_test(e, tag_loose) || (dists[i1] == FLT_MAX || dists[i2] == FLT_MAX)) {
        /* Propagate along edge from vertex with smallest to largest distance. */
        if (dists[i1] > dists[i2]) {
          SWAP(int, i1, i2);
          SWAP(BMVert *, v1, v2);
        }

        if (bmesh_test_dist_add(v2, v1, nullptr, dists, index, mtx)) {
          /* Add adjacent loose edges to the queue, or all edges if this is a loose edge.
           * Other edges are handled by propagation across edges below. */
          BMEdge *e_other;
          BMIter eiter;
          BM_ITER_ELEM (e_other, &eiter, v2, BM_EDGES_OF_VERT) {
            if (e_other != e && BM_elem_flag_test(e_other, tag_queued) == 0 &&
                !BM_elem_flag_test(e_other, BM_ELEM_HIDDEN) &&
                (BM_elem_flag_test(e, tag_loose) || BM_elem_flag_test(e_other, tag_loose)))
            {
              BM_elem_flag_enable(e_other, tag_queued);
              BLI_LINKSTACK_PUSH(queue_next, e_other);
            }
          }
        }
      }

      if (!BM_elem_flag_test(e, tag_loose)) {
        /* Propagate across edge to vertices in adjacent faces. */
        BMLoop *l;
        BMIter liter;
        BM_ITER_ELEM (l, &liter, e, BM_LOOPS_OF_EDGE) {
          if (BM_elem_flag_test(l->f, BM_ELEM_HIDDEN)) {
            continue;
          }
          /* Don't check hidden edges or vertices in this loop
           * since any hidden edge causes the face to be hidden too. */
          for (BMLoop *l_other = l->next->next; l_other != l; l_other = l_other->next) {
            BMVert *v_other = l_other->v;
            BLI_assert(!ELEM(v_other, v1, v2));

            if (bmesh_test_dist_add(v_other, v1, v2, dists, index, mtx)) {
              /* Add adjacent edges to the queue, if they are ready to propagate across/along.
               * Always propagate along loose edges, and for other edges only propagate across
               * if both vertices have a known distances. */
              BMEdge *e_other;
              BMIter eiter;
              BM_ITER_ELEM (e_other, &eiter, v_other, BM_EDGES_OF_VERT) {
                if (e_other != e && BM_elem_flag_test(e_other, tag_queued) == 0 &&
                    !BM_elem_flag_test(e_other, BM_ELEM_HIDDEN) &&
                    (BM_elem_flag_test(e_other, tag_loose) ||
                     dists[BM_elem_index_get(BM_edge_other_vert(e_other, v_other))] != FLT_MAX))
                {
                  BM_elem_flag_enable(e_other, tag_queued);
                  BLI_LINKSTACK_PUSH(queue_next, e_other);
                }
              }
            }
          }
        }
      }
    }

    /* Clear for the next loop. */
    for (LinkNode *lnk = queue_next; lnk; lnk = lnk->next) {
      BMEdge *e_link = static_cast<BMEdge *>(lnk->link);

      BM_elem_flag_disable(e_link, tag_queued);
    }

    BLI_LINKSTACK_SWAP(queue, queue_next);

    /* None should be tagged now since 'queue_next' is empty. */
    BLI_assert(BM_iter_mesh_count_flag(BM_EDGES_OF_MESH, bm, tag_queued, true) == 0);
  } while (BLI_LINKSTACK_SIZE(queue));

  BLI_LINKSTACK_FREE(queue);
  BLI_LINKSTACK_FREE(queue_next);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name TransDataMirror Creation
 * \{ */

/* Used for both mirror epsilon and TD_MIRROR_EDGE_ */
#define TRANSFORM_MAXDIST_MIRROR 0.00002f

static bool is_in_quadrant_v3(const float co[3], const int quadrant[3], const float epsilon)
{
  if (quadrant[0] && ((co[0] * quadrant[0]) < -epsilon)) {
    return false;
  }
  if (quadrant[1] && ((co[1] * quadrant[1]) < -epsilon)) {
    return false;
  }
  if (quadrant[2] && ((co[2] * quadrant[2]) < -epsilon)) {
    return false;
  }
  return true;
}

void transform_convert_mesh_mirrordata_calc(BMEditMesh *em,
                                            const bool use_select,
                                            const bool use_topology,
                                            const bool mirror_axis[3],
                                            TransMirrorData *r_mirror_data)
{
  MirrorDataVert *vert_map;

  BMesh *bm = em->bm;
  BMVert *eve;
  BMIter iter;
  int i, flag, totvert = bm->totvert;

  vert_map = static_cast<MirrorDataVert *>(MEM_callocN(totvert * sizeof(*vert_map), __func__));

  float select_sum[3] = {0};
  BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, i) {
    vert_map[i] = MirrorDataVert{-1, 0};
    if (BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
      continue;
    }
    if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
      add_v3_v3(select_sum, eve->co);
    }
  }

  /* Tag only elements that will be transformed within the quadrant. */
  int quadrant[3];
  for (int a = 0; a < 3; a++) {
    if (mirror_axis[a]) {
      quadrant[a] = select_sum[a] >= 0.0f ? 1 : -1;
    }
    else {
      quadrant[a] = 0;
    }
  }

  uint mirror_elem_len = 0;
  int *index[3] = {nullptr, nullptr, nullptr};
  bool is_single_mirror_axis = (mirror_axis[0] + mirror_axis[1] + mirror_axis[2]) == 1;
  bool test_selected_only = use_select && is_single_mirror_axis;
  for (int a = 0; a < 3; a++) {
    if (!mirror_axis[a]) {
      continue;
    }

    index[a] = static_cast<int *>(MEM_mallocN(totvert * sizeof(*index[a]), __func__));
    EDBM_verts_mirror_cache_begin_ex(
        em, a, false, test_selected_only, true, use_topology, TRANSFORM_MAXDIST_MIRROR, index[a]);

    flag = TD_MIRROR_X << a;
    BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, i) {
      int i_mirr = index[a][i];
      if (i_mirr < 0) {
        continue;
      }
      if (BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
        continue;
      }
      if (use_select && !BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
        continue;
      }
      if (!is_in_quadrant_v3(eve->co, quadrant, TRANSFORM_MAXDIST_MIRROR)) {
        continue;
      }
      if (vert_map[i_mirr].flag != 0) {
        /* One mirror per element.
         * It can happen when vertices occupy the same position. */
        continue;
      }
      if (vert_map[i].flag & flag) {
        /* It's already a mirror.
         * Avoid a mirror vertex dependency cycle.
         * This can happen when the vertices are within the mirror threshold. */
        continue;
      }

      vert_map[i_mirr] = MirrorDataVert{i, flag};
      mirror_elem_len++;
    }
  }

  if (!mirror_elem_len) {
    MEM_freeN(vert_map);
    vert_map = nullptr;
  }
  else if (!is_single_mirror_axis) {
    /* Adjustment for elements that are mirrors of mirrored elements. */
    for (int a = 0; a < 3; a++) {
      if (!mirror_axis[a]) {
        continue;
      }

      flag = TD_MIRROR_X << a;
      for (i = 0; i < totvert; i++) {
        int i_mirr = index[a][i];
        if (i_mirr < 0) {
          continue;
        }
        if (vert_map[i].index != -1 && !(vert_map[i].flag & flag)) {
          if (vert_map[i_mirr].index == -1) {
            mirror_elem_len++;
          }
          vert_map[i_mirr].index = vert_map[i].index;
          vert_map[i_mirr].flag |= vert_map[i].flag | flag;
        }
      }
    }
  }

  MEM_SAFE_FREE(index[0]);
  MEM_SAFE_FREE(index[1]);
  MEM_SAFE_FREE(index[2]);

  r_mirror_data->vert_map = vert_map;
  r_mirror_data->mirror_elem_len = mirror_elem_len;
}

void transform_convert_mesh_mirrordata_free(TransMirrorData *mirror_data)
{
  if (mirror_data->vert_map) {
    MEM_freeN(mirror_data->vert_map);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Crazy Space
 * \{ */

void transform_convert_mesh_crazyspace_detect(TransInfo *t,
                                              TransDataContainer *tc,
                                              BMEditMesh *em,
                                              TransMeshDataCrazySpace *r_crazyspace_data)
{
  float(*quats)[4] = nullptr;
  float(*defmats)[3][3] = nullptr;
  const int prop_mode = (t->flag & T_PROP_EDIT) ? (t->flag & T_PROP_EDIT_ALL) : 0;
  if (BKE_modifiers_get_cage_index(t->scene, tc->obedit, nullptr, true) != -1) {
    float(*defcos)[3] = nullptr;
    int totleft = -1;
    if (BKE_modifiers_is_correctable_deformed(t->scene, tc->obedit)) {
      BKE_scene_graph_evaluated_ensure(t->depsgraph, CTX_data_main(t->context));

      /* Use evaluated state because we need b-bone cache. */
      Scene *scene_eval = (Scene *)DEG_get_evaluated_id(t->depsgraph, &t->scene->id);
      Object *obedit_eval = (Object *)DEG_get_evaluated_id(t->depsgraph, &tc->obedit->id);
      BMEditMesh *em_eval = BKE_editmesh_from_object(obedit_eval);
      /* check if we can use deform matrices for modifier from the
       * start up to stack, they are more accurate than quats */
      totleft = BKE_crazyspace_get_first_deform_matrices_editbmesh(
          t->depsgraph, scene_eval, obedit_eval, em_eval, &defmats, &defcos);
    }

    /* If we still have more modifiers, also do crazy-space
     * correction with \a quats, relative to the coordinates after
     * the modifiers that support deform matrices \a defcos. */

#if 0 /* TODO(@ideasman42): fix crazy-space & extrude so it can be enabled for general use. \
       */
    if ((totleft > 0) || (totleft == -1))
#else
    if (totleft > 0)
#endif
    {
      float(*mappedcos)[3] = nullptr;
      mappedcos = BKE_crazyspace_get_mapped_editverts(t->depsgraph, tc->obedit);
      quats = static_cast<float(*)[4]>(
          MEM_mallocN(em->bm->totvert * sizeof(*quats), "crazy quats"));
      BKE_crazyspace_set_quats_editmesh(em, defcos, mappedcos, quats, !prop_mode);
      if (mappedcos) {
        MEM_freeN(mappedcos);
      }
    }

    if (defcos) {
      MEM_freeN(defcos);
    }
  }
  r_crazyspace_data->quats = quats;
  r_crazyspace_data->defmats = defmats;
}

void transform_convert_mesh_crazyspace_transdata_set(const float mtx[3][3],
                                                     const float smtx[3][3],
                                                     const float defmat[3][3],
                                                     const float quat[4],
                                                     TransData *r_td)
{
  /* CrazySpace */
  if (quat || defmat) {
    float mat[3][3], qmat[3][3], imat[3][3];

    /* Use both or either quat and defmat correction. */
    if (quat) {
      quat_to_mat3(qmat, quat);

      if (defmat) {
        mul_m3_series(mat, defmat, qmat, mtx);
      }
      else {
        mul_m3_m3m3(mat, mtx, qmat);
      }
    }
    else {
      mul_m3_m3m3(mat, mtx, defmat);
    }

    invert_m3_m3(imat, mat);

    copy_m3_m3(r_td->smtx, imat);
    copy_m3_m3(r_td->mtx, mat);
  }
  else {
    copy_m3_m3(r_td->smtx, smtx);
    copy_m3_m3(r_td->mtx, mtx);
  }
}

void transform_convert_mesh_crazyspace_free(TransMeshDataCrazySpace *r_crazyspace_data)
{
  if (r_crazyspace_data->quats) {
    MEM_freeN(r_crazyspace_data->quats);
  }
  if (r_crazyspace_data->defmats) {
    MEM_freeN(r_crazyspace_data->defmats);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edit Mesh Verts Transform Creation
 * \{ */

static void mesh_transdata_center_copy(const TransIslandData *island_data,
                                       const int island_index,
                                       const float iloc[3],
                                       float r_center[3])
{
  if (island_data->center && island_index != -1) {
    copy_v3_v3(r_center, island_data->center[island_index]);
  }
  else {
    copy_v3_v3(r_center, iloc);
  }
}

/* way to overwrite what data is edited with transform */
static void VertsToTransData(TransInfo *t,
                             TransData *td,
                             TransDataExtension *tx,
                             BMEditMesh *em,
                             BMVert *eve,
                             const TransIslandData *island_data,
                             const int island_index)
{
  float *no, _no[3];
  BLI_assert(BM_elem_flag_test(eve, BM_ELEM_HIDDEN) == 0);

  td->flag = 0;
  // if (key)
  //  td->loc = key->co;
  // else
  td->loc = eve->co;
  copy_v3_v3(td->iloc, td->loc);

  if ((t->mode == TFM_SHRINKFATTEN) && (em->selectmode & SCE_SELECT_FACE) &&
      BM_elem_flag_test(eve, BM_ELEM_SELECT) && BM_vert_calc_normal_ex(eve, BM_ELEM_SELECT, _no))
  {
    no = _no;
  }
  else {
    no = eve->no;
  }

  mesh_transdata_center_copy(island_data, island_index, td->iloc, td->center);

  if ((island_index != -1) && island_data->axismtx) {
    copy_m3_m3(td->axismtx, island_data->axismtx[island_index]);
  }
  else if (t->around == V3D_AROUND_LOCAL_ORIGINS) {
    createSpaceNormal(td->axismtx, no);
  }
  else {
    /* Setting normals */
    copy_v3_v3(td->axismtx[2], no);
    td->axismtx[0][0] = td->axismtx[0][1] = td->axismtx[0][2] = td->axismtx[1][0] =
        td->axismtx[1][1] = td->axismtx[1][2] = 0.0f;
  }

  td->ext = nullptr;
  td->val = nullptr;
  td->extra = eve;
  if (t->mode == TFM_SHRINKFATTEN) {
    td->ext = tx;
    tx->isize[0] = BM_vert_calc_shell_factor_ex(eve, no, BM_ELEM_SELECT);
  }
}

static void createTransEditVerts(bContext * /*C*/, TransInfo *t)
{
  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransDataExtension *tx = nullptr;
    BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);
    Mesh *me = static_cast<Mesh *>(tc->obedit->data);
    BMesh *bm = em->bm;
    BMVert *eve;
    BMIter iter;
    float mtx[3][3], smtx[3][3];
    int a;
    const int prop_mode = (t->flag & T_PROP_EDIT) ? (t->flag & T_PROP_EDIT_ALL) : 0;

    TransIslandData island_data = {nullptr};
    TransMirrorData mirror_data = {nullptr};
    TransMeshDataCrazySpace crazyspace_data = {nullptr};

    /**
     * Quick check if we can transform.
     *
     * \note ignore modes here, even in edge/face modes,
     * transform data is created by selected vertices.
     */

    /* Support other objects using proportional editing to adjust these, unless connected is
     * enabled. */
    if ((!prop_mode || (prop_mode & T_PROP_CONNECTED)) && (bm->totvertsel == 0)) {
      continue;
    }

    int data_len = 0;
    if (prop_mode) {
      BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
        if (!BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
          data_len++;
        }
      }
    }
    else {
      data_len = bm->totvertsel;
    }

    if (data_len == 0) {
      continue;
    }

    /* Snap rotation along normal needs a common axis for whole islands,
     * otherwise one get random crazy results, see #59104.
     * However, we do not want to use the island center for the pivot/translation reference. */
    const bool is_snap_rotate = ((t->mode == TFM_TRANSLATION) &&
                                 /* There is not guarantee that snapping
                                  * is initialized yet at this point... */
                                 (usingSnappingNormal(t) ||
                                  (t->settings->snap_flag & SCE_SNAP_ROTATE) != 0) &&
                                 (t->around != V3D_AROUND_LOCAL_ORIGINS));

    /* Even for translation this is needed because of island-orientation, see: #51651. */
    const bool is_island_center = (t->around == V3D_AROUND_LOCAL_ORIGINS) || is_snap_rotate;
    if (is_island_center) {
      /* In this specific case, near-by vertices will need to know
       * the island of the nearest connected vertex. */
      const bool calc_single_islands = ((prop_mode & T_PROP_CONNECTED) &&
                                        (t->around == V3D_AROUND_LOCAL_ORIGINS) &&
                                        (em->selectmode & SCE_SELECT_VERTEX));

      const bool calc_island_center = !is_snap_rotate;
      /* The island axismtx is only necessary in some modes.
       * TODO(Germano): Extend the list to exclude other modes. */
      const bool calc_island_axismtx = !ELEM(t->mode, TFM_SHRINKFATTEN);

      transform_convert_mesh_islands_calc(
          em, calc_single_islands, calc_island_center, calc_island_axismtx, &island_data);
    }

    copy_m3_m4(mtx, tc->obedit->object_to_world);
    /* we use a pseudo-inverse so that when one of the axes is scaled to 0,
     * matrix inversion still works and we can still moving along the other */
    pseudoinverse_m3_m3(smtx, mtx, PSEUDOINVERSE_EPSILON);

    /* Original index of our connected vertex when connected distances are calculated.
     * Optional, allocate if needed. */
    int *dists_index = nullptr;
    float *dists = nullptr;
    if (prop_mode & T_PROP_CONNECTED) {
      dists = static_cast<float *>(MEM_mallocN(bm->totvert * sizeof(float), __func__));
      if (is_island_center) {
        dists_index = static_cast<int *>(MEM_mallocN(bm->totvert * sizeof(int), __func__));
      }
      transform_convert_mesh_connectivity_distance(em->bm, mtx, dists, dists_index);
    }

    /* Create TransDataMirror. */
    if (tc->use_mirror_axis_any) {
      bool use_topology = (me->editflag & ME_EDIT_MIRROR_TOPO) != 0;
      bool use_select = (t->flag & T_PROP_EDIT) == 0;
      const bool mirror_axis[3] = {
          bool(tc->use_mirror_axis_x), bool(tc->use_mirror_axis_y), bool(tc->use_mirror_axis_z)};
      transform_convert_mesh_mirrordata_calc(
          em, use_select, use_topology, mirror_axis, &mirror_data);

      if (mirror_data.vert_map) {
        tc->data_mirror_len = mirror_data.mirror_elem_len;
        tc->data_mirror = static_cast<TransDataMirror *>(
            MEM_callocN(mirror_data.mirror_elem_len * sizeof(*tc->data_mirror), __func__));

        BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, a) {
          if (prop_mode || BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
            if (mirror_data.vert_map[a].index != -1) {
              data_len--;
            }
          }
        }
      }
    }

    /* Detect CrazySpace [tm]. */
    transform_convert_mesh_crazyspace_detect(t, tc, em, &crazyspace_data);

    /* Create TransData. */
    BLI_assert(data_len >= 1);
    tc->data_len = data_len;
    tc->data = static_cast<TransData *>(
        MEM_callocN(data_len * sizeof(TransData), "TransObData(Mesh EditMode)"));
    if (t->mode == TFM_SHRINKFATTEN) {
      /* warning, this is overkill, we only need 2 extra floats,
       * but this stores loads of extra stuff, for TFM_SHRINKFATTEN its even more overkill
       * since we may not use the 'alt' transform mode to maintain shell thickness,
       * but with generic transform code its hard to lazy init vars */
      tx = tc->data_ext = static_cast<TransDataExtension *>(
          MEM_callocN(tc->data_len * sizeof(TransDataExtension), "TransObData ext"));
    }

    TransData *tob = tc->data;
    TransDataMirror *td_mirror = tc->data_mirror;
    BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, a) {
      if (BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
        continue;
      }

      int island_index = -1;
      if (island_data.island_vert_map) {
        const int connected_index = (dists_index && dists_index[a] != -1) ? dists_index[a] : a;
        island_index = island_data.island_vert_map[connected_index];
      }

      if (mirror_data.vert_map && mirror_data.vert_map[a].index != -1) {
        int elem_index = mirror_data.vert_map[a].index;
        BMVert *v_src = BM_vert_at_index(bm, elem_index);

        if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
          mirror_data.vert_map[a].flag |= TD_SELECTED;
        }

        td_mirror->extra = eve;
        td_mirror->loc = eve->co;
        copy_v3_v3(td_mirror->iloc, eve->co);
        td_mirror->flag = mirror_data.vert_map[a].flag;
        td_mirror->loc_src = v_src->co;
        mesh_transdata_center_copy(&island_data, island_index, td_mirror->iloc, td_mirror->center);

        td_mirror++;
      }
      else if (prop_mode || BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
        /* Do not use the island center in case we are using islands
         * only to get axis for snap/rotate to normal... */
        VertsToTransData(t, tob, tx, em, eve, &island_data, island_index);
        if (tx) {
          tx++;
        }

        /* selected */
        if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
          tob->flag |= TD_SELECTED;
        }

        if (prop_mode) {
          if (prop_mode & T_PROP_CONNECTED) {
            tob->dist = dists[a];
          }
          else {
            tob->dist = FLT_MAX;
          }
        }

        /* CrazySpace */
        transform_convert_mesh_crazyspace_transdata_set(
            mtx,
            smtx,
            crazyspace_data.defmats ? crazyspace_data.defmats[a] : nullptr,
            crazyspace_data.quats && BM_elem_flag_test(eve, BM_ELEM_TAG) ?
                crazyspace_data.quats[a] :
                nullptr,
            tob);

        if (tc->use_mirror_axis_any) {
          if (tc->use_mirror_axis_x && fabsf(tob->loc[0]) < TRANSFORM_MAXDIST_MIRROR) {
            tob->flag |= TD_MIRROR_EDGE_X;
          }
          if (tc->use_mirror_axis_y && fabsf(tob->loc[1]) < TRANSFORM_MAXDIST_MIRROR) {
            tob->flag |= TD_MIRROR_EDGE_Y;
          }
          if (tc->use_mirror_axis_z && fabsf(tob->loc[2]) < TRANSFORM_MAXDIST_MIRROR) {
            tob->flag |= TD_MIRROR_EDGE_Z;
          }
        }

        tob++;
      }
    }

    transform_convert_mesh_islanddata_free(&island_data);
    transform_convert_mesh_mirrordata_free(&mirror_data);
    transform_convert_mesh_crazyspace_free(&crazyspace_data);
    if (dists) {
      MEM_freeN(dists);
    }
    if (dists_index) {
      MEM_freeN(dists_index);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Recalc Mesh Data (Partial Update)
 * \{ */

static BMPartialUpdate *mesh_partial_ensure(TransInfo *t,
                                            TransDataContainer *tc,
                                            enum ePartialType partial_type)
{
  TransCustomDataMesh *tcmd = mesh_customdata_ensure(tc);

  TransCustomData_PartialUpdate *pupdate = &tcmd->partial_update[partial_type];

  if (pupdate->cache) {

    /* Recalculate partial update data when the proportional editing size changes.
     *
     * Note that decreasing the proportional editing size requires the existing
     * partial data is used before recreating this partial data at the smaller size.
     * Since excluding geometry from being transformed requires an update.
     *
     * Extra logic is needed to account for this situation. */

    bool recalc;
    if (pupdate->prop_size_prev < t->prop_size) {
      /* Size increase, simply recalculate. */
      recalc = true;
    }
    else if (pupdate->prop_size_prev > t->prop_size) {
      /* Size decreased, first use this partial data since reducing the size will transform
       * geometry which needs recalculating. */
      pupdate->prop_size_prev = t->prop_size;
      recalc = false;
    }
    else if (pupdate->prop_size != t->prop_size) {
      BLI_assert(pupdate->prop_size > pupdate->prop_size_prev);
      recalc = true;
    }
    else {
      BLI_assert(t->prop_size == pupdate->prop_size_prev);
      recalc = false;
    }

    if (!recalc) {
      return pupdate->cache;
    }

    BM_mesh_partial_destroy(pupdate->cache);
    pupdate->cache = nullptr;
  }

  BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);

  BM_mesh_elem_index_ensure(em->bm, BM_VERT);

  /* Only use `verts_group` or `verts_mask`. */
  int *verts_group = nullptr;
  int verts_group_count = 0; /* Number of non-zero elements in `verts_group`. */

  BLI_bitmap *verts_mask = nullptr;
  int verts_mask_count = 0; /* Number of elements enabled in `verts_mask`. */

  if ((partial_type == PARTIAL_TYPE_GROUP) && ((t->flag & T_PROP_EDIT) || tc->use_mirror_axis_any))
  {
    verts_group = static_cast<int *>(
        MEM_callocN(sizeof(*verts_group) * em->bm->totvert, __func__));
    int i;
    TransData *td;
    for (i = 0, td = tc->data; i < tc->data_len; i++, td++) {
      if (td->factor == 0.0f) {
        continue;
      }
      const BMVert *v = (BMVert *)td->extra;
      const int v_index = BM_elem_index_get(v);
      BLI_assert(verts_group[v_index] == 0);
      if (td->factor < 1.0f) {
        /* Don't use grouping logic with the factor is under 1.0. */
        verts_group[v_index] = -1;
      }
      else {
        BLI_assert(td->factor == 1.0f);
        verts_group[v_index] = 1;
        if (tc->use_mirror_axis_any) {
          /* Use bits 2-4 for central alignment (don't overlap the first bit). */
          const int flag = td->flag & (TD_MIRROR_EDGE_X | TD_MIRROR_EDGE_Y | TD_MIRROR_EDGE_Z);
          verts_group[v_index] |= (flag >> TD_MIRROR_EDGE_AXIS_SHIFT) << 1;
        }
      }
      verts_mask_count += 1;
    }

    TransDataMirror *td_mirror = tc->data_mirror;
    for (i = 0; i < tc->data_mirror_len; i++, td_mirror++) {
      BMVert *v_mirr = (BMVert *)POINTER_OFFSET(td_mirror->loc_src, -offsetof(BMVert, co));
      /* The equality check is to account for the case when topology mirror moves
       * the vertex from it's original location to match it's symmetrical position,
       * with proportional editing enabled. */
      const int v_mirr_index = BM_elem_index_get(v_mirr);
      if (verts_group[v_mirr_index] == 0 && equals_v3v3(td_mirror->loc, td_mirror->iloc)) {
        continue;
      }

      BMVert *v_mirr_other = (BMVert *)td_mirror->extra;
      /* This assert should never fail since there is no overlap
       * between mirrored vertices and non-mirrored. */
      BLI_assert(verts_group[BM_elem_index_get(v_mirr_other)] == 0);
      const int v_mirr_other_index = BM_elem_index_get(v_mirr_other);

      if (verts_group[v_mirr_index] == -1) {
        verts_group[v_mirr_other_index] = -1;
      }
      else {
        /* Use bits 5-8 for mirror (don't overlap previous bits). */
        const int flag = td_mirror->flag & (TD_MIRROR_X | TD_MIRROR_Y | TD_MIRROR_Z);
        verts_group[v_mirr_other_index] |= (flag >> TD_MIRROR_EDGE_AXIS_SHIFT) << 4;
      }
      verts_mask_count += 1;
    }
  }
  else {
    /* See the body of the comments in the previous block for details. */
    verts_mask = BLI_BITMAP_NEW(em->bm->totvert, __func__);
    int i;
    TransData *td;
    for (i = 0, td = tc->data; i < tc->data_len; i++, td++) {
      if (td->factor == 0.0f) {
        continue;
      }
      const BMVert *v = (BMVert *)td->extra;
      const int v_index = BM_elem_index_get(v);
      BLI_assert(!BLI_BITMAP_TEST(verts_mask, v_index));
      BLI_BITMAP_ENABLE(verts_mask, v_index);
      verts_mask_count += 1;
    }

    TransDataMirror *td_mirror = tc->data_mirror;
    for (i = 0; i < tc->data_mirror_len; i++, td_mirror++) {
      BMVert *v_mirr = (BMVert *)POINTER_OFFSET(td_mirror->loc_src, -offsetof(BMVert, co));
      if (!BLI_BITMAP_TEST(verts_mask, BM_elem_index_get(v_mirr)) &&
          equals_v3v3(td_mirror->loc, td_mirror->iloc))
      {
        continue;
      }

      BMVert *v_mirr_other = (BMVert *)td_mirror->extra;
      BLI_assert(!BLI_BITMAP_TEST(verts_mask, BM_elem_index_get(v_mirr_other)));
      const int v_mirr_other_index = BM_elem_index_get(v_mirr_other);
      BLI_BITMAP_ENABLE(verts_mask, v_mirr_other_index);
      verts_mask_count += 1;
    }
  }

  switch (partial_type) {
    case PARTIAL_TYPE_ALL: {
      BMPartialUpdate_Params params{};
      params.do_tessellate = true;
      params.do_normals = true;
      pupdate->cache = BM_mesh_partial_create_from_verts(
          em->bm, &params, verts_mask, verts_mask_count);
      break;
    }
    case PARTIAL_TYPE_GROUP: {
      BMPartialUpdate_Params params{};
      params.do_tessellate = true;
      params.do_normals = true;
      pupdate->cache = (verts_group ? BM_mesh_partial_create_from_verts_group_multi(
                                          em->bm, &params, verts_group, verts_group_count) :
                                      BM_mesh_partial_create_from_verts_group_single(
                                          em->bm, &params, verts_mask, verts_mask_count));
      break;
    }
    case PARTIAL_NONE: {
      BLI_assert_unreachable();
    }
  }

  if (verts_group) {
    MEM_freeN(verts_group);
  }
  else {
    MEM_freeN(verts_mask);
  }

  pupdate->prop_size_prev = t->prop_size;
  pupdate->prop_size = t->prop_size;

  return pupdate->cache;
}

static void mesh_partial_types_calc(TransInfo *t, PartialTypeState *r_partial_state)
{
  /* Calculate the kind of partial updates which can be performed. */
  enum ePartialType partial_for_normals = PARTIAL_NONE;
  enum ePartialType partial_for_looptri = PARTIAL_NONE;

  /* Note that operations such as #TFM_CREASE are not handled here
   * (if they were, leaving as #PARTIAL_NONE would be appropriate). */
  switch (t->mode) {
    case TFM_TRANSLATION: {
      partial_for_looptri = PARTIAL_TYPE_GROUP;
      partial_for_normals = PARTIAL_TYPE_GROUP;
      /* Translation can rotate when snapping to normal. */
      if (transform_snap_is_active(t) && usingSnappingNormal(t) && validSnappingNormal(t)) {
        partial_for_normals = PARTIAL_TYPE_ALL;
      }
      break;
    }
    case TFM_ROTATION: {
      partial_for_looptri = PARTIAL_TYPE_GROUP;
      partial_for_normals = PARTIAL_TYPE_ALL;
      break;
    }
    case TFM_RESIZE: {
      partial_for_looptri = PARTIAL_TYPE_GROUP;
      partial_for_normals = PARTIAL_TYPE_GROUP;
      /* Non-uniform scale needs to recalculate all normals
       * since their relative locations change.
       * Uniform negative scale can keep normals as-is since the faces are flipped,
       * normals remain unchanged. */
      if ((t->con.mode & CON_APPLY) ||
          (t->values_final[0] != t->values_final[1] || t->values_final[0] != t->values_final[2]))
      {
        partial_for_normals = PARTIAL_TYPE_ALL;
      }
      break;
    }
    default: {
      partial_for_looptri = PARTIAL_TYPE_ALL;
      partial_for_normals = PARTIAL_TYPE_ALL;
      break;
    }
  }

  /* With projection, transform isn't affine. */
  if (transform_snap_project_individual_is_active(t)) {
    if (partial_for_looptri == PARTIAL_TYPE_GROUP) {
      partial_for_looptri = PARTIAL_TYPE_ALL;
    }
    if (partial_for_normals == PARTIAL_TYPE_GROUP) {
      partial_for_normals = PARTIAL_TYPE_ALL;
    }
  }

  r_partial_state->for_looptri = partial_for_looptri;
  r_partial_state->for_normals = partial_for_normals;
}

static void mesh_partial_update(TransInfo *t,
                                TransDataContainer *tc,
                                const PartialTypeState *partial_state)
{
  BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);

  TransCustomDataMesh *tcmd = mesh_customdata_ensure(tc);

  const PartialTypeState *partial_state_prev = &tcmd->partial_update_state_prev;

  /* Promote the partial update types based on the previous state
   * so the values that no longer modified are reset before being left as-is.
   * Needed for translation which can toggle snap-to-normal during transform. */
  const enum ePartialType partial_for_looptri = MAX2(partial_state->for_looptri,
                                                     partial_state_prev->for_looptri);
  const enum ePartialType partial_for_normals = MAX2(partial_state->for_normals,
                                                     partial_state_prev->for_normals);

  if ((partial_for_looptri == PARTIAL_TYPE_ALL) && (partial_for_normals == PARTIAL_TYPE_ALL) &&
      (em->bm->totvert == em->bm->totvertsel))
  {
    /* The additional cost of generating the partial connectivity data isn't justified
     * when all data needs to be updated.
     *
     * While proportional editing can cause all geometry to need updating with a partial
     * selection. It's impractical to calculate this ahead of time. Further, the down side of
     * using partial updates when their not needed is negligible. */
    BKE_editmesh_looptri_and_normals_calc(em);
  }
  else {
    if (partial_for_looptri != PARTIAL_NONE) {
      BMPartialUpdate *bmpinfo = mesh_partial_ensure(t, tc, partial_for_looptri);
      BMeshCalcTessellation_Params params{};
      params.face_normals = true;
      BKE_editmesh_looptri_calc_with_partial_ex(em, bmpinfo, &params);
    }

    if (partial_for_normals != PARTIAL_NONE) {
      BMPartialUpdate *bmpinfo = mesh_partial_ensure(t, tc, partial_for_normals);
      /* While not a large difference, take advantage of existing normals where possible. */
      const bool face_normals = !((partial_for_looptri == PARTIAL_TYPE_ALL) ||
                                  ((partial_for_looptri == PARTIAL_TYPE_GROUP) &&
                                   (partial_for_normals == PARTIAL_TYPE_GROUP)));
      BMeshNormalsUpdate_Params params{};
      params.face_normals = face_normals;
      BM_mesh_normals_update_with_partial_ex(em->bm, bmpinfo, &params);
    }
  }

  /* Store the previous requested (not the previous used),
   * since the values used may have been promoted based on the previous types. */
  tcmd->partial_update_state_prev = *partial_state;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Recalc Mesh Data
 * \{ */

static void mesh_transdata_mirror_apply(TransDataContainer *tc)
{
  if (tc->use_mirror_axis_any) {
    int i;
    TransData *td;
    for (i = 0, td = tc->data; i < tc->data_len; i++, td++) {
      if (td->flag & (TD_MIRROR_EDGE_X | TD_MIRROR_EDGE_Y | TD_MIRROR_EDGE_Z)) {
        if (td->flag & TD_MIRROR_EDGE_X) {
          td->loc[0] = 0.0f;
        }
        if (td->flag & TD_MIRROR_EDGE_Y) {
          td->loc[1] = 0.0f;
        }
        if (td->flag & TD_MIRROR_EDGE_Z) {
          td->loc[2] = 0.0f;
        }
      }
    }

    TransDataMirror *td_mirror = tc->data_mirror;
    for (i = 0; i < tc->data_mirror_len; i++, td_mirror++) {
      copy_v3_v3(td_mirror->loc, td_mirror->loc_src);
      if (td_mirror->flag & TD_MIRROR_X) {
        td_mirror->loc[0] *= -1;
      }
      if (td_mirror->flag & TD_MIRROR_Y) {
        td_mirror->loc[1] *= -1;
      }
      if (td_mirror->flag & TD_MIRROR_Z) {
        td_mirror->loc[2] *= -1;
      }
    }
  }
}

static void recalcData_mesh(TransInfo *t)
{
  bool is_canceling = t->state == TRANS_CANCEL;
  /* Apply corrections. */
  if (!is_canceling) {
    transform_snap_project_individual_apply(t);

    bool do_mirror = !(t->flag & T_NO_MIRROR);
    FOREACH_TRANS_DATA_CONTAINER (t, tc) {
      /* Apply clipping after so we never project past the clip plane #25423. */
      transform_convert_clip_mirror_modifier_apply(tc);

      if (do_mirror) {
        mesh_transdata_mirror_apply(tc);
      }

      mesh_customdatacorrect_apply(tc, false);
    }
  }
  else {
    mesh_customdatacorrect_restore(t);
  }

  PartialTypeState partial_state;
  mesh_partial_types_calc(t, &partial_state);

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    DEG_id_tag_update(static_cast<ID *>(tc->obedit->data), ID_RECALC_GEOMETRY);

    mesh_partial_update(t, tc, &partial_state);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Special After Transform Mesh
 * \{ */

static void special_aftertrans_update__mesh(bContext * /*C*/, TransInfo *t)
{
  const bool is_canceling = (t->state == TRANS_CANCEL);
  const bool use_automerge = !is_canceling && (t->flag & (T_AUTOMERGE | T_AUTOSPLIT)) != 0;

  if (!is_canceling && ELEM(t->mode, TFM_EDGE_SLIDE, TFM_VERT_SLIDE)) {
    /* NOTE(joeedh): Handle multi-res re-projection,
     * done on transform completion since it's really slow. */
    FOREACH_TRANS_DATA_CONTAINER (t, tc) {
      mesh_customdatacorrect_apply(tc, true);
    }
  }

  if (use_automerge) {
    FOREACH_TRANS_DATA_CONTAINER (t, tc) {
      BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);
      BMesh *bm = em->bm;
      char hflag;
      bool has_face_sel = (bm->totfacesel != 0);

      if (tc->use_mirror_axis_any) {
        /* Rather than adjusting the selection (which the user would notice)
         * tag all mirrored verts, then auto-merge those. */
        BM_mesh_elem_hflag_disable_all(bm, BM_VERT, BM_ELEM_TAG, false);

        TransDataMirror *td_mirror = tc->data_mirror;
        for (int i = tc->data_mirror_len; i--; td_mirror++) {
          BM_elem_flag_enable((BMVert *)td_mirror->extra, BM_ELEM_TAG);
        }

        hflag = BM_ELEM_SELECT | BM_ELEM_TAG;
      }
      else {
        hflag = BM_ELEM_SELECT;
      }

      if (t->flag & T_AUTOSPLIT) {
        EDBM_automerge_and_split(
            tc->obedit, true, true, true, hflag, t->scene->toolsettings->doublimit);
      }
      else {
        EDBM_automerge(tc->obedit, true, hflag, t->scene->toolsettings->doublimit);
      }

      /* Special case, this is needed or faces won't re-select.
       * Flush selected edges to faces. */
      if (has_face_sel && (em->selectmode == SCE_SELECT_FACE)) {
        EDBM_selectmode_flush_ex(em, SCE_SELECT_EDGE);
      }
    }
  }

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    /* table needs to be created for each edit command, since vertices can move etc */
    ED_mesh_mirror_spatial_table_end(tc->obedit);
    /* TODO(@ideasman42): xform: We need support for many mirror objects at once! */
    break;
  }
}

/** \} */

TransConvertTypeInfo TransConvertType_Mesh = {
    /*flags*/ (T_EDIT | T_POINTS),
    /*create_trans_data*/ createTransEditVerts,
    /*recalc_data*/ recalcData_mesh,
    /*special_aftertrans_update*/ special_aftertrans_update__mesh,
};
