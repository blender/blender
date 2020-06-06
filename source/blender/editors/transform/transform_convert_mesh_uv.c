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
                           bool selected)
{
  /* uv coords are scaled by aspects. this is needed for rotations and
   * proportional editing to be consistent with the stretched uv coords
   * that are displayed. this also means that for display and numinput,
   * and when the uv coords are flushed, these are converted each time */
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
    td->dist = FLT_MAX;
  }
  unit_m3(td->mtx);
  unit_m3(td->smtx);
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
    BLI_bitmap *island_enabled = NULL;
    struct {
      float co[2];
      int co_num;
    } *island_center = NULL;
    int count = 0, countsel = 0, count_rejected = 0;
    const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

    if (!ED_space_image_show_uvedit(sima, tc->obedit)) {
      continue;
    }

    /* count */
    if (is_prop_connected || is_island_center) {
      /* create element map with island information */
      const bool use_facesel = (ts->uv_flag & UV_SYNC_SELECTION) == 0;
      const bool use_uvsel = !is_prop_connected;
      elementmap = BM_uv_element_map_create(em->bm, scene, use_facesel, use_uvsel, false, true);
      if (elementmap == NULL) {
        continue;
      }

      if (is_prop_connected) {
        island_enabled = BLI_BITMAP_NEW(elementmap->totalIslands, "TransIslandData(UV Editing)");
      }

      if (is_island_center) {
        island_center = MEM_callocN(sizeof(*island_center) * elementmap->totalIslands, __func__);
      }
    }

    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      BMLoop *l;

      if (!uvedit_face_visible_test(scene, efa)) {
        BM_elem_flag_disable(efa, BM_ELEM_TAG);
        continue;
      }

      BM_elem_flag_enable(efa, BM_ELEM_TAG);
      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        if (uvedit_uv_select_test(scene, l, cd_loop_uv_offset)) {
          countsel++;

          if (is_prop_connected || island_center) {
            UvElement *element = BM_uv_element_get(elementmap, efa, l);

            if (is_prop_connected) {
              BLI_BITMAP_ENABLE(island_enabled, element->island);
            }

            if (is_island_center) {
              if (element->flag == false) {
                MLoopUV *luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
                add_v2_v2(island_center[element->island].co, luv->uv);
                island_center[element->island].co_num++;
                element->flag = true;
              }
            }
          }
        }

        if (is_prop_edit) {
          count++;
        }
      }
    }

    /* note: in prop mode we need at least 1 selected */
    if (countsel == 0) {
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

    BM_ITER_MESH (efa, &iter, em->bm, BM_FACES_OF_MESH) {
      BMLoop *l;

      if (!BM_elem_flag_test(efa, BM_ELEM_TAG)) {
        continue;
      }

      BM_ITER_ELEM (l, &liter, efa, BM_LOOPS_OF_FACE) {
        const bool selected = uvedit_uv_select_test(scene, l, cd_loop_uv_offset);
        MLoopUV *luv;
        const float *center = NULL;

        if (!is_prop_edit && !selected) {
          continue;
        }

        if (is_prop_connected || is_island_center) {
          UvElement *element = BM_uv_element_get(elementmap, efa, l);
          if (element) {
            if (is_prop_connected) {
              if (!BLI_BITMAP_TEST(island_enabled, element->island)) {
                count_rejected++;
                continue;
              }
            }

            if (is_island_center) {
              center = island_center[element->island].co;
            }
          }
        }

        BM_elem_flag_enable(l, BM_ELEM_TAG);
        luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
        UVsToTransData(t->aspect, td++, td2d++, luv->uv, center, selected);
      }
    }

    if (is_prop_connected) {
      tc->data_len -= count_rejected;
    }

    if (sima->flag & SI_LIVE_UNWRAP) {
      ED_uvedit_live_unwrap_begin(t->scene, tc->obedit);
    }

  finally:
    if (is_prop_connected || is_island_center) {
      BM_uv_element_map_free(elementmap);

      if (is_prop_connected) {
        MEM_freeN(island_enabled);
      }

      if (island_center) {
        MEM_freeN(island_center);
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UVs Transform Flush
 *
 * \{ */

void flushTransUVs(TransInfo *t)
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

/** \} */
