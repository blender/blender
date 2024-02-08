/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#include <cstddef>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_fileops.h"
#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "IMB_interp.hh"

#include "DNA_brush_types.h"
#include "DNA_customdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLO_writefile.hh"

#include "BKE_asset.hh"
#include "BKE_blendfile.hh"
#include "BKE_brush.hh"
#include "BKE_context.hh"
#include "BKE_image.h"
#include "BKE_lib_id.hh"
#include "BKE_lib_override.hh"
#include "BKE_main.hh"
#include "BKE_paint.hh"
#include "BKE_preferences.h"
#include "BKE_report.h"

#include "ED_asset_handle.hh"
#include "ED_asset_list.hh"
#include "ED_asset_mark_clear.hh"
#include "ED_asset_menu_utils.hh"
#include "ED_image.hh"
#include "ED_paint.hh"
#include "ED_screen.hh"

#include "WM_api.hh"
#include "WM_toolsystem.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "AS_asset_library.hh"
#include "AS_asset_representation.hh"

#include "curves_sculpt_intern.hh"
#include "paint_intern.hh"
#include "sculpt_intern.hh"

/* Brush operators */
static int brush_add_exec(bContext *C, wmOperator * /*op*/)
{
  // int type = RNA_enum_get(op->ptr, "type");
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *br = BKE_paint_brush(paint);
  Main *bmain = CTX_data_main(C);
  PaintMode mode = BKE_paintmode_get_active_from_context(C);

  if (br) {
    br = (Brush *)BKE_id_copy(bmain, &br->id);
  }
  else {
    br = BKE_brush_add(bmain, "Brush", BKE_paint_object_mode_from_paintmode(mode));
  }
  id_us_min(&br->id); /* fake user only */

  BKE_paint_brush_set(paint, br);

  return OPERATOR_FINISHED;
}

static void BRUSH_OT_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Brush";
  ot->description = "Add brush by mode type";
  ot->idname = "BRUSH_OT_add";

  /* api callbacks */
  ot->exec = brush_add_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static eGPBrush_Presets gpencil_get_brush_preset_from_tool(bToolRef *tool,
                                                           enum eContextObjectMode mode)
{
  switch (mode) {
    case CTX_MODE_PAINT_GPENCIL_LEGACY: {
      if (STREQ(tool->runtime->data_block, "DRAW")) {
        return GP_BRUSH_PRESET_PENCIL;
      }
      if (STREQ(tool->runtime->data_block, "FILL")) {
        return GP_BRUSH_PRESET_FILL_AREA;
      }
      if (STREQ(tool->runtime->data_block, "ERASE")) {
        return GP_BRUSH_PRESET_ERASER_SOFT;
      }
      if (STREQ(tool->runtime->data_block, "TINT")) {
        return GP_BRUSH_PRESET_TINT;
      }
      break;
    }
    case CTX_MODE_SCULPT_GPENCIL_LEGACY: {
      if (STREQ(tool->runtime->data_block, "SMOOTH")) {
        return GP_BRUSH_PRESET_SMOOTH_STROKE;
      }
      if (STREQ(tool->runtime->data_block, "STRENGTH")) {
        return GP_BRUSH_PRESET_STRENGTH_STROKE;
      }
      if (STREQ(tool->runtime->data_block, "THICKNESS")) {
        return GP_BRUSH_PRESET_THICKNESS_STROKE;
      }
      if (STREQ(tool->runtime->data_block, "GRAB")) {
        return GP_BRUSH_PRESET_GRAB_STROKE;
      }
      if (STREQ(tool->runtime->data_block, "PUSH")) {
        return GP_BRUSH_PRESET_PUSH_STROKE;
      }
      if (STREQ(tool->runtime->data_block, "TWIST")) {
        return GP_BRUSH_PRESET_TWIST_STROKE;
      }
      if (STREQ(tool->runtime->data_block, "PINCH")) {
        return GP_BRUSH_PRESET_PINCH_STROKE;
      }
      if (STREQ(tool->runtime->data_block, "RANDOMIZE")) {
        return GP_BRUSH_PRESET_RANDOMIZE_STROKE;
      }
      if (STREQ(tool->runtime->data_block, "CLONE")) {
        return GP_BRUSH_PRESET_CLONE_STROKE;
      }
      break;
    }
    case CTX_MODE_WEIGHT_GPENCIL_LEGACY: {
      if (STREQ(tool->runtime->data_block, "DRAW")) {
        return GP_BRUSH_PRESET_WEIGHT_DRAW;
      }
      if (STREQ(tool->runtime->data_block, "BLUR")) {
        return GP_BRUSH_PRESET_WEIGHT_BLUR;
      }
      if (STREQ(tool->runtime->data_block, "AVERAGE")) {
        return GP_BRUSH_PRESET_WEIGHT_AVERAGE;
      }
      if (STREQ(tool->runtime->data_block, "SMEAR")) {
        return GP_BRUSH_PRESET_WEIGHT_SMEAR;
      }
      break;
    }
    case CTX_MODE_VERTEX_GPENCIL_LEGACY: {
      if (STREQ(tool->runtime->data_block, "DRAW")) {
        return GP_BRUSH_PRESET_VERTEX_DRAW;
      }
      if (STREQ(tool->runtime->data_block, "BLUR")) {
        return GP_BRUSH_PRESET_VERTEX_BLUR;
      }
      if (STREQ(tool->runtime->data_block, "AVERAGE")) {
        return GP_BRUSH_PRESET_VERTEX_AVERAGE;
      }
      if (STREQ(tool->runtime->data_block, "SMEAR")) {
        return GP_BRUSH_PRESET_VERTEX_SMEAR;
      }
      if (STREQ(tool->runtime->data_block, "REPLACE")) {
        return GP_BRUSH_PRESET_VERTEX_REPLACE;
      }
      break;
    }
    default:
      return GP_BRUSH_PRESET_UNKNOWN;
  }
  return GP_BRUSH_PRESET_UNKNOWN;
}

