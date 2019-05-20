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
 * Eyedropper (ID data-blocks)
 *
 * Defines:
 * - #UI_OT_eyedropper_id
 */

#include "MEM_guardedalloc.h"

#include "DNA_space_types.h"
#include "DNA_screen_types.h"
#include "DNA_object_types.h"

#include "BLI_string.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_screen.h"
#include "BKE_report.h"
#include "BKE_idcode.h"

#include "RNA_access.h"

#include "UI_interface.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_space_api.h"
#include "ED_screen.h"
#include "ED_view3d.h"

#include "interface_intern.h"
#include "interface_eyedropper_intern.h"

/**
 * \note #DataDropper is only internal name to avoid confusion with other kinds of eye-droppers.
 */
typedef struct DataDropper {
  PointerRNA ptr;
  PropertyRNA *prop;
  short idcode;
  const char *idcode_name;
  bool is_undo;

  ID *init_id; /* for resetting on cancel */

  ARegionType *art;
  void *draw_handle_pixel;
  char name[200];
} DataDropper;

static void datadropper_draw_cb(const struct bContext *C, ARegion *ar, void *arg)
{
  DataDropper *ddr = arg;
  eyedropper_draw_cursor_text(C, ar, ddr->name);
}

static int datadropper_init(bContext *C, wmOperator *op)
{
  int index_dummy;
  StructRNA *type;

  SpaceType *st;
  ARegionType *art;

  st = BKE_spacetype_from_id(SPACE_VIEW3D);
  art = BKE_regiontype_from_id(st, RGN_TYPE_WINDOW);

  DataDropper *ddr = MEM_callocN(sizeof(DataDropper), __func__);

  uiBut *but = UI_context_active_but_prop_get(C, &ddr->ptr, &ddr->prop, &index_dummy);

  if ((ddr->ptr.data == NULL) || (ddr->prop == NULL) ||
      (RNA_property_editable(&ddr->ptr, ddr->prop) == false) ||
      (RNA_property_type(ddr->prop) != PROP_POINTER)) {
    MEM_freeN(ddr);
    return false;
  }
  op->customdata = ddr;

  ddr->is_undo = UI_but_flag_is_set(but, UI_BUT_UNDO);

  ddr->art = art;
  ddr->draw_handle_pixel = ED_region_draw_cb_activate(
      art, datadropper_draw_cb, ddr, REGION_DRAW_POST_PIXEL);

  type = RNA_property_pointer_type(&ddr->ptr, ddr->prop);
  ddr->idcode = RNA_type_to_ID_code(type);
  BLI_assert(ddr->idcode != 0);
  /* Note we can translate here (instead of on draw time),
   * because this struct has very short lifetime. */
  ddr->idcode_name = TIP_(BKE_idcode_to_name(ddr->idcode));

  PointerRNA ptr = RNA_property_pointer_get(&ddr->ptr, ddr->prop);
  ddr->init_id = ptr.id.data;

  return true;
}

static void datadropper_exit(bContext *C, wmOperator *op)
{
  WM_cursor_modal_restore(CTX_wm_window(C));

  if (op->customdata) {
    DataDropper *ddr = (DataDropper *)op->customdata;

    if (ddr->art) {
      ED_region_draw_cb_exit(ddr->art, ddr->draw_handle_pixel);
    }

    MEM_freeN(op->customdata);

    op->customdata = NULL;
  }

  WM_event_add_mousemove(C);
}

/* *** datadropper id helper functions *** */
/**
 * \brief get the ID from the screen.
 */
