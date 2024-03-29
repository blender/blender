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

#include "IMB_interp.hh"

#include "DNA_brush_types.h"
#include "DNA_customdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLO_writefile.hh"

#include "BKE_asset.hh"
#include "BKE_asset_edit.hh"
#include "BKE_blendfile.hh"
#include "BKE_brush.hh"
#include "BKE_context.hh"
#include "BKE_image.h"
#include "BKE_lib_id.hh"
#include "BKE_lib_remap.hh"
#include "BKE_main.hh"
#include "BKE_paint.hh"
#include "BKE_preferences.h"
#include "BKE_report.hh"

#include "ED_asset_handle.hh"
#include "ED_asset_library.hh"
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

#include "BLT_translation.hh"

#include "AS_asset_catalog_path.hh"
#include "AS_asset_catalog_tree.hh"
#include "AS_asset_library.hh"
#include "AS_asset_representation.hh"

#include "UI_interface_icons.hh"
#include "UI_resources.hh"

#include "curves_sculpt_intern.hh"
#include "paint_intern.hh"
#include "sculpt_intern.hh"

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
  Main *bmain = CTX_data_main(C);  // TODO: add to asset main?

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

  const Brush *brush = BKE_paint_brush_for_read(paint);
  if (brush) {
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

namespace blender::ed::sculpt_paint {

/**************************** Brush Assets **********************************/

static int brush_asset_select_exec(bContext *C, wmOperator *op)
{
  /* This operator currently covers both cases: the file/asset browser file list and the asset list
   * used for the asset-view template. Once the asset list design is used by the Asset Browser,
   * this can be simplified to just that case. */
  Main *bmain = CTX_data_main(C);
  const asset_system::AssetRepresentation *asset =
      asset::operator_asset_reference_props_get_asset_from_all_library(*C, *op->ptr, op->reports);
  if (!asset) {
    return OPERATOR_CANCELLED;
  }

  AssetWeakReference brush_asset_reference = asset->make_weak_reference();
  Brush *brush = reinterpret_cast<Brush *>(
      blender::bke::asset_edit_id_from_weak_reference(*bmain, ID_BR, brush_asset_reference));

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
static AssetWeakReference brush_asset_create_weakref_hack(const bUserAssetLibrary *user_asset_lib,
                                                          const std::string &file_path)
{
  AssetWeakReference asset_weak_ref{};

  StringRef asset_root_path = user_asset_lib->dirpath;
  BLI_assert(file_path.find(asset_root_path) == 0);
  std::string relative_asset_path = file_path.substr(size_t(asset_root_path.size()) + 1);

  asset_weak_ref.asset_library_type = ASSET_LIBRARY_CUSTOM;
  asset_weak_ref.asset_library_identifier = BLI_strdup(user_asset_lib->name);
  asset_weak_ref.relative_asset_identifier = BLI_strdupn(relative_asset_path.c_str(),
                                                         relative_asset_path.size());

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

static bool brush_asset_save_as_poll(bContext *C)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = (paint) ? BKE_paint_brush(paint) : nullptr;
  if (paint == nullptr || brush == nullptr) {
    return false;
  }

  if (BLI_listbase_is_empty(&U.asset_libraries)) {
    CTX_wm_operator_poll_msg_set(C, "No asset library available to save to");
    return false;
  }

  return true;
}

static const bUserAssetLibrary *get_asset_library_from_prop(PointerRNA &ptr)
{
  const int enum_value = RNA_enum_get(&ptr, "asset_library_reference");
  const AssetLibraryReference lib_ref = asset::library_reference_from_enum_value(enum_value);
  return BKE_preferences_asset_library_find_index(&U, lib_ref.custom_library_index);
}

static asset_system::AssetCatalog &asset_library_ensure_catalog(
    asset_system::AssetLibrary &library, const asset_system::AssetCatalogPath &path)
{
  if (asset_system::AssetCatalog *catalog = library.catalog_service().find_catalog_by_path(path)) {
    return *catalog;
  }
  return *library.catalog_service().create_catalog(path);
}

static asset_system::AssetCatalog &asset_library_ensure_catalogs_in_path(
    asset_system::AssetLibrary &library, const asset_system::AssetCatalogPath &path)
{
  /* Adding multiple catalogs in a path at a time with #AssetCatalogService::create_catalog()
   * doesn't work; add each potentially new catalog in the hierarchy manually here. */
  asset_system::AssetCatalogPath parent = "";
  path.iterate_components([&](StringRef component_name, bool /*is_last_component*/) {
    asset_library_ensure_catalog(library, parent / component_name);
    parent = parent / component_name;
  });
  return *library.catalog_service().find_catalog_by_path(path);
}

static AssetLibraryReference user_library_to_library_ref(const bUserAssetLibrary &user_library)
{
  AssetLibraryReference library_ref{};
  library_ref.custom_library_index = BLI_findindex(&U.asset_libraries, &user_library);
  library_ref.type = ASSET_LIBRARY_CUSTOM;
  return library_ref;
}

static int brush_asset_save_as_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
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

  const bUserAssetLibrary *user_library = get_asset_library_from_prop(*op->ptr);
  if (!user_library) {
    return OPERATOR_CANCELLED;
  }

  asset_system::AssetLibrary *library = AS_asset_library_load(
      CTX_data_main(C), user_library_to_library_ref(*user_library));
  if (!library) {
    BKE_report(op->reports, RPT_ERROR, "Failed to load asset library");
    return OPERATOR_CANCELLED;
  }

  /* Turn brush into asset if it isn't yet. */
  if (!ID_IS_ASSET(&brush->id)) {
    asset::mark_id(&brush->id);
    asset::generate_preview(C, &brush->id);
  }
  BLI_assert(ID_IS_ASSET(&brush->id));

  /* Add asset to catalog. */
  char catalog_path[MAX_NAME];
  RNA_string_get(op->ptr, "catalog_path", catalog_path);

  std::optional<asset_system::CatalogID> catalog_id;
  std::optional<StringRefNull> catalog_simple_name;

  if (catalog_path[0]) {
    const asset_system::AssetCatalog &catalog = asset_library_ensure_catalogs_in_path(
        *library, catalog_path);
    catalog_id = catalog.catalog_id;
    catalog_simple_name = catalog.simple_name;
  }

  const std::optional<std::string> final_full_asset_filepath = blender::bke::asset_edit_id_save_as(
      *bmain, &brush->id, name, catalog_id, catalog_simple_name, user_library, op->reports);

  if (!final_full_asset_filepath) {
    return OPERATOR_CANCELLED;
  }

  library->catalog_service().write_to_disk(*final_full_asset_filepath);

  AssetWeakReference new_brush_weak_ref = brush_asset_create_weakref_hack(
      user_library, *final_full_asset_filepath);

  brush = reinterpret_cast<Brush *>(
      blender::bke::asset_edit_id_from_weak_reference(*bmain, ID_BR, new_brush_weak_ref));

  if (!BKE_paint_brush_asset_set(paint, brush, new_brush_weak_ref)) {
    /* Note brush sset was still saved in editable asset library, so was not a no-op. */
    BKE_report(op->reports, RPT_WARNING, "Unable to activate just-saved brush asset");
  }

  refresh_asset_library(C, *user_library);
  WM_main_add_notifier(NC_ASSET | ND_ASSET_LIST | NA_ADDED, nullptr);
  WM_main_add_notifier(NC_BRUSH | NA_EDITED, brush);

  return OPERATOR_FINISHED;
}

static int brush_asset_save_as_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = BKE_paint_brush(paint);

  RNA_string_set(op->ptr, "name", brush->id.name + 2);

  if (const bUserAssetLibrary *library = brush_asset_get_default_library()) {
    const AssetLibraryReference library_ref = user_library_to_library_ref(*library);
    const int enum_value = asset::library_reference_to_enum_value(&library_ref);
    RNA_enum_set(op->ptr, "asset_library_reference", enum_value);
  }

  return WM_operator_props_dialog_popup(C, op, 400, std::nullopt, IFACE_("Save"));
}

static const EnumPropertyItem *rna_asset_library_reference_itemf(bContext * /*C*/,
                                                                 PointerRNA * /*ptr*/,
                                                                 PropertyRNA * /*prop*/,
                                                                 bool *r_free)
{
  const EnumPropertyItem *items = asset::library_reference_to_rna_enum_itemf(false);
  if (!items) {
    *r_free = false;
    return nullptr;
  }

  *r_free = true;
  return items;
}

static void visit_asset_catalog_for_search_fn(
    const bContext *C,
    PointerRNA *ptr,
    PropertyRNA * /*prop*/,
    const char *edit_text,
    FunctionRef<void(StringPropertySearchVisitParams)> visit_fn)
{
  const bUserAssetLibrary *user_library = get_asset_library_from_prop(*ptr);
  if (!user_library) {
    return;
  }

  asset_system::AssetLibrary *library = AS_asset_library_load(
      CTX_data_main(C), user_library_to_library_ref(*user_library));
  if (!library) {
    return;
  }

  if (edit_text && edit_text[0] != '\0') {
    asset_system::AssetCatalogPath edit_path = edit_text;
    if (!library->catalog_service().find_catalog_by_path(edit_path)) {
      visit_fn(StringPropertySearchVisitParams{edit_path.str(), std::nullopt, ICON_ADD});
    }
  }

  const asset_system::AssetCatalogTree &full_tree = library->catalog_service().catalog_tree();
  full_tree.foreach_item([&](const asset_system::AssetCatalogTreeItem &item) {
    visit_fn(StringPropertySearchVisitParams{item.catalog_path().str(), std::nullopt});
  });
}

static void BRUSH_OT_asset_save_as(wmOperatorType *ot)
{
  ot->name = "Save as Brush Asset";
  ot->description =
      "Save a copy of the active brush asset into the default asset library, and make it the "
      "active brush";
  ot->idname = "BRUSH_OT_asset_save_as";

  ot->exec = brush_asset_save_as_exec;
  ot->invoke = brush_asset_save_as_invoke;
  ot->poll = brush_asset_save_as_poll;

  ot->prop = RNA_def_string(
      ot->srna, "name", nullptr, MAX_NAME, "Name", "Name for the new brush asset");

  PropertyRNA *prop = RNA_def_property(ot->srna, "asset_library_reference", PROP_ENUM, PROP_NONE);
  RNA_def_enum_funcs(prop, rna_asset_library_reference_itemf);
  RNA_def_property_ui_text(prop, "Library", "Asset library used to store the new brush");

  prop = RNA_def_string(
      ot->srna, "catalog_path", nullptr, MAX_NAME, "Catalog", "Catalog to use for the new asset");
  RNA_def_property_string_search_func_runtime(
      prop, visit_asset_catalog_for_search_fn, PROP_STRING_SEARCH_SUGGESTION);
}

static bool brush_asset_delete_poll(bContext *C)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = (paint) ? BKE_paint_brush(paint) : nullptr;
  if (paint == nullptr || brush == nullptr) {
    return false;
  }

  /* Asset brush, check if belongs to an editable blend file. */
  if (paint->brush_asset_reference && ID_IS_ASSET(brush)) {
    if (!blender::bke::asset_edit_id_is_editable(&brush->id)) {
      CTX_wm_operator_poll_msg_set(C, "Asset blend file is not editable");
      return false;
    }
  }

  return true;
}

