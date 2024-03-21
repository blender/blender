/* SPDX-FileCopyrightText: 2015 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgpencil
 * Brush based operators for editing Grease Pencil strokes.
 */

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_math_color.h"
#include "BLI_math_vector.h"

#include "DNA_brush_types.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_material_types.h"

#include "BKE_context.hh"
#include "BKE_material.h"
#include "BKE_paint.hh"
#include "BKE_report.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "ED_gpencil_legacy.hh"
#include "ED_screen.hh"

#include "DEG_depsgraph.hh"

#include "gpencil_intern.h"

static const EnumPropertyItem gpencil_modesEnumPropertyItem_mode[] = {
    {GPPAINT_MODE_STROKE, "STROKE", 0, "Stroke", ""},
    {GPPAINT_MODE_FILL, "FILL", 0, "Fill", ""},
    {GPPAINT_MODE_BOTH, "BOTH", 0, "Stroke & Fill", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

/* Helper: Check if any stroke is selected. */
static bool is_any_stroke_selected(bContext *C, const bool is_multiedit, const bool is_curve_edit)
{
  bool is_selected = false;

  /* If not enabled any mask mode, the strokes are considered as not selected. */
  ToolSettings *ts = CTX_data_tool_settings(C);
  if (!GPENCIL_ANY_VERTEX_MASK(ts->gpencil_selectmode_vertex)) {
    return false;
  }

  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    bGPDframe *init_gpf = static_cast<bGPDframe *>((is_multiedit) ? gpl->frames.first :
                                                                    gpl->actframe);
    for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
      if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {
        if (gpf == nullptr) {
          continue;
        }
        LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
          /* skip strokes that are invalid for current view */
          if (ED_gpencil_stroke_can_use(C, gps) == false) {
            continue;
          }

          if (is_curve_edit) {
            if (gps->editcurve == nullptr) {
              continue;
            }
            bGPDcurve *gpc = gps->editcurve;
            if (gpc->flag & GP_CURVE_SELECT) {
              is_selected = true;
              break;
            }
          }
          else {
            if (gps->flag & GP_STROKE_SELECT) {
              is_selected = true;
              break;
            }
          }
        }
        /* If not multi-edit, exit loop. */
        if (!is_multiedit) {
          break;
        }
      }
    }
  }
  CTX_DATA_END;

  return is_selected;
}

/* Poll callback for stroke vertex paint operator. */
static bool gpencil_vertexpaint_mode_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  if ((ob == nullptr) || (ob->type != OB_GPENCIL_LEGACY)) {
    return false;
  }

  bGPdata *gpd = (bGPdata *)ob->data;
  if (GPENCIL_VERTEX_MODE(gpd)) {
    /* Any data to use. */
    if (gpd->layers.first) {
      return true;
    }
  }

  return false;
}

