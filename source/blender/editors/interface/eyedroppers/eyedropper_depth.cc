/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * This file defines an eyedropper for picking 3D depth value (primary use is depth-of-field).
 *
 * Defines:
 * - #UI_OT_eyedropper_depth
 */

#include "MEM_guardedalloc.h"

#include "DNA_camera_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"

#include "BLI_math_vector.h"
#include "BLI_string_utf8.h"

#include "BKE_context.hh"
#include "BKE_lib_id.hh"
#include "BKE_report.hh"
#include "BKE_screen.hh"
#include "BKE_unit.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_path.hh"
#include "RNA_prototypes.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_screen.hh"
#include "ED_space_api.hh"
#include "ED_view3d.hh"

#include "eyedropper_intern.hh"
#include "interface_intern.hh"

/**
 * \note #DepthDropper is only internal name to avoid confusion with other kinds of eye-droppers.
 */
struct DepthDropper {
  PointerRNA ptr = {};
  PropertyRNA *prop = nullptr;
  bool is_undo = false;

  bool is_set = false;
  float init_depth = 0.0f; /* For resetting on cancel. */

  bool accum_start = false; /* Has mouse been pressed. */
  float accum_depth = 0.0f;
  int accum_tot = 0;

  ARegionType *art = nullptr;
  void *draw_handle_pixel = nullptr;
  int name_pos[2] = {};
  char name[200] = {};
};

static void depthdropper_draw_cb(const bContext * /*C*/, ARegion * /*region*/, void *arg)
{
  DepthDropper *ddr = static_cast<DepthDropper *>(arg);
  eyedropper_draw_cursor_text_region(ddr->name_pos, ddr->name);
}

static bool depthdropper_get_path(PointerRNA *ctx_ptr,
                                  wmOperator *op,
                                  const char *prop_path,
                                  PointerRNA *r_ptr,
                                  PropertyRNA **r_prop)
{
  PropertyRNA *unused_prop;

  if (prop_path[0] == '\0') {
    return false;
  }

  if (!r_prop) {
    r_prop = &unused_prop;
  }

  /* Get rna from path. */
  if (!RNA_path_resolve(ctx_ptr, prop_path, r_ptr, r_prop)) {
    BKE_reportf(op->reports, RPT_ERROR, "Could not resolve path '%s'", prop_path);
    return false;
  }

  /* Check property type. */
  PropertyType prop_type = RNA_property_type(*r_prop);
  if (prop_type != PROP_FLOAT) {
    BKE_reportf(op->reports, RPT_ERROR, "Property from path '%s' is not a float", prop_path);
    return false;
  }

  /* Success. */
  return true;
}

static bool depthdropper_test(bContext *C, wmOperator *op)
{
  PointerRNA ptr;
  PropertyRNA *prop;
  int index_dummy;
  uiBut *but;

  /* Check if the custom prop_data_path is set. */
  if ((prop = RNA_struct_find_property(op->ptr, "prop_data_path")) &&
      RNA_property_is_set(op->ptr, prop))
  {
    return true;
  }

  /* check if there's an active button taking depth value */
  if ((CTX_wm_window(C) != nullptr) &&
      (but = UI_context_active_but_prop_get(C, &ptr, &prop, &index_dummy)) &&
      (but->type == ButType::Num) && (prop != nullptr))
  {
    if ((RNA_property_type(prop) == PROP_FLOAT) &&
        (RNA_property_subtype(prop) & PROP_UNIT_LENGTH) &&
        (RNA_property_array_check(prop) == false))
    {
      return true;
    }
  }
  else {
    RegionView3D *rv3d = CTX_wm_region_view3d(C);
    if (rv3d && rv3d->persp == RV3D_CAMOB) {
      View3D *v3d = CTX_wm_view3d(C);
      if (v3d->camera && v3d->camera->data &&
          BKE_id_is_editable(CTX_data_main(C), static_cast<const ID *>(v3d->camera->data)))
      {
        return true;
      }
    }
  }

  return false;
}