static int brush_add_gpencil_exec(bContext *C, wmOperator * /*op*/)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *br = BKE_paint_brush(paint);
  Main *bmain = CTX_data_main(C);

  if (br) {
    br = (Brush *)BKE_id_copy(bmain, &br->id);
  }
  else {
    /* Get the active tool to determine what type of brush is active. */
    bScreen *screen = CTX_wm_screen(C);
    if (screen == nullptr) {
      return OPERATOR_CANCELLED;
    }

    bToolRef *tool = nullptr;
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      if (area->spacetype == SPACE_VIEW3D) {
        /* Check the current tool is a brush. */
        bToolRef *tref = area->runtime.tool;
        if (tref && tref->runtime && tref->runtime->data_block[0]) {
          tool = tref;
          break;
        }
      }
    }

    if (tool == nullptr) {
      return OPERATOR_CANCELLED;
    }

    /* Get Brush mode base on context mode. */
    const enum eContextObjectMode mode = CTX_data_mode_enum(C);
    eObjectMode obmode = OB_MODE_PAINT_GPENCIL_LEGACY;
    switch (mode) {
      case CTX_MODE_PAINT_GPENCIL_LEGACY:
        obmode = OB_MODE_PAINT_GPENCIL_LEGACY;
        break;
      case CTX_MODE_SCULPT_GPENCIL_LEGACY:
        obmode = OB_MODE_SCULPT_GPENCIL_LEGACY;
        break;
      case CTX_MODE_WEIGHT_GPENCIL_LEGACY:
        obmode = OB_MODE_WEIGHT_GPENCIL_LEGACY;
        break;
      case CTX_MODE_VERTEX_GPENCIL_LEGACY:
        obmode = OB_MODE_VERTEX_GPENCIL_LEGACY;
        break;
      default:
        return OPERATOR_CANCELLED;
        break;
    }

    /* Get brush preset using the actual tool. */
    eGPBrush_Presets preset = gpencil_get_brush_preset_from_tool(tool, mode);

    /* Capitalize Brush name first letter using the tool name. */
    char name[64];
    STRNCPY(name, tool->runtime->data_block);
    BLI_str_tolower_ascii(name, sizeof(name));
    name[0] = BLI_toupper_ascii(name[0]);

    /* Create the brush and assign default values. */
    br = BKE_brush_add(bmain, name, obmode);
    if (br) {
      BKE_brush_init_gpencil_settings(br);
      BKE_gpencil_brush_preset_set(bmain, br, preset);
    }
  }

  if (br) {
    id_us_min(&br->id); /* fake user only */
    BKE_paint_brush_set(paint, br);
  }

  return OPERATOR_FINISHED;
}

static void BRUSH_OT_add_gpencil(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Drawing Brush";
  ot->description = "Add brush for Grease Pencil";
  ot->idname = "BRUSH_OT_add_gpencil";

  /* api callbacks */
  ot->exec = brush_add_gpencil_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int brush_scale_size_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = BKE_paint_brush(paint);
  const bool is_gpencil = (brush && brush->gpencil_settings != nullptr);
  // Object *ob = CTX_data_active_object(C);
  float scalar = RNA_float_get(op->ptr, "scalar");

  if (brush) {
    /* pixel radius */
    {
      const int old_size = (!is_gpencil) ? BKE_brush_size_get(scene, brush) : brush->size;
      int size = int(scalar * old_size);

      if (abs(old_size - size) < U.pixelsize) {
        if (scalar > 1) {
          size += U.pixelsize;
        }
        else if (scalar < 1) {
          size -= U.pixelsize;
        }
      }
      /* Grease Pencil does not use unified size. */
      if (is_gpencil) {
        brush->size = max_ii(size, 1);
        WM_main_add_notifier(NC_BRUSH | NA_EDITED, brush);
        return OPERATOR_FINISHED;
      }

      BKE_brush_size_set(scene, brush, size);
    }

    /* unprojected radius */
    {
      float unprojected_radius = scalar * BKE_brush_unprojected_radius_get(scene, brush);

      if (unprojected_radius < 0.001f) { /* XXX magic number */
        unprojected_radius = 0.001f;
      }

      BKE_brush_unprojected_radius_set(scene, brush, unprojected_radius);
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

  /* api callbacks */
  ot->exec = brush_scale_size_exec;

  /* flags */
  ot->flag = 0;

  RNA_def_float(ot->srna, "scalar", 1, 0, 2, "Scalar", "Factor to scale brush size by", 0, 2);
}

/* Palette operators */

static int palette_new_exec(bContext *C, wmOperator * /*op*/)
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

  /* api callbacks */
  ot->exec = palette_new_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static bool palette_poll(bContext *C)
{
  Paint *paint = BKE_paint_get_active_from_context(C);

  if (paint && paint->palette != nullptr && !ID_IS_LINKED(paint->palette) &&
      !ID_IS_OVERRIDE_LIBRARY(paint->palette))
  {
    return true;
  }

  return false;
}

static int palette_color_add_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_scene(C);
  Paint *paint = BKE_paint_get_active_from_context(C);
  PaintMode mode = BKE_paintmode_get_active_from_context(C);
  Palette *palette = paint->palette;
  PaletteColor *color;

  color = BKE_palette_color_add(palette);
  palette->active_color = BLI_listbase_count(&palette->colors) - 1;

  if (paint->brush) {
    const Brush *brush = paint->brush;
    if (ELEM(mode,
             PaintMode::Texture3D,
             PaintMode::Texture2D,
             PaintMode::Vertex,
             PaintMode::Sculpt))
    {
      copy_v3_v3(color->rgb, BKE_brush_color_get(scene, brush));
      color->value = 0.0;
    }
    else if (mode == PaintMode::Weight) {
      zero_v3(color->rgb);
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

  /* api callbacks */
  ot->exec = palette_color_add_exec;
  ot->poll = palette_poll;
  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int palette_color_delete_exec(bContext *C, wmOperator * /*op*/)
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

  /* api callbacks */
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

static int palette_extract_img_exec(bContext *C, wmOperator *op)
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
        float color[4];
        IMB_sampleImageAtLocation(ibuf, float(col), float(row), false, color);
        for (int i = 0; i < 3; i++) {
          color[i] = truncf(color[i] * range) / range;
        }

        uint key = rgb_to_cpack(color[0], color[1], color[2]);
        if (!BLI_ghash_haskey(color_table, POINTER_FROM_INT(key))) {
          BLI_ghash_insert(color_table, POINTER_FROM_INT(key), POINTER_FROM_INT(key));
        }
      }
    }

    done = BKE_palette_from_hash(bmain, color_table, image->id.name + 2, false);
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

  /* api callbacks */
  ot->exec = palette_extract_img_exec;
  ot->poll = palette_extract_img_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  prop = RNA_def_int(ot->srna, "threshold", 1, 1, 1, "Threshold", "", 1, 1);
  RNA_def_property_flag(prop, PropertyFlag(PROP_HIDDEN | PROP_SKIP_SAVE));
}

/* Sort Palette color by Hue and Saturation. */
static int palette_sort_exec(bContext *C, wmOperator *op)
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
    color_array = MEM_cnew_array<tPaletteColorHSV>(totcol, __func__);
    /* Put all colors in an array. */
    int t = 0;
    LISTBASE_FOREACH (PaletteColor *, color, &palette->colors) {
      float h, s, v;
      rgb_to_hsv(color->rgb[0], color->rgb[1], color->rgb[2], &h, &s, &v);
      col_elm = &color_array[t];
      copy_v3_v3(col_elm->rgb, color->rgb);
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
        copy_v3_v3(palcol->rgb, col_elm->rgb);
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

  /* api callbacks */
  ot->exec = palette_sort_exec;
  ot->poll = palette_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = RNA_def_enum(ot->srna, "type", sort_type, 1, "Type", "");
}

/* Move colors in palette. */
static int palette_color_move_exec(bContext *C, wmOperator *op)
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

  /* api callbacks */
  ot->exec = palette_color_move_exec;
  ot->poll = palette_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  ot->prop = RNA_def_enum(ot->srna, "type", slot_move, 0, "Type", "");
}

