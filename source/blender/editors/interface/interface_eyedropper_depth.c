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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup edinterface
 *
 * This file defines an eyedropper for picking 3D depth value (primary use is depth-of-field).
 *
 * Defines:
 * - #UI_OT_eyedropper_depth
 */

#include "MEM_guardedalloc.h"

#include "DNA_space_types.h"
#include "DNA_screen_types.h"
#include "DNA_object_types.h"
#include "DNA_camera_types.h"
#include "DNA_view3d_types.h"

#include "BLI_string.h"
#include "BLI_math_vector.h"

#include "BKE_context.h"
#include "BKE_screen.h"
#include "BKE_unit.h"

#include "DEG_depsgraph.h"

#include "RNA_access.h"

#include "UI_interface.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_view3d.h"

#include "interface_intern.h"
#include "interface_eyedropper_intern.h"

/**
 * \note #DepthDropper is only internal name to avoid confusion with other kinds of eye-droppers.
 */
typedef struct DepthDropper {
  PointerRNA ptr;
  PropertyRNA *prop;
  bool is_undo;

  bool is_set;
  float init_depth; /* for resetting on cancel */

  bool accum_start; /* has mouse been presed */
  float accum_depth;
  int accum_tot;

  ARegionType *art;
  void *draw_handle_pixel;
  char name[200];
} DepthDropper;

static void depthdropper_draw_cb(const struct bContext *C, ARegion *ar, void *arg)
{
  DepthDropper *ddr = arg;
  eyedropper_draw_cursor_text(C, ar, ddr->name);
}

static int depthdropper_init(bContext *C, wmOperator *op)
{
  int index_dummy;

  SpaceType *st;
  ARegionType *art;

  st = BKE_spacetype_from_id(SPACE_VIEW3D);
  art = BKE_regiontype_from_id(st, RGN_TYPE_WINDOW);

  DepthDropper *ddr = MEM_callocN(sizeof(DepthDropper), __func__);

  uiBut *but = UI_context_active_but_prop_get(C, &ddr->ptr, &ddr->prop, &index_dummy);

  /* fallback to the active camera's dof */
  if (ddr->prop == NULL) {
    RegionView3D *rv3d = CTX_wm_region_view3d(C);
    if (rv3d && rv3d->persp == RV3D_CAMOB) {
      View3D *v3d = CTX_wm_view3d(C);
      if (v3d->camera && v3d->camera->data && !ID_IS_LINKED(v3d->camera->data)) {
        Camera *camera = (Camera *)v3d->camera->data;
        RNA_pointer_create(&camera->id, &RNA_CameraDOFSettings, &camera->dof, &ddr->ptr);
        ddr->prop = RNA_struct_find_property(&ddr->ptr, "focus_distance");
        ddr->is_undo = true;
      }
    }
  }
  else {
    ddr->is_undo = UI_but_flag_is_set(but, UI_BUT_UNDO);
  }

  if ((ddr->ptr.data == NULL) || (ddr->prop == NULL) ||
      (RNA_property_editable(&ddr->ptr, ddr->prop) == false) ||
      (RNA_property_type(ddr->prop) != PROP_FLOAT)) {
    MEM_freeN(ddr);
    return false;
  }
  op->customdata = ddr;

  ddr->art = art;
  ddr->draw_handle_pixel = ED_region_draw_cb_activate(
      art, depthdropper_draw_cb, ddr, REGION_DRAW_POST_PIXEL);
  ddr->init_depth = RNA_property_float_get(&ddr->ptr, ddr->prop);

  return true;
}

static void depthdropper_exit(bContext *C, wmOperator *op)
{
  WM_cursor_modal_restore(CTX_wm_window(C));

  if (op->customdata) {
    DepthDropper *ddr = (DepthDropper *)op->customdata;

    if (ddr->art) {
      ED_region_draw_cb_exit(ddr->art, ddr->draw_handle_pixel);
    }

    MEM_freeN(op->customdata);

    op->customdata = NULL;
  }
}

