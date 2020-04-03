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
 * Eyedropper (RGB Color)
 *
 * Defines:
 * - #UI_OT_eyedropper_gpencil_color
 */

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "BLT_translation.h"

#include "DNA_gpencil_types.h"
#include "DNA_space_types.h"

#include "BKE_context.h"
#include "BKE_gpencil.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_paint.h"
#include "BKE_report.h"

#include "UI_interface.h"

#include "IMB_colormanagement.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "ED_gpencil.h"
#include "ED_screen.h"
#include "ED_undo.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "interface_eyedropper_intern.h"
#include "interface_intern.h"

typedef struct EyedropperGPencil {
  struct ColorManagedDisplay *display;
  /** color under cursor RGB */
  float color[3];
  /** Mode */
  int mode;
} EyedropperGPencil;

/* Helper: Draw status message while the user is running the operator */
static void eyedropper_gpencil_status_indicators(bContext *C)
{
  char msg_str[UI_MAX_DRAW_STR];
  BLI_strncpy(
      msg_str, TIP_("LMB: Stroke - Shift: Fill - Shift+Ctrl: Stroke + Fill"), UI_MAX_DRAW_STR);

  ED_workspace_status_text(C, msg_str);
}

/* Initialize. */
static bool eyedropper_gpencil_init(bContext *C, wmOperator *op)
{
  EyedropperGPencil *eye = MEM_callocN(sizeof(EyedropperGPencil), __func__);

  op->customdata = eye;
  Scene *scene = CTX_data_scene(C);

  const char *display_device;
  display_device = scene->display_settings.display_device;
  eye->display = IMB_colormanagement_display_get_named(display_device);

  eye->mode = RNA_enum_get(op->ptr, "mode");
  return true;
}

/* Exit and free memory. */
static void eyedropper_gpencil_exit(bContext *C, wmOperator *op)
{
  /* Clear status message area. */
  ED_workspace_status_text(C, NULL);

  MEM_SAFE_FREE(op->customdata);
}

static void eyedropper_add_material(
    bContext *C, float col_conv[4], const bool only_stroke, const bool only_fill, const bool both)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = CTX_data_active_object(C);
  Material *ma = NULL;

  bool found = false;

  /* Look for a similar material in grease pencil slots. */
  short *totcol = BKE_object_material_len_p(ob);
  for (short i = 0; i < *totcol; i++) {
    ma = BKE_object_material_get(ob, i + 1);
    if (ma == NULL) {
      continue;
    }

    MaterialGPencilStyle *gp_style = ma->gp_style;
    if (gp_style != NULL) {
      /* Check stroke color. */
      bool found_stroke = compare_v3v3(gp_style->stroke_rgba, col_conv, 0.01f) &&
                          (gp_style->flag & GP_MATERIAL_STROKE_SHOW);
      /* Check fill color. */
      bool found_fill = compare_v3v3(gp_style->fill_rgba, col_conv, 0.01f) &&
                        (gp_style->flag & GP_MATERIAL_FILL_SHOW);

      if ((only_stroke) && (found_stroke) && ((gp_style->flag & GP_MATERIAL_FILL_SHOW) == 0)) {
        found = true;
      }
      else if ((only_fill) && (found_fill) && ((gp_style->flag & GP_MATERIAL_STROKE_SHOW) == 0)) {
        found = true;
      }
      else if ((both) && (found_stroke) && (found_fill)) {
        found = true;
      }

      /* Found existing material. */
      if (found) {
        ob->actcol = i + 1;
        WM_main_add_notifier(NC_MATERIAL | ND_SHADING_LINKS, NULL);
        WM_main_add_notifier(NC_SPACE | ND_SPACE_VIEW3D, NULL);
        return;
      }
    }
  }

  /* If material was not found add a new material with stroke and/or fill color
   * depending of the secondary key (LMB: Stroke, Shift: Fill, Shift+Ctrl: Stroke/Fill)
   */
  int idx;
  Material *ma_new = BKE_gpencil_object_material_new(bmain, ob, "Material", &idx);
  WM_main_add_notifier(NC_OBJECT | ND_OB_SHADING, &ob->id);
  WM_main_add_notifier(NC_MATERIAL | ND_SHADING_LINKS, NULL);
  DEG_relations_tag_update(bmain);

  BLI_assert(ma_new != NULL);

  MaterialGPencilStyle *gp_style_new = ma_new->gp_style;
  BLI_assert(gp_style_new != NULL);

  /* Only create Stroke (default option). */
  if (only_stroke) {
    /* Stroke color. */
    gp_style_new->flag |= GP_MATERIAL_STROKE_SHOW;
    gp_style_new->flag &= ~GP_MATERIAL_FILL_SHOW;
    copy_v3_v3(gp_style_new->stroke_rgba, col_conv);
    zero_v4(gp_style_new->fill_rgba);
  }
  /* Fill Only. */
  else if (only_fill) {
    /* Fill color. */
    gp_style_new->flag &= ~GP_MATERIAL_STROKE_SHOW;
    gp_style_new->flag |= GP_MATERIAL_FILL_SHOW;
    zero_v4(gp_style_new->stroke_rgba);
    copy_v3_v3(gp_style_new->fill_rgba, col_conv);
  }
  /* Stroke and Fill. */
  else if (both) {
    gp_style_new->flag |= GP_MATERIAL_STROKE_SHOW | GP_MATERIAL_FILL_SHOW;
    copy_v3_v3(gp_style_new->stroke_rgba, col_conv);
    copy_v3_v3(gp_style_new->fill_rgba, col_conv);
  }
  /* Push undo for new created material. */
  ED_undo_push(C, "Add Grease Pencil Material");
}

