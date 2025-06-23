/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * Eyedropper (RGB Color)
 *
 * Defines:
 * - #UI_OT_eyedropper_grease_pencil_color
 */

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math_vector_types.hh"

#include "BLT_translation.hh"

#include "DNA_brush_types.h"
#include "DNA_material_types.h"

#include "BKE_brush.hh"
#include "BKE_context.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_lib_id.hh"
#include "BKE_material.hh"
#include "BKE_paint.hh"

#include "IMB_colormanagement.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "ED_screen.hh"
#include "ED_undo.hh"

#include "DEG_depsgraph_build.hh"

#include "eyedropper_intern.hh"
#include "interface_intern.hh"

namespace blender::ui::greasepencil {

enum class EyeMode : int8_t {
  Material = 0,
  Palette = 1,
  Brush = 2,
};

enum class MaterialMode : int8_t {
  Stroke = 0,
  Fill = 1,
  Both = 2,
};

struct EyedropperGreasePencil {
  const ColorManagedDisplay *display = nullptr;

  bool accum_start = false; /* has mouse been pressed */
  float3 accum_col = {};
  int accum_tot = 0;
  float3 color = {};

  /** Mode */
  EyeMode mode = EyeMode::Material;
  /** Material Mode */
  MaterialMode mat_mode = MaterialMode::Stroke;
};

/* Helper: Draw status message while the user is running the operator */
static void eyedropper_grease_pencil_status_indicators(bContext *C,
                                                       wmOperator *op,
                                                       const wmEvent *event)
{
  std::string header;
  header += IFACE_("Current: ");

  const bool is_ctrl = (event->modifier & KM_CTRL) != 0;
  const bool is_shift = (event->modifier & KM_SHIFT) != 0;

  EyedropperGreasePencil *eye = static_cast<EyedropperGreasePencil *>(op->customdata);

  MaterialMode mat_mode = eye->mat_mode;
  if (is_ctrl && !is_shift) {
    mat_mode = MaterialMode::Stroke;
  }
  if (is_shift && !is_ctrl) {
    mat_mode = MaterialMode::Fill;
  }
  if (is_ctrl && is_shift) {
    mat_mode = MaterialMode::Both;
  }

  switch (mat_mode) {
    case MaterialMode::Stroke: {
      header += IFACE_("Stroke");
      break;
    }
    case MaterialMode::Fill: {
      header += IFACE_("Fill");
      break;
    }
    case MaterialMode::Both: {
      header += IFACE_("Both");
      break;
    }
  }

  header += IFACE_(", Ctrl: Stroke, Shift: Fill, Shift+Ctrl: Both");

  ED_workspace_status_text(C, header.c_str());
}

static bool eyedropper_grease_pencil_init(bContext *C, wmOperator *op)
{
  EyedropperGreasePencil *eye = MEM_new<EyedropperGreasePencil>(__func__);

  op->customdata = eye;
  Scene *scene = CTX_data_scene(C);

  const char *display_device;
  display_device = scene->display_settings.display_device;
  eye->display = IMB_colormanagement_display_get_named(display_device);

  eye->accum_start = true;
  eye->mode = EyeMode(RNA_enum_get(op->ptr, "mode"));
  eye->mat_mode = MaterialMode(RNA_enum_get(op->ptr, "material_mode"));
  return true;
}

static void eyedropper_grease_pencil_exit(bContext *C, wmOperator *op)
{
  /* Clear status message area. */
  ED_workspace_status_text(C, nullptr);

  EyedropperGreasePencil *eye = static_cast<EyedropperGreasePencil *>(op->customdata);

  MEM_delete<EyedropperGreasePencil>(eye);
  /* Clear pointer. */
  op->customdata = nullptr;
}

static void eyedropper_add_material(bContext *C,
                                    const float3 col_conv,
                                    const MaterialMode mat_mode)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = CTX_data_active_object(C);
  Material *ma = nullptr;

  bool found = false;