static int gpencil_vertexpaint_brightness_contrast_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  bGPdata *gpd = (bGPdata *)ob->data;
  const bool is_multiedit = bool(GPENCIL_MULTIEDIT_SESSIONS_ON(gpd));
  const eGp_Vertex_Mode mode = eGp_Vertex_Mode(RNA_enum_get(op->ptr, "mode"));
  const bool any_selected = is_any_stroke_selected(C, is_multiedit, false);

  float gain, offset;
  {
    float brightness = RNA_float_get(op->ptr, "brightness");
    float contrast = RNA_float_get(op->ptr, "contrast");
    brightness /= 100.0f;
    float delta = contrast / 200.0f;
    /*
     * The algorithm is by Werner D. Streidt
     * (http://visca.com/ffactory/archives/5-99/msg00021.html)
     * Extracted of OpenCV `demhist.c`.
     */
    if (contrast > 0) {
      gain = 1.0f - delta * 2.0f;
      gain = 1.0f / max_ff(gain, FLT_EPSILON);
      offset = gain * (brightness - delta);
    }
    else {
      delta *= -1;
      gain = max_ff(1.0f - delta * 2.0f, 0.0f);
      offset = gain * brightness + delta;
    }
  }

  /* Loop all selected strokes. */
  bool changed = false;
  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    bGPDframe *init_gpf = static_cast<bGPDframe *>((is_multiedit) ? gpl->frames.first :
                                                                    gpl->actframe);

    for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
      if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {
        if (gpf == nullptr) {
          continue;
        }

        LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
          /* skip strokes that are invalid for current view */
          if (ED_gpencil_stroke_can_use(C, gps) == false) {
            continue;
          }

          if ((!any_selected) || (gps->flag & GP_STROKE_SELECT)) {
            /* Fill color. */
            if (mode != GPPAINT_MODE_STROKE) {
              if (gps->vert_color_fill[3] > 0.0f) {
                changed = true;
                for (int i2 = 0; i2 < 3; i2++) {
                  gps->vert_color_fill[i2] = gain * gps->vert_color_fill[i2] + offset;
                }
              }
            }
            /* Stroke points. */
            if (mode != GPPAINT_MODE_FILL) {
              changed = true;
              int i;
              bGPDspoint *pt;
              for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
                if (((!any_selected) || (pt->flag & GP_SPOINT_SELECT)) &&
                    (pt->vert_color[3] > 0.0f))
                {
                  for (int i2 = 0; i2 < 3; i2++) {
                    pt->vert_color[i2] = gain * pt->vert_color[i2] + offset;
                  }
                }
              }
            }
          }
        }
        /* If not multi-edit, exit loop. */
        if (!is_multiedit) {
          break;
        }
      }
    }
  }
  CTX_DATA_END;

  /* notifiers */
  if (changed) {
    DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
  }

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_vertex_color_brightness_contrast(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Vertex Paint Brightness/Contrast";
  ot->idname = "GPENCIL_OT_vertex_color_brightness_contrast";
  ot->description = "Adjust vertex color brightness/contrast";

  /* api callbacks */
  ot->exec = gpencil_vertexpaint_brightness_contrast_exec;
  ot->poll = gpencil_vertexpaint_mode_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* params */
  ot->prop = RNA_def_enum(
      ot->srna, "mode", gpencil_modesEnumPropertyItem_mode, GPPAINT_MODE_BOTH, "Mode", "");
  const float min = -100, max = +100;
  prop = RNA_def_float(ot->srna, "brightness", 0.0f, min, max, "Brightness", "", min, max);
  prop = RNA_def_float(ot->srna, "contrast", 0.0f, min, max, "Contrast", "", min, max);
  RNA_def_property_ui_range(prop, min, max, 1, 1);
}

static int gpencil_vertexpaint_hsv_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  bGPdata *gpd = (bGPdata *)ob->data;

  const bool is_multiedit = bool(GPENCIL_MULTIEDIT_SESSIONS_ON(gpd));
  const eGp_Vertex_Mode mode = eGp_Vertex_Mode(RNA_enum_get(op->ptr, "mode"));
  const bool any_selected = is_any_stroke_selected(C, is_multiedit, false);
  float hue = RNA_float_get(op->ptr, "h");
  float sat = RNA_float_get(op->ptr, "s");
  float val = RNA_float_get(op->ptr, "v");

  bool changed = false;
  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    bGPDframe *init_gpf = static_cast<bGPDframe *>((is_multiedit) ? gpl->frames.first :
                                                                    gpl->actframe);

    for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
      if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {
        if (gpf == nullptr) {
          continue;
        }

        LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
          /* skip strokes that are invalid for current view */
          if (ED_gpencil_stroke_can_use(C, gps) == false) {
            continue;
          }

          if ((!any_selected) || (gps->flag & GP_STROKE_SELECT)) {
            float hsv[3];

            /* Fill color. */
            if (mode != GPPAINT_MODE_STROKE) {
              if (gps->vert_color_fill[3] > 0.0f) {
                changed = true;

                rgb_to_hsv_v(gps->vert_color_fill, hsv);

                hsv[0] += (hue - 0.5f);
                if (hsv[0] > 1.0f) {
                  hsv[0] -= 1.0f;
                }
                else if (hsv[0] < 0.0f) {
                  hsv[0] += 1.0f;
                }
                hsv[1] *= sat;
                hsv[2] *= val;

                hsv_to_rgb_v(hsv, gps->vert_color_fill);
              }
            }

            /* Stroke points. */
            if (mode != GPPAINT_MODE_FILL) {
              changed = true;
              int i;
              bGPDspoint *pt;
              for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
                if (((!any_selected) || (pt->flag & GP_SPOINT_SELECT)) &&
                    (pt->vert_color[3] > 0.0f))
                {
                  rgb_to_hsv_v(pt->vert_color, hsv);

                  hsv[0] += (hue - 0.5f);
                  if (hsv[0] > 1.0f) {
                    hsv[0] -= 1.0f;
                  }
                  else if (hsv[0] < 0.0f) {
                    hsv[0] += 1.0f;
                  }
                  hsv[1] *= sat;
                  hsv[2] *= val;

                  hsv_to_rgb_v(hsv, pt->vert_color);
                }
              }
            }
          }
        }
        /* If not multi-edit, exit loop. */
        if (!is_multiedit) {
          break;
        }
      }
    }
  }

  CTX_DATA_END;

  /* notifiers */
  if (changed) {
    DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
  }

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_vertex_color_hsv(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Vertex Paint Hue/Saturation/Value";
  ot->idname = "GPENCIL_OT_vertex_color_hsv";
  ot->description = "Adjust vertex color HSV values";

  /* api callbacks */
  ot->exec = gpencil_vertexpaint_hsv_exec;
  ot->poll = gpencil_vertexpaint_mode_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* params */
  ot->prop = RNA_def_enum(
      ot->srna, "mode", gpencil_modesEnumPropertyItem_mode, GPPAINT_MODE_BOTH, "Mode", "");
  RNA_def_float(ot->srna, "h", 0.5f, 0.0f, 1.0f, "Hue", "", 0.0f, 1.0f);
  RNA_def_float(ot->srna, "s", 1.0f, 0.0f, 2.0f, "Saturation", "", 0.0f, 2.0f);
  RNA_def_float(ot->srna, "v", 1.0f, 0.0f, 2.0f, "Value", "", 0.0f, 2.0f);
}

