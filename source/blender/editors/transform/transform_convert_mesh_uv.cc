/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include "DNA_meshdata_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_bitmap.h"
#include "BLI_linklist_stack.h"
#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_editmesh.h"
#include "BKE_mesh_mapping.hh"

#include "ED_image.h"
#include "ED_mesh.h"
#include "ED_uvedit.h"

#include "WM_api.hh" /* for WM_event_add_notifier to deal with stabilization nodes */

#include "transform.hh"
#include "transform_convert.hh"

/* -------------------------------------------------------------------- */
/** \name UVs Transform Creation
 * \{ */

static void UVsToTransData(const float aspect[2],
                           TransData *td,
                           TransData2D *td2d,
                           float *uv,
                           const float *center,
                           float calc_dist,
                           bool selected)
{
  /* UV coords are scaled by aspects. this is needed for rotations and
   * proportional editing to be consistent with the stretched UV coords
   * that are displayed. this also means that for display and number-input,
   * and when the UV coords are flushed, these are converted each time. */
  td2d->loc[0] = uv[0] * aspect[0];
  td2d->loc[1] = uv[1] * aspect[1];
  td2d->loc[2] = 0.0f;
  td2d->loc2d = uv;

  td->flag = 0;
  td->loc = td2d->loc;
  copy_v2_v2(td->center, center ? center : td->loc);
  td->center[2] = 0.0f;
  copy_v3_v3(td->iloc, td->loc);

  memset(td->axismtx, 0, sizeof(td->axismtx));
  td->axismtx[2][2] = 1.0f;

  td->ext = nullptr;
  td->val = nullptr;

  if (selected) {
    td->flag |= TD_SELECTED;
    td->dist = 0.0;
  }
  else {
    td->dist = calc_dist;
  }
  unit_m3(td->mtx);
  unit_m3(td->smtx);
}

/**
 * \param dists: Store the closest connected distance to selected vertices.
 */
