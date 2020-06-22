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
 */

/** \file
 * \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"
#include <stdlib.h>

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "DNA_brush_types.h"
#include "DNA_customdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_brush.h"
#include "BKE_context.h"
#include "BKE_image.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_paint.h"
#include "BKE_report.h"

#include "ED_image.h"
#include "ED_paint.h"
#include "ED_screen.h"

#include "WM_api.h"
#include "WM_toolsystem.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "paint_intern.h"
#include "sculpt_intern.h"

#include <string.h>
//#include <stdio.h>
#include <stddef.h>

/* Brush operators */
static int brush_add_exec(bContext *C, wmOperator *UNUSED(op))
{
  /*int type = RNA_enum_get(op->ptr, "type");*/
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *br = BKE_paint_brush(paint);
  Main *bmain = CTX_data_main(C);
  ePaintMode mode = BKE_paintmode_get_active_from_context(C);

  if (br) {
    br = BKE_brush_copy(bmain, br);
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

static int brush_add_gpencil_exec(bContext *C, wmOperator *UNUSED(op))
{
  /*int type = RNA_enum_get(op->ptr, "type");*/
  ToolSettings *ts = CTX_data_tool_settings(C);
  Paint *paint = &ts->gp_paint->paint;
  Brush *br = BKE_paint_brush(paint);
  Main *bmain = CTX_data_main(C);

  if (br) {
    br = BKE_brush_copy(bmain, br);
  }
  else {
    br = BKE_brush_add(bmain, "Brush", OB_MODE_PAINT_GPENCIL);

    /* Init grease pencil specific data. */
    BKE_brush_init_gpencil_settings(br);
  }

  id_us_min(&br->id); /* fake user only */

  BKE_paint_brush_set(paint, br);

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
  // Object *ob = CTX_data_active_object(C);
  float scalar = RNA_float_get(op->ptr, "scalar");

  if (brush) {
    // pixel radius
    {
      const int old_size = BKE_brush_size_get(scene, brush);
      int size = (int)(scalar * old_size);

      if (abs(old_size - size) < U.pixelsize) {
        if (scalar > 1) {
          size += U.pixelsize;
        }
        else if (scalar < 1) {
          size -= U.pixelsize;
        }
      }

      BKE_brush_size_set(scene, brush, size);
    }

    // unprojected radius
    {
      float unprojected_radius = scalar * BKE_brush_unprojected_radius_get(scene, brush);

      if (unprojected_radius < 0.001f) {  // XXX magic number
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

static int palette_new_exec(bContext *C, wmOperator *UNUSED(op))
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

  if (paint && paint->palette != NULL) {
    return true;
  }

  return false;
}

static int palette_color_add_exec(bContext *C, wmOperator *UNUSED(op))
{
  Scene *scene = CTX_data_scene(C);
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *brush = paint->brush;
  ePaintMode mode = BKE_paintmode_get_active_from_context(C);
  Palette *palette = paint->palette;
  PaletteColor *color;

  color = BKE_palette_color_add(palette);
  palette->active_color = BLI_listbase_count(&palette->colors) - 1;

  if (ELEM(mode, PAINT_MODE_TEXTURE_3D, PAINT_MODE_TEXTURE_2D, PAINT_MODE_VERTEX)) {
    copy_v3_v3(color->rgb, BKE_brush_color_get(scene, brush));
    color->value = 0.0;
  }
  else if (mode == PAINT_MODE_WEIGHT) {
    zero_v3(color->rgb);
    color->value = brush->weight;
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

static int palette_color_delete_exec(bContext *C, wmOperator *UNUSED(op))
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  Palette *palette = paint->palette;
  PaletteColor *color = BLI_findlink(&palette->colors, palette->active_color);

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
  if ((sl != NULL) && (sl->spacetype == SPACE_IMAGE)) {
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

  if (ibuf && ibuf->rect) {
    /* Extract all colors. */
    for (int row = 0; row < ibuf->y; row++) {
      for (int col = 0; col < ibuf->x; col++) {
        float color[4];
        IMB_sampleImageAtLocation(ibuf, (float)col, (float)row, false, color);
        const float range = pow(10.0f, threshold);
        color[0] = truncf(color[0] * range) / range;
        color[1] = truncf(color[1] * range) / range;
        color[2] = truncf(color[2] * range) / range;

        uint key = rgb_to_cpack(color[0], color[1], color[2]);
        if (!BLI_ghash_haskey(color_table, POINTER_FROM_INT(key))) {
          BLI_ghash_insert(color_table, POINTER_FROM_INT(key), POINTER_FROM_INT(key));
        }
      }
    }

    done = BKE_palette_from_hash(bmain, color_table, image->id.name + 2, false);
  }

  /* Free memory. */
  BLI_ghash_free(color_table, NULL, NULL);
  BKE_image_release_ibuf(image, ibuf, lock);

  if (done) {
    BKE_reportf(op->reports, RPT_INFO, "Palette created");
  }

  return OPERATOR_FINISHED;
}

static void PALETTE_OT_extract_from_image(wmOperatorType *ot)
{
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
  RNA_def_int(ot->srna, "threshold", 1, 1, 4, "Threshold", "", 1, 4);
}

/* Sort Palette color by Hue and Saturation. */
static int palette_sort_exec(bContext *C, wmOperator *op)
{
  const int type = RNA_enum_get(op->ptr, "type");

  Paint *paint = BKE_paint_get_active_from_context(C);
  Palette *palette = paint->palette;

  if (palette == NULL) {
    return OPERATOR_CANCELLED;
  }

  tPaletteColorHSV *color_array = NULL;
  tPaletteColorHSV *col_elm = NULL;

  const int totcol = BLI_listbase_count(&palette->colors);

  if (totcol > 0) {
    color_array = MEM_calloc_arrayN(totcol, sizeof(tPaletteColorHSV), __func__);
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
    PaletteColor *color_next = NULL;
    for (PaletteColor *color = palette->colors.first; color; color = color_next) {
      color_next = color->next;
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

  WM_event_add_notifier(C, NC_BRUSH | NA_EDITED, NULL);

  return OPERATOR_FINISHED;
}

static void PALETTE_OT_sort(wmOperatorType *ot)
{
  static const EnumPropertyItem sort_type[] = {
      {1, "HSV", 0, "Hue, Saturation, Value", ""},
      {2, "SVH", 0, "Saturation, Value, Hue", ""},
      {3, "VHS", 0, "Value, Hue, Saturation", ""},
      {4, "LUMINANCE", 0, "Luminance", ""},
      {0, NULL, 0, NULL, NULL},
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
  PaletteColor *palcolor = BLI_findlink(&palette->colors, palette->active_color);

  if (palcolor == NULL) {
    return OPERATOR_CANCELLED;
  }

  const int direction = RNA_enum_get(op->ptr, "type");

  BLI_assert(ELEM(direction, -1, 0, 1)); /* we use value below */
  if (BLI_listbase_link_move(&palette->colors, palcolor, direction)) {
    palette->active_color += direction;
    WM_event_add_notifier(C, NC_BRUSH | NA_EDITED, NULL);
  }

  return OPERATOR_FINISHED;
}

static void PALETTE_OT_color_move(wmOperatorType *ot)
{
  static const EnumPropertyItem slot_move[] = {
      {-1, "UP", 0, "Up", ""},
      {1, "DOWN", 0, "Down", ""},
      {0, NULL, 0, NULL, NULL},
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
  Palette *palette_join = NULL;
  bool done = false;

  char name[MAX_ID_NAME - 2];
  RNA_string_get(op->ptr, "palette", name);

  if ((palette == NULL) || (name[0] == '\0')) {
    return OPERATOR_CANCELLED;
  }

  palette_join = (Palette *)BKE_libblock_find_name(bmain, ID_PAL, name);
  if (palette_join == NULL) {
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
    PaletteColor *color_next = NULL;
    for (PaletteColor *color = palette_join->colors.first; color; color = color_next) {
      color_next = color->next;
      BKE_palette_color_remove(palette_join, color);
    }

    /* Notifier. */
    WM_event_add_notifier(C, NC_BRUSH | NA_EDITED, NULL);
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
  RNA_def_string(ot->srna, "palette", NULL, MAX_ID_NAME - 2, "Palette", "Name of the Palette");
}

static int brush_reset_exec(bContext *C, wmOperator *UNUSED(op))
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

  if (!brush_orig && !(brush_orig = bmain->brushes.first)) {
    return NULL;
  }

  if (brush_tool(brush_orig, paint->runtime.tool_offset) != tool) {
    /* If current brush's tool is different from what we need,
     * start cycling from the beginning of the list.
     * Such logic will activate the same exact brush not relating from
     * which tool user requests other tool.
     */

    /* Try to tool-slot first. */
    first_brush = BKE_paint_toolslots_brush_get(paint, tool);
    if (first_brush == NULL) {
      first_brush = bmain->brushes.first;
    }
  }
  else {
    /* If user wants to switch to brush with the same  tool as
     * currently active brush do a cycling via all possible
     * brushes with requested tool.
     */
    first_brush = brush_orig->id.next ? brush_orig->id.next : bmain->brushes.first;
  }

  /* get the next brush with the active tool */
  brush = first_brush;
  do {
    if ((brush->ob_mode & paint->runtime.ob_mode) &&
        (brush_tool(brush, paint->runtime.tool_offset) == tool)) {
      return brush;
    }

    brush = brush->id.next ? brush->id.next : bmain->brushes.first;
  } while (brush != first_brush);

  return NULL;
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
  else if (brush_orig->toggle_brush) {
    /* if current brush is using the desired tool, try to toggle
     * back to the previously selected brush. */
    return brush_orig->toggle_brush;
  }
  else {
    return NULL;
  }
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

  if (((brush == NULL) && create_missing) &&
      ((brush_orig == NULL) || brush_tool(brush_orig, paint->runtime.tool_offset) != tool)) {
    brush = BKE_brush_add(bmain, tool_name, paint->runtime.ob_mode);
    id_us_min(&brush->id); /* fake user only */
    brush_tool_set(brush, paint->runtime.tool_offset, tool);
    brush->toggle_brush = brush_orig;
  }

  if (brush) {
    BKE_paint_brush_set(paint, brush);
    BKE_paint_invalidate_overlay_all();

    WM_main_add_notifier(NC_BRUSH | NA_EDITED, brush);

    /* Tool System
     * This is needed for when there is a non-sculpt tool active (transform for e.g.).
     * In case we are toggling (and the brush changed to the toggle_brush), we need to get the
     * tool_name again. */
    int tool_result = brush_tool(brush, paint->runtime.tool_offset);
    ePaintMode paint_mode = BKE_paintmode_get_active_from_context(C);
    const EnumPropertyItem *items = BKE_paint_get_tool_enum_from_paintmode(paint_mode);
    RNA_enum_name_from_value(items, tool_result, &tool_name);

    char tool_id[MAX_NAME];
    SNPRINTF(tool_id, "builtin_brush.%s", tool_name);
    WM_toolsystem_ref_set_by_id(C, tool_id);

    return true;
  }
  else {
    return false;
  }
}

static const ePaintMode brush_select_paint_modes[] = {
    PAINT_MODE_SCULPT,
    PAINT_MODE_VERTEX,
    PAINT_MODE_WEIGHT,
    PAINT_MODE_TEXTURE_3D,
    PAINT_MODE_GPENCIL,
    PAINT_MODE_VERTEX_GPENCIL,
    PAINT_MODE_SCULPT_GPENCIL,
    PAINT_MODE_WEIGHT_GPENCIL,
};

static int brush_select_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  const bool create_missing = RNA_boolean_get(op->ptr, "create_missing");
  const bool toggle = RNA_boolean_get(op->ptr, "toggle");
  const char *tool_name = "Brush";
  int tool = 0;

  ePaintMode paint_mode = PAINT_MODE_INVALID;
  for (int i = 0; i < ARRAY_SIZE(brush_select_paint_modes); i++) {
    paint_mode = brush_select_paint_modes[i];
    const char *op_prop_id = BKE_paint_get_tool_prop_id_from_paintmode(paint_mode);
    PropertyRNA *prop = RNA_struct_find_property(op->ptr, op_prop_id);
    if (RNA_property_is_set(op->ptr, prop)) {
      tool = RNA_property_enum_get(op->ptr, prop);
      break;
    }
  }

  if (paint_mode == PAINT_MODE_INVALID) {
    return OPERATOR_CANCELLED;
  }

  Paint *paint = BKE_paint_get_active_from_paintmode(scene, paint_mode);
  if (paint == NULL) {
    return OPERATOR_CANCELLED;
  }
  const EnumPropertyItem *items = BKE_paint_get_tool_enum_from_paintmode(paint_mode);
  RNA_enum_name_from_value(items, tool, &tool_name);

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
    const ePaintMode paint_mode = brush_select_paint_modes[i];
    const char *prop_id = BKE_paint_get_tool_prop_id_from_paintmode(paint_mode);
    prop = RNA_def_enum(
        ot->srna, prop_id, BKE_paint_get_tool_enum_from_paintmode(paint_mode), 0, prop_id, "");
    RNA_def_property_flag(prop, PROP_HIDDEN);
  }

  prop = RNA_def_boolean(
      ot->srna, "toggle", 0, "Toggle", "Toggle between two brushes rather than cycling");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  prop = RNA_def_boolean(ot->srna,
                         "create_missing",
                         0,
                         "Create Missing",
                         "If the requested brush type does not exist, create a new brush");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

/***** Stencil Control *****/

typedef enum {
  STENCIL_TRANSLATE,
  STENCIL_SCALE,
  STENCIL_ROTATE,
} StencilControlMode;

typedef enum {
  STENCIL_PRIMARY = 0,
  STENCIL_SECONDARY = 1,
} StencilTextureMode;

typedef enum {
  STENCIL_CONSTRAINT_X = 1,
  STENCIL_CONSTRAINT_Y = 2,
} StencilConstraint;

typedef struct {
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
} StencilControlData;

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
  float mvalf[2] = {event->mval[0], event->mval[1]};
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

  scd = MEM_mallocN(sizeof(StencilControlData), "stencil_control");
  scd->mask = mask;
  scd->br = br;

  copy_v2_v2(scd->init_mouse, mvalf);

  stencil_set_target(scd);

  scd->mode = RNA_enum_get(op->ptr, "mode");
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

static void stencil_control_cancel(bContext *UNUSED(C), wmOperator *op)
{
  StencilControlData *scd = op->customdata;

  stencil_restore(scd);
  MEM_freeN(op->customdata);
}

static void stencil_control_calculate(StencilControlData *scd, const int mval[2])
{
#define PIXEL_MARGIN 5

  float mdiff[2];
  float mvalf[2] = {mval[0], mval[1]};
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
        angle += (float)(2 * M_PI);
      }
      if (angle > (float)(2 * M_PI)) {
        angle -= (float)(2 * M_PI);
      }
      *scd->rot_target = angle;
      break;
    }
  }
#undef PIXEL_MARGIN
}

static int stencil_control_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  StencilControlData *scd = op->customdata;

  if (event->type == scd->launch_event && event->val == KM_RELEASE) {
    MEM_freeN(op->customdata);
    WM_event_add_notifier(C, NC_WINDOW, NULL);
    return OPERATOR_FINISHED;
  }

  switch (event->type) {
    case MOUSEMOVE:
      stencil_control_calculate(scd, event->mval);
      break;
    case EVT_ESCKEY:
      if (event->val == KM_PRESS) {
        stencil_control_cancel(C, op);
        WM_event_add_notifier(C, NC_WINDOW, NULL);
        return OPERATOR_CANCELLED;
      }
      break;
    case EVT_XKEY:
      if (event->val == KM_PRESS) {

        if (scd->constrain_mode == STENCIL_CONSTRAINT_X) {
          scd->constrain_mode = 0;
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
          scd->constrain_mode = 0;
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
  ePaintMode mode = BKE_paintmode_get_active_from_context(C);

  Paint *paint;
  Brush *br;

  if (!paint_supports_texture(mode)) {
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
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem stencil_texture_items[] = {
      {STENCIL_PRIMARY, "PRIMARY", 0, "Primary", ""},
      {STENCIL_SECONDARY, "SECONDARY", 0, "Secondary", ""},
      {0, NULL, 0, NULL, NULL},
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
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
  prop = RNA_def_enum(ot->srna, "texmode", stencil_texture_items, STENCIL_PRIMARY, "Tool", "");
  RNA_def_property_flag(prop, PROP_HIDDEN | PROP_SKIP_SAVE);
}

static int stencil_fit_image_aspect_exec(bContext *C, wmOperator *op)
{
  Paint *paint = BKE_paint_get_active_from_context(C);
  Brush *br = BKE_paint_brush(paint);
  bool use_scale = RNA_boolean_get(op->ptr, "use_scale");
  bool use_repeat = RNA_boolean_get(op->ptr, "use_repeat");
  bool do_mask = RNA_boolean_get(op->ptr, "mask");
  Tex *tex = NULL;
  MTex *mtex = NULL;
  if (br) {
    mtex = do_mask ? &br->mask_mtex : &br->mtex;
    tex = mtex->tex;
  }

  if (tex && tex->type == TEX_IMAGE && tex->ima) {
    float aspx, aspy;
    Image *ima = tex->ima;
    float orig_area, stencil_area, factor;
    ED_image_get_uv_aspect(ima, NULL, &aspx, &aspy);

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

  WM_event_add_notifier(C, NC_WINDOW, NULL);

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

  RNA_def_boolean(ot->srna, "use_repeat", 1, "Use Repeat", "Use repeat mapping values");
  RNA_def_boolean(ot->srna, "use_scale", 1, "Use Scale", "Use texture scale values");
  RNA_def_boolean(
      ot->srna, "mask", 0, "Modify Mask Stencil", "Modify either the primary or mask stencil");
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

  WM_event_add_notifier(C, NC_WINDOW, NULL);

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
      ot->srna, "mask", 0, "Modify Mask Stencil", "Modify either the primary or mask stencil");
}

/**************************** registration **********************************/

void ED_operatormacros_paint(void)
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

void ED_operatortypes_paint(void)
{
  /* palette */
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
  WM_operatortype_append(BRUSH_OT_reset);
  WM_operatortype_append(BRUSH_OT_stencil_control);
  WM_operatortype_append(BRUSH_OT_stencil_fit_image_aspect);
  WM_operatortype_append(BRUSH_OT_stencil_reset_transform);

  /* note, particle uses a different system, can be added with existing operators in wm.py */
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
  WM_operatortype_append(PAINT_OT_face_select_hide);
  WM_operatortype_append(PAINT_OT_face_select_reveal);

  /* partial visibility */
  WM_operatortype_append(PAINT_OT_hide_show);

  /* paint masking */
  WM_operatortype_append(PAINT_OT_mask_flood_fill);
  WM_operatortype_append(PAINT_OT_mask_lasso_gesture);
}

void ED_keymap_paint(wmKeyConfig *keyconf)
{
  wmKeyMap *keymap;

  keymap = WM_keymap_ensure(keyconf, "Paint Curve", 0, 0);
  keymap->poll = paint_curve_poll;

  /* Sculpt mode */
  keymap = WM_keymap_ensure(keyconf, "Sculpt", 0, 0);
  keymap->poll = SCULPT_mode_poll;

  /* Vertex Paint mode */
  keymap = WM_keymap_ensure(keyconf, "Vertex Paint", 0, 0);
  keymap->poll = vertex_paint_mode_poll;

  /* Weight Paint mode */
  keymap = WM_keymap_ensure(keyconf, "Weight Paint", 0, 0);
  keymap->poll = weight_paint_mode_poll;

  /*Weight paint's Vertex Selection Mode */
  keymap = WM_keymap_ensure(keyconf, "Paint Vertex Selection (Weight, Vertex)", 0, 0);
  keymap->poll = vert_paint_poll;

  /* Image/Texture Paint mode */
  keymap = WM_keymap_ensure(keyconf, "Image Paint", 0, 0);
  keymap->poll = image_texture_paint_poll;

  /* face-mask mode */
  keymap = WM_keymap_ensure(keyconf, "Paint Face Mask (Weight, Vertex, Texture)", 0, 0);
  keymap->poll = facemask_paint_poll;

  /* paint stroke */
  keymap = paint_stroke_modal_keymap(keyconf);
  WM_modalkeymap_assign(keymap, "SCULPT_OT_brush_stroke");
}