static int gpencil_vertexpaint_invert_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  bGPdata *gpd = (bGPdata *)ob->data;

  const bool is_multiedit = bool(GPENCIL_MULTIEDIT_SESSIONS_ON(gpd));
  const eGp_Vertex_Mode mode = eGp_Vertex_Mode(RNA_enum_get(op->ptr, "mode"));
  const bool any_selected = is_any_stroke_selected(C, is_multiedit, false);

  bool changed = false;
  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    bGPDframe *init_gpf = static_cast<bGPDframe *>((is_multiedit) ? gpl->frames.first :
                                                                    gpl->actframe);

    for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
      if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {
        if (gpf == nullptr) {
          continue;
        }

        LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
          /* skip strokes that are invalid for current view */
          if (ED_gpencil_stroke_can_use(C, gps) == false) {
            continue;
          }

          if ((!any_selected) || (gps->flag & GP_STROKE_SELECT)) {
            /* Fill color. */
            if (mode != GPPAINT_MODE_STROKE) {
              if (gps->vert_color_fill[3] > 0.0f) {
                changed = true;
                for (int i2 = 0; i2 < 3; i2++) {
                  gps->vert_color_fill[i2] = 1.0f - gps->vert_color_fill[i2];
                }
              }
            }

            /* Stroke points. */
            if (mode != GPPAINT_MODE_FILL) {
              changed = true;
              int i;
              bGPDspoint *pt;

              for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
                if (((!any_selected) || (pt->flag & GP_SPOINT_SELECT)) &&
                    (pt->vert_color[3] > 0.0f))
                {
                  for (int i2 = 0; i2 < 3; i2++) {
                    pt->vert_color[i2] = 1.0f - pt->vert_color[i2];
                  }
                }
              }
            }
          }
        }
        /* If not multi-edit, exit loop. */
        if (!is_multiedit) {
          break;
        }
      }
    }
  }
  CTX_DATA_END;

  /* notifiers */
  if (changed) {
    DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
  }

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_vertex_color_invert(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Vertex Paint Invert";
  ot->idname = "GPENCIL_OT_vertex_color_invert";
  ot->description = "Invert RGB values";

  /* api callbacks */
  ot->exec = gpencil_vertexpaint_invert_exec;
  ot->poll = gpencil_vertexpaint_mode_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* params */
  ot->prop = RNA_def_enum(
      ot->srna, "mode", gpencil_modesEnumPropertyItem_mode, GPPAINT_MODE_BOTH, "Mode", "");
}