/* Join Palette swatches. */
static int palette_join_exec(bContext *C, wmOperator *op)
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
        copy_v3_v3(palcol->rgb, color->rgb);
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

  /* api callbacks */
  ot->exec = palette_join_exec;
  ot->poll = palette_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_string(ot->srna, "palette", nullptr, MAX_ID_NAME - 2, "Palette", "Name of the Palette");
}

static int brush_reset_exec(bContext *C, wmOperator * /*op*/)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = BKE_paint_brush(paint);
  Object *ob = CTX_data_active_object(C);

  if (!ob || !brush) {
    return OPERATOR_CANCELLED;
  }

  /* TODO: other modes */
  if (ob->mode & OB_MODE_SCULPT) {
    BKE_brush_sculpt_reset(brush);
  }
  else {
    return OPERATOR_CANCELLED;
  }
  WM_event_add_notifier(C, NC_BRUSH | NA_EDITED, brush);

  return OPERATOR_FINISHED;
}

static void BRUSH_OT_reset(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Reset Brush";
  ot->description = "Return brush to defaults based on current tool";
  ot->idname = "BRUSH_OT_reset";

  /* api callbacks */
  ot->exec = brush_reset_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int brush_tool(const Brush *brush, size_t tool_offset)
{
  return *(((char *)brush) + tool_offset);
}

static void brush_tool_set(const Brush *brush, size_t tool_offset, int tool)
{
  *(((char *)brush) + tool_offset) = tool;
}

static Brush *brush_tool_cycle(Main *bmain, Paint *paint, Brush *brush_orig, const int tool)
{
  Brush *brush, *first_brush;

  if (!brush_orig && !(brush_orig = static_cast<Brush *>(bmain->brushes.first))) {
    return nullptr;
  }

  if (brush_tool(brush_orig, paint->runtime.tool_offset) != tool) {
    /* If current brush's tool is different from what we need,
     * start cycling from the beginning of the list.
     * Such logic will activate the same exact brush not relating from
     * which tool user requests other tool.
     */

    /* Try to tool-slot first. */
    first_brush = BKE_paint_toolslots_brush_get(paint, tool);
    if (first_brush == nullptr) {
      first_brush = static_cast<Brush *>(bmain->brushes.first);
    }
  }
  else {
    /* If user wants to switch to brush with the same tool as
     * currently active brush do a cycling via all possible
     * brushes with requested tool. */
    first_brush = brush_orig->id.next ? static_cast<Brush *>(brush_orig->id.next) :
                                        static_cast<Brush *>(bmain->brushes.first);
  }

  /* get the next brush with the active tool */
  brush = first_brush;
  do {
    if ((brush->ob_mode & paint->runtime.ob_mode) &&
        (brush_tool(brush, paint->runtime.tool_offset) == tool))
    {
      return brush;
    }

    brush = brush->id.next ? static_cast<Brush *>(brush->id.next) :
                             static_cast<Brush *>(bmain->brushes.first);
  } while (brush != first_brush);

  return nullptr;
}

static Brush *brush_tool_toggle(Main *bmain, Paint *paint, Brush *brush_orig, const int tool)
{
  if (!brush_orig || brush_tool(brush_orig, paint->runtime.tool_offset) != tool) {
    Brush *br;
    /* if the current brush is not using the desired tool, look
     * for one that is */
    br = brush_tool_cycle(bmain, paint, brush_orig, tool);
    /* store the previously-selected brush */
    if (br) {
      br->toggle_brush = brush_orig;
    }

    return br;
  }
  if (brush_orig->toggle_brush) {
    /* if current brush is using the desired tool, try to toggle
     * back to the previously selected brush. */
    return brush_orig->toggle_brush;
  }
  return nullptr;
}

static bool brush_generic_tool_set(bContext *C,
                                   Main *bmain,
                                   Paint *paint,
                                   const int tool,
                                   const char *tool_name,
                                   const bool create_missing,
                                   const bool toggle)
{
  Brush *brush, *brush_orig = BKE_paint_brush(paint);

  if (toggle) {
    brush = brush_tool_toggle(bmain, paint, brush_orig, tool);
  }
  else {
    brush = brush_tool_cycle(bmain, paint, brush_orig, tool);
  }

  if (((brush == nullptr) && create_missing) &&
      ((brush_orig == nullptr) || brush_tool(brush_orig, paint->runtime.tool_offset) != tool))
  {
    brush = BKE_brush_add(bmain, tool_name, eObjectMode(paint->runtime.ob_mode));
    id_us_min(&brush->id); /* fake user only */
    brush_tool_set(brush, paint->runtime.tool_offset, tool);
    brush->toggle_brush = brush_orig;
  }

  if (brush) {
    BKE_paint_brush_set(paint, brush);
    BKE_paint_invalidate_overlay_all();

    WM_main_add_notifier(NC_BRUSH | NA_EDITED, brush);
    WM_toolsystem_ref_set_by_id(C, "builtin.brush");
    return true;
  }
  return false;
}

static const PaintMode brush_select_paint_modes[] = {
    PaintMode::Sculpt,
    PaintMode::Vertex,
    PaintMode::Weight,
    PaintMode::Texture3D,
    PaintMode::GPencil,
    PaintMode::VertexGPencil,
    PaintMode::SculptGPencil,
    PaintMode::WeightGPencil,
    PaintMode::SculptCurves,
};

static int brush_select_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  const bool create_missing = RNA_boolean_get(op->ptr, "create_missing");
  const bool toggle = RNA_boolean_get(op->ptr, "toggle");
  const char *tool_name = "Brush";
  int tool = 0;

  PaintMode paint_mode = PaintMode::Invalid;
  for (int i = 0; i < ARRAY_SIZE(brush_select_paint_modes); i++) {
    paint_mode = brush_select_paint_modes[i];
    const char *op_prop_id = BKE_paint_get_tool_prop_id_from_paintmode(paint_mode);
    PropertyRNA *prop = RNA_struct_find_property(op->ptr, op_prop_id);
    if (RNA_property_is_set(op->ptr, prop)) {
      tool = RNA_property_enum_get(op->ptr, prop);
      break;
    }
  }

  if (paint_mode == PaintMode::Invalid) {
    return OPERATOR_CANCELLED;
  }

  Paint *paint = BKE_paint_get_active_from_paintmode(scene, paint_mode);
  if (paint == nullptr) {
    return OPERATOR_CANCELLED;
  }

  if (brush_generic_tool_set(C, bmain, paint, tool, tool_name, create_missing, toggle)) {
    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

static void PAINT_OT_brush_select(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Brush Select";
  ot->description = "Select a paint mode's brush by tool type";
  ot->idname = "PAINT_OT_brush_select";

  /* api callbacks */
  ot->exec = brush_select_exec;

  /* flags */
  ot->flag = 0;

  /* props */
  /* All properties are hidden, so as not to show the redo panel. */
  for (int i = 0; i < ARRAY_SIZE(brush_select_paint_modes); i++) {
    const PaintMode paint_mode = brush_select_paint_modes[i];
    const char *prop_id = BKE_paint_get_tool_prop_id_from_paintmode(paint_mode);
    prop = RNA_def_enum(
        ot->srna, prop_id, BKE_paint_get_tool_enum_from_paintmode(paint_mode), 0, prop_id, "");
    RNA_def_property_translation_context(
        prop, BKE_paint_get_tool_enum_translation_context_from_paintmode(paint_mode));
    RNA_def_property_flag(prop, PROP_HIDDEN);
  }

  prop = RNA_def_boolean(
      ot->srna, "toggle", false, "Toggle", "Toggle between two brushes rather than cycling");
  RNA_def_property_flag(prop, PropertyFlag(PROP_HIDDEN | PROP_SKIP_SAVE));
  prop = RNA_def_boolean(ot->srna,
                         "create_missing",
                         false,
                         "Create Missing",
                         "If the requested brush type does not exist, create a new brush");
  RNA_def_property_flag(prop, PropertyFlag(PROP_HIDDEN | PROP_SKIP_SAVE));
}

namespace blender::ed::sculpt_paint {

/**************************** Brush Assets **********************************/

static int brush_asset_select_exec(bContext *C, wmOperator *op)
{
  /* This operator currently covers both cases: the file/asset browser file list and the asset list
   * used for the asset-view template. Once the asset list design is used by the Asset Browser,
   * this can be simplified to just that case. */
  const asset_system::AssetRepresentation *asset =
      asset::operator_asset_reference_props_get_asset_from_all_library(*C, *op->ptr, op->reports);
  if (!asset) {
    return OPERATOR_CANCELLED;
  }

  AssetWeakReference *brush_asset_reference = asset->make_weak_reference();
  Brush *brush = BKE_brush_asset_runtime_ensure(CTX_data_main(C), brush_asset_reference);

  Paint *paint = BKE_paint_get_active_from_context(C);

  if (!BKE_paint_brush_asset_set(paint, brush, brush_asset_reference)) {
    /* Note brush datablock was still added, so was not a no-op. */
    BKE_report(op->reports, RPT_WARNING, "Unable to select brush, wrong object mode");
    return OPERATOR_FINISHED;
  }

  WM_main_add_notifier(NC_ASSET | NA_ACTIVATED, nullptr);
  WM_main_add_notifier(NC_SCENE | ND_TOOLSETTINGS, nullptr);
  WM_toolsystem_ref_set_by_id(C, "builtin.brush");

  return OPERATOR_FINISHED;
}

static void BRUSH_OT_asset_select(wmOperatorType *ot)
{
  ot->name = "Select Brush Asset";
  ot->description = "Select a brush asset as current sculpt and paint tool";
  ot->idname = "BRUSH_OT_asset_select";

  ot->exec = brush_asset_select_exec;

  asset::operator_asset_reference_props_register(*ot->srna);
}

/* FIXME Quick dirty hack to generate a weak ref from 'raw' paths.
 * This needs to be properly implemented in assetlib code.
 */
static AssetWeakReference *brush_asset_create_weakref_hack(const bUserAssetLibrary *user_asset_lib,
                                                           std::string &file_path)
{
  AssetWeakReference *asset_weak_ref = MEM_new<AssetWeakReference>(__func__);

  StringRef asset_root_path = user_asset_lib->dirpath;
  BLI_assert(file_path.find(asset_root_path) == 0);
  std::string relative_asset_path = file_path.substr(size_t(asset_root_path.size()) + 1);

  asset_weak_ref->asset_library_type = ASSET_LIBRARY_CUSTOM;
  asset_weak_ref->asset_library_identifier = BLI_strdup(user_asset_lib->name);
  asset_weak_ref->relative_asset_identifier = BLI_strdup(relative_asset_path.c_str());

  return asset_weak_ref;
}

static const bUserAssetLibrary *brush_asset_get_default_library()
{
  if (BLI_listbase_is_empty(&U.asset_libraries)) {
    return nullptr;
  }
  LISTBASE_FOREACH (const bUserAssetLibrary *, asset_library, &U.asset_libraries) {
    if (asset_library->flag & ASSET_LIBRARY_DEFAULT) {
      return asset_library;
    }
  }
  return static_cast<const bUserAssetLibrary *>(U.asset_libraries.first);
}

static void refresh_asset_library(const bContext *C, const bUserAssetLibrary &user_library)
{
  /* TODO: Should the all library reference be automatically cleared? */
  AssetLibraryReference all_lib_ref = asset_system::all_library_reference();
  asset::list::clear(&all_lib_ref, C);

  /* TODO: this is convoluted, can we create a reference from pointer? */
  for (const AssetLibraryReference &lib_ref : asset_system::all_valid_asset_library_refs()) {
    if (lib_ref.type == ASSET_LIBRARY_CUSTOM) {
      const bUserAssetLibrary *ref_user_library = BKE_preferences_asset_library_find_index(
          &U, lib_ref.custom_library_index);
      if (ref_user_library == &user_library) {
        asset::list::clear(&lib_ref, C);
        return;
      }
    }
  }
}

static std::string brush_asset_root_path_for_save(const bUserAssetLibrary &user_library)
{
  if (user_library.dirpath[0] == '\0') {
    return "";
  }

  char libpath[FILE_MAX];
  BLI_strncpy(libpath, user_library.dirpath, sizeof(libpath));
  BLI_path_slash_native(libpath);
  BLI_path_normalize(libpath);

  return std::string(libpath) + SEP + "Saved" + SEP + "Brushes";
}

static std::string brush_asset_blendfile_path_for_save(ReportList *reports,
                                                       const bUserAssetLibrary &user_library,
                                                       const StringRefNull base_name)
{
  std::string root_path = brush_asset_root_path_for_save(user_library);
  BLI_assert(!root_path.empty());

  if (!BLI_dir_create_recursive(root_path.c_str())) {
    BKE_report(reports, RPT_ERROR, "Failed to create asset library directory to save brush");
    return "";
  }

  char base_name_filesafe[FILE_MAXFILE];
  BLI_strncpy(base_name_filesafe, base_name.c_str(), sizeof(base_name_filesafe));
  BLI_path_make_safe_filename(base_name_filesafe);

  if (!BLI_is_file((root_path + SEP + base_name_filesafe + BLENDER_ASSET_FILE_SUFFIX).c_str())) {
    return root_path + SEP + base_name_filesafe + BLENDER_ASSET_FILE_SUFFIX;
  }
  int i = 1;
  while (BLI_is_file((root_path + SEP + base_name_filesafe + "_" + std::to_string(i++) +
                      BLENDER_ASSET_FILE_SUFFIX)
                         .c_str()))
    ;
  return root_path + SEP + base_name_filesafe + "_" + std::to_string(i - 1) +
         BLENDER_ASSET_FILE_SUFFIX;
}

static bool brush_asset_write_in_library(Main *bmain,
                                         Brush *brush,
                                         const char *name,
                                         const StringRefNull filepath,
                                         std::string &final_full_file_path,
                                         ReportList *reports)
{
  /* XXX
   * FIXME
   *
   * This code is _pure evil_. It does in-place manipulation on IDs in global Main database,
   * temporarilly remove them and add them back...
   *
   * Use it as-is for now (in a similar way as python API or copy-to-buffer works). Nut the whole
   * 'BKE_blendfile_write_partial' code needs to be completely refactored.
   *
   * Ideas:
   *   - Have `BKE_blendfile_write_partial_begin` return a new temp Main.
   *   - Replace `BKE_blendfile_write_partial_tag_ID` by API to add IDs to this temp Main.
   *     + This should _duplicate_ the ID, not remove the original one from the source Main!
   *   - Have API to automatically also duplicate dependencies into temp Main.
   *     + Have options to e.g. make all duplicated IDs 'local' (i.e. remove their library data).
   *   - `BKE_blendfile_write_partial` then simply write the given temp main.
   *   - `BKE_blendfile_write_partial_end` frees the temp Main.
   */

  const short brush_flag = brush->id.flag;
  const int brush_tag = brush->id.tag;
  const int brush_us = brush->id.us;
  const std::string brush_name = brush->id.name + 2;
  IDOverrideLibrary *brush_liboverride = brush->id.override_library;
  AssetMetaData *brush_asset_data = brush->id.asset_data;
  const int write_flags = 0; /* Could use #G_FILE_COMPRESS ? */
  const eBLO_WritePathRemap remap_mode = BLO_WRITE_PATH_REMAP_RELATIVE;

  BKE_blendfile_write_partial_begin(bmain);

  brush->id.flag |= LIB_FAKEUSER;
  brush->id.tag &= ~LIB_TAG_RUNTIME;
  brush->id.us = 1;
  BLI_strncpy(brush->id.name + 2, name, sizeof(brush->id.name) - 2);
  if (!ID_IS_ASSET(&brush->id)) {
    brush->id.asset_data = brush->id.override_library->reference->asset_data;
  }
  brush->id.override_library = nullptr;

  BKE_blendfile_write_partial_tag_ID(&brush->id, true);

  /* TODO: check overwriting existing file. */
  /* TODO: ensure filepath contains only valid characters for file system. */
  const bool sucess = BKE_blendfile_write_partial(
      bmain, filepath.c_str(), write_flags, remap_mode, reports);

  if (sucess) {
    final_full_file_path = std::string(filepath) + SEP + "Brush" + SEP + name;
  }

  BKE_blendfile_write_partial_end(bmain);

  BKE_blendfile_write_partial_tag_ID(&brush->id, false);
  brush->id.flag = brush_flag;
  brush->id.tag = brush_tag;
  brush->id.us = brush_us;
  BLI_strncpy(brush->id.name + 2, brush_name.c_str(), sizeof(brush->id.name) - 2);
  brush->id.override_library = brush_liboverride;
  brush->id.asset_data = brush_asset_data;

  return sucess;
}

static bool brush_asset_save_as_poll(bContext *C)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = (paint) ? BKE_paint_brush(paint) : nullptr;
  if (paint == nullptr || brush == nullptr) {
    return false;
  }

  const bUserAssetLibrary *user_library = brush_asset_get_default_library();
  if (user_library == nullptr || user_library->dirpath[0] == '\0') {
    CTX_wm_operator_poll_msg_set(C, "No default asset library available to save to");
    return false;
  }

  return true;
}

static int brush_asset_save_as_exec(bContext *C, wmOperator *op)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = (paint) ? BKE_paint_brush(paint) : nullptr;
  if (paint == nullptr || brush == nullptr) {
    return OPERATOR_CANCELLED;
  }

  /* Determine file path to save to. */
  PropertyRNA *name_prop = RNA_struct_find_property(op->ptr, "name");
  char name[MAX_NAME] = "";
  if (RNA_property_is_set(op->ptr, name_prop)) {
    RNA_property_string_get(op->ptr, name_prop, name);
  }
  if (name[0] == '\0') {
    STRNCPY(name, brush->id.name + 2);
  }

  const bUserAssetLibrary *library = brush_asset_get_default_library();
  if (!library) {
    return OPERATOR_CANCELLED;
  }
  const std::string filepath = brush_asset_blendfile_path_for_save(op->reports, *library, name);
  if (filepath.empty()) {
    return OPERATOR_CANCELLED;
  }

  /* Turn brush into asset if it isn't yet. */
  if (!BKE_paint_brush_is_valid_asset(brush)) {
    asset::mark_id(&brush->id);
    asset::generate_preview(C, &brush->id);
  }
  BLI_assert(BKE_paint_brush_is_valid_asset(brush));

  /* Save to asset library. */
  std::string final_full_asset_filepath;
  const bool sucess = brush_asset_write_in_library(
      CTX_data_main(C), brush, name, filepath, final_full_asset_filepath, op->reports);

  if (!sucess) {
    BKE_report(op->reports, RPT_ERROR, "Failed to write to asset library");
    return OPERATOR_CANCELLED;
  }

  const bUserAssetLibrary *library = brush_asset_get_default_library();
  AssetWeakReference *new_brush_weak_ref = brush_asset_create_weakref_hack(
      library, final_full_asset_filepath);

  /* TODO: maybe not needed, even less so if there is more visual confirmation of change. */
  BKE_reportf(op->reports, RPT_INFO, "Saved \"%s\"", filepath.c_str());

  Main *bmain = CTX_data_main(C);
  brush = BKE_brush_asset_runtime_ensure(bmain, new_brush_weak_ref);

  if (!BKE_paint_brush_asset_set(paint, brush, new_brush_weak_ref)) {
    /* Note brush sset was still saved in editable asset library, so was not a no-op. */
    BKE_report(op->reports, RPT_WARNING, "Unable to activate just-saved brush asset");
  }

  refresh_asset_library(C, *library);
  WM_main_add_notifier(NC_ASSET | ND_ASSET_LIST | NA_ADDED, nullptr);
  WM_main_add_notifier(NC_BRUSH | NA_EDITED, brush);

  return OPERATOR_FINISHED;
}

static int brush_asset_save_as_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = BKE_paint_brush(paint);

  RNA_string_set(op->ptr, "name", brush->id.name + 2);

  /* TODO: add information about the asset library this will be saved to? */
  /* TODO: autofocus name? */
  return WM_operator_props_dialog_popup(C, op, 400);
}