static int depthdropper_init(bContext *C, wmOperator *op)
{
  DepthDropper *ddr = MEM_new<DepthDropper>(__func__);
  PropertyRNA *prop;
  if ((prop = RNA_struct_find_property(op->ptr, "prop_data_path")) &&
      RNA_property_is_set(op->ptr, prop))
  {
    std::string prop_data_path = RNA_string_get(op->ptr, "prop_data_path");
    if (prop_data_path.empty()) {
      MEM_delete(ddr);
      return false;
    }
    PointerRNA ctx_ptr = RNA_pointer_create_discrete(nullptr, &RNA_Context, C);
    if (!depthdropper_get_path(&ctx_ptr, op, prop_data_path.c_str(), &ddr->ptr, &ddr->prop)) {
      MEM_delete(ddr);
      return false;
    }
  }
  else {
    /* fallback to the active camera's dof */
    int index_dummy;
    uiBut *but = UI_context_active_but_prop_get(C, &ddr->ptr, &ddr->prop, &index_dummy);
    if (ddr->prop == nullptr) {
      RegionView3D *rv3d = CTX_wm_region_view3d(C);
      if (rv3d && rv3d->persp == RV3D_CAMOB) {
        View3D *v3d = CTX_wm_view3d(C);
        if (v3d->camera && v3d->camera->data &&
            BKE_id_is_editable(CTX_data_main(C), static_cast<const ID *>(v3d->camera->data)))
        {
          Camera *camera = (Camera *)v3d->camera->data;
          ddr->ptr = RNA_pointer_create_discrete(
              &camera->id, &RNA_CameraDOFSettings, &camera->dof);
          ddr->prop = RNA_struct_find_property(&ddr->ptr, "focus_distance");
          ddr->is_undo = true;
        }
      }
    }
    else {
      ddr->is_undo = UI_but_flag_is_set(but, UI_BUT_UNDO);
    }
  }

  if ((ddr->ptr.data == nullptr) || (ddr->prop == nullptr) ||
      (RNA_property_editable(&ddr->ptr, ddr->prop) == false) ||
      (RNA_property_type(ddr->prop) != PROP_FLOAT))
  {
    MEM_delete(ddr);
    return false;
  }
  op->customdata = ddr;

  SpaceType *st = BKE_spacetype_from_id(SPACE_VIEW3D);
  ARegionType *art = BKE_regiontype_from_id(st, RGN_TYPE_WINDOW);

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
    op->customdata = nullptr;
    MEM_delete(ddr);
  }
}

/* *** depthdropper id helper functions *** */
/**
 * \brief get the ID from the screen.
 */
static void depthdropper_depth_sample_pt(bContext *C,
                                         DepthDropper *ddr,
                                         const int m_xy[2],
                                         float *r_depth)
{
  /* we could use some clever */
  bScreen *screen = CTX_wm_screen(C);
  ScrArea *area = BKE_screen_find_area_xy(screen, SPACE_TYPE_ANY, m_xy);
  Scene *scene = CTX_data_scene(C);

  ScrArea *area_prev = CTX_wm_area(C);
  ARegion *region_prev = CTX_wm_region(C);

  ddr->name[0] = '\0';

  if (area) {
    if (area->spacetype == SPACE_VIEW3D) {
      ARegion *region = BKE_area_find_region_xy(area, RGN_TYPE_WINDOW, m_xy);
      if (region) {
        Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
        View3D *v3d = static_cast<View3D *>(area->spacedata.first);
        RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
        /* weak, we could pass in some reference point */
        const blender::float3 &view_co = (v3d->camera && rv3d->persp == RV3D_CAMOB) ?
                                             v3d->camera->object_to_world().location() :
                                             rv3d->viewinv[3];

        const int mval[2] = {m_xy[0] - region->winrct.xmin, m_xy[1] - region->winrct.ymin};
        copy_v2_v2_int(ddr->name_pos, mval);

        float co[3];

        CTX_wm_area_set(C, area);
        CTX_wm_region_set(C, region);

        /* Unfortunately it's necessary to always draw otherwise we leave stale text. */
        ED_region_tag_redraw(region);

        view3d_operator_needs_gpu(C);

        /* Ensure the depth buffer is updated for #ED_view3d_autodist. */
        ED_view3d_depth_override(
            depsgraph, region, v3d, nullptr, V3D_DEPTH_NO_GPENCIL, false, nullptr);

        if (ED_view3d_autodist(region, v3d, mval, co, nullptr)) {
          const float mval_center_fl[2] = {float(region->winx) / 2, float(region->winy) / 2};
          float co_align[3];

          /* quick way to get view-center aligned point */
          ED_view3d_win_to_3d(v3d, region, co, mval_center_fl, co_align);

          *r_depth = len_v3v3(view_co, co_align);

          BKE_unit_value_as_string(ddr->name,
                                   sizeof(ddr->name),
                                   double(*r_depth),
                                   -4,
                                   B_UNIT_LENGTH,
                                   scene->unit,
                                   false);
        }
        else {
          STRNCPY_UTF8(ddr->name, "Nothing under cursor");
        }
      }
    }
  }

  CTX_wm_area_set(C, area_prev);
  CTX_wm_region_set(C, region_prev);
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
    depth /= float(ddr->accum_tot);
  }
  depthdropper_depth_set(C, ddr, depth);
}

/* single point sample & set */
static void depthdropper_depth_sample(bContext *C, DepthDropper *ddr, const int m_xy[2])
{
  float depth = -1.0f;
  if (depth != -1.0f) {
    depthdropper_depth_sample_pt(C, ddr, m_xy, &depth);
    depthdropper_depth_set(C, ddr, depth);
  }
}