static int gpencil_vertexpaint_levels_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  bGPdata *gpd = (bGPdata *)ob->data;

  const bool is_multiedit = bool(GPENCIL_MULTIEDIT_SESSIONS_ON(gpd));
  const eGp_Vertex_Mode mode = eGp_Vertex_Mode(RNA_enum_get(op->ptr, "mode"));
  const bool any_selected = is_any_stroke_selected(C, is_multiedit, false);
  float gain = RNA_float_get(op->ptr, "gain");
  float offset = RNA_float_get(op->ptr, "offset");

  bool changed = false;
  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    bGPDframe *init_gpf = static_cast<bGPDframe *>((is_multiedit) ? gpl->frames.first :
                                                                    gpl->actframe);

    for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
      if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {
        if (gpf == nullptr) {
          continue;
        }

        LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
          /* skip strokes that are invalid for current view */
          if (ED_gpencil_stroke_can_use(C, gps) == false) {
            continue;
          }

          if ((!any_selected) || (gps->flag & GP_STROKE_SELECT)) {
            /* Fill color. */
            if (mode != GPPAINT_MODE_STROKE) {
              if (gps->vert_color_fill[3] > 0.0f) {
                changed = true;
                for (int i2 = 0; i2 < 3; i2++) {
                  gps->vert_color_fill[i2] = gain * (gps->vert_color_fill[i2] + offset);
                }
              }
            }
            /* Stroke points. */
            if (mode != GPPAINT_MODE_FILL) {
              changed = true;
              int i;
              bGPDspoint *pt;

              for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
                if (((!any_selected) || (pt->flag & GP_SPOINT_SELECT)) &&
                    (pt->vert_color[3] > 0.0f))
                {
                  for (int i2 = 0; i2 < 3; i2++) {
                    pt->vert_color[i2] = gain * (pt->vert_color[i2] + offset);
                  }
                }
              }
            }
          }
        }
        /* If not multi-edit, exit loop. */
        if (!is_multiedit) {
          break;
        }
      }
    }
  }
  CTX_DATA_END;

  /* notifiers */
  if (changed) {
    DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
  }

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_vertex_color_levels(wmOperatorType *ot)
{

  /* identifiers */
  ot->name = "Vertex Paint Levels";
  ot->idname = "GPENCIL_OT_vertex_color_levels";
  ot->description = "Adjust levels of vertex colors";

  /* api callbacks */
  ot->exec = gpencil_vertexpaint_levels_exec;
  ot->poll = gpencil_vertexpaint_mode_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* params */
  ot->prop = RNA_def_enum(
      ot->srna, "mode", gpencil_modesEnumPropertyItem_mode, GPPAINT_MODE_BOTH, "Mode", "");

  RNA_def_float(
      ot->srna, "offset", 0.0f, -1.0f, 1.0f, "Offset", "Value to add to colors", -1.0f, 1.0f);
  RNA_def_float(
      ot->srna, "gain", 1.0f, 0.0f, FLT_MAX, "Gain", "Value to multiply colors by", 0.0f, 10.0f);
}

static int gpencil_vertexpaint_set_exec(bContext *C, wmOperator *op)
{
  ToolSettings *ts = CTX_data_tool_settings(C);
  Object *ob = CTX_data_active_object(C);
  bGPdata *gpd = (bGPdata *)ob->data;
  Paint *paint = &ts->gp_vertexpaint->paint;
  Brush *brush = BKE_paint_brush(paint);

  const bool is_multiedit = bool(GPENCIL_MULTIEDIT_SESSIONS_ON(gpd));
  const eGp_Vertex_Mode mode = eGp_Vertex_Mode(RNA_enum_get(op->ptr, "mode"));
  const bool any_selected = is_any_stroke_selected(C, is_multiedit, false);
  float factor = RNA_float_get(op->ptr, "factor");

  bool changed = false;
  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    bGPDframe *init_gpf = static_cast<bGPDframe *>((is_multiedit) ? gpl->frames.first :
                                                                    gpl->actframe);

    for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
      if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {
        if (gpf == nullptr) {
          continue;
        }

        LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
          /* skip strokes that are invalid for current view */
          if (ED_gpencil_stroke_can_use(C, gps) == false) {
            continue;
          }

          if ((!any_selected) || (gps->flag & GP_STROKE_SELECT)) {
            /* Fill color. */
            if (mode != GPPAINT_MODE_STROKE) {
              changed = true;
              copy_v3_v3(gps->vert_color_fill, brush->rgb);
              gps->vert_color_fill[3] = factor;
              srgb_to_linearrgb_v4(gps->vert_color_fill, gps->vert_color_fill);
            }

            /* Stroke points. */
            if (mode != GPPAINT_MODE_FILL) {
              changed = true;
              int i;
              bGPDspoint *pt;

              float color[4];
              copy_v3_v3(color, brush->rgb);
              color[3] = factor;
              srgb_to_linearrgb_v4(color, color);
              for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
                if ((!any_selected) || (pt->flag & GP_SPOINT_SELECT)) {
                  copy_v4_v4(pt->vert_color, color);
                }
              }
            }
          }
        }
        /* If not multi-edit, exit loop. */
        if (!is_multiedit) {
          break;
        }
      }
    }
  }
  CTX_DATA_END;

  /* notifiers */
  if (changed) {
    DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
  }

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_vertex_color_set(wmOperatorType *ot)
{

  /* identifiers */
  ot->name = "Vertex Paint Set Color";
  ot->idname = "GPENCIL_OT_vertex_color_set";
  ot->description = "Set active color to all selected vertex";

  /* api callbacks */
  ot->exec = gpencil_vertexpaint_set_exec;
  ot->poll = gpencil_vertexpaint_mode_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* params */
  ot->prop = RNA_def_enum(
      ot->srna, "mode", gpencil_modesEnumPropertyItem_mode, GPPAINT_MODE_BOTH, "Mode", "");
  RNA_def_float(ot->srna, "factor", 1.0f, 0.001f, 1.0f, "Factor", "Mix Factor", 0.001f, 1.0f);
}