static int brush_asset_delete_exec(bContext *C, wmOperator *op)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = BKE_paint_brush(paint);
  Main *bmain = CTX_data_main(C);

  bUserAssetLibrary *library = BKE_preferences_asset_library_find_by_name(
      &U, paint->brush_asset_reference->asset_library_identifier);
  if (!library) {
    return OPERATOR_CANCELLED;
  }

  blender::bke::asset_edit_id_delete(*bmain, &brush->id, op->reports);

  refresh_asset_library(C, *library);

  BKE_paint_brush_set_default(bmain, paint);

  WM_main_add_notifier(NC_ASSET | ND_ASSET_LIST | NA_REMOVED, nullptr);
  WM_main_add_notifier(NC_BRUSH | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

static int brush_asset_delete_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  return WM_operator_confirm_ex(
      C,
      op,
      IFACE_("Delete Brush Asset"),
      IFACE_("Permanently delete brush asset blend file. This can't be undone."),
      IFACE_("Delete"),
      ALERT_ICON_WARNING,
      false);
}

static void BRUSH_OT_asset_delete(wmOperatorType *ot)
{
  ot->name = "Delete Brush Asset";
  ot->description = "Delete the active brush asset both from the local session and asset library";
  ot->idname = "BRUSH_OT_asset_delete";

  ot->exec = brush_asset_delete_exec;
  ot->invoke = brush_asset_delete_invoke;
  ot->poll = brush_asset_delete_poll;
}