/* *** depthdropper id helper functions *** */
/**
 * \brief get the ID from the screen.
 */
static void depthdropper_depth_sample_pt(
    bContext *C, DepthDropper *ddr, int mx, int my, float *r_depth)
{
  /* we could use some clever */
  bScreen *screen = CTX_wm_screen(C);
  ScrArea *sa = BKE_screen_find_area_xy(screen, SPACE_TYPE_ANY, mx, my);
  Scene *scene = CTX_data_scene(C);

  ScrArea *area_prev = CTX_wm_area(C);
  ARegion *ar_prev = CTX_wm_region(C);

  ddr->name[0] = '\0';

  if (sa) {
    if (sa->spacetype == SPACE_VIEW3D) {
      ARegion *ar = BKE_area_find_region_xy(sa, RGN_TYPE_WINDOW, mx, my);
      if (ar) {
        struct Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
        View3D *v3d = sa->spacedata.first;
        RegionView3D *rv3d = ar->regiondata;
        /* weak, we could pass in some reference point */
        const float *view_co = v3d->camera ? v3d->camera->obmat[3] : rv3d->viewinv[3];
        const int mval[2] = {mx - ar->winrct.xmin, my - ar->winrct.ymin};
        float co[3];

        CTX_wm_area_set(C, sa);
        CTX_wm_region_set(C, ar);

        /* grr, always draw else we leave stale text */
        ED_region_tag_redraw(ar);

        view3d_operator_needs_opengl(C);

        if (ED_view3d_autodist(depsgraph, ar, v3d, mval, co, true, NULL)) {
          const float mval_center_fl[2] = {(float)ar->winx / 2, (float)ar->winy / 2};
          float co_align[3];

          /* quick way to get view-center aligned point */
          ED_view3d_win_to_3d(v3d, ar, co, mval_center_fl, co_align);

          *r_depth = len_v3v3(view_co, co_align);

          bUnit_AsString2(ddr->name,
                          sizeof(ddr->name),
                          (double)*r_depth,
                          4,
                          B_UNIT_LENGTH,
                          &scene->unit,
                          false);
        }
        else {
          BLI_strncpy(ddr->name, "Nothing under cursor", sizeof(ddr->name));
        }
      }
    }
  }

  CTX_wm_area_set(C, area_prev);
  CTX_wm_region_set(C, ar_prev);
}

/* sets the sample depth RGB, maintaining A */
static void depthdropper_depth_set(bContext *C, DepthDropper *ddr, const float depth)
{
  RNA_property_float_set(&ddr->ptr, ddr->prop, depth);
  ddr->is_set = true;
  RNA_property_update(C, &ddr->ptr, ddr->prop);
}

/* set sample from accumulated values */
static void depthdropper_depth_set_accum(bContext *C, DepthDropper *ddr)
{
  float depth = ddr->accum_depth;
  if (ddr->accum_tot) {
    depth /= (float)ddr->accum_tot;
  }
  depthdropper_depth_set(C, ddr, depth);
}

/* single point sample & set */
static void depthdropper_depth_sample(bContext *C, DepthDropper *ddr, int mx, int my)
{
  float depth = -1.0f;
  if (depth != -1.0f) {
    depthdropper_depth_sample_pt(C, ddr, mx, my, &depth);
    depthdropper_depth_set(C, ddr, depth);
  }
}

static void depthdropper_depth_sample_accum(bContext *C, DepthDropper *ddr, int mx, int my)
{
  float depth = -1.0f;
  depthdropper_depth_sample_pt(C, ddr, mx, my, &depth);
  if (depth != -1.0f) {
    ddr->accum_depth += depth;
    ddr->accum_tot++;
  }
}

static void depthdropper_cancel(bContext *C, wmOperator *op)
{
  DepthDropper *ddr = op->customdata;
  if (ddr->is_set) {
    depthdropper_depth_set(C, ddr, ddr->init_depth);
  }
  depthdropper_exit(C, op);
}