static void BRUSH_OT_asset_save_as(wmOperatorType *ot)
{
  ot->name = "Save As Brush Asset";
  ot->description =
      "Save a copy of the active brush asset into the default asset library, and make it the "
      "active brush";
  ot->idname = "BRUSH_OT_asset_save_as";

  ot->exec = brush_asset_save_as_exec;
  ot->invoke = brush_asset_save_as_invoke;
  ot->poll = brush_asset_save_as_poll;

  RNA_def_string(ot->srna, "name", nullptr, MAX_NAME, "Name", "Name used to save the brush asset");
}

static bool asset_is_editable(const AssetWeakReference &asset_weak_ref)
{
  /* Fairly simple checks, based on filepath only:
   *   - The blendlib filepath ends up with the `.asset.blend` extension.
   *   - The blendlib is located in the expected sub-directory of the editable asset library.
   *
   * TODO: Right now no check is done on file content, e.g. to ensure that the blendlib file has
   * not been manually edited by the user (that it does not have any UI IDs e.g.). */

  char path_buffer[FILE_MAX_LIBEXTRA];
  char *dir, *group, *name;
  AS_asset_full_path_explode_from_weak_ref(&asset_weak_ref, path_buffer, &dir, &group, &name);

  if (!StringRef(dir).endswith(BLENDER_ASSET_FILE_SUFFIX)) {
    return false;
  }

  const bUserAssetLibrary *library = BKE_preferences_asset_library_find_by_name(
      &U, asset_weak_ref.asset_library_identifier);
  if (!library) {
    return false;
  }

  std::string root_path_for_save = brush_asset_root_path_for_save(*library);
  if (root_path_for_save.empty() || !StringRef(dir).startswith(root_path_for_save)) {
    return false;
  }

  /* TODO: Do we want more checks here? E.g. check actual content of the file? */
  return true;
}