  /* Look for a similar material in grease pencil slots. */
  short *totcol = BKE_object_material_len_p(ob);
  for (short i = 0; i < *totcol; i++) {
    ma = BKE_object_material_get(ob, i + 1);
    if (ma == nullptr) {
      continue;
    }

    MaterialGPencilStyle *gp_style = ma->gp_style;
    if (gp_style != nullptr) {
      /* Check stroke color. */
      bool found_stroke = compare_v3v3(gp_style->stroke_rgba, col_conv, 0.01f) &&
                          (gp_style->flag & GP_MATERIAL_STROKE_SHOW);
      /* Check fill color. */
      bool found_fill = compare_v3v3(gp_style->fill_rgba, col_conv, 0.01f) &&
                        (gp_style->flag & GP_MATERIAL_FILL_SHOW);

      if ((mat_mode == MaterialMode::Stroke) && (found_stroke) &&
          ((gp_style->flag & GP_MATERIAL_FILL_SHOW) == 0))
      {
        found = true;
      }
      else if ((mat_mode == MaterialMode::Fill) && found_fill &&
               ((gp_style->flag & GP_MATERIAL_STROKE_SHOW) == 0))
      {
        found = true;
      }
      else if ((mat_mode == MaterialMode::Both) && found_stroke && found_fill) {
        found = true;
      }

      /* Found existing material. */
      if (found) {
        ob->actcol = i + 1;
        WM_main_add_notifier(NC_MATERIAL | ND_SHADING_LINKS, nullptr);
        WM_main_add_notifier(NC_SPACE | ND_SPACE_VIEW3D, nullptr);
        return;
      }
    }
  }

  /* If material was not found add a new material with stroke and/or fill color
   * depending of the secondary key (LMB: Stroke, Shift: Fill, Shift+Ctrl: Stroke/Fill)
   */
  int idx;
  Material *ma_new = BKE_grease_pencil_object_material_new(bmain, ob, "Material", &idx);
  WM_main_add_notifier(NC_OBJECT | ND_OB_SHADING, &ob->id);
  WM_main_add_notifier(NC_MATERIAL | ND_SHADING_LINKS, nullptr);
  DEG_relations_tag_update(bmain);

  BLI_assert(ma_new != nullptr);

  MaterialGPencilStyle *gp_style_new = ma_new->gp_style;
  BLI_assert(gp_style_new != nullptr);

  /* Only create Stroke (default option). */
  if (mat_mode == MaterialMode::Stroke) {
    /* Stroke color. */
    gp_style_new->flag |= GP_MATERIAL_STROKE_SHOW;
    gp_style_new->flag &= ~GP_MATERIAL_FILL_SHOW;
    copy_v3_v3(gp_style_new->stroke_rgba, col_conv);
    zero_v4(gp_style_new->fill_rgba);
  }
  /* Fill Only. */
  else if (mat_mode == MaterialMode::Fill) {
    /* Fill color. */
    gp_style_new->flag &= ~GP_MATERIAL_STROKE_SHOW;
    gp_style_new->flag |= GP_MATERIAL_FILL_SHOW;
    zero_v4(gp_style_new->stroke_rgba);
    copy_v3_v3(gp_style_new->fill_rgba, col_conv);
  }
  /* Stroke and Fill. */
  else if (mat_mode == MaterialMode::Both) {
    gp_style_new->flag |= GP_MATERIAL_STROKE_SHOW | GP_MATERIAL_FILL_SHOW;
    copy_v3_v3(gp_style_new->stroke_rgba, col_conv);
    copy_v3_v3(gp_style_new->fill_rgba, col_conv);
  }
  /* Push undo for new created material. */
  ED_undo_push(C, "Add Grease Pencil Material");
}