static void datadropper_id_sample_pt(bContext *C, DataDropper *ddr, int mx, int my, ID **r_id)
{
  /* we could use some clever */
  bScreen *screen = CTX_wm_screen(C);
  ScrArea *sa = BKE_screen_find_area_xy(screen, -1, mx, my);

  ScrArea *area_prev = CTX_wm_area(C);
  ARegion *ar_prev = CTX_wm_region(C);

  ddr->name[0] = '\0';

  if (sa) {
    if (sa->spacetype == SPACE_VIEW3D) {
      ARegion *ar = BKE_area_find_region_xy(sa, RGN_TYPE_WINDOW, mx, my);
      if (ar) {
        const int mval[2] = {mx - ar->winrct.xmin, my - ar->winrct.ymin};
        Base *base;

        CTX_wm_area_set(C, sa);
        CTX_wm_region_set(C, ar);

        /* grr, always draw else we leave stale text */
        ED_region_tag_redraw(ar);

        base = ED_view3d_give_base_under_cursor(C, mval);
        if (base) {
          Object *ob = base->object;
          ID *id = NULL;
          if (ddr->idcode == ID_OB) {
            id = (ID *)ob;
          }
          else if (ob->data) {
            if (GS(((ID *)ob->data)->name) == ddr->idcode) {
              id = (ID *)ob->data;
            }
            else {
              BLI_snprintf(
                  ddr->name, sizeof(ddr->name), "Incompatible, expected a %s", ddr->idcode_name);
            }
          }

          PointerRNA idptr;
          RNA_id_pointer_create(id, &idptr);

          if (id && RNA_property_pointer_poll(&ddr->ptr, ddr->prop, &idptr)) {
            BLI_snprintf(ddr->name, sizeof(ddr->name), "%s: %s", ddr->idcode_name, id->name + 2);
            *r_id = id;
          }
        }
      }
    }
  }

  CTX_wm_area_set(C, area_prev);
  CTX_wm_region_set(C, ar_prev);
}

/* sets the ID, returns success */
static bool datadropper_id_set(bContext *C, DataDropper *ddr, ID *id)
{
  PointerRNA ptr_value;

  RNA_id_pointer_create(id, &ptr_value);

  RNA_property_pointer_set(&ddr->ptr, ddr->prop, ptr_value, NULL);

  RNA_property_update(C, &ddr->ptr, ddr->prop);

  ptr_value = RNA_property_pointer_get(&ddr->ptr, ddr->prop);

  return (ptr_value.id.data == id);
}

/* single point sample & set */
static bool datadropper_id_sample(bContext *C, DataDropper *ddr, int mx, int my)
{
  ID *id = NULL;

  datadropper_id_sample_pt(C, ddr, mx, my, &id);
  return datadropper_id_set(C, ddr, id);
}

static void datadropper_cancel(bContext *C, wmOperator *op)
{
  DataDropper *ddr = op->customdata;
  datadropper_id_set(C, ddr, ddr->init_id);
  datadropper_exit(C, op);
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
        const bool success = datadropper_id_sample(C, ddr, event->x, event->y);
        datadropper_exit(C, op);
        if (success) {
          /* Could support finished & undo-skip. */
          return is_undo ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
        }
        else {
          BKE_report(op->reports, RPT_WARNING, "Failed to set value");
          return OPERATOR_CANCELLED;
        }
      }
    }
  }
  else if (event->type == MOUSEMOVE) {
    ID *id = NULL;
    datadropper_id_sample_pt(C, ddr, event->x, event->y, &id);
  }

  return OPERATOR_RUNNING_MODAL;
}

/* Modal Operator init */
static int datadropper_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  /* init */
  if (datadropper_init(C, op)) {
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
static int datadropper_exec(bContext *C, wmOperator *op)
{
  /* init */
  if (datadropper_init(C, op)) {
    /* cleanup */
    datadropper_exit(C, op);

    return OPERATOR_FINISHED;
  }
  else {
    return OPERATOR_CANCELLED;
  }
}

static bool datadropper_poll(bContext *C)
{
  PointerRNA ptr;
  PropertyRNA *prop;
  int index_dummy;
  uiBut *but;

  /* data dropper only supports object data */
  if ((CTX_wm_window(C) != NULL) &&
      (but = UI_context_active_but_prop_get(C, &ptr, &prop, &index_dummy)) &&
      (but->type == UI_BTYPE_SEARCH_MENU) && (but->flag & UI_BUT_VALUE_CLEAR)) {
    if (prop && RNA_property_type(prop) == PROP_POINTER) {
      StructRNA *type = RNA_property_pointer_type(&ptr, prop);
      const short idcode = RNA_type_to_ID_code(type);
      if ((idcode == ID_OB) || OB_DATA_SUPPORT_ID(idcode)) {
        return 1;
      }
    }
  }

  return 0;
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