static bool brush_asset_update_poll(bContext *C)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = (paint) ? BKE_paint_brush(paint) : nullptr;
  if (paint == nullptr || brush == nullptr) {
    return false;
  }

  if ((brush->id.tag & LIB_TAG_ASSET_MAIN) == 0) {
    return false;
  }

  if (!(paint->brush_asset_reference && ID_IS_ASSET(brush))) {
    return false;
  }

  if (!blender::bke::asset_edit_id_is_editable(&brush->id)) {
    CTX_wm_operator_poll_msg_set(C, "Asset blend file is not editable");
    return false;
  }

  return true;
}

static int brush_asset_update_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = BKE_paint_brush(paint);
  const AssetWeakReference *asset_weak_ref = paint->brush_asset_reference;

  const bUserAssetLibrary *user_library = BKE_preferences_asset_library_find_by_name(
      &U, asset_weak_ref->asset_library_identifier);
  if (!user_library) {
    return OPERATOR_CANCELLED;
  }

  BLI_assert(ID_IS_ASSET(brush));

  blender::bke::asset_edit_id_save(*bmain, &brush->id, op->reports);

  refresh_asset_library(C, *user_library);
  WM_main_add_notifier(NC_ASSET | ND_ASSET_LIST | NA_EDITED, nullptr);
  WM_main_add_notifier(NC_BRUSH | NA_EDITED, brush);

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
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = (paint) ? BKE_paint_brush(paint) : nullptr;
  if (paint == nullptr || brush == nullptr) {
    return false;
  }

  return paint->brush_asset_reference && (brush->id.tag & LIB_TAG_ASSET_MAIN);
}

