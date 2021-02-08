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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup edtransform
 */

#include "DNA_meshdata_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_bitmap.h"
#include "BLI_linklist_stack.h"
#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_editmesh.h"
#include "BKE_mesh_mapping.h"

#include "ED_image.h"
#include "ED_mesh.h"
#include "ED_uvedit.h"

#include "WM_api.h" /* for WM_event_add_notifier to deal with stabilization nodes */

#include "transform.h"
#include "transform_convert.h"

/* -------------------------------------------------------------------- */
/** \name UVs Transform Creation
 *
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

  td->ext = NULL;
  td->val = NULL;

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
static void uv_set_connectivity_distance(BMesh *bm, float *dists, const float aspect[2])
{
  /* Mostly copied from #transform_convert_mesh_connectivity_distance. */
  BLI_LINKSTACK_DECLARE(queue, BMLoop *);

  /* Any BM_ELEM_TAG'd loop is added to 'queue_next', this makes sure that we don't add things
   * twice. */
  BLI_LINKSTACK_DECLARE(queue_next, BMLoop *);

  BLI_LINKSTACK_INIT(queue);
  BLI_LINKSTACK_INIT(queue_next);

  const int cd_loop_uv_offset = CustomData_get_offset(&bm->ldata, CD_MLOOPUV);
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
      MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);

      bool uv_vert_sel = luv->flag & MLOOPUV_VERTSEL;

      if (uv_vert_sel) {
        BLI_LINKSTACK_PUSH(queue, l);
        dist = 0.0f;
      }
      else {
        dist = FLT_MAX;
      }

      /* Make sure all loops are in a clean tag state. */
      BLI_assert(BM_elem_flag_test(l, BM_ELEM_TAG) == 0);

      int loop_idx = BM_elem_index_get(l);

      dists[loop_idx] = dist;
    }
  }

  /* Need to be very careful of feedback loops here, store previous dist's to avoid feedback. */
  float *dists_prev = MEM_dupallocN(dists);

  do {
    while ((l = BLI_LINKSTACK_POP(queue))) {
      BLI_assert(dists[BM_elem_index_get(l)] != FLT_MAX);

      BMLoop *l_other, *l_connected;
      BMIter l_connected_iter;

      MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
      float l_uv[2];

      copy_v2_v2(l_uv, luv->uv);
      mul_v2_v2(l_uv, aspect);

      BM_ITER_ELEM (l_other, &liter, l->f, BM_LOOPS_OF_FACE) {
        if (l_other == l) {
          continue;
        }
        float other_uv[2], edge_vec[2];
        MLoopUV *luv_other = BM_ELEM_CD_GET_VOID_P(l_other, cd_loop_uv_offset);

        copy_v2_v2(other_uv, luv_other->uv);
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

        other_vert_sel = luv_other->flag & MLOOPUV_VERTSEL;

        BM_ITER_ELEM (l_connected, &l_connected_iter, l_other->v, BM_LOOPS_OF_VERT) {
          if (l_connected == l_other) {
            continue;
          }
          /* Visible faces was tagged in #createTransUVs. */
          if (!BM_elem_flag_test(l_connected->f, BM_ELEM_TAG)) {
            continue;
          }

          MLoopUV *luv_connected = BM_ELEM_CD_GET_VOID_P(l_connected, cd_loop_uv_offset);
          connected_vert_sel = luv_connected->flag & MLOOPUV_VERTSEL;

          /* Check if this loop is connected in UV space.
           * If the uv loops share the same selection state (if not, they are not connected as
           * they have been ripped or other edit commands have separated them). */
          bool connected = other_vert_sel == connected_vert_sel &&
                           equals_v2v2(luv_other->uv, luv_connected->uv);
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
      BMLoop *l_link = lnk->link;
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
}

void createTransUVs(bContext *C, TransInfo *t)
{
  SpaceImage *sima = CTX_wm_space_image(C);
  Scene *scene = t->scene;
  ToolSettings *ts = CTX_data_tool_settings(C);

  const bool is_prop_edit = (t->flag & T_PROP_EDIT) != 0;
  const bool is_prop_connected = (t->flag & T_PROP_CONNECTED) != 0;
  const bool is_island_center = (t->around == V3D_AROUND_LOCAL_ORIGINS);

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {

    TransData *td = NULL;
    TransData2D *td2d = NULL;
    BMEditMesh *em = BKE_editmesh_from_object(tc->obedit);
    BMFace *efa;
    BMIter iter, liter;
    UvElementMap *elementmap = NULL;
    struct {
      float co[2];
      int co_num;
    } *island_center = NULL;
    int count = 0, countsel = 0;
    const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

    if (!ED_space_image_show_uvedit(sima, tc->obedit)) {
      continue;
    }

    /* count */
    if (is_island_center) {
      /* create element map with island information */
      const bool use_facesel = (ts->uv_flag & UV_SYNC_SELECTION) == 0;
      elementmap = BM_uv_element_map_create(em->bm, scene, use_facesel, true, false, true);
      if (elementmap == NULL) {
        continue;
      }

      island_center = MEM_callocN(sizeof(*island_center) * elementmap->totalIslands, __func__);
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
        if (uvedit_uv_select_test(scene, l, cd_loop_uv_offset)) {
          countsel++;

          if (island_center) {
            UvElement *element = BM_uv_element_get(elementmap, efa, l);

            if (element->flag == false) {
              MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
              add_v2_v2(island_center[element->island].co, luv->uv);
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

    float *prop_dists = NULL;

    /* Support other objects using PET to adjust these, unless connected is enabled. */
    if (((is_prop_edit && !is_prop_connected) ? count : countsel) == 0) {
      goto finally;
    }

    if (is_island_center) {
      int i;

      for (i = 0; i < elementmap->totalIslands; i++) {
        mul_v2_fl(island_center[i].co, 1.0f / island_center[i].co_num);
        mul_v2_v2(island_center[i].co, t->aspect);
      }
    }

    tc->data_len = (is_prop_edit) ? count : countsel;
    tc->data = MEM_callocN(tc->data_len * sizeof(TransData), "TransObData(UV Editing)");
    /* for each 2d uv coord a 3d vector is allocated, so that they can be
     * treated just as if they were 3d verts */
    tc->data_2d = MEM_callocN(tc->data_len * sizeof(TransData2D), "TransObData2D(UV Editing)");

    if (sima->flag & SI_CLIP_UV) {
      t->flag |= T_CLIP_UV;
    }

    td = tc->data;
    td2d = tc->data_2d;

    if (is_prop_connected) {
      prop_dists = MEM_callocN(em->bm->totloop * sizeof(float), "TransObPropDists(UV Editing)");

      uv_set_connectivity_distance(em->bm, prop_dists, t->aspect);
    }

    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      BMLoop *l;

      if (!BM_elem_flag_test(efa, BM_ELEM_TAG)) {
        continue;
      }

      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        const bool selected = uvedit_uv_select_test(scene, l, cd_loop_uv_offset);
        MLoopUV *luv;
        const float *center = NULL;
        float prop_distance = FLT_MAX;

        if (!is_prop_edit && !selected) {
          continue;
        }

        if (is_prop_connected) {
          const int idx = BM_elem_index_get(l);
          prop_distance = prop_dists[idx];
        }

        if (is_island_center) {
          UvElement *element = BM_uv_element_get(elementmap, efa, l);
          if (element) {
            center = island_center[element->island].co;
          }
        }

        luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
        UVsToTransData(t->aspect, td++, td2d++, luv->uv, center, prop_distance, selected);
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
 *
 * \{ */

static void flushTransUVs(TransInfo *t)
{
  SpaceImage *sima = t->area->spacedata.first;
  const bool use_pixel_snap = ((sima->pixel_snap_mode != SI_PIXEL_SNAP_DISABLED) &&
                               (t->state != TRANS_CANCEL));

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData2D *td;
    int a;
    float aspect_inv[2], size[2];

    aspect_inv[0] = 1.0f / t->aspect[0];
    aspect_inv[1] = 1.0f / t->aspect[1];

    if (use_pixel_snap) {
      int size_i[2];
      ED_space_image_get_size(sima, &size_i[0], &size_i[1]);
      size[0] = size_i[0];
      size[1] = size_i[1];
    }

    /* flush to 2d vector from internally used 3d vector */
    for (a = 0, td = tc->data_2d; a < tc->data_len; a++, td++) {
      td->loc2d[0] = td->loc[0] * aspect_inv[0];
      td->loc2d[1] = td->loc[1] * aspect_inv[1];

      if (use_pixel_snap) {
        td->loc2d[0] *= size[0];
        td->loc2d[1] *= size[1];

        switch (sima->pixel_snap_mode) {
          case SI_PIXEL_SNAP_CENTER:
            td->loc2d[0] = roundf(td->loc2d[0] - 0.5f) + 0.5f;
            td->loc2d[1] = roundf(td->loc2d[1] - 0.5f) + 0.5f;
            break;
          case SI_PIXEL_SNAP_CORNER:
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

/* helper for recalcData() - for Image Editor transforms */
void recalcData_uv(TransInfo *t)
{
  SpaceImage *sima = t->area->spacedata.first;

  flushTransUVs(t);
  if (sima->flag & SI_LIVE_UNWRAP) {
    ED_uvedit_live_unwrap_re_solve();
  }

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    if (tc->data_len) {
      DEG_id_tag_update(tc->obedit->data, 0);
    }
  }
}

/** \} */
