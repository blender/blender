/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_ghash.h"
#include "BLI_listbase.h"
#include "BLI_math_color.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "IMB_interp.hh"

#include "DNA_brush_types.h"
#include "DNA_scene_types.h"

#include "BKE_brush.hh"
#include "BKE_context.hh"
#include "BKE_image.hh"
#include "BKE_lib_id.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_paint.hh"
#include "BKE_paint_types.hh"
#include "BKE_report.hh"

#include "ED_image.hh"
#include "ED_paint.hh"
#include "ED_screen.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "IMB_colormanagement.hh"

#include "curves_sculpt_intern.hh"
#include "paint_hide.hh"
#include "paint_intern.hh"
#include "paint_mask.hh"
#include "sculpt_intern.hh"

static wmOperatorStatus brush_scale_size_exec(bContext *C, wmOperator *op)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = BKE_paint_brush(paint);
  float scalar = RNA_float_get(op->ptr, "scalar");

  /* Grease Pencil brushes in Paint mode do not use unified size. */
  const bool use_unified_size = !(brush && brush->gpencil_settings &&
                                  brush->ob_mode == OB_MODE_PAINT_GREASE_PENCIL);

  if (brush) {
    /* Pixel diameter. */
    {
      const int old_size = (use_unified_size) ? BKE_brush_size_get(paint, brush) : brush->size;
      int size = int(scalar * old_size);

      if (abs(old_size - size) < U.pixelsize) {
        if (scalar > 1) {
          size += U.pixelsize;
        }
        else if (scalar < 1) {
          size -= U.pixelsize;
        }
      }

      if (use_unified_size) {
        BKE_brush_size_set(paint, brush, size);
      }
      else {
        brush->size = max_ii(size, 1);
        BKE_brush_tag_unsaved_changes(brush);
      }
    }

    /* Unprojected diameter. */
    {
      float unprojected_size = scalar * (use_unified_size ?
                                             BKE_brush_unprojected_size_get(paint, brush) :
                                             brush->unprojected_size);

      unprojected_size = std::max(unprojected_size, 0.001f);

      if (use_unified_size) {
        BKE_brush_unprojected_size_set(paint, brush, unprojected_size);
      }
      else {
        brush->unprojected_size = unprojected_size;
        BKE_brush_tag_unsaved_changes(brush);
      }
    }

    WM_main_add_notifier(NC_BRUSH | NA_EDITED, brush);
  }

  return OPERATOR_FINISHED;
}

static void BRUSH_OT_scale_size(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Scale Sculpt/Paint Brush Size";
  ot->description = "Change brush size by a scalar";
  ot->idname = "BRUSH_OT_scale_size";

  /* API callbacks. */
  ot->exec = brush_scale_size_exec;

  /* flags */
  ot->flag = 0;

  RNA_def_float(ot->srna, "scalar", 1, 0, 2, "Scalar", "Factor to scale brush size by", 0, 2);
}

/* Palette operators */

static wmOperatorStatus palette_new_exec(bContext *C, wmOperator * /*op*/)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  Main *bmain = CTX_data_main(C);
  Palette *palette;

  palette = BKE_palette_add(bmain, "Palette");

  BKE_paint_palette_set(paint, palette);

  return OPERATOR_FINISHED;
}

static void PALETTE_OT_new(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add New Palette";
  ot->description = "Add new palette";
  ot->idname = "PALETTE_OT_new";

  /* API callbacks. */
  ot->exec = palette_new_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static bool palette_poll(bContext *C)
{
  Paint *paint = BKE_paint_get_active_from_context(C);

  if (paint && paint->palette != nullptr && ID_IS_EDITABLE(paint->palette) &&
      !ID_IS_OVERRIDE_LIBRARY(paint->palette))
  {
    return true;
  }

  return false;
}

static wmOperatorStatus palette_color_add_exec(bContext *C, wmOperator * /*op*/)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  PaintMode mode = BKE_paintmode_get_active_from_context(C);
  Palette *palette = paint->palette;
  PaletteColor *color;

  color = BKE_palette_color_add(palette);
  palette->active_color = BLI_listbase_count(&palette->colors) - 1;

  const Brush *brush = BKE_paint_brush_for_read(paint);
  if (brush) {
    if (ELEM(mode,
             PaintMode::Texture3D,
             PaintMode::Texture2D,
             PaintMode::Vertex,
             PaintMode::Sculpt,
             PaintMode::GPencil,
             PaintMode::VertexGPencil))
    {
      copy_v3_v3(color->color, BKE_brush_color_get(paint, brush));
      color->value = 0.0;
    }
    else if (mode == PaintMode::Weight) {
      zero_v3(color->color);
      color->value = brush->weight;
    }
  }

  return OPERATOR_FINISHED;
}