/* Helper to extract color from vertex color to create a palette. */
static bool gpencil_extract_palette_from_vertex(bContext *C,
                                                const bool selected,
                                                const int threshold)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = CTX_data_active_object(C);
  bool done = false;
  const float range = pow(10.0f, threshold);
  float col[3];

  GHash *color_table = BLI_ghash_int_new(__func__);

  /* Extract all colors. */
  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        if (ED_gpencil_stroke_can_use(C, gps) == false) {
          continue;
        }
        if (ED_gpencil_stroke_material_editable(ob, gpl, gps) == false) {
          continue;
        }
        MaterialGPencilStyle *gp_style = BKE_gpencil_material_settings(ob, gps->mat_nr + 1);
        if (gp_style == nullptr) {
          continue;
        }

        if ((selected) && ((gps->flag & GP_STROKE_SELECT) == 0)) {
          continue;
        }

        bool use_stroke = (gp_style->flag & GP_MATERIAL_STROKE_SHOW);
        bool use_fill = (gp_style->flag & GP_MATERIAL_FILL_SHOW);

        /* Material is disabled. */
        if ((!use_fill) && (!use_stroke)) {
          continue;
        }

        /* Only solid strokes or stencil. */
        if ((use_stroke) && ((gp_style->stroke_style == GP_MATERIAL_STROKE_STYLE_TEXTURE) &&
                             ((gp_style->flag & GP_MATERIAL_STROKE_PATTERN) == 0)))
        {
          continue;
        }

        /* Only solid fill. */
        if ((use_fill) && (gp_style->fill_style != GP_MATERIAL_FILL_STYLE_SOLID)) {
          continue;
        }

        /* Fill color. */
        if (gps->vert_color_fill[3] > 0.0f) {
          col[0] = truncf(gps->vert_color_fill[0] * range) / range;
          col[1] = truncf(gps->vert_color_fill[1] * range) / range;
          col[2] = truncf(gps->vert_color_fill[2] * range) / range;

          uint key = rgb_to_cpack(col[0], col[1], col[2]);

          if (!BLI_ghash_haskey(color_table, POINTER_FROM_INT(key))) {
            BLI_ghash_insert(color_table, POINTER_FROM_INT(key), POINTER_FROM_INT(key));
          }
        }

        /* Read all points to get all colors. */
        bGPDspoint *pt;
        int i;
        for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
          col[0] = truncf(pt->vert_color[0] * range) / range;
          col[1] = truncf(pt->vert_color[1] * range) / range;
          col[2] = truncf(pt->vert_color[2] * range) / range;

          uint key = rgb_to_cpack(col[0], col[1], col[2]);
          if (!BLI_ghash_haskey(color_table, POINTER_FROM_INT(key))) {
            BLI_ghash_insert(color_table, POINTER_FROM_INT(key), POINTER_FROM_INT(key));
          }
        }
      }
    }
  }
  CTX_DATA_END;

  /* Create the Palette. */
  done = BKE_palette_from_hash(bmain, color_table, ob->id.name + 2, true);

  /* Free memory. */
  BLI_ghash_free(color_table, nullptr, nullptr);

  return done;
}

/* Convert Materials to Vertex Color. */
struct GPMatArray {
  uint key;
  Material *ma;
  int index;
};