/* main modal status check */
static int depthdropper_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  DepthDropper *ddr = (DepthDropper *)op->customdata;

  /* handle modal keymap */
  if (event->type == EVT_MODAL_MAP) {
    switch (event->val) {
      case EYE_MODAL_CANCEL:
        depthdropper_cancel(C, op);
        return OPERATOR_CANCELLED;
      case EYE_MODAL_SAMPLE_CONFIRM: {
        const bool is_undo = ddr->is_undo;
        if (ddr->accum_tot == 0) {
          depthdropper_depth_sample(C, ddr, event->x, event->y);
        }
        else {
          depthdropper_depth_set_accum(C, ddr);
        }
        depthdropper_exit(C, op);
        /* Could support finished & undo-skip. */
        return is_undo ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
      }
      case EYE_MODAL_SAMPLE_BEGIN:
        /* enable accum and make first sample */
        ddr->accum_start = true;
        depthdropper_depth_sample_accum(C, ddr, event->x, event->y);
        break;
      case EYE_MODAL_SAMPLE_RESET:
        ddr->accum_tot = 0;
        ddr->accum_depth = 0.0f;
        depthdropper_depth_sample_accum(C, ddr, event->x, event->y);
        depthdropper_depth_set_accum(C, ddr);
        break;
    }
  }
  else if (event->type == MOUSEMOVE) {
    if (ddr->accum_start) {
      /* button is pressed so keep sampling */
      depthdropper_depth_sample_accum(C, ddr, event->x, event->y);
      depthdropper_depth_set_accum(C, ddr);
    }
  }

  return OPERATOR_RUNNING_MODAL;
}

/* Modal Operator init */
static int depthdropper_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  /* init */
  if (depthdropper_init(C, op)) {
    WM_cursor_modal_set(CTX_wm_window(C), BC_EYEDROPPER_CURSOR);

    /* add temp handler */
    WM_event_add_modal_handler(C, op);

    return OPERATOR_RUNNING_MODAL;
  }
  else {
    return OPERATOR_CANCELLED;
  }
}

/* Repeat operator */
static int depthdropper_exec(bContext *C, wmOperator *op)
{
  /* init */
  if (depthdropper_init(C, op)) {
    /* cleanup */
    depthdropper_exit(C, op);

    return OPERATOR_FINISHED;
  }
  else {
    return OPERATOR_CANCELLED;
  }
}

static bool depthdropper_poll(bContext *C)
{
  PointerRNA ptr;
  PropertyRNA *prop;
  int index_dummy;
  uiBut *but;

  /* check if there's an active button taking depth value */
  if ((CTX_wm_window(C) != NULL) &&
      (but = UI_context_active_but_prop_get(C, &ptr, &prop, &index_dummy)) &&
      (but->type == UI_BTYPE_NUM) && (prop != NULL)) {
    if ((RNA_property_type(prop) == PROP_FLOAT) &&
        (RNA_property_subtype(prop) & PROP_UNIT_LENGTH) &&
        (RNA_property_array_check(prop) == false)) {
      return 1;
    }
  }
  else {
    RegionView3D *rv3d = CTX_wm_region_view3d(C);
    if (rv3d && rv3d->persp == RV3D_CAMOB) {
      View3D *v3d = CTX_wm_view3d(C);
      if (v3d->camera && v3d->camera->data && !ID_IS_LINKED(v3d->camera->data)) {
        return 1;
      }
    }
  }

  return 0;
}

void UI_OT_eyedropper_depth(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Eyedropper Depth";
  ot->idname = "UI_OT_eyedropper_depth";
  ot->description = "Sample depth from the 3D view";

  /* api callbacks */
  ot->invoke = depthdropper_invoke;
  ot->modal = depthdropper_modal;
  ot->cancel = depthdropper_cancel;
  ot->exec = depthdropper_exec;
  ot->poll = depthdropper_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_BLOCKING | OPTYPE_INTERNAL;

  /* properties */
}