static void uv_set_connectivity_distance(const ToolSettings *ts,
                                         BMesh *bm,
                                         float *dists,
                                         const float aspect[2])
{
#define TMP_LOOP_SELECT_TAG BM_ELEM_TAG_ALT
  /* Mostly copied from #transform_convert_mesh_connectivity_distance. */
  BLI_LINKSTACK_DECLARE(queue, BMLoop *);

  /* Any BM_ELEM_TAG'd loop is added to 'queue_next', this makes sure that we don't add things
   * twice. */
  BLI_LINKSTACK_DECLARE(queue_next, BMLoop *);

  BLI_LINKSTACK_INIT(queue);
  BLI_LINKSTACK_INIT(queue_next);

  const BMUVOffsets offsets = BM_uv_map_get_offsets(bm);

  BMIter fiter, liter;
  BMVert *f;
  BMLoop *l;

  BM_mesh_elem_index_ensure(bm, BM_LOOP);

  BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
    /* Visible faces was tagged in #createTransUVs. */
    if (!BM_elem_flag_test(f, BM_ELEM_TAG)) {
      continue;
    }

    BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
      float dist;
      bool uv_vert_sel = uvedit_uv_select_test_ex(ts, l, offsets);

      if (uv_vert_sel) {
        BLI_LINKSTACK_PUSH(queue, l);
        BM_elem_flag_enable(l, TMP_LOOP_SELECT_TAG);
        dist = 0.0f;
      }
      else {
        BM_elem_flag_disable(l, TMP_LOOP_SELECT_TAG);
        dist = FLT_MAX;
      }

      /* Make sure all loops are in a clean tag state. */
      BLI_assert(BM_elem_flag_test(l, BM_ELEM_TAG) == 0);

      int loop_idx = BM_elem_index_get(l);

      dists[loop_idx] = dist;
    }
  }

  /* Need to be very careful of feedback loops here, store previous dist's to avoid feedback. */
  float *dists_prev = static_cast<float *>(MEM_dupallocN(dists));

  do {
    while ((l = BLI_LINKSTACK_POP(queue))) {
      BLI_assert(dists[BM_elem_index_get(l)] != FLT_MAX);

      BMLoop *l_other, *l_connected;
      BMIter l_connected_iter;

      float *luv = BM_ELEM_CD_GET_FLOAT_P(l, offsets.uv);
      float l_uv[2];

      copy_v2_v2(l_uv, luv);
      mul_v2_v2(l_uv, aspect);

      BM_ITER_ELEM (l_other, &liter, l->f, BM_LOOPS_OF_FACE) {
        if (l_other == l) {
          continue;
        }
        float other_uv[2], edge_vec[2];
        float *luv_other = BM_ELEM_CD_GET_FLOAT_P(l_other, offsets.uv);

        copy_v2_v2(other_uv, luv_other);
        mul_v2_v2(other_uv, aspect);

        sub_v2_v2v2(edge_vec, l_uv, other_uv);

        const int i = BM_elem_index_get(l);
        const int i_other = BM_elem_index_get(l_other);
        float dist = len_v2(edge_vec) + dists_prev[i];

        if (dist < dists[i_other]) {
          dists[i_other] = dist;
        }
        else {
          /* The face loop already has a shorter path to it. */
          continue;
        }

        bool other_vert_sel, connected_vert_sel;

        other_vert_sel = BM_elem_flag_test_bool(l_other, TMP_LOOP_SELECT_TAG);

        BM_ITER_ELEM (l_connected, &l_connected_iter, l_other->v, BM_LOOPS_OF_VERT) {
          if (l_connected == l_other) {
            continue;
          }
          /* Visible faces was tagged in #createTransUVs. */
          if (!BM_elem_flag_test(l_connected->f, BM_ELEM_TAG)) {
            continue;
          }

          float *luv_connected = BM_ELEM_CD_GET_FLOAT_P(l_connected, offsets.uv);
          connected_vert_sel = BM_elem_flag_test_bool(l_connected, TMP_LOOP_SELECT_TAG);

          /* Check if this loop is connected in UV space.
           * If the uv loops share the same selection state (if not, they are not connected as
           * they have been ripped or other edit commands have separated them). */
          bool connected = other_vert_sel == connected_vert_sel &&
                           equals_v2v2(luv_other, luv_connected);
          if (!connected) {
            continue;
          }

          /* The loop vert is occupying the same space, so it has the same distance. */
          const int i_connected = BM_elem_index_get(l_connected);
          dists[i_connected] = dist;

          if (BM_elem_flag_test(l_connected, BM_ELEM_TAG) == 0) {
            BM_elem_flag_enable(l_connected, BM_ELEM_TAG);
            BLI_LINKSTACK_PUSH(queue_next, l_connected);
          }
        }
      }
    }

    /* Clear elem flags for the next loop. */
    for (LinkNode *lnk = queue_next; lnk; lnk = lnk->next) {
      BMLoop *l_link = static_cast<BMLoop *>(lnk->link);
      const int i = BM_elem_index_get(l_link);

      BM_elem_flag_disable(l_link, BM_ELEM_TAG);

      /* Store all new dist values. */
      dists_prev[i] = dists[i];
    }

    BLI_LINKSTACK_SWAP(queue, queue_next);

  } while (BLI_LINKSTACK_SIZE(queue));

#ifndef NDEBUG
  /* Check that we didn't leave any loops tagged */
  BM_ITER_MESH (f, &fiter, bm, BM_FACES_OF_MESH) {
    /* Visible faces was tagged in #createTransUVs. */
    if (!BM_elem_flag_test(f, BM_ELEM_TAG)) {
      continue;
    }

    BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
      BLI_assert(BM_elem_flag_test(l, BM_ELEM_TAG) == 0);
    }
  }