static uint get_material_type(MaterialGPencilStyle *gp_style,
                              bool use_stroke,
                              bool use_fill,
                              char *name,
                              size_t name_maxncpy)
{
  uint i = 0;
  if ((use_stroke) && (use_fill)) {
    switch (gp_style->mode) {
      case GP_MATERIAL_MODE_LINE: {
        i = 1;
        BLI_strncpy(name, "Line Stroke-Fill", name_maxncpy);
        break;
      }
      case GP_MATERIAL_MODE_DOT: {
        i = 2;
        BLI_strncpy(name, "Dots Stroke-Fill", name_maxncpy);
        break;
      }
      case GP_MATERIAL_MODE_SQUARE: {
        i = 3;
        BLI_strncpy(name, "Squares Stroke-Fill", name_maxncpy);
        break;
      }
      default:
        break;
    }
  }
  else if (use_stroke) {
    switch (gp_style->mode) {
      case GP_MATERIAL_MODE_LINE: {
        i = 4;
        BLI_strncpy(name, "Line Stroke", name_maxncpy);
        break;
      }
      case GP_MATERIAL_MODE_DOT: {
        i = 5;
        BLI_strncpy(name, "Dots Stroke", name_maxncpy);
        break;
      }
      case GP_MATERIAL_MODE_SQUARE: {
        i = 6;
        BLI_strncpy(name, "Squares Stroke", name_maxncpy);
        break;
      }
      default:
        break;
    }
  }
  else {
    i = 7;
    BLI_strncpy(name, "Solid Fill", name_maxncpy);
  }

  /* Create key TSSSSFFFF (T: Type S: Stroke Alpha F: Fill Alpha) */
  i *= 1e8;
  if (use_stroke) {
    i += gp_style->stroke_rgba[3] * 1e7;
  }
  if (use_fill) {
    i += gp_style->fill_rgba[3] * 1e3;
  }

  return i;
}

static bool gpencil_material_to_vertex_poll(bContext *C)
{
  /* only supported with grease pencil objects */
  Object *ob = CTX_data_active_object(C);
  if ((ob == nullptr) || (ob->type != OB_GPENCIL_LEGACY)) {
    return false;
  }

  return true;
}

