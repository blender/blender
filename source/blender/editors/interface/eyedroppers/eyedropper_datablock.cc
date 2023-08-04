/* SPDX-FileCopyrightText: 2009 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * Eyedropper (ID data-blocks)
 *
 * Defines:
 * - #UI_OT_eyedropper_id
 */

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BLI_math_vector.h"
#include "BLI_string.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_idtype.h"
#include "BKE_report.h"
#include "BKE_screen.h"

#include "RNA_access.h"

#include "UI_interface.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_outliner.h"
#include "ED_screen.hh"
#include "ED_space_api.h"
#include "ED_view3d.h"

#include "eyedropper_intern.hh"
#include "interface_intern.hh"

/**
 * \note #DataDropper is only internal name to avoid confusion with other kinds of eye-droppers.
 */
struct DataDropper {
  PointerRNA ptr;
  PropertyRNA *prop;
  short idcode;
  const char *idcode_name;
  bool is_undo;

  ID *init_id; /* for resetting on cancel */

  ScrArea *cursor_area; /* Area under the cursor */
  ARegionType *art;
  void *draw_handle_pixel;
  int name_pos[2];
  char name[200];
};

static void datadropper_draw_cb(const bContext * /*C*/, ARegion * /*region*/, void *arg)
{
  DataDropper *ddr = static_cast<DataDropper *>(arg);
  eyedropper_draw_cursor_text_region(ddr->name_pos, ddr->name);
}

static int datadropper_init(bContext *C, wmOperator *op)
{
  int index_dummy;
  StructRNA *type;

  SpaceType *st;
  ARegionType *art;

  st = BKE_spacetype_from_id(SPACE_VIEW3D);
  art = BKE_regiontype_from_id(st, RGN_TYPE_WINDOW);

  DataDropper *ddr = MEM_cnew<DataDropper>(__func__);

  uiBut *but = UI_context_active_but_prop_get(C, &ddr->ptr, &ddr->prop, &index_dummy);

  if ((ddr->ptr.data == nullptr) || (ddr->prop == nullptr) ||
      (RNA_property_editable(&ddr->ptr, ddr->prop) == false) ||
      (RNA_property_type(ddr->prop) != PROP_POINTER))
  {
    MEM_freeN(ddr);
    return false;
  }
  op->customdata = ddr;

  ddr->is_undo = UI_but_flag_is_set(but, UI_BUT_UNDO);

  ddr->cursor_area = CTX_wm_area(C);
  ddr->art = art;
  ddr->draw_handle_pixel = ED_region_draw_cb_activate(
      art, datadropper_draw_cb, ddr, REGION_DRAW_POST_PIXEL);

  type = RNA_property_pointer_type(&ddr->ptr, ddr->prop);
  ddr->idcode = RNA_type_to_ID_code(type);
  BLI_assert(ddr->idcode != 0);
  /* Note we can translate here (instead of on draw time),
   * because this struct has very short lifetime. */
  ddr->idcode_name = TIP_(BKE_idtype_idcode_to_name(ddr->idcode));

  const PointerRNA ptr = RNA_property_pointer_get(&ddr->ptr, ddr->prop);
  ddr->init_id = ptr.owner_id;

  return true;
}

static void datadropper_exit(bContext *C, wmOperator *op)
{
  wmWindow *win = CTX_wm_window(C);

  WM_cursor_modal_restore(win);

  if (op->customdata) {
    DataDropper *ddr = (DataDropper *)op->customdata;

    if (ddr->art) {
      ED_region_draw_cb_exit(ddr->art, ddr->draw_handle_pixel);
    }

    MEM_freeN(op->customdata);

    op->customdata = nullptr;
  }

  WM_event_add_mousemove(win);
}

/* *** datadropper id helper functions *** */
/**
 * \brief get the ID from the 3D view or outliner.
 */
static void datadropper_id_sample_pt(
    bContext *C, wmWindow *win, ScrArea *area, DataDropper *ddr, const int m_xy[2], ID **r_id)
{
  wmWindow *win_prev = CTX_wm_window(C);
  ScrArea *area_prev = CTX_wm_area(C);
  ARegion *region_prev = CTX_wm_region(C);

  ddr->name[0] = '\0';

  if (area) {
    if (ELEM(area->spacetype, SPACE_VIEW3D, SPACE_OUTLINER)) {
      ARegion *region = BKE_area_find_region_xy(area, RGN_TYPE_WINDOW, m_xy);
      if (region) {
        const int mval[2] = {m_xy[0] - region->winrct.xmin, m_xy[1] - region->winrct.ymin};
        Base *base;

        CTX_wm_window_set(C, win);
        CTX_wm_area_set(C, area);
        CTX_wm_region_set(C, region);

        /* Unfortunately it's necessary to always draw else we leave stale text. */
        ED_region_tag_redraw(region);

        if (area->spacetype == SPACE_VIEW3D) {
          base = ED_view3d_give_base_under_cursor(C, mval);
        }
        else {
          base = ED_outliner_give_base_under_cursor(C, mval);
        }

        if (base) {
          Object *ob = base->object;
          ID *id = nullptr;
          if (ddr->idcode == ID_OB) {
            id = (ID *)ob;
          }
          else if (ob->data) {
            if (GS(((ID *)ob->data)->name) == ddr->idcode) {
              id = (ID *)ob->data;
            }
            else {
              SNPRINTF(ddr->name, "Incompatible, expected a %s", ddr->idcode_name);
            }
          }

          PointerRNA idptr;
          RNA_id_pointer_create(id, &idptr);

          if (id && RNA_property_pointer_poll(&ddr->ptr, ddr->prop, &idptr)) {
            SNPRINTF(ddr->name, "%s: %s", ddr->idcode_name, id->name + 2);
            *r_id = id;
          }

          copy_v2_v2_int(ddr->name_pos, mval);
        }
      }
    }
  }

  CTX_wm_window_set(C, win_prev);
  CTX_wm_area_set(C, area_prev);
  CTX_wm_region_set(C, region_prev);
}