/* Create a new palette color and palette if needed. */
static void eyedropper_add_palette_color(bContext *C, float col_conv[4])
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = scene->toolsettings;
  GpPaint *gp_paint = ts->gp_paint;
  GpVertexPaint *gp_vertexpaint = ts->gp_vertexpaint;
  Paint *paint = &gp_paint->paint;
  Paint *vertexpaint = &gp_vertexpaint->paint;

  /* Check for Palette in Draw and Vertex Paint Mode. */
  if (paint->palette == NULL) {
    paint->palette = BKE_palette_add(bmain, "Grease Pencil");
    if (vertexpaint->palette == NULL) {
      vertexpaint->palette = paint->palette;
    }
  }
  /* Check if the color exist already. */
  Palette *palette = paint->palette;
  LISTBASE_FOREACH (PaletteColor *, palcolor, &palette->colors) {
    if (compare_v3v3(palcolor->rgb, col_conv, 0.01f)) {
      return;
    }
  }

  /* Create Colors. */
  PaletteColor *palcol = BKE_palette_color_add(palette);
  if (palcol) {
    copy_v3_v3(palcol->rgb, col_conv);
  }
}

/* Set the material or the palette color. */
static void eyedropper_gpencil_color_set(bContext *C, const wmEvent *event, EyedropperGPencil *eye)
{

  const bool only_stroke = ((!event->ctrl) && (!event->shift));
  const bool only_fill = ((!event->ctrl) && (event->shift));
  const bool both = ((event->ctrl) && (event->shift));

  float col_conv[4];

  /* Convert from linear rgb space to display space because grease pencil colors are in display
   *  space, and this conversion is needed to undo the conversion to linear performed by
   *  eyedropper_color_sample_fl. */
  if (eye->display) {
    copy_v3_v3(col_conv, eye->color);
    IMB_colormanagement_scene_linear_to_display_v3(col_conv, eye->display);
  }
  else {
    copy_v3_v3(col_conv, eye->color);
  }

  /* Add material or Palette color*/
  if (eye->mode == 0) {
    eyedropper_add_material(C, col_conv, only_stroke, only_fill, both);
  }
  else {
    eyedropper_add_palette_color(C, col_conv);
  }
}