static int gpencil_material_to_vertex_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = CTX_data_active_object(C);
  bGPdata *gpd = (bGPdata *)ob->data;
  const bool remove = RNA_boolean_get(op->ptr, "remove");
  const bool palette = RNA_boolean_get(op->ptr, "palette");
  const bool selected = RNA_boolean_get(op->ptr, "selected");

  char name[32] = "";
  Material *ma = nullptr;
  GPMatArray *mat_elm = nullptr;

  bool changed = false;

  short *totcol = BKE_object_material_len_p(ob);
  if (totcol == nullptr) {
    return OPERATOR_CANCELLED;
  }

  /* These arrays hold all materials and index in the material slots for all combinations. */
  int totmat = *totcol;
  GPMatArray *mat_table = static_cast<GPMatArray *>(
      MEM_calloc_arrayN(totmat, sizeof(GPMatArray), __func__));

  /* Update stroke material index. */
  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        if (ED_gpencil_stroke_can_use(C, gps) == false) {
          continue;
        }
        if (ED_gpencil_stroke_material_editable(ob, gpl, gps) == false) {
          continue;
        }

        if ((selected) && ((gps->flag & GP_STROKE_SELECT) == 0)) {
          continue;
        }

        MaterialGPencilStyle *gp_style = BKE_gpencil_material_settings(ob, gps->mat_nr + 1);
        if (gp_style == nullptr) {
          continue;
        }

        bool use_stroke = ((gp_style->flag & GP_MATERIAL_STROKE_SHOW) &&
                           (gp_style->stroke_rgba[3] > 0.0f));
        bool use_fill = ((gp_style->flag & GP_MATERIAL_FILL_SHOW) &&
                         (gp_style->fill_rgba[3] > 0.0f));
        bool is_stencil = ((gp_style->stroke_style == GP_MATERIAL_STROKE_STYLE_TEXTURE) &&
                           (gp_style->flag & GP_MATERIAL_STROKE_PATTERN));
        /* Material is disabled. */
        if ((!use_fill) && (!use_stroke)) {
          continue;
        }

        /* Only solid strokes or stencil. */
        if ((use_stroke) && ((gp_style->stroke_style == GP_MATERIAL_STROKE_STYLE_TEXTURE) &&
                             ((gp_style->flag & GP_MATERIAL_STROKE_PATTERN) == 0)))
        {
          continue;
        }

        /* Only solid fill. */
        if ((use_fill) && (gp_style->fill_style != GP_MATERIAL_FILL_STYLE_SOLID)) {
          continue;
        }

        /* Only for no Stencil materials. */
        if (!is_stencil) {
          /* Create material type unique key by type and alpha. */
          uint key = get_material_type(gp_style, use_stroke, use_fill, name, sizeof(name));

          /* Check if material exist. */
          bool found = false;
          int i;
          for (i = 0; i < totmat; i++) {
            mat_elm = &mat_table[i];
            if (mat_elm->ma == nullptr) {
              break;
            }
            if (key == mat_elm->key) {
              found = true;
              break;
            }
          }

          /* If not found create a new material. */
          if (!found) {
            ma = BKE_gpencil_material_add(bmain, name);
            if (use_stroke) {
              ma->gp_style->flag |= GP_MATERIAL_STROKE_SHOW;
            }
            else {
              ma->gp_style->flag &= ~GP_MATERIAL_STROKE_SHOW;
            }

            if (use_fill) {
              ma->gp_style->flag |= GP_MATERIAL_FILL_SHOW;
            }
            else {
              ma->gp_style->flag &= ~GP_MATERIAL_FILL_SHOW;
            }

            ma->gp_style->stroke_rgba[3] = gp_style->stroke_rgba[3];
            ma->gp_style->fill_rgba[3] = gp_style->fill_rgba[3];

            BKE_object_material_slot_add(bmain, ob);
            BKE_object_material_assign(bmain, ob, ma, ob->totcol, BKE_MAT_ASSIGN_USERPREF);

            mat_elm->key = key;
            mat_elm->ma = ma;
            mat_elm->index = ob->totcol - 1;
          }
          else {
            mat_elm = &mat_table[i];
          }

          /* Update stroke */
          gps->mat_nr = mat_elm->index;
        }

        changed = true;
        copy_v3_v3(gps->vert_color_fill, gp_style->fill_rgba);
        gps->vert_color_fill[3] = 1.0f;

        /* Update all points. */
        bGPDspoint *pt;
        int i;
        for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
          copy_v3_v3(pt->vert_color, gp_style->stroke_rgba);
          pt->vert_color[3] = 1.0f;
        }
      }
    }
  }
  CTX_DATA_END;

  /* notifiers */
  if (changed) {
    DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
  }

  /* Free memory. */
  MEM_SAFE_FREE(mat_table);

  /* Generate a Palette. */
  if (palette) {
    gpencil_extract_palette_from_vertex(C, selected, 1);
  }

  /* Clean unused materials. */
  if (remove) {
    WM_operator_name_call(
        C, "OBJECT_OT_material_slot_remove_unused", WM_OP_INVOKE_REGION_WIN, nullptr, nullptr);
  }

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_material_to_vertex_color(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Convert Stroke Materials to Vertex Color";
  ot->idname = "GPENCIL_OT_material_to_vertex_color";
  ot->description = "Replace materials in strokes with Vertex Color";

  /* api callbacks */
  ot->exec = gpencil_material_to_vertex_exec;
  ot->poll = gpencil_material_to_vertex_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_boolean(ot->srna,
                             "remove",
                             true,
                             "Remove Unused Materials",
                             "Remove any unused material after the conversion");
  RNA_def_boolean(ot->srna, "palette", true, "Create Palette", "Create a new palette with colors");
  RNA_def_boolean(ot->srna, "selected", false, "Only Selected", "Convert only selected strokes");
  RNA_def_int(ot->srna, "threshold", 3, 1, 4, "Threshold", "", 1, 4);
}

/* Extract Palette from Vertex Color. */
static bool gpencil_extract_palette_vertex_poll(bContext *C)
{
  /* only supported with grease pencil objects */
  Object *ob = CTX_data_active_object(C);
  if ((ob == nullptr) || (ob->type != OB_GPENCIL_LEGACY)) {
    return false;
  }

  return true;
}

