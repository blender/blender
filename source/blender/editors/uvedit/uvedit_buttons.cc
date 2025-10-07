/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eduv
 */

#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BKE_context.hh"
#include "BKE_customdata.hh"
#include "BKE_editmesh.hh"
#include "BKE_layer.hh"
#include "BKE_screen.hh"

#include "DEG_depsgraph.hh"

#include "ED_image.hh"
#include "ED_uvedit.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"

#include "WM_api.hh"
#include "WM_types.hh"

using blender::Span;
using blender::Vector;

#define B_UVEDIT_VERTEX 3

/* UV Utilities */

static int uvedit_center(Scene *scene, const Span<Object *> objects, float center[2])
{
  BMFace *f;
  BMLoop *l;
  BMIter iter, liter;
  float *luv;
  int tot = 0;

  zero_v2(center);

  for (Object *obedit : objects) {
    BMEditMesh *em = BKE_editmesh_from_object(obedit);
    const BMUVOffsets offsets = BM_uv_map_offsets_get(em->bm);

    BM_ITER_MESH (f, &iter, em->bm, BM_FACES_OF_MESH) {
      if (!uvedit_face_visible_test(scene, f)) {
        continue;
      }

      BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
        if (uvedit_uv_select_test(scene, em->bm, l, offsets)) {
          luv = BM_ELEM_CD_GET_FLOAT_P(l, offsets.uv);
          add_v2_v2(center, luv);
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

static void uvedit_translate(Scene *scene, const Span<Object *> objects, const float delta[2])
{
  BMFace *f;
  BMLoop *l;
  BMIter iter, liter;
  float *luv;

  for (Object *obedit : objects) {
    BMEditMesh *em = BKE_editmesh_from_object(obedit);

    const BMUVOffsets offsets = BM_uv_map_offsets_get(em->bm);

    BM_ITER_MESH (f, &iter, em->bm, BM_FACES_OF_MESH) {
      if (!uvedit_face_visible_test(scene, f)) {
        continue;
      }

      BM_ITER_ELEM (l, &liter, f, BM_LOOPS_OF_FACE) {
        if (uvedit_uv_select_test(scene, em->bm, l, offsets)) {
          luv = BM_ELEM_CD_GET_FLOAT_P(l, offsets.uv);
          add_v2_v2(luv, delta);
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
  float center[2];
  int imx, imy, step, digits;
  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      scene, CTX_data_view_layer(C), CTX_wm_view3d(C));

  ED_space_image_get_size(sima, &imx, &imy);

  if (uvedit_center(scene, objects, center)) {
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

    uiBut *but;

    int y = 0;
    UI_block_align_begin(block);
    but = uiDefButF(block,
                    ButType::Num,
                    B_UVEDIT_VERTEX,
                    IFACE_("X:"),
                    0,
                    y -= UI_UNIT_Y,
                    200,
                    UI_UNIT_Y,
                    &uvedit_old_center[0],
                    UNPACK2(range_xy[0]),
                    "");
    UI_but_number_step_size_set(but, step);
    UI_but_number_precision_set(but, digits);
    but = uiDefButF(block,
                    ButType::Num,
                    B_UVEDIT_VERTEX,
                    IFACE_("Y:"),
                    0,
                    y -= UI_UNIT_Y,
                    200,
                    UI_UNIT_Y,
                    &uvedit_old_center[1],
                    UNPACK2(range_xy[1]),
                    "");
    UI_but_number_step_size_set(but, step);
    UI_but_number_precision_set(but, digits);
    UI_block_align_end(block);
  }
}

static void do_uvedit_vertex(bContext *C, void * /*arg*/, int event)
{
  SpaceImage *sima = CTX_wm_space_image(C);
  Scene *scene = CTX_data_scene(C);
  float center[2], delta[2];
  int imx, imy;

  if (event != B_UVEDIT_VERTEX) {
    return;
  }

  Vector<Object *> objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data_with_uvs(
      scene, CTX_data_view_layer(C), CTX_wm_view3d(C));

  ED_space_image_get_size(sima, &imx, &imy);
  uvedit_center(scene, objects, center);

  if (sima->flag & SI_COORDFLOATS) {
    delta[0] = uvedit_old_center[0] - center[0];
    delta[1] = uvedit_old_center[1] - center[1];
  }
  else {
    delta[0] = uvedit_old_center[0] / imx - center[0];
    delta[1] = uvedit_old_center[1] / imy - center[1];
  }

  uvedit_translate(scene, objects, delta);

  WM_event_add_notifier(C, NC_IMAGE, sima->image);
  for (Object *obedit : objects) {
    DEG_id_tag_update((ID *)obedit->data, ID_RECALC_GEOMETRY);
  }
}

/* Panels */

static bool image_panel_uv_poll(const bContext *C, PanelType * /*pt*/)
{
  SpaceImage *sima = CTX_wm_space_image(C);
  if (sima->mode != SI_MODE_UV) {
    return false;
  }
  Object *obedit = CTX_data_edit_object(C);
  return ED_uvedit_test(obedit);
}

static void image_panel_uv(const bContext *C, Panel *panel)
{
  uiBlock *block;

  block = panel->layout->absolute_block();
  UI_block_func_handle_set(block, do_uvedit_vertex, nullptr);

  uvedit_vertex_buttons(C, block);
}

void ED_uvedit_buttons_register(ARegionType *art)
{
  PanelType *pt = MEM_callocN<PanelType>(__func__);

  STRNCPY_UTF8(pt->idname, "IMAGE_PT_uv");
  STRNCPY_UTF8(pt->label, N_("UV Vertex")); /* XXX C panels unavailable through RNA bpy.types! */
  /* Could be 'Item' matching 3D view, avoid new tab for two buttons. */
  STRNCPY_UTF8(pt->category, "Image");
  pt->draw = image_panel_uv;
  pt->poll = image_panel_uv_poll;
  BLI_addtail(&art->paneltypes, pt);
}