/* Sample the color below cursor. */
static void eyedropper_gpencil_color_sample(bContext *C, EyedropperGPencil *eye, int mx, int my)
{
  eyedropper_color_sample_fl(C, mx, my, eye->color);
}

/* Cancel operator. */
static void eyedropper_gpencil_cancel(bContext *C, wmOperator *op)
{
  eyedropper_gpencil_exit(C, op);
}

/* Main modal status check. */
static int eyedropper_gpencil_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  EyedropperGPencil *eye = (EyedropperGPencil *)op->customdata;
  /* Handle modal keymap */
  switch (event->type) {
    case EVT_MODAL_MAP: {
      switch (event->val) {
        case EYE_MODAL_SAMPLE_BEGIN: {
          return OPERATOR_RUNNING_MODAL;
        }
        case EYE_MODAL_CANCEL: {
          eyedropper_gpencil_cancel(C, op);
          return OPERATOR_CANCELLED;
        }
        case EYE_MODAL_SAMPLE_CONFIRM: {
          eyedropper_gpencil_color_sample(C, eye, event->x, event->y);

          /* Create material. */
          eyedropper_gpencil_color_set(C, event, eye);
          WM_main_add_notifier(NC_GPENCIL | ND_DATA | NA_EDITED, NULL);

          eyedropper_gpencil_exit(C, op);
          return OPERATOR_FINISHED;
          break;
        }
        default: {
          break;
        }
      }
      break;
    }
    case MOUSEMOVE:
    case INBETWEEN_MOUSEMOVE: {
      eyedropper_gpencil_color_sample(C, eye, event->x, event->y);
      break;
    }
    default: {
      break;
    }
  }

  return OPERATOR_RUNNING_MODAL;
}

/* Modal Operator init */
static int eyedropper_gpencil_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
  /* Init. */
  if (eyedropper_gpencil_init(C, op)) {
    /* Add modal temp handler. */
    WM_event_add_modal_handler(C, op);
    /* Status message. */
    eyedropper_gpencil_status_indicators(C);

    return OPERATOR_RUNNING_MODAL;
  }
  else {
    return OPERATOR_PASS_THROUGH;
  }
}

/* Repeat operator */
static int eyedropper_gpencil_exec(bContext *C, wmOperator *op)
{
  /* init */
  if (eyedropper_gpencil_init(C, op)) {

    /* cleanup */
    eyedropper_gpencil_exit(C, op);

    return OPERATOR_FINISHED;
  }
  else {
    return OPERATOR_PASS_THROUGH;
  }
}

static bool eyedropper_gpencil_poll(bContext *C)
{
  /* Only valid if the current active object is grease pencil. */
  Object *obact = CTX_data_active_object(C);
  if ((obact == NULL) || (obact->type != OB_GPENCIL)) {
    return false;
  }

  /* Test we have a window below. */
  return (CTX_wm_window(C) != NULL);
}

void UI_OT_eyedropper_gpencil_color(wmOperatorType *ot)
{
  static const EnumPropertyItem items_mode[] = {
      {0, "MATERIAL", 0, "Material", ""},
      {1, "PALETTE", 0, "Palette", ""},
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Grease Pencil Eyedropper";
  ot->idname = "UI_OT_eyedropper_gpencil_color";
  ot->description = "Sample a color from the Blender Window and create Grease Pencil material";

  /* api callbacks */
  ot->invoke = eyedropper_gpencil_invoke;
  ot->modal = eyedropper_gpencil_modal;
  ot->cancel = eyedropper_gpencil_cancel;
  ot->exec = eyedropper_gpencil_exec;
  ot->poll = eyedropper_gpencil_poll;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_BLOCKING;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna, "mode", items_mode, 0, "Mode", "");
}