#endif

  BLI_LINKSTACK_FREE(queue);
  BLI_LINKSTACK_FREE(queue_next);

  MEM_freeN(dists_prev);
#undef TMP_LOOP_SELECT_TAG
}

static void createTransUVs(bContext *C, TransInfo *t)
{
  SpaceImage *sima = CTX_wm_space_image(C);
  Scene *scene = t->scene;

  const bool is_prop_edit = (t->flag & T_PROP_EDIT) != 0;
  const bool is_prop_connected = (t->flag & T_PROP_CONNECTED) != 0;
  const bool is_island_center = (t->around == V3D_AROUND_LOCAL_ORIGINS);

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {

    TransData *td = nullptr;
    TransData2D *td2d = nullptr;
    BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);
    BMFace *efa;
    BMIter iter, liter;
    UvElementMap *elementmap = nullptr;
    struct IslandCenter {
      float co[2];
      int co_num;
    } *island_center = nullptr;
    int count = 0, countsel = 0;
    const BMUVOffsets offsets = BM_uv_map_get_offsets(em->bm);

    if (!ED_space_image_show_uvedit(sima, tc->obedit)) {
      continue;
    }

    /* count */
    if (is_island_center) {
      /* create element map with island information */
      elementmap = BM_uv_element_map_create(em->bm, scene, true, false, true, true);
      if (elementmap == nullptr) {
        continue;
      }

      island_center = static_cast<IslandCenter *>(
          MEM_callocN(sizeof(*island_center) * elementmap->total_islands, __func__));
    }

    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      BMLoop *l;

      if (!uvedit_face_visible_test(scene, efa)) {
        BM_elem_flag_disable(efa, BM_ELEM_TAG);
        continue;
      }

      BM_elem_flag_enable(efa, BM_ELEM_TAG);
      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        /* Make sure that the loop element flag is cleared for when we use it in
         * uv_set_connectivity_distance later. */
        BM_elem_flag_disable(l, BM_ELEM_TAG);
        if (uvedit_uv_select_test(scene, l, offsets)) {
          countsel++;

          if (island_center) {
            UvElement *element = BM_uv_element_get(elementmap, l);

            if (element && !element->flag) {
              float *luv = BM_ELEM_CD_GET_FLOAT_P(l, offsets.uv);
              add_v2_v2(island_center[element->island].co, luv);
              island_center[element->island].co_num++;
              element->flag = true;
            }
          }
        }

        if (is_prop_edit) {
          count++;
        }
      }
    }

    float *prop_dists = nullptr;

    /* Support other objects using proportional editing to adjust these, unless connected is
     * enabled. */
    if (((is_prop_edit && !is_prop_connected) ? count : countsel) == 0) {
      goto finally;
    }

    if (is_island_center) {
      for (int i = 0; i < elementmap->total_islands; i++) {
        mul_v2_fl(island_center[i].co, 1.0f / island_center[i].co_num);
        mul_v2_v2(island_center[i].co, t->aspect);
      }
    }

    tc->data_len = (is_prop_edit) ? count : countsel;
    tc->data = static_cast<TransData *>(
        MEM_callocN(tc->data_len * sizeof(TransData), "TransObData(UV Editing)"));
    /* for each 2d uv coord a 3d vector is allocated, so that they can be
     * treated just as if they were 3d verts */
    tc->data_2d = static_cast<TransData2D *>(
        MEM_callocN(tc->data_len * sizeof(TransData2D), "TransObData2D(UV Editing)"));

    if (sima->flag & SI_CLIP_UV) {
      t->flag |= T_CLIP_UV;
    }

    td = tc->data;
    td2d = tc->data_2d;

    if (is_prop_connected) {
      prop_dists = static_cast<float *>(
          MEM_callocN(em->bm->totloop * sizeof(float), "TransObPropDists(UV Editing)"));

      uv_set_connectivity_distance(t->settings, em->bm, prop_dists, t->aspect);
    }

    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      BMLoop *l;

      if (!BM_elem_flag_test(efa, BM_ELEM_TAG)) {
        continue;
      }

      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        const bool selected = uvedit_uv_select_test(scene, l, offsets);
        float(*luv)[2];
        const float *center = nullptr;
        float prop_distance = FLT_MAX;

        if (!is_prop_edit && !selected) {
          continue;
        }

        if (is_prop_connected) {
          const int idx = BM_elem_index_get(l);
          prop_distance = prop_dists[idx];
        }

        if (is_island_center) {
          UvElement *element = BM_uv_element_get(elementmap, l);
          if (element) {
            center = island_center[element->island].co;
          }
        }

        luv = (float(*)[2])BM_ELEM_CD_GET_FLOAT_P(l, offsets.uv);
        UVsToTransData(t->aspect, td++, td2d++, *luv, center, prop_distance, selected);
      }
    }

    if (sima->flag & SI_LIVE_UNWRAP) {
      ED_uvedit_live_unwrap_begin(t->scene, tc->obedit);
    }

  finally:
    if (is_prop_connected) {
      MEM_SAFE_FREE(prop_dists);
    }
    if (is_island_center) {
      BM_uv_element_map_free(elementmap);

      MEM_freeN(island_center);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UVs Transform Flush
 * \{ */

static void flushTransUVs(TransInfo *t)
{
  SpaceImage *sima = static_cast<SpaceImage *>(t->area->spacedata.first);
  const bool use_pixel_round = ((sima->pixel_round_mode != SI_PIXEL_ROUND_DISABLED) &&
                                (t->state != TRANS_CANCEL));

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData2D *td;
    int a;
    float aspect_inv[2], size[2];

    aspect_inv[0] = 1.0f / t->aspect[0];
    aspect_inv[1] = 1.0f / t->aspect[1];

    if (use_pixel_round) {
      int size_i[2];
      ED_space_image_get_size(sima, &size_i[0], &size_i[1]);
      size[0] = size_i[0];
      size[1] = size_i[1];
    }

    /* flush to 2d vector from internally used 3d vector */
    for (a = 0, td = tc->data_2d; a < tc->data_len; a++, td++) {
      td->loc2d[0] = td->loc[0] * aspect_inv[0];
      td->loc2d[1] = td->loc[1] * aspect_inv[1];

      if (use_pixel_round) {
        td->loc2d[0] *= size[0];
        td->loc2d[1] *= size[1];

        switch (sima->pixel_round_mode) {
          case SI_PIXEL_ROUND_CENTER:
            td->loc2d[0] = roundf(td->loc2d[0] - 0.5f) + 0.5f;
            td->loc2d[1] = roundf(td->loc2d[1] - 0.5f) + 0.5f;
            break;
          case SI_PIXEL_ROUND_CORNER:
            td->loc2d[0] = roundf(td->loc2d[0]);
            td->loc2d[1] = roundf(td->loc2d[1]);
            break;
        }

        td->loc2d[0] /= size[0];
        td->loc2d[1] /= size[1];
      }
    }
  }
}

static void recalcData_uv(TransInfo *t)
{
  SpaceImage *sima = static_cast<SpaceImage *>(t->area->spacedata.first);

  flushTransUVs(t);
  if (sima->flag & SI_LIVE_UNWRAP) {
    ED_uvedit_live_unwrap_re_solve();
  }

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    if (tc->data_len) {
      DEG_id_tag_update(static_cast<ID *>(tc->obedit->data), ID_RECALC_GEOMETRY);
    }
  }
}

/** \} */

TransConvertTypeInfo TransConvertType_MeshUV = {
    /*flags*/ (T_EDIT | T_POINTS | T_2D_EDIT),
    /*create_trans_data*/ createTransUVs,
    /*recalc_data*/ recalcData_uv,
    /*special_aftertrans_update*/ nullptr,
};