/* Create a new palette color and palette if needed. */
static void eyedropper_add_palette_color(bContext *C, const float3 col_conv)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = scene->toolsettings;
  GpPaint *gp_paint = ts->gp_paint;
  GpVertexPaint *gp_vertexpaint = ts->gp_vertexpaint;
  Paint *paint = &gp_paint->paint;
  Paint *vertexpaint = &gp_vertexpaint->paint;

  /* Check for Palette in Draw and Vertex Paint Mode. */
  if (paint->palette == nullptr) {
    Palette *palette = BKE_palette_add(bmain, "Grease Pencil");
    id_us_min(&palette->id);

    BKE_paint_palette_set(paint, palette);

    if (vertexpaint->palette == nullptr) {
      BKE_paint_palette_set(vertexpaint, palette);
    }
  }

  /* Check if the color exist already. */
  Palette *palette = paint->palette;
  int i;
  LISTBASE_FOREACH_INDEX (PaletteColor *, palcolor, &palette->colors, i) {
    if (compare_v3v3(palcolor->rgb, col_conv, 0.01f)) {
      palette->active_color = i;
      return;
    }
  }

  /* Create Colors. */
  PaletteColor *palcol = BKE_palette_color_add(palette);
  if (palcol) {
    palette->active_color = BLI_listbase_count(&palette->colors) - 1;
    copy_v3_v3(palcol->rgb, col_conv);
  }
}

/* Set the active brush's color. */
static void eyedropper_set_brush_color(bContext *C, const float3 &col_conv)
{
  Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = scene->toolsettings;
  Paint *paint = &ts->gp_paint->paint;
  Brush *brush = BKE_paint_brush(paint);
  if (brush == nullptr) {
    return;
  }

  copy_v3_v3(brush->rgb, col_conv);
  BKE_brush_tag_unsaved_changes(brush);
}

/* Set the material or the palette color. */
static void eyedropper_grease_pencil_color_set(bContext *C,
                                               const wmEvent *event,
                                               EyedropperGreasePencil *eye)
{
  const bool is_ctrl = (event->modifier & KM_CTRL) != 0;
  const bool is_shift = (event->modifier & KM_SHIFT) != 0;

  MaterialMode mat_mode = eye->mat_mode;
  if (is_ctrl && !is_shift) {
    mat_mode = MaterialMode::Stroke;
  }
  if (is_shift && !is_ctrl) {
    mat_mode = MaterialMode::Fill;
  }
  if (is_ctrl && is_shift) {
    mat_mode = MaterialMode::Both;
  }

  float3 col_conv = eye->color;

  /* Convert from linear rgb space to display space because palette and brush colors are in display
   *  space, and this conversion is needed to undo the conversion to linear performed by
   *  eyedropper_color_sample_fl. */
  if (eye->display && ELEM(eye->mode, EyeMode::Palette, EyeMode::Brush)) {
    IMB_colormanagement_scene_linear_to_display_v3(col_conv, eye->display);
  }

  switch (eye->mode) {
    case EyeMode::Material:
      eyedropper_add_material(C, col_conv, mat_mode);
      break;
    case EyeMode::Palette:
      eyedropper_add_palette_color(C, col_conv);
      break;
    case EyeMode::Brush:
      eyedropper_set_brush_color(C, col_conv);
      break;
  }
}

/* Sample the color below cursor. */
static void eyedropper_grease_pencil_color_sample(bContext *C,
                                                  EyedropperGreasePencil *eye,
                                                  const int m_xy[2])
{
  /* Accumulate color. */
  float3 col;
  eyedropper_color_sample_fl(C, nullptr, m_xy, col);

  eye->accum_col += col;
  eye->accum_tot++;

  eye->color = eye->accum_col;
  if (eye->accum_tot > 1) {
    eye->color = eye->accum_col / float(eye->accum_tot);
  }
}

static void eyedropper_grease_pencil_cancel(bContext *C, wmOperator *op)
{
  eyedropper_grease_pencil_exit(C, op);
}