static int brush_asset_revert_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = BKE_paint_brush(paint);

  blender::bke::asset_edit_id_revert(*bmain, &brush->id, op->reports);

  WM_main_add_notifier(NC_BRUSH | NA_EDITED, nullptr);
  WM_main_add_notifier(NC_TEXTURE | ND_NODES, nullptr);

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
  WM_operatortype_append(BRUSH_OT_add_gpencil);
  WM_operatortype_append(BRUSH_OT_scale_size);
  WM_operatortype_append(BRUSH_OT_curve_preset);
  WM_operatortype_append(BRUSH_OT_sculpt_curves_falloff_preset);
  WM_operatortype_append(BRUSH_OT_stencil_control);
  WM_operatortype_append(BRUSH_OT_stencil_fit_image_aspect);
  WM_operatortype_append(BRUSH_OT_stencil_reset_transform);
  WM_operatortype_append(BRUSH_OT_asset_select);
  WM_operatortype_append(BRUSH_OT_asset_save_as);
  WM_operatortype_append(BRUSH_OT_asset_delete);
  WM_operatortype_append(BRUSH_OT_asset_update);
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
  WM_operatortype_append(hide::PAINT_OT_hide_show_all);
  WM_operatortype_append(hide::PAINT_OT_hide_show_masked);
  WM_operatortype_append(hide::PAINT_OT_hide_show);
  WM_operatortype_append(hide::PAINT_OT_hide_show_lasso_gesture);
  WM_operatortype_append(hide::PAINT_OT_hide_show_line_gesture);
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