static int gpencil_extract_palette_vertex_exec(bContext *C, wmOperator *op)
{
  const bool selected = RNA_boolean_get(op->ptr, "selected");
  const int threshold = RNA_int_get(op->ptr, "threshold");

  if (gpencil_extract_palette_from_vertex(C, selected, threshold)) {
    BKE_reportf(op->reports, RPT_INFO, "Palette created");
  }
  else {
    BKE_reportf(op->reports, RPT_ERROR, "Unable to find Vertex Information to create palette");
  }

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_extract_palette_vertex(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Extract Palette from Vertex Color";
  ot->idname = "GPENCIL_OT_extract_palette_vertex";
  ot->description = "Extract all colors used in Grease Pencil Vertex and create a Palette";

  /* api callbacks */
  ot->exec = gpencil_extract_palette_vertex_exec;
  ot->poll = gpencil_extract_palette_vertex_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_boolean(
      ot->srna, "selected", false, "Only Selected", "Convert only selected strokes");
  RNA_def_int(ot->srna, "threshold", 1, 1, 4, "Threshold", "", 1, 4);
}

/* -------------------------------------------------------------------- */
/** \name Reset Stroke Vertex Color Operator
 * \{ */

static void gpencil_reset_vertex(bGPDstroke *gps, eGp_Vertex_Mode mode)
{
  if (mode != GPPAINT_MODE_STROKE) {
    zero_v4(gps->vert_color_fill);
  }

  if (mode != GPPAINT_MODE_FILL) {
    bGPDspoint *pt;
    for (int i = 0; i < gps->totpoints; i++) {
      pt = &gps->points[i];
      zero_v4(pt->vert_color);
    }
  }
}

static int gpencil_stroke_reset_vertex_color_exec(bContext *C, wmOperator *op)
{
  Object *obact = CTX_data_active_object(C);
  bGPdata *gpd = (bGPdata *)obact->data;
  const bool is_curve_edit = bool(GPENCIL_CURVE_EDIT_SESSIONS_ON(gpd));
  const bool is_multiedit = bool(GPENCIL_MULTIEDIT_SESSIONS_ON(gpd));
  const eGp_Vertex_Mode mode = eGp_Vertex_Mode(RNA_enum_get(op->ptr, "mode"));

  /* First need to check if there are something selected. If not, apply to all strokes. */
  const bool any_selected = is_any_stroke_selected(C, is_multiedit, is_curve_edit);

  /* Reset Vertex colors. */
  bool changed = false;
  CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) {
    bGPDframe *init_gpf = static_cast<bGPDframe *>((is_multiedit) ? gpl->frames.first :
                                                                    gpl->actframe);

    for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {
      if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {
        if (gpf == nullptr) {
          continue;
        }

        LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
          /* skip strokes that are invalid for current view */
          if (ED_gpencil_stroke_can_use(C, gps) == false) {
            continue;
          }

          if (is_curve_edit) {
            if (gps->editcurve == nullptr) {
              continue;
            }
            bGPDcurve *gpc = gps->editcurve;
            if ((!any_selected) || (gpc->flag & GP_CURVE_SELECT)) {
              gpencil_reset_vertex(gps, mode);
            }
          }
          else {
            if ((!any_selected) || (gps->flag & GP_STROKE_SELECT)) {
              gpencil_reset_vertex(gps, mode);
            }
          }

          changed = true;
        }
        /* If not multi-edit, exit loop. */
        if (!is_multiedit) {
          break;
        }
      }
    }
  }
  CTX_DATA_END;

  if (changed) {
    /* updates */
    DEG_id_tag_update(&gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_SYNC_TO_EVAL);
    DEG_id_tag_update(&obact->id, ID_RECALC_SYNC_TO_EVAL);
    WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);
  }

  return OPERATOR_FINISHED;
}

void GPENCIL_OT_stroke_reset_vertex_color(wmOperatorType *ot)
{
  static EnumPropertyItem mode_types_items[] = {
      {GPPAINT_MODE_STROKE, "STROKE", 0, "Stroke", "Reset Vertex Color to Stroke only"},
      {GPPAINT_MODE_FILL, "FILL", 0, "Fill", "Reset Vertex Color to Fill only"},
      {GPPAINT_MODE_BOTH, "BOTH", 0, "Stroke & Fill", "Reset Vertex Color to Stroke and Fill"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Reset Vertex Color";
  ot->idname = "GPENCIL_OT_stroke_reset_vertex_color";
  ot->description = "Reset vertex color for all or selected strokes";

  /* callbacks */
  ot->exec = gpencil_stroke_reset_vertex_color_exec;
  ot->poll = gpencil_vertexpaint_mode_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_enum(ot->srna, "mode", mode_types_items, GPPAINT_MODE_BOTH, "Mode", "");
}

/** \} */