static bool brush_asset_delete_poll(bContext *C)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = (paint) ? BKE_paint_brush(paint) : nullptr;
  if (paint == nullptr || brush == nullptr) {
    return false;
  }

  /* Asset brush, check if belongs to an editable blend file. */
  if (paint->brush_asset_reference && BKE_paint_brush_is_valid_asset(brush)) {
    if (!asset_is_editable(*paint->brush_asset_reference)) {
      CTX_wm_operator_poll_msg_set(C, "Asset blend file is not editable");
      return false;
    }
  }

  return true;
}

static int brush_asset_delete_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = BKE_paint_brush(paint);

  bUserAssetLibrary *library = BKE_preferences_asset_library_find_by_name(
      &U, paint->brush_asset_reference->asset_library_identifier);
  if (!library) {
    return OPERATOR_CANCELLED;
  }

  if (paint->brush_asset_reference && BKE_paint_brush_is_valid_asset(brush)) {
    /* Delete from asset library on disk. */
    char path_buffer[FILE_MAX_LIBEXTRA];
    char *filepath;
    AS_asset_full_path_explode_from_weak_ref(
        paint->brush_asset_reference, path_buffer, &filepath, nullptr, nullptr);

    if (BLI_delete(filepath, false, false) != 0) {
      BKE_report(op->reports, RPT_ERROR, "Failed to delete asset library file");
    }
  }

  /* Delete from session. If local override, also delete linked one.
   * TODO: delete both in one step? */
  ID *original_brush = (!ID_IS_LINKED(&brush->id) && ID_IS_OVERRIDE_LIBRARY_REAL(&brush->id)) ?
                           brush->id.override_library->reference :
                           nullptr;
  BKE_id_delete(bmain, brush);
  if (original_brush) {
    BKE_id_delete(bmain, original_brush);
  }

  refresh_asset_library(C, *library);
  WM_main_add_notifier(NC_ASSET | ND_ASSET_LIST | NA_REMOVED, nullptr);
  WM_main_add_notifier(NC_BRUSH | NA_EDITED, nullptr);

  /* TODO: activate default brush. */

  return OPERATOR_FINISHED;
}