static void PALETTE_OT_color_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "New Palette Color";
  ot->description = "Add new color to active palette";
  ot->idname = "PALETTE_OT_color_add";

  /* API callbacks. */
  ot->exec = palette_color_add_exec;
  ot->poll = palette_poll;
  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static wmOperatorStatus palette_color_delete_exec(bContext *C, wmOperator * /*op*/)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  Palette *palette = paint->palette;
  PaletteColor *color = static_cast<PaletteColor *>(
      BLI_findlink(&palette->colors, palette->active_color));

  if (color) {
    BKE_palette_color_remove(palette, color);
  }

  return OPERATOR_FINISHED;
}

static void PALETTE_OT_color_delete(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Delete Palette Color";
  ot->description = "Remove active color from palette";
  ot->idname = "PALETTE_OT_color_delete";

  /* API callbacks. */
  ot->exec = palette_color_delete_exec;
  ot->poll = palette_poll;
  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* --- Extract Palette from Image. */
static bool palette_extract_img_poll(bContext *C)
{
  SpaceLink *sl = CTX_wm_space_data(C);
  if ((sl != nullptr) && (sl->spacetype == SPACE_IMAGE)) {
    SpaceImage *sima = CTX_wm_space_image(C);
    Image *image = sima->image;
    ImageUser iuser = sima->iuser;
    return BKE_image_has_ibuf(image, &iuser);
  }

  return false;
}

static wmOperatorStatus palette_extract_img_exec(bContext *C, wmOperator *op)
{
  const int threshold = RNA_int_get(op->ptr, "threshold");

  Main *bmain = CTX_data_main(C);
  bool done = false;

  SpaceImage *sima = CTX_wm_space_image(C);
  Image *image = sima->image;
  ImageUser iuser = sima->iuser;
  void *lock;
  ImBuf *ibuf;
  GHash *color_table = BLI_ghash_int_new(__func__);

  ibuf = BKE_image_acquire_ibuf(image, &iuser, &lock);

  if (ibuf && ibuf->byte_buffer.data) {
    /* Extract all colors. */
    const int range = int(pow(10.0f, threshold));
    for (int row = 0; row < ibuf->y; row++) {
      for (int col = 0; col < ibuf->x; col++) {
        float color[3];
        IMB_sampleImageAtLocation(ibuf, float(col), float(row), color);
        /* Convert to sRGB for hex. */
        IMB_colormanagement_scene_linear_to_srgb_v3(color, color);
        for (int i = 0; i < 3; i++) {
          color[i] = truncf(color[i] * range) / range;
        }

        uint key = rgb_to_cpack(color[0], color[1], color[2]);
        if (!BLI_ghash_haskey(color_table, POINTER_FROM_INT(key))) {
          BLI_ghash_insert(color_table, POINTER_FROM_INT(key), POINTER_FROM_INT(key));
        }
      }
    }

    done = BKE_palette_from_hash(bmain, color_table, image->id.name + 2);
  }

  /* Free memory. */
  BLI_ghash_free(color_table, nullptr, nullptr);
  BKE_image_release_ibuf(image, ibuf, lock);

  if (done) {
    BKE_reportf(op->reports, RPT_INFO, "Palette created");
  }

  return OPERATOR_FINISHED;
}

static void PALETTE_OT_extract_from_image(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Extract Palette from Image";
  ot->idname = "PALETTE_OT_extract_from_image";
  ot->description = "Extract all colors used in Image and create a Palette";

  /* API callbacks. */
  ot->exec = palette_extract_img_exec;
  ot->poll = palette_extract_img_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  prop = RNA_def_int(ot->srna, "threshold", 1, 1, 1, "Threshold", "", 1, 1);
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

/* Sort Palette color by Hue and Saturation. */
static wmOperatorStatus palette_sort_exec(bContext *C, wmOperator *op)
{
  const int type = RNA_enum_get(op->ptr, "type");

  Paint *paint = BKE_paint_get_active_from_context(C);
  Palette *palette = paint->palette;

  if (palette == nullptr) {
    return OPERATOR_CANCELLED;
  }

  tPaletteColorHSV *color_array = nullptr;
  tPaletteColorHSV *col_elm = nullptr;

  const int totcol = BLI_listbase_count(&palette->colors);

  if (totcol > 0) {
    color_array = MEM_calloc_arrayN<tPaletteColorHSV>(totcol, __func__);
    /* Put all colors in an array. */
    int t = 0;
    LISTBASE_FOREACH (PaletteColor *, color, &palette->colors) {
      float h, s, v;
      rgb_to_hsv(color->color[0], color->color[1], color->color[2], &h, &s, &v);
      col_elm = &color_array[t];
      copy_v3_v3(col_elm->rgb, color->color);
      col_elm->value = color->value;
      col_elm->h = h;
      col_elm->s = s;
      col_elm->v = v;
      t++;
    }
    /* Sort */
    if (type == 1) {
      BKE_palette_sort_hsv(color_array, totcol);
    }
    else if (type == 2) {
      BKE_palette_sort_svh(color_array, totcol);
    }
    else if (type == 3) {
      BKE_palette_sort_vhs(color_array, totcol);
    }
    else {
      BKE_palette_sort_luminance(color_array, totcol);
    }

    /* Clear old color swatches. */
    LISTBASE_FOREACH_MUTABLE (PaletteColor *, color, &palette->colors) {
      BKE_palette_color_remove(palette, color);
    }

    /* Recreate swatches sorted. */
    for (int i = 0; i < totcol; i++) {
      col_elm = &color_array[i];
      PaletteColor *palcol = BKE_palette_color_add(palette);
      if (palcol) {
        copy_v3_v3(palcol->color, col_elm->rgb);
      }
    }
  }

  /* Free memory. */
  if (totcol > 0) {
    MEM_SAFE_FREE(color_array);
  }

  WM_event_add_notifier(C, NC_BRUSH | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

static void PALETTE_OT_sort(wmOperatorType *ot)
{
  static const EnumPropertyItem sort_type[] = {
      {1, "HSV", 0, "Hue, Saturation, Value", ""},
      {2, "SVH", 0, "Saturation, Value, Hue", ""},
      {3, "VHS", 0, "Value, Hue, Saturation", ""},
      {4, "LUMINANCE", 0, "Luminance", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Sort Palette";
  ot->idname = "PALETTE_OT_sort";
  ot->description = "Sort Palette Colors";

  /* API callbacks. */
  ot->exec = palette_sort_exec;
  ot->poll = palette_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = RNA_def_enum(ot->srna, "type", sort_type, 1, "Type", "");
}

/* Move colors in palette. */
static wmOperatorStatus palette_color_move_exec(bContext *C, wmOperator *op)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  Palette *palette = paint->palette;
  PaletteColor *palcolor = static_cast<PaletteColor *>(
      BLI_findlink(&palette->colors, palette->active_color));

  if (palcolor == nullptr) {
    return OPERATOR_CANCELLED;
  }

  const int direction = RNA_enum_get(op->ptr, "type");

  BLI_assert(ELEM(direction, -1, 0, 1)); /* we use value below */
  if (BLI_listbase_link_move(&palette->colors, palcolor, direction)) {
    palette->active_color += direction;
    WM_event_add_notifier(C, NC_BRUSH | NA_EDITED, nullptr);
  }

  return OPERATOR_FINISHED;
}

static void PALETTE_OT_color_move(wmOperatorType *ot)
{
  static const EnumPropertyItem slot_move[] = {
      {-1, "UP", 0, "Up", ""},
      {1, "DOWN", 0, "Down", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Move Palette Color";
  ot->idname = "PALETTE_OT_color_move";
  ot->description = "Move the active Color up/down in the list";

  /* API callbacks. */
  ot->exec = palette_color_move_exec;
  ot->poll = palette_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = RNA_def_enum(ot->srna, "type", slot_move, 0, "Type", "");
}

/* Join Palette swatches. */
static wmOperatorStatus palette_join_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Paint *paint = BKE_paint_get_active_from_context(C);
  Palette *palette = paint->palette;
  Palette *palette_join = nullptr;
  bool done = false;

  char name[MAX_ID_NAME - 2];
  RNA_string_get(op->ptr, "palette", name);

  if ((palette == nullptr) || (name[0] == '\0')) {
    return OPERATOR_CANCELLED;
  }

  palette_join = (Palette *)BKE_libblock_find_name(bmain, ID_PAL, name);
  if (palette_join == nullptr) {
    return OPERATOR_CANCELLED;
  }

  const int totcol = BLI_listbase_count(&palette_join->colors);

  if (totcol > 0) {
    LISTBASE_FOREACH (PaletteColor *, color, &palette_join->colors) {
      PaletteColor *palcol = BKE_palette_color_add(palette);
      if (palcol) {
        copy_v3_v3(palcol->color, color->color);
        palcol->value = color->value;
        done = true;
      }
    }
  }

  if (done) {
    /* Clear old color swatches. */
    LISTBASE_FOREACH_MUTABLE (PaletteColor *, color, &palette_join->colors) {
      BKE_palette_color_remove(palette_join, color);
    }

    /* Notifier. */
    WM_event_add_notifier(C, NC_BRUSH | NA_EDITED, nullptr);
  }

  return OPERATOR_FINISHED;
}

static void PALETTE_OT_join(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Join Palette Swatches";
  ot->idname = "PALETTE_OT_join";
  ot->description = "Join Palette Swatches";

  /* API callbacks. */
  ot->exec = palette_join_exec;
  ot->poll = palette_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_string(ot->srna, "palette", nullptr, MAX_ID_NAME - 2, "Palette", "Name of the Palette");
}

/***** Stencil Control *****/

enum StencilControlMode {
  STENCIL_TRANSLATE,
  STENCIL_SCALE,
  STENCIL_ROTATE,
};

enum StencilTextureMode {
  STENCIL_PRIMARY = 0,
  STENCIL_SECONDARY = 1,
};

enum StencilConstraint {
  STENCIL_CONSTRAINT_X = 1,
  STENCIL_CONSTRAINT_Y = 2,
};

struct StencilControlData {
  float init_mouse[2];
  float init_spos[2];
  float init_sdim[2];
  float init_rot;
  float init_angle;
  float lenorig;
  float area_size[2];
  StencilControlMode mode;
  StencilConstraint constrain_mode;
  /** We are tweaking mask or color stencil. */
  int mask;
  Brush *br;
  float *dim_target;
  float *rot_target;
  float *pos_target;
  short launch_event;
};

static void stencil_set_target(StencilControlData *scd)
{
  Brush *br = scd->br;
  float mdiff[2];
  if (scd->mask) {
    copy_v2_v2(scd->init_sdim, br->mask_stencil_dimension);
    copy_v2_v2(scd->init_spos, br->mask_stencil_pos);
    scd->init_rot = br->mask_mtex.rot;

    scd->dim_target = br->mask_stencil_dimension;
    scd->rot_target = &br->mask_mtex.rot;
    scd->pos_target = br->mask_stencil_pos;

    sub_v2_v2v2(mdiff, scd->init_mouse, br->mask_stencil_pos);
  }
  else {
    copy_v2_v2(scd->init_sdim, br->stencil_dimension);
    copy_v2_v2(scd->init_spos, br->stencil_pos);
    scd->init_rot = br->mtex.rot;

    scd->dim_target = br->stencil_dimension;
    scd->rot_target = &br->mtex.rot;
    scd->pos_target = br->stencil_pos;

    sub_v2_v2v2(mdiff, scd->init_mouse, br->stencil_pos);
  }

  scd->lenorig = len_v2(mdiff);

  scd->init_angle = atan2f(mdiff[1], mdiff[0]);
}

static wmOperatorStatus stencil_control_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *br = BKE_paint_brush(paint);
  const float mvalf[2] = {float(event->mval[0]), float(event->mval[1])};
  ARegion *region = CTX_wm_region(C);
  StencilControlData *scd;
  int mask = RNA_enum_get(op->ptr, "texmode");

  if (mask) {
    if (br->mask_mtex.brush_map_mode != MTEX_MAP_MODE_STENCIL) {
      return OPERATOR_CANCELLED;
    }
  }
  else {
    if (br->mtex.brush_map_mode != MTEX_MAP_MODE_STENCIL) {
      return OPERATOR_CANCELLED;
    }
  }

  scd = MEM_mallocN<StencilControlData>(__func__);
  scd->mask = mask;
  scd->br = br;

  copy_v2_v2(scd->init_mouse, mvalf);

  stencil_set_target(scd);

  scd->mode = StencilControlMode(RNA_enum_get(op->ptr, "mode"));
  scd->launch_event = WM_userdef_event_type_from_keymap_type(event->type);
  scd->area_size[0] = region->winx;
  scd->area_size[1] = region->winy;

  op->customdata = scd;
  WM_event_add_modal_handler(C, op);

  return OPERATOR_RUNNING_MODAL;
}

static void stencil_restore(StencilControlData *scd)
{
  copy_v2_v2(scd->dim_target, scd->init_sdim);
  copy_v2_v2(scd->pos_target, scd->init_spos);
  *scd->rot_target = scd->init_rot;
}

static void stencil_control_cancel(bContext * /*C*/, wmOperator *op)
{
  StencilControlData *scd = static_cast<StencilControlData *>(op->customdata);
  stencil_restore(scd);
  MEM_freeN(scd);
}

static void stencil_control_calculate(StencilControlData *scd, const int mval[2])
{
#define PIXEL_MARGIN 5

  float mdiff[2];
  const float mvalf[2] = {float(mval[0]), float(mval[1])};
  switch (scd->mode) {
    case STENCIL_TRANSLATE:
      sub_v2_v2v2(mdiff, mvalf, scd->init_mouse);
      add_v2_v2v2(scd->pos_target, scd->init_spos, mdiff);
      CLAMP(scd->pos_target[0],
            -scd->dim_target[0] + PIXEL_MARGIN,
            scd->area_size[0] + scd->dim_target[0] - PIXEL_MARGIN);

      CLAMP(scd->pos_target[1],
            -scd->dim_target[1] + PIXEL_MARGIN,
            scd->area_size[1] + scd->dim_target[1] - PIXEL_MARGIN);
      BKE_brush_tag_unsaved_changes(scd->br);

      break;
    case STENCIL_SCALE: {
      float len, factor;
      sub_v2_v2v2(mdiff, mvalf, scd->pos_target);
      len = len_v2(mdiff);
      factor = len / scd->lenorig;
      copy_v2_v2(mdiff, scd->init_sdim);
      if (scd->constrain_mode != STENCIL_CONSTRAINT_Y) {
        mdiff[0] = factor * scd->init_sdim[0];
      }
      if (scd->constrain_mode != STENCIL_CONSTRAINT_X) {
        mdiff[1] = factor * scd->init_sdim[1];
      }
      clamp_v2(mdiff, 5.0f, 10000.0f);
      copy_v2_v2(scd->dim_target, mdiff);
      BKE_brush_tag_unsaved_changes(scd->br);
      break;
    }
    case STENCIL_ROTATE: {
      float angle;
      sub_v2_v2v2(mdiff, mvalf, scd->pos_target);
      angle = atan2f(mdiff[1], mdiff[0]);
      angle = scd->init_rot + angle - scd->init_angle;
      if (angle < 0.0f) {
        angle += float(2 * M_PI);
      }
      if (angle > float(2 * M_PI)) {
        angle -= float(2 * M_PI);
      }
      *scd->rot_target = angle;
      BKE_brush_tag_unsaved_changes(scd->br);
      break;
    }
  }
#undef PIXEL_MARGIN
}

static wmOperatorStatus stencil_control_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  StencilControlData *scd = static_cast<StencilControlData *>(op->customdata);

  if (event->type == scd->launch_event && event->val == KM_RELEASE) {
    MEM_freeN(scd);
    WM_event_add_notifier(C, NC_WINDOW, nullptr);
    return OPERATOR_FINISHED;
  }

  switch (event->type) {
    case MOUSEMOVE:
      stencil_control_calculate(scd, event->mval);
      break;
    case EVT_ESCKEY:
      if (event->val == KM_PRESS) {
        stencil_control_cancel(C, op);
        WM_event_add_notifier(C, NC_WINDOW, nullptr);
        return OPERATOR_CANCELLED;
      }
      break;
    case EVT_XKEY:
      if (event->val == KM_PRESS) {

        if (scd->constrain_mode == STENCIL_CONSTRAINT_X) {
          scd->constrain_mode = StencilConstraint(0);
        }
        else {
          scd->constrain_mode = STENCIL_CONSTRAINT_X;
        }

        stencil_control_calculate(scd, event->mval);
      }
      break;
    case EVT_YKEY:
      if (event->val == KM_PRESS) {
        if (scd->constrain_mode == STENCIL_CONSTRAINT_Y) {
          scd->constrain_mode = StencilConstraint(0);
        }
        else {
          scd->constrain_mode = STENCIL_CONSTRAINT_Y;
        }

        stencil_control_calculate(scd, event->mval);
      }
      break;
    default:
      break;
  }

  ED_region_tag_redraw(CTX_wm_region(C));

  return OPERATOR_RUNNING_MODAL;
}

static bool stencil_control_poll(bContext *C)
{
  PaintMode mode = BKE_paintmode_get_active_from_context(C);

  Paint *paint;
  Brush *br;

  if (!blender::ed::sculpt_paint::paint_supports_texture(mode)) {
    return false;
  }

  paint = BKE_paint_get_active_from_context(C);
  br = BKE_paint_brush(paint);
  return (br && (br->mtex.brush_map_mode == MTEX_MAP_MODE_STENCIL ||
                 br->mask_mtex.brush_map_mode == MTEX_MAP_MODE_STENCIL));
}

static void BRUSH_OT_stencil_control(wmOperatorType *ot)
{
  static const EnumPropertyItem stencil_control_items[] = {
      {STENCIL_TRANSLATE, "TRANSLATION", 0, "Translation", ""},
      {STENCIL_SCALE, "SCALE", 0, "Scale", ""},
      {STENCIL_ROTATE, "ROTATION", 0, "Rotation", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem stencil_texture_items[] = {
      {STENCIL_PRIMARY, "PRIMARY", 0, "Primary", ""},
      {STENCIL_SECONDARY, "SECONDARY", 0, "Secondary", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };
  /* identifiers */
  ot->name = "Stencil Brush Control";
  ot->description = "Control the stencil brush";
  ot->idname = "BRUSH_OT_stencil_control";

  /* API callbacks. */
  ot->invoke = stencil_control_invoke;
  ot->modal = stencil_control_modal;
  ot->cancel = stencil_control_cancel;
  ot->poll = stencil_control_poll;

  /* flags */
  ot->flag = 0;

  PropertyRNA *prop;
  prop = RNA_def_enum(ot->srna, "mode", stencil_control_items, STENCIL_TRANSLATE, "Tool", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  prop = RNA_def_enum(ot->srna, "texmode", stencil_texture_items, STENCIL_PRIMARY, "Tool", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

static wmOperatorStatus stencil_fit_image_aspect_exec(bContext *C, wmOperator *op)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *br = BKE_paint_brush(paint);
  bool use_scale = RNA_boolean_get(op->ptr, "use_scale");
  bool use_repeat = RNA_boolean_get(op->ptr, "use_repeat");
  bool do_mask = RNA_boolean_get(op->ptr, "mask");
  Tex *tex = nullptr;
  MTex *mtex = nullptr;
  if (br) {
    mtex = do_mask ? &br->mask_mtex : &br->mtex;
    tex = mtex->tex;
  }

  if (tex && tex->type == TEX_IMAGE && tex->ima) {
    float aspx, aspy;
    Image *ima = tex->ima;
    float orig_area, stencil_area, factor;
    ED_image_get_uv_aspect(ima, nullptr, &aspx, &aspy);

    if (use_scale) {
      aspx *= mtex->size[0];
      aspy *= mtex->size[1];
    }

    if (use_repeat && tex->extend == TEX_REPEAT) {
      aspx *= tex->xrepeat;
      aspy *= tex->yrepeat;
    }

    orig_area = fabsf(aspx * aspy);

    if (do_mask) {
      stencil_area = fabsf(br->mask_stencil_dimension[0] * br->mask_stencil_dimension[1]);
    }
    else {
      stencil_area = fabsf(br->stencil_dimension[0] * br->stencil_dimension[1]);
    }

    factor = sqrtf(stencil_area / orig_area);

    if (do_mask) {
      br->mask_stencil_dimension[0] = fabsf(factor * aspx);
      br->mask_stencil_dimension[1] = fabsf(factor * aspy);
    }
    else {
      br->stencil_dimension[0] = fabsf(factor * aspx);
      br->stencil_dimension[1] = fabsf(factor * aspy);
    }
    BKE_brush_tag_unsaved_changes(br);
  }

  WM_event_add_notifier(C, NC_WINDOW, nullptr);

  return OPERATOR_FINISHED;
}

static void BRUSH_OT_stencil_fit_image_aspect(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Image Aspect";
  ot->description =
      "When using an image texture, adjust the stencil size to fit the image aspect ratio";
  ot->idname = "BRUSH_OT_stencil_fit_image_aspect";

  /* API callbacks. */
  ot->exec = stencil_fit_image_aspect_exec;
  ot->poll = stencil_control_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna, "use_repeat", true, "Use Repeat", "Use repeat mapping values");
  RNA_def_boolean(ot->srna, "use_scale", true, "Use Scale", "Use texture scale values");
  RNA_def_boolean(
      ot->srna, "mask", false, "Modify Mask Stencil", "Modify either the primary or mask stencil");
}

static wmOperatorStatus stencil_reset_transform_exec(bContext *C, wmOperator *op)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *br = BKE_paint_brush(paint);
  bool do_mask = RNA_boolean_get(op->ptr, "mask");

  if (!br) {
    return OPERATOR_CANCELLED;
  }

  if (do_mask) {
    br->mask_stencil_pos[0] = 256;
    br->mask_stencil_pos[1] = 256;

    br->mask_stencil_dimension[0] = 256;
    br->mask_stencil_dimension[1] = 256;

    br->mask_mtex.rot = 0;
  }
  else {
    br->stencil_pos[0] = 256;
    br->stencil_pos[1] = 256;

    br->stencil_dimension[0] = 256;
    br->stencil_dimension[1] = 256;

    br->mtex.rot = 0;
  }

  BKE_brush_tag_unsaved_changes(br);
  WM_event_add_notifier(C, NC_WINDOW, nullptr);

  return OPERATOR_FINISHED;
}

static void BRUSH_OT_stencil_reset_transform(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Reset Transform";
  ot->description = "Reset the stencil transformation to the default";
  ot->idname = "BRUSH_OT_stencil_reset_transform";

  /* API callbacks. */
  ot->exec = stencil_reset_transform_exec;
  ot->poll = stencil_control_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(
      ot->srna, "mask", false, "Modify Mask Stencil", "Modify either the primary or mask stencil");
}

/**************************** registration **********************************/

void ED_operatormacros_paint()
{
  wmOperatorType *ot;
  wmOperatorTypeMacro *otmacro;

  ot = WM_operatortype_append_macro("PAINTCURVE_OT_add_point_slide",
                                    "Add Curve Point and Slide",
                                    "Add new curve point and slide it",
                                    OPTYPE_UNDO);
  ot->description = "Add new curve point and slide it";
  WM_operatortype_macro_define(ot, "PAINTCURVE_OT_add_point");
  otmacro = WM_operatortype_macro_define(ot, "PAINTCURVE_OT_slide");
  RNA_boolean_set(otmacro->ptr, "align", true);
  RNA_boolean_set(otmacro->ptr, "select", false);
}

void ED_operatortypes_paint()
{
  /* palette */
  using namespace blender::ed::sculpt_paint;
  WM_operatortype_append(PALETTE_OT_new);
  WM_operatortype_append(PALETTE_OT_color_add);
  WM_operatortype_append(PALETTE_OT_color_delete);

  WM_operatortype_append(PALETTE_OT_extract_from_image);
  WM_operatortype_append(PALETTE_OT_sort);
  WM_operatortype_append(PALETTE_OT_color_move);
  WM_operatortype_append(PALETTE_OT_join);

  /* paint curve */
  WM_operatortype_append(PAINTCURVE_OT_new);
  WM_operatortype_append(PAINTCURVE_OT_add_point);
  WM_operatortype_append(PAINTCURVE_OT_delete_point);
  WM_operatortype_append(PAINTCURVE_OT_select);
  WM_operatortype_append(PAINTCURVE_OT_slide);
  WM_operatortype_append(PAINTCURVE_OT_draw);
  WM_operatortype_append(PAINTCURVE_OT_cursor);

  /* brush */
  WM_operatortype_append(BRUSH_OT_scale_size);
  WM_operatortype_append(BRUSH_OT_stencil_control);
  WM_operatortype_append(BRUSH_OT_stencil_fit_image_aspect);
  WM_operatortype_append(BRUSH_OT_stencil_reset_transform);
  WM_operatortype_append(BRUSH_OT_asset_activate);
  WM_operatortype_append(BRUSH_OT_asset_save_as);
  WM_operatortype_append(BRUSH_OT_asset_edit_metadata);
  WM_operatortype_append(BRUSH_OT_asset_load_preview);
  WM_operatortype_append(BRUSH_OT_asset_delete);
  WM_operatortype_append(BRUSH_OT_asset_save);
  WM_operatortype_append(BRUSH_OT_asset_revert);

  /* image */
  WM_operatortype_append(PAINT_OT_texture_paint_toggle);
  WM_operatortype_append(PAINT_OT_image_paint);
  WM_operatortype_append(PAINT_OT_sample_color);
  WM_operatortype_append(PAINT_OT_grab_clone);
  WM_operatortype_append(PAINT_OT_project_image);
  WM_operatortype_append(PAINT_OT_image_from_view);
  WM_operatortype_append(PAINT_OT_brush_colors_flip);
  WM_operatortype_append(PAINT_OT_add_texture_paint_slot);
  WM_operatortype_append(PAINT_OT_add_simple_uvs);

  /* weight */
  WM_operatortype_append(PAINT_OT_weight_paint_toggle);
  WM_operatortype_append(PAINT_OT_weight_paint);
  WM_operatortype_append(PAINT_OT_weight_set);
  WM_operatortype_append(PAINT_OT_weight_from_bones);
  WM_operatortype_append(PAINT_OT_weight_gradient);
  WM_operatortype_append(PAINT_OT_weight_sample);
  WM_operatortype_append(PAINT_OT_weight_sample_group);

  /* uv */
  WM_operatortype_append(SCULPT_OT_uv_sculpt_grab);
  WM_operatortype_append(SCULPT_OT_uv_sculpt_relax);
  WM_operatortype_append(SCULPT_OT_uv_sculpt_pinch);

  /* vertex selection */
  WM_operatortype_append(PAINT_OT_vert_select_all);
  WM_operatortype_append(PAINT_OT_vert_select_ungrouped);
  WM_operatortype_append(PAINT_OT_vert_select_hide);
  WM_operatortype_append(PAINT_OT_vert_select_linked);
  WM_operatortype_append(PAINT_OT_vert_select_linked_pick);
  WM_operatortype_append(PAINT_OT_vert_select_more);
  WM_operatortype_append(PAINT_OT_vert_select_less);

  /* vertex */
  WM_operatortype_append(PAINT_OT_vertex_paint_toggle);
  WM_operatortype_append(PAINT_OT_vertex_paint);
  WM_operatortype_append(PAINT_OT_vertex_color_set);
  WM_operatortype_append(PAINT_OT_vertex_color_smooth);

  WM_operatortype_append(PAINT_OT_vertex_color_brightness_contrast);
  WM_operatortype_append(PAINT_OT_vertex_color_hsv);
  WM_operatortype_append(PAINT_OT_vertex_color_invert);
  WM_operatortype_append(PAINT_OT_vertex_color_levels);
  WM_operatortype_append(PAINT_OT_vertex_color_from_weight);

  /* face-select */
  WM_operatortype_append(PAINT_OT_face_select_linked);
  WM_operatortype_append(PAINT_OT_face_select_linked_pick);
  WM_operatortype_append(PAINT_OT_face_select_all);
  WM_operatortype_append(PAINT_OT_face_select_more);
  WM_operatortype_append(PAINT_OT_face_select_less);
  WM_operatortype_append(PAINT_OT_face_select_hide);
  WM_operatortype_append(PAINT_OT_face_select_loop);

  WM_operatortype_append(PAINT_OT_face_vert_reveal);

  /* partial visibility */
  WM_operatortype_append(hide::PAINT_OT_hide_show_all);
  WM_operatortype_append(hide::PAINT_OT_hide_show_masked);
  WM_operatortype_append(hide::PAINT_OT_hide_show);
  WM_operatortype_append(hide::PAINT_OT_hide_show_lasso_gesture);
  WM_operatortype_append(hide::PAINT_OT_hide_show_line_gesture);
  WM_operatortype_append(hide::PAINT_OT_hide_show_polyline_gesture);
  WM_operatortype_append(hide::PAINT_OT_visibility_invert);
  WM_operatortype_append(hide::PAINT_OT_visibility_filter);

  /* paint masking */
  WM_operatortype_append(mask::PAINT_OT_mask_flood_fill);
  WM_operatortype_append(mask::PAINT_OT_mask_lasso_gesture);
  WM_operatortype_append(mask::PAINT_OT_mask_box_gesture);
  WM_operatortype_append(mask::PAINT_OT_mask_line_gesture);
  WM_operatortype_append(mask::PAINT_OT_mask_polyline_gesture);
}

void ED_keymap_paint(wmKeyConfig *keyconf)
{
  using namespace blender::ed::sculpt_paint;
  wmKeyMap *keymap;

  keymap = WM_keymap_ensure(keyconf, "Paint Curve", SPACE_EMPTY, RGN_TYPE_WINDOW);
  keymap->poll = paint_curve_poll;

  /* Sculpt mode */
  keymap = WM_keymap_ensure(keyconf, "Sculpt", SPACE_EMPTY, RGN_TYPE_WINDOW);
  keymap->poll = SCULPT_mode_poll;

  /* Vertex Paint mode */
  keymap = WM_keymap_ensure(keyconf, "Vertex Paint", SPACE_EMPTY, RGN_TYPE_WINDOW);
  keymap->poll = vertex_paint_mode_poll;

  /* Weight Paint mode */
  keymap = WM_keymap_ensure(keyconf, "Weight Paint", SPACE_EMPTY, RGN_TYPE_WINDOW);
  keymap->poll = weight_paint_mode_poll;

  /* Weight paint's Vertex Selection Mode. */
  keymap = WM_keymap_ensure(
      keyconf, "Paint Vertex Selection (Weight, Vertex)", SPACE_EMPTY, RGN_TYPE_WINDOW);
  keymap->poll = vert_paint_poll;

  /* Image/Texture Paint mode */
  keymap = WM_keymap_ensure(keyconf, "Image Paint", SPACE_EMPTY, RGN_TYPE_WINDOW);
  keymap->poll = image_texture_paint_poll;

  /* face-mask mode */
  keymap = WM_keymap_ensure(
      keyconf, "Paint Face Mask (Weight, Vertex, Texture)", SPACE_EMPTY, RGN_TYPE_WINDOW);
  keymap->poll = facemask_paint_poll;

  /* paint stroke */
  keymap = paint_stroke_modal_keymap(keyconf);
  WM_modalkeymap_assign(keymap, "SCULPT_OT_brush_stroke");

  /* Curves Sculpt mode. */
  keymap = WM_keymap_ensure(keyconf, "Sculpt Curves", SPACE_EMPTY, RGN_TYPE_WINDOW);
  keymap->poll = curves_sculpt_poll;

  /* sculpt expand. */
  expand::modal_keymap(keyconf);
}
