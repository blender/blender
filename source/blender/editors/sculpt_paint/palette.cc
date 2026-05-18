/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */
#include "DNA_brush_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_set.hh"

#include "BKE_brush.hh"
#include "BKE_context.hh"
#include "BKE_image.hh"
#include "BKE_lib_id.hh"
#include "BKE_library.hh"
#include "BKE_paint.hh"
#include "BKE_paint_types.hh"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "paint_intern.hh"

namespace blender {

struct PaletteColorHSV {
  float rgb[3] = {};
  float value = 0;
  float h = 0;
  float s = 0;
  float v = 0;
};

static int palettecolor_compare_hsv(const void *a1, const void *a2)
{
  const PaletteColorHSV *ps1 = static_cast<const PaletteColorHSV *>(a1);
  const PaletteColorHSV *ps2 = static_cast<const PaletteColorHSV *>(a2);

  /* Hue */
  if (ps1->h > ps2->h) {
    return 1;
  }
  if (ps1->h < ps2->h) {
    return -1;
  }

  /* Saturation. */
  if (ps1->s > ps2->s) {
    return 1;
  }
  if (ps1->s < ps2->s) {
    return -1;
  }

  /* Value. */
  if (1.0f - ps1->v > 1.0f - ps2->v) {
    return 1;
  }
  if (1.0f - ps1->v < 1.0f - ps2->v) {
    return -1;
  }

  return 0;
}

static void palette_sort_hsv(PaletteColorHSV *color_array, const int totcol)
{
  qsort(color_array, totcol, sizeof(PaletteColorHSV), palettecolor_compare_hsv);
}

static int palettecolor_compare_svh(const void *a1, const void *a2)
{
  const PaletteColorHSV *ps1 = static_cast<const PaletteColorHSV *>(a1);
  const PaletteColorHSV *ps2 = static_cast<const PaletteColorHSV *>(a2);

  /* Saturation. */
  if (ps1->s > ps2->s) {
    return 1;
  }
  if (ps1->s < ps2->s) {
    return -1;
  }

  /* Value. */
  if (1.0f - ps1->v > 1.0f - ps2->v) {
    return 1;
  }
  if (1.0f - ps1->v < 1.0f - ps2->v) {
    return -1;
  }

  /* Hue */
  if (ps1->h > ps2->h) {
    return 1;
  }
  if (ps1->h < ps2->h) {
    return -1;
  }

  return 0;
}

static void palette_sort_svh(PaletteColorHSV *color_array, const int totcol)
{
  qsort(color_array, totcol, sizeof(PaletteColorHSV), palettecolor_compare_svh);
}

static int palettecolor_compare_vhs(const void *a1, const void *a2)
{
  const PaletteColorHSV *ps1 = static_cast<const PaletteColorHSV *>(a1);
  const PaletteColorHSV *ps2 = static_cast<const PaletteColorHSV *>(a2);

  /* Value. */
  if (1.0f - ps1->v > 1.0f - ps2->v) {
    return 1;
  }
  if (1.0f - ps1->v < 1.0f - ps2->v) {
    return -1;
  }

  /* Hue */
  if (ps1->h > ps2->h) {
    return 1;
  }
  if (ps1->h < ps2->h) {
    return -1;
  }

  /* Saturation. */
  if (ps1->s > ps2->s) {
    return 1;
  }
  if (ps1->s < ps2->s) {
    return -1;
  }

  return 0;
}

static void palette_sort_vhs(PaletteColorHSV *color_array, const int totcol)
{
  qsort(color_array, totcol, sizeof(PaletteColorHSV), palettecolor_compare_vhs);
}

static int palettecolor_compare_luminance(const void *a1, const void *a2)
{
  const PaletteColorHSV *ps1 = static_cast<const PaletteColorHSV *>(a1);
  const PaletteColorHSV *ps2 = static_cast<const PaletteColorHSV *>(a2);

  float lumi1 = (ps1->rgb[0] + ps1->rgb[1] + ps1->rgb[2]) / 3.0f;
  float lumi2 = (ps2->rgb[0] + ps2->rgb[1] + ps2->rgb[2]) / 3.0f;

  if (lumi1 > lumi2) {
    return -1;
  }
  if (lumi1 < lumi2) {
    return 1;
  }

  return 0;
}

static void palette_sort_luminance(PaletteColorHSV *color_array, const int totcol)
{
  /* Sort by Luminance (calculated with the average, enough for sorting). */
  qsort(color_array, totcol, sizeof(PaletteColorHSV), palettecolor_compare_luminance);
}

static bool palette_from_hash(Main *bmain, const Set<uint> &color_table, const char *name)
{
  if (color_table.is_empty()) {
    return false;
  }

  /* Put colors into array. */
  Array<PaletteColorHSV> color_array(color_table.size());
  int64_t index = 0;
  for (uint col : color_table) {
    PaletteColorHSV pal_col;
    cpack_to_rgb(col, &pal_col.rgb[0], &pal_col.rgb[1], &pal_col.rgb[2]);
    rgb_to_hsv(pal_col.rgb[0], pal_col.rgb[1], pal_col.rgb[2], &pal_col.h, &pal_col.s, &pal_col.v);
    pal_col.value = 0;
    color_array[index++] = pal_col;
  }

  /* Sort by Hue and saturation. */
  palette_sort_hsv(color_array.data(), color_array.size());

  /* Create the Palette. */
  Palette *palette = BKE_palette_add(bmain, name);
  if (!palette) {
    return false;
  }
  for (const PaletteColorHSV &col_src : color_array) {
    PaletteColor *col_dst = BKE_palette_color_add(palette);
    if (col_dst) {
      IMB_colormanagement_srgb_to_scene_linear_v3(col_dst->color, col_src.rgb);
    }
  }
  return true;
}

static wmOperatorStatus palette_new_exec(bContext *C, wmOperator * /*op*/)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  Main *bmain = CTX_data_main(C);
  Palette *palette;