static void BRUSH_OT_asset_delete(wmOperatorType *ot)
{
  ot->name = "Delete Brush Asset";
  ot->description = "Delete the active brush asset both from the local session and asset library";
  ot->idname = "BRUSH_OT_asset_delete";

  ot->exec = brush_asset_delete_exec;
  ot->invoke = WM_operator_confirm;
  ot->poll = brush_asset_delete_poll;
}

static bool brush_asset_update_poll(bContext *C)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = (paint) ? BKE_paint_brush(paint) : nullptr;
  if (paint == nullptr || brush == nullptr) {
    return false;
  }

  if (!(paint->brush_asset_reference && BKE_paint_brush_is_valid_asset(brush))) {
    return false;
  }

  if (!asset_is_editable(*paint->brush_asset_reference)) {
    CTX_wm_operator_poll_msg_set(C, "Asset blend file is not editable");
    return false;
  }

  return true;
}

static int brush_asset_update_exec(bContext *C, wmOperator *op)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = nullptr;
  const AssetWeakReference *asset_weak_ref =
      BKE_paint_brush_asset_get(paint, &brush).value_or(nullptr);

  char path_buffer[FILE_MAX_LIBEXTRA];
  char *filepath;
  AS_asset_full_path_explode_from_weak_ref(
      asset_weak_ref, path_buffer, &filepath, nullptr, nullptr);

  BLI_assert(BKE_paint_brush_is_valid_asset(brush));

  std::string final_full_asset_filepath;
  brush_asset_write_in_library(CTX_data_main(C),
                               brush,
                               brush->id.name + 2,
                               filepath,
                               final_full_asset_filepath,
                               op->reports);

  return OPERATOR_FINISHED;
}