static void depthdropper_depth_sample_accum(bContext *C, DepthDropper *ddr, const int m_xy[2])
{
  float depth = -1.0f;
  depthdropper_depth_sample_pt(C, ddr, m_xy, &depth);
  if (depth != -1.0f) {
    ddr->accum_depth += depth;
    ddr->accum_tot++;
  }
}

static void depthdropper_cancel(bContext *C, wmOperator *op)
{
  DepthDropper *ddr = static_cast<DepthDropper *>(op->customdata);
  if (ddr->is_set) {
    depthdropper_depth_set(C, ddr, ddr->init_depth);
  }
  depthdropper_exit(C, op);
}

/* main modal status check */
static wmOperatorStatus depthdropper_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  DepthDropper *ddr = static_cast<DepthDropper *>(op->customdata);

  /* handle modal keymap */
  if (event->type == EVT_MODAL_MAP) {
    switch (event->val) {
      case EYE_MODAL_CANCEL:
        depthdropper_cancel(C, op);
        return OPERATOR_CANCELLED;
      case EYE_MODAL_SAMPLE_CONFIRM: {
        const bool is_undo = ddr->is_undo;
        if (ddr->accum_tot == 0) {
          depthdropper_depth_sample(C, ddr, event->xy);
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
        depthdropper_depth_sample_accum(C, ddr, event->xy);
        break;
      case EYE_MODAL_SAMPLE_RESET:
        ddr->accum_tot = 0;
        ddr->accum_depth = 0.0f;
        depthdropper_depth_sample_accum(C, ddr, event->xy);
        depthdropper_depth_set_accum(C, ddr);
        break;
    }
  }
  else if (event->type == MOUSEMOVE) {
    if (ddr->accum_start) {
      /* button is pressed so keep sampling */
      depthdropper_depth_sample_accum(C, ddr, event->xy);
      depthdropper_depth_set_accum(C, ddr);
    }
  }

  return OPERATOR_RUNNING_MODAL;
}

/* Modal Operator init */
static wmOperatorStatus depthdropper_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  if (!depthdropper_test(C, op)) {
    /* If the operator can't be executed, make sure to not consume the event. */
    return OPERATOR_PASS_THROUGH;
  }
  /* init */
  if (depthdropper_init(C, op)) {
    wmWindow *win = CTX_wm_window(C);
    /* Workaround for de-activating the button clearing the cursor, see #76794 */
    UI_context_active_but_clear(C, win, CTX_wm_region(C));
    WM_cursor_modal_set(win, WM_CURSOR_EYEDROPPER);

    /* add temp handler */
    WM_event_add_modal_handler(C, op);

    return OPERATOR_RUNNING_MODAL;
  }
  return OPERATOR_CANCELLED;
}

/* Repeat operator */
static wmOperatorStatus depthdropper_exec(bContext *C, wmOperator *op)
{
  /* init */
  if (depthdropper_init(C, op)) {
    /* cleanup */
    depthdropper_exit(C, op);

    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

static bool depthdropper_poll(bContext *C)
{
  PointerRNA ptr;
  PropertyRNA *prop;
  int index_dummy;
  uiBut *but;

  /* check if there's an active button taking depth value */
  if ((CTX_wm_window(C) != nullptr) &&
      (but = UI_context_active_but_prop_get(C, &ptr, &prop, &index_dummy)))
  {
    if (but->icon == ICON_EYEDROPPER) {
      return true;
    }
    /* Context menu button. */
    if (but->optype && STREQ(but->optype->idname, "UI_OT_eyedropper_depth")) {
      return true;
    }

    if ((but->type == ButType::Num) && (prop != nullptr) &&
        (RNA_property_type(prop) == PROP_FLOAT) &&
        (RNA_property_subtype(prop) & PROP_UNIT_LENGTH) &&
        (RNA_property_array_check(prop) == false))
    {
      return true;
    }
  }
  else {
    RegionView3D *rv3d = CTX_wm_region_view3d(C);
    if (rv3d && rv3d->persp == RV3D_CAMOB) {
      View3D *v3d = CTX_wm_view3d(C);
      if (v3d->camera && v3d->camera->data &&
          BKE_id_is_editable(CTX_data_main(C), static_cast<const ID *>(v3d->camera->data)))
      {
        return true;
      }
    }
  }

  return false;
}

void UI_OT_eyedropper_depth(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Eyedropper Depth";
  ot->idname = "UI_OT_eyedropper_depth";
  ot->description = "Sample depth from the 3D view";

  /* API callbacks. */
  ot->invoke = depthdropper_invoke;
  ot->modal = depthdropper_modal;
  ot->cancel = depthdropper_cancel;
  ot->exec = depthdropper_exec;
  ot->poll = depthdropper_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_BLOCKING | OPTYPE_INTERNAL;

  /* Paths relative to the context. */
  PropertyRNA *prop;
  prop = RNA_def_string(ot->srna,
                        "prop_data_path",
                        nullptr,
                        0,
                        "Data Path",
                        "Path of property to be set with the depth");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}