  palette = BKE_palette_add(bmain, "Palette");

  BKE_paint_palette_set(paint, palette);

  return OPERATOR_FINISHED;
}

void PALETTE_OT_new(wmOperatorType *ot)
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

void PALETTE_OT_color_add(wmOperatorType *ot)
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

void PALETTE_OT_color_delete(wmOperatorType *ot)
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
  ImBuf *ibuf = BKE_image_acquire_ibuf(image, &iuser, &lock);

  if (ibuf && ibuf->byte_data()) {
    /* Extract all colors. */
    Set<uint> color_table;
    const int range = int(pow(10.0f, threshold));
    Span<uchar4> byte_buffer = Span(reinterpret_cast<const uchar4 *>(ibuf->byte_data()),
                                    IMB_get_pixel_count(ibuf));
    for (uchar4 pix : byte_buffer) {
      float color[3];
      rgb_uchar_to_float(color, pix);
      IMB_colormanagement_colorspace_to_scene_linear_v3(color, ibuf->byte_buffer.colorspace);
      IMB_colormanagement_scene_linear_to_srgb_v3(color, color);

      for (int i = 0; i < 3; i++) {
        color[i] = truncf(color[i] * range) / range;
      }
      uint key = rgb_to_cpack(color[0], color[1], color[2]);
      color_table.add(key);
    }

    done = palette_from_hash(bmain, color_table, image->id.name + 2);
  }

  BKE_image_release_ibuf(image, ibuf, lock);

  if (done) {
    BKE_reportf(op->reports, RPT_INFO, "Palette created");
  }

  return OPERATOR_FINISHED;
}

void PALETTE_OT_extract_from_image(wmOperatorType *ot)
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

  PaletteColorHSV *color_array = nullptr;
  PaletteColorHSV *col_elm = nullptr;

  const int totcol = BLI_listbase_count(&palette->colors);

  if (totcol > 0) {
    color_array = MEM_new_array<PaletteColorHSV>(totcol, __func__);
    /* Put all colors in an array. */
    int t = 0;
    for (PaletteColor &color : palette->colors) {
      float h, s, v;
      rgb_to_hsv(color.color[0], color.color[1], color.color[2], &h, &s, &v);
      col_elm = &color_array[t];
      copy_v3_v3(col_elm->rgb, color.color);
      col_elm->value = color.value;
      col_elm->h = h;
      col_elm->s = s;
      col_elm->v = v;
      t++;
    }
    /* Sort */
    if (type == 1) {
      palette_sort_hsv(color_array, totcol);
    }
    else if (type == 2) {
      palette_sort_svh(color_array, totcol);
    }
    else if (type == 3) {
      palette_sort_vhs(color_array, totcol);
    }
    else {
      palette_sort_luminance(color_array, totcol);
    }

    /* Clear old color swatches. */
    for (PaletteColor &color : palette->colors.items_mutable()) {
      BKE_palette_color_remove(palette, &color);
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
    MEM_SAFE_DELETE(color_array);
  }

  WM_event_add_notifier(C, NC_BRUSH | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void PALETTE_OT_sort(wmOperatorType *ot)
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

void PALETTE_OT_color_move(wmOperatorType *ot)
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

  palette_join = id_cast<Palette *>(BKE_libblock_find_name(bmain, ID_PAL, name));
  if (palette_join == nullptr) {
    return OPERATOR_CANCELLED;
  }

  const int totcol = BLI_listbase_count(&palette_join->colors);

  if (totcol > 0) {
    for (PaletteColor &color : palette_join->colors) {
      PaletteColor *palcol = BKE_palette_color_add(palette);
      if (palcol) {
        copy_v3_v3(palcol->color, color.color);
        palcol->value = color.value;
        done = true;
      }
    }
  }

  if (done) {
    /* Clear old color swatches. */
    for (PaletteColor &color : palette_join->colors.items_mutable()) {
      BKE_palette_color_remove(palette_join, &color);
    }

    /* Notifier. */
    WM_event_add_notifier(C, NC_BRUSH | NA_EDITED, nullptr);
  }

  return OPERATOR_FINISHED;
}

void PALETTE_OT_join(wmOperatorType *ot)
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
}  // namespace blender