static void BRUSH_OT_asset_update(wmOperatorType *ot)
{
  ot->name = "Update Brush Asset";
  ot->description = "Update the active brush asset in the asset library with current settings";
  ot->idname = "BRUSH_OT_asset_update";

  ot->exec = brush_asset_update_exec;
  ot->poll = brush_asset_update_poll;
}

static bool brush_asset_revert_poll(bContext *C)
{
  /* TODO: check if there is anything to revert? */
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = (paint) ? BKE_paint_brush(paint) : nullptr;
  if (paint == nullptr || brush == nullptr) {
    return false;
  }

  return paint->brush_asset_reference && BKE_paint_brush_is_valid_asset(brush);
}

static int brush_asset_revert_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = BKE_paint_brush(paint);

  /* TODO: check if doing this for the hierarchy is ok. */
  /* TODO: the overrides don't update immediately when tweaking brush settings. */
  BKE_lib_override_library_id_hierarchy_reset(bmain, &brush->id, false);

  WM_main_add_notifier(NC_BRUSH | NA_EDITED, brush);

  return OPERATOR_FINISHED;
}

static void BRUSH_OT_asset_revert(wmOperatorType *ot)
{
  ot->name = "Revert Brush Asset";
  ot->description =
      "Revert the active brush settings to the default values from the asset library";
  ot->idname = "BRUSH_OT_asset_revert";

  ot->exec = brush_asset_revert_exec;
  ot->poll = brush_asset_revert_poll;
}

}  // namespace blender::ed::sculpt_paint

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