/* Main modal status check. */
static wmOperatorStatus eyedropper_grease_pencil_modal(bContext *C,
                                                       wmOperator *op,
                                                       const wmEvent *event)
{
  eyedropper_grease_pencil_status_indicators(C, op, event);
  EyedropperGreasePencil *eye = static_cast<EyedropperGreasePencil *>(op->customdata);

  /* Handle modal keymap */
  switch (event->type) {
    case EVT_MODAL_MAP: {
      switch (event->val) {
        case EYE_MODAL_SAMPLE_BEGIN:
          /* enable accum and make first sample */
          eye->accum_start = true;
          eyedropper_grease_pencil_color_sample(C, eye, event->xy);
          break;
        case EYE_MODAL_SAMPLE_RESET:
          eye->accum_tot = 0;
          eye->accum_col = float3(0.0f, 0.0f, 0.0f);
          eyedropper_grease_pencil_color_sample(C, eye, event->xy);
          break;
        case EYE_MODAL_CANCEL: {
          eyedropper_grease_pencil_cancel(C, op);
          return OPERATOR_CANCELLED;
        }
        case EYE_MODAL_SAMPLE_CONFIRM: {
          eyedropper_grease_pencil_color_sample(C, eye, event->xy);

          /* Create material. */
          eyedropper_grease_pencil_color_set(C, event, eye);
          WM_main_add_notifier(NC_GPENCIL | ND_DATA | NA_EDITED, nullptr);

          eyedropper_grease_pencil_exit(C, op);
          return OPERATOR_FINISHED;
        }
        default: {
          break;
        }
      }
      break;
    }
    case MOUSEMOVE:
    case INBETWEEN_MOUSEMOVE: {
      if (eye->accum_start) {
        /* button is pressed so keep sampling */
        eyedropper_grease_pencil_color_sample(C, eye, event->xy);
      }
      break;
    }
    default: {
      break;
    }
  }

  return OPERATOR_RUNNING_MODAL;
}

static wmOperatorStatus eyedropper_grease_pencil_invoke(bContext *C,
                                                        wmOperator *op,
                                                        const wmEvent *event)
{
  if (eyedropper_grease_pencil_init(C, op)) {
    /* Add modal temp handler. */
    WM_event_add_modal_handler(C, op);
    /* Status message. */
    eyedropper_grease_pencil_status_indicators(C, op, event);

    return OPERATOR_RUNNING_MODAL;
  }
  return OPERATOR_PASS_THROUGH;
}

/* Repeat operator */
static wmOperatorStatus eyedropper_grease_pencil_exec(bContext *C, wmOperator *op)
{
  if (eyedropper_grease_pencil_init(C, op)) {

    /* cleanup */
    eyedropper_grease_pencil_exit(C, op);

    return OPERATOR_FINISHED;
  }
  return OPERATOR_PASS_THROUGH;
}

static bool eyedropper_grease_pencil_poll(bContext *C)
{
  /* Only valid if the current active object is grease pencil. */
  Object *obact = CTX_data_active_object(C);
  if ((obact == nullptr) || (obact->type != OB_GREASE_PENCIL)) {
    return false;
  }

  /* Test we have a window below. */
  return (CTX_wm_window(C) != nullptr);
}

}  // namespace blender::ui::greasepencil

void UI_OT_eyedropper_grease_pencil_color(wmOperatorType *ot)
{
  using namespace blender::ui::greasepencil;
  static const EnumPropertyItem items_mode[] = {
      {int(EyeMode::Material), "MATERIAL", 0, "Material", ""},
      {int(EyeMode::Palette), "PALETTE", 0, "Palette", ""},
      {int(EyeMode::Brush), "BRUSH", 0, "Brush", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem items_material_mode[] = {
      {int(MaterialMode::Stroke), "STROKE", 0, "Stroke", ""},
      {int(MaterialMode::Fill), "FILL", 0, "Fill", ""},
      {int(MaterialMode::Both), "BOTH", 0, "Both", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* Identifiers. */
  ot->name = "Grease Pencil Eyedropper";
  ot->idname = "UI_OT_eyedropper_grease_pencil_color";
  ot->description = "Sample a color from the Blender Window and create Grease Pencil material";

  /* API callbacks. */
  ot->invoke = eyedropper_grease_pencil_invoke;
  ot->modal = eyedropper_grease_pencil_modal;
  ot->cancel = eyedropper_grease_pencil_cancel;
  ot->exec = eyedropper_grease_pencil_exec;
  ot->poll = eyedropper_grease_pencil_poll;

  /* Flags. */
  ot->flag = OPTYPE_UNDO | OPTYPE_BLOCKING;

  /* Properties. */
  ot->prop = RNA_def_enum(ot->srna, "mode", items_mode, int(EyeMode::Material), "Mode", "");
  ot->prop = RNA_def_enum(ot->srna,
                          "material_mode",
                          items_material_mode,
                          int(MaterialMode::Stroke),
                          "Material Mode",
                          "");
}