/* sets the ID, returns success */
static bool datadropper_id_set(bContext *C, DataDropper *ddr, ID *id)
{
  PointerRNA ptr_value;

  RNA_id_pointer_create(id, &ptr_value);

  RNA_property_pointer_set(&ddr->ptr, ddr->prop, ptr_value, nullptr);

  RNA_property_update(C, &ddr->ptr, ddr->prop);

  ptr_value = RNA_property_pointer_get(&ddr->ptr, ddr->prop);

  return (ptr_value.owner_id == id);
}

/* single point sample & set */
static bool datadropper_id_sample(bContext *C, DataDropper *ddr, const int m_xy[2])
{
  ID *id = nullptr;

  int mval[2];
  wmWindow *win;
  ScrArea *area;
  datadropper_win_area_find(C, m_xy, mval, &win, &area);

  datadropper_id_sample_pt(C, win, area, ddr, mval, &id);
  return datadropper_id_set(C, ddr, id);
}

static void datadropper_cancel(bContext *C, wmOperator *op)
{
  DataDropper *ddr = static_cast<DataDropper *>(op->customdata);
  datadropper_id_set(C, ddr, ddr->init_id);
  datadropper_exit(C, op);
}

/* To switch the draw callback when region under mouse event changes */
static void datadropper_set_draw_callback_region(ScrArea *area, DataDropper *ddr)
{
  if (area) {
    /* If spacetype changed */
    if (area->spacetype != ddr->cursor_area->spacetype) {
      /* Remove old callback */
      ED_region_draw_cb_exit(ddr->art, ddr->draw_handle_pixel);

      /* Redraw old area */
      ARegion *region = BKE_area_find_region_type(ddr->cursor_area, RGN_TYPE_WINDOW);
      ED_region_tag_redraw(region);

      /* Set draw callback in new region */
      ARegionType *art = BKE_regiontype_from_id(area->type, RGN_TYPE_WINDOW);

      ddr->cursor_area = area;
      ddr->art = art;
      ddr->draw_handle_pixel = ED_region_draw_cb_activate(
          art, datadropper_draw_cb, ddr, REGION_DRAW_POST_PIXEL);
    }
  }
}

/* main modal status check */
static int datadropper_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  DataDropper *ddr = (DataDropper *)op->customdata;

  /* handle modal keymap */
  if (event->type == EVT_MODAL_MAP) {
    switch (event->val) {
      case EYE_MODAL_CANCEL:
        datadropper_cancel(C, op);
        return OPERATOR_CANCELLED;
      case EYE_MODAL_SAMPLE_CONFIRM: {
        const bool is_undo = ddr->is_undo;
        const bool success = datadropper_id_sample(C, ddr, event->xy);
        datadropper_exit(C, op);
        if (success) {
          /* Could support finished & undo-skip. */
          return is_undo ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
        }
        BKE_report(op->reports, RPT_WARNING, "Failed to set value");
        return OPERATOR_CANCELLED;
      }
    }
  }
  else if (event->type == MOUSEMOVE) {
    ID *id = nullptr;

    int mval[2];
    wmWindow *win;
    ScrArea *area;
    datadropper_win_area_find(C, event->xy, mval, &win, &area);

    /* Set the region for eyedropper cursor text drawing */
    datadropper_set_draw_callback_region(area, ddr);

    datadropper_id_sample_pt(C, win, area, ddr, mval, &id);
  }

  return OPERATOR_RUNNING_MODAL;
}

/* Modal Operator init */
static int datadropper_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  /* init */
  if (datadropper_init(C, op)) {
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
static int datadropper_exec(bContext *C, wmOperator *op)
{
  /* init */
  if (datadropper_init(C, op)) {
    /* cleanup */
    datadropper_exit(C, op);

    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

static bool datadropper_poll(bContext *C)
{
  PointerRNA ptr;
  PropertyRNA *prop;
  int index_dummy;
  uiBut *but;

  /* data dropper only supports object data */
  if ((CTX_wm_window(C) != nullptr) &&
      (but = UI_context_active_but_prop_get(C, &ptr, &prop, &index_dummy)) &&
      (but->type == UI_BTYPE_SEARCH_MENU) && (but->flag & UI_BUT_VALUE_CLEAR))
  {
    if (prop && RNA_property_type(prop) == PROP_POINTER) {
      StructRNA *type = RNA_property_pointer_type(&ptr, prop);
      const short idcode = RNA_type_to_ID_code(type);
      if ((idcode == ID_OB) || OB_DATA_SUPPORT_ID(idcode)) {
        return true;
      }
    }
  }

  return false;
}

void UI_OT_eyedropper_id(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Eyedropper Data-Block";
  ot->idname = "UI_OT_eyedropper_id";
  ot->description = "Sample a data-block from the 3D View to store in a property";

  /* api callbacks */
  ot->invoke = datadropper_invoke;
  ot->modal = datadropper_modal;
  ot->cancel = datadropper_cancel;
  ot->exec = datadropper_exec;
  ot->poll = datadropper_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_BLOCKING | OPTYPE_INTERNAL;

  /* properties */
}