static int stencil_control_invoke(bContext *C, wmOperator *op, const wmEvent *event)
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

  scd = static_cast<StencilControlData *>(MEM_mallocN(sizeof(StencilControlData), __func__));
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
      break;
    }
  }
#undef PIXEL_MARGIN
}

static int stencil_control_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  StencilControlData *scd = static_cast<StencilControlData *>(op->customdata);

  if (event->type == scd->launch_event && event->val == KM_RELEASE) {
    MEM_freeN(op->customdata);
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

  /* api callbacks */
  ot->invoke = stencil_control_invoke;
  ot->modal = stencil_control_modal;
  ot->cancel = stencil_control_cancel;
  ot->poll = stencil_control_poll;

  /* flags */
  ot->flag = 0;

  PropertyRNA *prop;
  prop = RNA_def_enum(ot->srna, "mode", stencil_control_items, STENCIL_TRANSLATE, "Tool", "");
  RNA_def_property_flag(prop, PropertyFlag(PROP_HIDDEN | PROP_SKIP_SAVE));
  prop = RNA_def_enum(ot->srna, "texmode", stencil_texture_items, STENCIL_PRIMARY, "Tool", "");
  RNA_def_property_flag(prop, PropertyFlag(PROP_HIDDEN | PROP_SKIP_SAVE));
}

static int stencil_fit_image_aspect_exec(bContext *C, wmOperator *op)
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

  /* api callbacks */
  ot->exec = stencil_fit_image_aspect_exec;
  ot->poll = stencil_control_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna, "use_repeat", true, "Use Repeat", "Use repeat mapping values");
  RNA_def_boolean(ot->srna, "use_scale", true, "Use Scale", "Use texture scale values");
  RNA_def_boolean(
      ot->srna, "mask", false, "Modify Mask Stencil", "Modify either the primary or mask stencil");
}

static int stencil_reset_transform_exec(bContext *C, wmOperator *op)
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

  WM_event_add_notifier(C, NC_WINDOW, nullptr);

  return OPERATOR_FINISHED;
}

static void BRUSH_OT_stencil_reset_transform(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Reset Transform";
  ot->description = "Reset the stencil transformation to the default";
  ot->idname = "BRUSH_OT_stencil_reset_transform";

  /* api callbacks */
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
  WM_operatortype_append(BRUSH_OT_add);
  WM_operatortype_append(BRUSH_OT_add_gpencil);
  WM_operatortype_append(BRUSH_OT_scale_size);
  WM_operatortype_append(BRUSH_OT_curve_preset);
  WM_operatortype_append(BRUSH_OT_sculpt_curves_falloff_preset);
  WM_operatortype_append(BRUSH_OT_reset);
  WM_operatortype_append(BRUSH_OT_stencil_control);
  WM_operatortype_append(BRUSH_OT_stencil_fit_image_aspect);
  WM_operatortype_append(BRUSH_OT_stencil_reset_transform);
  WM_operatortype_append(BRUSH_OT_asset_select);
  WM_operatortype_append(BRUSH_OT_asset_save_as);
  WM_operatortype_append(BRUSH_OT_asset_delete);
  WM_operatortype_append(BRUSH_OT_asset_update);
  WM_operatortype_append(BRUSH_OT_asset_revert);

  /* NOTE: particle uses a different system, can be added with existing operators in `wm.py`. */
  WM_operatortype_append(PAINT_OT_brush_select);

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
  WM_operatortype_append(SCULPT_OT_uv_sculpt_stroke);

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
  WM_operatortype_append(hide::PAINT_OT_hide_show);
  WM_operatortype_append(hide::PAINT_OT_visibility_invert);

  /* paint masking */
  WM_operatortype_append(mask::PAINT_OT_mask_flood_fill);
  WM_operatortype_append(mask::PAINT_OT_mask_lasso_gesture);
  WM_operatortype_append(mask::PAINT_OT_mask_box_gesture);
  WM_operatortype_append(mask::PAINT_OT_mask_line_gesture);
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
