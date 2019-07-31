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
 * \ingroup eduv
 */

#include <string.h>
#include <stdio.h>

#include "MEM_guardedalloc.h"

#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_editmesh.h"
#include "BKE_layer.h"
#include "BKE_screen.h"

#include "DEG_depsgraph.h"

#include "ED_image.h"
#include "ED_uvedit.h"

#include "UI_interface.h"

#include "WM_api.h"
#include "WM_types.h"

#define B_UVEDIT_VERTEX 3

/* UV Utilities */

static int uvedit_center(
    Scene *scene, Object **objects, uint objects_len, Image *ima, float center[2])
{
  BMFace *f;
  BMLoop *l;
  BMIter iter, liter;
  MLoopUV *luv;
  int tot = 0;

  zero_v2(center);

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

    BM_ITER_MESH (f, &iter, em->bm, BM_FACES_OF_MESH) {
      if (!uvedit_face_visible_test(scene, obedit, ima, f)) {
        continue;
      }

      BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
        if (uvedit_uv_select_test(scene, l, cd_loop_uv_offset)) {
          luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
          add_v2_v2(center, luv->uv);
          tot++;
        }
      }
    }
  }

  if (tot > 0) {
    center[0] /= tot;
    center[1] /= tot;
  }

  return tot;
}

static void uvedit_translate(
    Scene *scene, Object **objects, uint objects_len, Image *ima, float delta[2])
{
  BMFace *f;
  BMLoop *l;
  BMIter iter, liter;
  MLoopUV *luv;

  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    const int cd_loop_uv_offset = CustomData_get_offset(&em->bm->ldata, CD_MLOOPUV);

    BM_ITER_MESH (f, &iter, em->bm, BM_FACES_OF_MESH) {
      if (!uvedit_face_visible_test(scene, obedit, ima, f)) {
        continue;
      }

      BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
        if (uvedit_uv_select_test(scene, l, cd_loop_uv_offset)) {
          luv = BM_ELEM_CD_GET_VOID_P(l, cd_loop_uv_offset);
          add_v2_v2(luv->uv, delta);
        }
      }
    }
  }
}

/* Button Functions, using an evil static variable */

static float uvedit_old_center[2];

static void uvedit_vertex_buttons(const bContext *C, uiBlock *block)
{
  SpaceImage *sima = CTX_wm_space_image(C);
  Scene *scene = CTX_data_scene(C);
  Image *ima = sima->image;
  float center[2];
  int imx, imy, step, digits;
  float width = 8 * UI_UNIT_X;
  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      CTX_data_view_layer(C), CTX_wm_view3d(C), &objects_len);

  ED_space_image_get_size(sima, &imx, &imy);

  if (uvedit_center(scene, objects, objects_len, ima, center)) {
    float range_xy[2][2] = {
        {-10.0f, 10.0f},
        {-10.0f, 10.0f},
    };

    copy_v2_v2(uvedit_old_center, center);

    /* expand UI range by center */
    CLAMP_MAX(range_xy[0][0], uvedit_old_center[0]);
    CLAMP_MIN(range_xy[0][1], uvedit_old_center[0]);
    CLAMP_MAX(range_xy[1][0], uvedit_old_center[1]);
    CLAMP_MIN(range_xy[1][1], uvedit_old_center[1]);

    if (!(sima->flag & SI_COORDFLOATS)) {
      uvedit_old_center[0] *= imx;
      uvedit_old_center[1] *= imy;

      mul_v2_fl(range_xy[0], imx);
      mul_v2_fl(range_xy[1], imy);
    }

    if (sima->flag & SI_COORDFLOATS) {
      step = 1;
      digits = 3;
    }
    else {
      step = 100;
      digits = 2;
    }

    UI_block_align_begin(block);
    uiDefButF(block,
              UI_BTYPE_NUM,
              B_UVEDIT_VERTEX,
              IFACE_("X:"),
              0,
              0,
              width,
              UI_UNIT_Y,
              &uvedit_old_center[0],
              UNPACK2(range_xy[0]),
              step,
              digits,
              "");
    uiDefButF(block,
              UI_BTYPE_NUM,
              B_UVEDIT_VERTEX,
              IFACE_("Y:"),
              width,
              0,
              width,
              UI_UNIT_Y,
              &uvedit_old_center[1],
              UNPACK2(range_xy[1]),
              step,
              digits,
              "");
    UI_block_align_end(block);
  }

  MEM_freeN(objects);
}

static void do_uvedit_vertex(bContext *C, void *UNUSED(arg), int event)
{
  SpaceImage *sima = CTX_wm_space_image(C);
  Scene *scene = CTX_data_scene(C);
  Image *ima = sima->image;
  float center[2], delta[2];
  int imx, imy;

  if (event != B_UVEDIT_VERTEX) {
    return;
  }

  uint objects_len = 0;
  Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      CTX_data_view_layer(C), CTX_wm_view3d(C), &objects_len);

  ED_space_image_get_size(sima, &imx, &imy);
  uvedit_center(scene, objects, objects_len, ima, center);

  if (sima->flag & SI_COORDFLOATS) {
    delta[0] = uvedit_old_center[0] - center[0];
    delta[1] = uvedit_old_center[1] - center[1];
  }
  else {
    delta[0] = uvedit_old_center[0] / imx - center[0];
    delta[1] = uvedit_old_center[1] / imy - center[1];
  }

  uvedit_translate(scene, objects, objects_len, ima, delta);

  WM_event_add_notifier(C, NC_IMAGE, sima->image);
  for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
    Object *obedit = objects[ob_index];
    DEG_id_tag_update((ID *)obedit->data, ID_RECALC_GEOMETRY);
  }

  MEM_freeN(objects);
}

/* Panels */

static bool image_panel_uv_poll(const bContext *C, PanelType *UNUSED(pt))
{
  SpaceImage *sima = CTX_wm_space_image(C);
  if (sima->mode != SI_MODE_UV) {
    return false;
  }
  Object *obedit = CTX_data_edit_object(C);
  return ED_uvedit_test(obedit);
}

static void image_panel_uv(const bContext *C, Panel *pa)
{
  uiBlock *block;

  block = uiLayoutAbsoluteBlock(pa->layout);
  UI_block_func_handle_set(block, do_uvedit_vertex, NULL);

  uvedit_vertex_buttons(C, block);
}

void ED_uvedit_buttons_register(ARegionType *art)
{
  PanelType *pt;

  pt = MEM_callocN(sizeof(PanelType), "spacetype image panel uv");
  strcpy(pt->idname, "IMAGE_PT_uv");
  strcpy(pt->label, N_("UV Vertex")); /* XXX C panels unavailable through RNA bpy.types! */
  /* Could be 'Item' matching 3D view, avoid new tab for two buttons. */
  strcpy(pt->category, "Image");
  pt->draw = image_panel_uv;
  pt->poll = image_panel_uv_poll;
  BLI_addtail(&art->paneltypes, pt);
}
