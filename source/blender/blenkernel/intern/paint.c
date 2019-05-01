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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2009 by Nicholas Bishop
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_scene_types.h"
#include "DNA_brush_types.h"
#include "DNA_space_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_workspace_types.h"

#include "BLI_bitmap.h"
#include "BLI_utildefines.h"
#include "BLI_math_vector.h"
#include "BLI_listbase.h"

#include "BLT_translation.h"

#include "BKE_animsys.h"
#include "BKE_brush.h"
#include "BKE_ccg.h"
#include "BKE_colortools.h"
#include "BKE_deform.h"
#include "BKE_main.h"
#include "BKE_context.h"
#include "BKE_crazyspace.h"
#include "BKE_gpencil.h"
#include "BKE_image.h"
#include "BKE_key.h"
#include "BKE_library.h"
#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "BKE_mesh_runtime.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_pbvh.h"
#include "BKE_subdiv_ccg.h"
#include "BKE_subsurf.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "RNA_enum_types.h"

#include "bmesh.h"

const char PAINT_CURSOR_SCULPT[3] = {255, 100, 100};
const char PAINT_CURSOR_VERTEX_PAINT[3] = {255, 255, 255};
const char PAINT_CURSOR_WEIGHT_PAINT[3] = {200, 200, 255};
const char PAINT_CURSOR_TEXTURE_PAINT[3] = {255, 255, 255};

static eOverlayControlFlags overlay_flags = 0;

void BKE_paint_invalidate_overlay_tex(Scene *scene, ViewLayer *view_layer, const Tex *tex)
{
  Paint *p = BKE_paint_get_active(scene, view_layer);
  Brush *br = p->brush;

  if (!br) {
    return;
  }

  if (br->mtex.tex == tex) {
    overlay_flags |= PAINT_OVERLAY_INVALID_TEXTURE_PRIMARY;
  }
  if (br->mask_mtex.tex == tex) {
    overlay_flags |= PAINT_OVERLAY_INVALID_TEXTURE_SECONDARY;
  }
}

void BKE_paint_invalidate_cursor_overlay(Scene *scene, ViewLayer *view_layer, CurveMapping *curve)
{
  Paint *p = BKE_paint_get_active(scene, view_layer);
  Brush *br = p->brush;

  if (br && br->curve == curve) {
    overlay_flags |= PAINT_OVERLAY_INVALID_CURVE;
  }
}

void BKE_paint_invalidate_overlay_all(void)
{
  overlay_flags |= (PAINT_OVERLAY_INVALID_TEXTURE_SECONDARY |
                    PAINT_OVERLAY_INVALID_TEXTURE_PRIMARY | PAINT_OVERLAY_INVALID_CURVE);
}

eOverlayControlFlags BKE_paint_get_overlay_flags(void)
{
  return overlay_flags;
}

void BKE_paint_set_overlay_override(eOverlayFlags flags)
{
  if (flags & BRUSH_OVERLAY_OVERRIDE_MASK) {
    if (flags & BRUSH_OVERLAY_CURSOR_OVERRIDE_ON_STROKE) {
      overlay_flags |= PAINT_OVERLAY_OVERRIDE_CURSOR;
    }
    if (flags & BRUSH_OVERLAY_PRIMARY_OVERRIDE_ON_STROKE) {
      overlay_flags |= PAINT_OVERLAY_OVERRIDE_PRIMARY;
    }
    if (flags & BRUSH_OVERLAY_SECONDARY_OVERRIDE_ON_STROKE) {
      overlay_flags |= PAINT_OVERLAY_OVERRIDE_SECONDARY;
    }
  }
  else {
    overlay_flags &= ~(PAINT_OVERRIDE_MASK);
  }
}

void BKE_paint_reset_overlay_invalid(eOverlayControlFlags flag)
{
  overlay_flags &= ~(flag);
}

bool BKE_paint_ensure_from_paintmode(Scene *sce, ePaintMode mode)
{
  ToolSettings *ts = sce->toolsettings;
  Paint **paint_ptr = NULL;

  switch (mode) {
    case PAINT_MODE_SCULPT:
      paint_ptr = (Paint **)&ts->sculpt;
      break;
    case PAINT_MODE_VERTEX:
      paint_ptr = (Paint **)&ts->vpaint;
      break;
    case PAINT_MODE_WEIGHT:
      paint_ptr = (Paint **)&ts->wpaint;
      break;
    case PAINT_MODE_TEXTURE_2D:
    case PAINT_MODE_TEXTURE_3D:
      break;
    case PAINT_MODE_SCULPT_UV:
      paint_ptr = (Paint **)&ts->uvsculpt;
      break;
    case PAINT_MODE_GPENCIL:
      paint_ptr = (Paint **)&ts->gp_paint;
      break;
    case PAINT_MODE_INVALID:
      break;
  }
  if (paint_ptr && (*paint_ptr == NULL)) {
    BKE_paint_ensure(ts, paint_ptr);
    return true;
  }
  return false;
}

Paint *BKE_paint_get_active_from_paintmode(Scene *sce, ePaintMode mode)
{
  if (sce) {
    ToolSettings *ts = sce->toolsettings;

    switch (mode) {
      case PAINT_MODE_SCULPT:
        return &ts->sculpt->paint;
      case PAINT_MODE_VERTEX:
        return &ts->vpaint->paint;
      case PAINT_MODE_WEIGHT:
        return &ts->wpaint->paint;
      case PAINT_MODE_TEXTURE_2D:
      case PAINT_MODE_TEXTURE_3D:
        return &ts->imapaint.paint;
      case PAINT_MODE_SCULPT_UV:
        return &ts->uvsculpt->paint;
      case PAINT_MODE_GPENCIL:
        return &ts->gp_paint->paint;
      case PAINT_MODE_INVALID:
        return NULL;
      default:
        return &ts->imapaint.paint;
    }
  }

  return NULL;
}

const EnumPropertyItem *BKE_paint_get_tool_enum_from_paintmode(ePaintMode mode)
{
  switch (mode) {
    case PAINT_MODE_SCULPT:
      return rna_enum_brush_sculpt_tool_items;
    case PAINT_MODE_VERTEX:
      return rna_enum_brush_vertex_tool_items;
    case PAINT_MODE_WEIGHT:
      return rna_enum_brush_weight_tool_items;
    case PAINT_MODE_TEXTURE_2D:
    case PAINT_MODE_TEXTURE_3D:
      return rna_enum_brush_image_tool_items;
    case PAINT_MODE_SCULPT_UV:
      return rna_enum_brush_uv_sculpt_tool_items;
      return NULL;
    case PAINT_MODE_GPENCIL:
      return rna_enum_brush_gpencil_types_items;
    case PAINT_MODE_INVALID:
      break;
  }
  return NULL;
}

const char *BKE_paint_get_tool_prop_id_from_paintmode(ePaintMode mode)
{
  switch (mode) {
    case PAINT_MODE_SCULPT:
      return "sculpt_tool";
    case PAINT_MODE_VERTEX:
      return "vertex_tool";
    case PAINT_MODE_WEIGHT:
      return "weight_tool";
    case PAINT_MODE_TEXTURE_2D:
    case PAINT_MODE_TEXTURE_3D:
      return "image_tool";
    case PAINT_MODE_SCULPT_UV:
      return "uv_sculpt_tool";
    case PAINT_MODE_GPENCIL:
      return "gpencil_tool";
    default:
      /* invalid paint mode */
      return NULL;
  }
}

Paint *BKE_paint_get_active(Scene *sce, ViewLayer *view_layer)
{
  if (sce && view_layer) {
    ToolSettings *ts = sce->toolsettings;

    if (view_layer->basact && view_layer->basact->object) {
      switch (view_layer->basact->object->mode) {
        case OB_MODE_SCULPT:
          return &ts->sculpt->paint;
        case OB_MODE_VERTEX_PAINT:
          return &ts->vpaint->paint;
        case OB_MODE_WEIGHT_PAINT:
          return &ts->wpaint->paint;
        case OB_MODE_TEXTURE_PAINT:
          return &ts->imapaint.paint;
        case OB_MODE_PAINT_GPENCIL:
          return &ts->gp_paint->paint;
        case OB_MODE_EDIT:
          return &ts->uvsculpt->paint;
        default:
          break;
      }
    }

    /* default to image paint */
    return &ts->imapaint.paint;
  }

  return NULL;
}

Paint *BKE_paint_get_active_from_context(const bContext *C)
{
  Scene *sce = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  SpaceImage *sima;

  if (sce && view_layer) {
    ToolSettings *ts = sce->toolsettings;
    Object *obact = NULL;

    if (view_layer->basact && view_layer->basact->object) {
      obact = view_layer->basact->object;
    }

    if ((sima = CTX_wm_space_image(C)) != NULL) {
      if (obact && obact->mode == OB_MODE_EDIT) {
        if (sima->mode == SI_MODE_PAINT) {
          return &ts->imapaint.paint;
        }
        else if (sima->mode == SI_MODE_UV) {
          return &ts->uvsculpt->paint;
        }
      }
      else {
        return &ts->imapaint.paint;
      }
    }
    else {
      return BKE_paint_get_active(sce, view_layer);
    }
  }

  return NULL;
}

ePaintMode BKE_paintmode_get_active_from_context(const bContext *C)
{
  Scene *sce = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  SpaceImage *sima;

  if (sce && view_layer) {
    Object *obact = NULL;

    if (view_layer->basact && view_layer->basact->object) {
      obact = view_layer->basact->object;
    }

    if ((sima = CTX_wm_space_image(C)) != NULL) {
      if (obact && obact->mode == OB_MODE_EDIT) {
        if (sima->mode == SI_MODE_PAINT) {
          return PAINT_MODE_TEXTURE_2D;
        }
        else if (sima->mode == SI_MODE_UV) {
          return PAINT_MODE_SCULPT_UV;
        }
      }
      else {
        return PAINT_MODE_TEXTURE_2D;
      }
    }
    else if (obact) {
      switch (obact->mode) {
        case OB_MODE_SCULPT:
          return PAINT_MODE_SCULPT;
        case OB_MODE_VERTEX_PAINT:
          return PAINT_MODE_VERTEX;
        case OB_MODE_WEIGHT_PAINT:
          return PAINT_MODE_WEIGHT;
        case OB_MODE_TEXTURE_PAINT:
          return PAINT_MODE_TEXTURE_3D;
        case OB_MODE_EDIT:
          return PAINT_MODE_SCULPT_UV;
        default:
          return PAINT_MODE_TEXTURE_2D;
      }
    }
    else {
      /* default to image paint */
      return PAINT_MODE_TEXTURE_2D;
    }
  }

  return PAINT_MODE_INVALID;
}

ePaintMode BKE_paintmode_get_from_tool(const struct bToolRef *tref)
{
  if (tref->space_type == SPACE_VIEW3D) {
    switch (tref->mode) {
      case CTX_MODE_SCULPT:
        return PAINT_MODE_SCULPT;
      case CTX_MODE_PAINT_VERTEX:
        return PAINT_MODE_VERTEX;
      case CTX_MODE_PAINT_WEIGHT:
        return PAINT_MODE_WEIGHT;
      case CTX_MODE_PAINT_GPENCIL:
        return PAINT_MODE_GPENCIL;
      case CTX_MODE_PAINT_TEXTURE:
        return PAINT_MODE_TEXTURE_3D;
    }
  }
  else if (tref->space_type == SPACE_IMAGE) {
    switch (tref->mode) {
      case SI_MODE_PAINT:
        return PAINT_MODE_TEXTURE_2D;
      case SI_MODE_UV:
        return PAINT_MODE_SCULPT_UV;
    }
  }

  return PAINT_MODE_INVALID;
}

Brush *BKE_paint_brush(Paint *p)
{
  return p ? p->brush : NULL;
}

void BKE_paint_brush_set(Paint *p, Brush *br)
{
  if (p) {
    id_us_min((ID *)p->brush);
    id_us_plus((ID *)br);
    p->brush = br;

    BKE_paint_toolslots_brush_update(p);
  }
}

void BKE_paint_runtime_init(const ToolSettings *ts, Paint *paint)
{
  if (paint == &ts->imapaint.paint) {
    paint->runtime.tool_offset = offsetof(Brush, imagepaint_tool);
    paint->runtime.ob_mode = OB_MODE_TEXTURE_PAINT;
  }
  else if (paint == &ts->sculpt->paint) {
    paint->runtime.tool_offset = offsetof(Brush, sculpt_tool);
    paint->runtime.ob_mode = OB_MODE_SCULPT;
  }
  else if (paint == &ts->vpaint->paint) {
    paint->runtime.tool_offset = offsetof(Brush, vertexpaint_tool);
    paint->runtime.ob_mode = OB_MODE_VERTEX_PAINT;
  }
  else if (paint == &ts->wpaint->paint) {
    paint->runtime.tool_offset = offsetof(Brush, weightpaint_tool);
    paint->runtime.ob_mode = OB_MODE_WEIGHT_PAINT;
  }
  else if (paint == &ts->uvsculpt->paint) {
    paint->runtime.tool_offset = offsetof(Brush, uv_sculpt_tool);
    paint->runtime.ob_mode = OB_MODE_EDIT;
  }
  else if (paint == &ts->gp_paint->paint) {
    paint->runtime.tool_offset = offsetof(Brush, gpencil_tool);
    paint->runtime.ob_mode = OB_MODE_PAINT_GPENCIL;
  }
  else {
    BLI_assert(0);
  }
}

uint BKE_paint_get_brush_tool_offset_from_paintmode(const ePaintMode mode)
{
  switch (mode) {
    case PAINT_MODE_TEXTURE_2D:
    case PAINT_MODE_TEXTURE_3D:
      return offsetof(Brush, imagepaint_tool);
    case PAINT_MODE_SCULPT:
      return offsetof(Brush, sculpt_tool);
    case PAINT_MODE_VERTEX:
      return offsetof(Brush, vertexpaint_tool);
    case PAINT_MODE_WEIGHT:
      return offsetof(Brush, weightpaint_tool);
    case PAINT_MODE_SCULPT_UV:
      return offsetof(Brush, uv_sculpt_tool);
    case PAINT_MODE_GPENCIL:
      return offsetof(Brush, gpencil_tool);
    case PAINT_MODE_INVALID:
      break; /* We don't use these yet. */
  }
  return 0;
}

/** Free (or release) any data used by this paint curve (does not free the pcurve itself). */
void BKE_paint_curve_free(PaintCurve *pc)
{
  MEM_SAFE_FREE(pc->points);
  pc->tot_points = 0;
}

PaintCurve *BKE_paint_curve_add(Main *bmain, const char *name)
{
  PaintCurve *pc;

  pc = BKE_libblock_alloc(bmain, ID_PC, name, 0);

  return pc;
}

/**
 * Only copy internal data of PaintCurve ID from source to
 * already allocated/initialized destination.
 * You probably never want to use that directly,
 * use #BKE_id_copy or #BKE_id_copy_ex for typical needs.
 *
 * WARNING! This function will not handle ID user count!
 *
 * \param flag: Copying options (see BKE_library.h's LIB_ID_COPY_... flags for more).
 */
void BKE_paint_curve_copy_data(Main *UNUSED(bmain),
                               PaintCurve *pc_dst,
                               const PaintCurve *pc_src,
                               const int UNUSED(flag))
{
  if (pc_src->tot_points != 0) {
    pc_dst->points = MEM_dupallocN(pc_src->points);
  }
}

PaintCurve *BKE_paint_curve_copy(Main *bmain, const PaintCurve *pc)
{
  PaintCurve *pc_copy;
  BKE_id_copy(bmain, &pc->id, (ID **)&pc_copy);
  return pc_copy;
}

void BKE_paint_curve_make_local(Main *bmain, PaintCurve *pc, const bool lib_local)
{
  BKE_id_make_local_generic(bmain, &pc->id, true, lib_local);
}

Palette *BKE_paint_palette(Paint *p)
{
  return p ? p->palette : NULL;
}

void BKE_paint_palette_set(Paint *p, Palette *palette)
{
  if (p) {
    id_us_min((ID *)p->palette);
    p->palette = palette;
    id_us_plus((ID *)p->palette);
  }
}

void BKE_paint_curve_set(Brush *br, PaintCurve *pc)
{
  if (br) {
    id_us_min((ID *)br->paint_curve);
    br->paint_curve = pc;
    id_us_plus((ID *)br->paint_curve);
  }
}

void BKE_paint_curve_clamp_endpoint_add_index(PaintCurve *pc, const int add_index)
{
  pc->add_index = (add_index || pc->tot_points == 1) ? (add_index + 1) : 0;
}

/** Remove color from palette. Must be certain color is inside the palette! */
void BKE_palette_color_remove(Palette *palette, PaletteColor *color)
{
  if (BLI_listbase_count_at_most(&palette->colors, palette->active_color) ==
      palette->active_color) {
    palette->active_color--;
  }

  BLI_remlink(&palette->colors, color);

  if (palette->active_color < 0 && !BLI_listbase_is_empty(&palette->colors)) {
    palette->active_color = 0;
  }

  MEM_freeN(color);
}

void BKE_palette_clear(Palette *palette)
{
  BLI_freelistN(&palette->colors);
  palette->active_color = 0;
}

Palette *BKE_palette_add(Main *bmain, const char *name)
{
  Palette *palette = BKE_id_new(bmain, ID_PAL, name);
  return palette;
}

/**
 * Only copy internal data of Palette ID from source
 * to already allocated/initialized destination.
 * You probably never want to use that directly,
 * use #BKE_id_copy or #BKE_id_copy_ex for typical needs.
 *
 * WARNING! This function will not handle ID user count!
 *
 * \param flag: Copying options (see BKE_library.h's LIB_ID_COPY_... flags for more).
 */
void BKE_palette_copy_data(Main *UNUSED(bmain),
                           Palette *palette_dst,
                           const Palette *palette_src,
                           const int UNUSED(flag))
{
  BLI_duplicatelist(&palette_dst->colors, &palette_src->colors);
}

Palette *BKE_palette_copy(Main *bmain, const Palette *palette)
{
  Palette *palette_copy;
  BKE_id_copy(bmain, &palette->id, (ID **)&palette_copy);
  return palette_copy;
}

void BKE_palette_make_local(Main *bmain, Palette *palette, const bool lib_local)
{
  BKE_id_make_local_generic(bmain, &palette->id, true, lib_local);
}

void BKE_palette_init(Palette *palette)
{
  /* Enable fake user by default. */
  id_fake_user_set(&palette->id);
}

/** Free (or release) any data used by this palette (does not free the palette itself). */
void BKE_palette_free(Palette *palette)
{
  BLI_freelistN(&palette->colors);
}

PaletteColor *BKE_palette_color_add(Palette *palette)
{
  PaletteColor *color = MEM_callocN(sizeof(*color), "Palette Color");
  BLI_addtail(&palette->colors, color);
  return color;
}

bool BKE_palette_is_empty(const struct Palette *palette)
{
  return BLI_listbase_is_empty(&palette->colors);
}

/* are we in vertex paint or weight paint face select mode? */
bool BKE_paint_select_face_test(Object *ob)
{
  return ((ob != NULL) && (ob->type == OB_MESH) && (ob->data != NULL) &&
          (((Mesh *)ob->data)->editflag & ME_EDIT_PAINT_FACE_SEL) &&
          (ob->mode & (OB_MODE_VERTEX_PAINT | OB_MODE_WEIGHT_PAINT | OB_MODE_TEXTURE_PAINT)));
}

/* are we in weight paint vertex select mode? */
bool BKE_paint_select_vert_test(Object *ob)
{
  return ((ob != NULL) && (ob->type == OB_MESH) && (ob->data != NULL) &&
          (((Mesh *)ob->data)->editflag & ME_EDIT_PAINT_VERT_SEL) &&
          (ob->mode & OB_MODE_WEIGHT_PAINT || ob->mode & OB_MODE_VERTEX_PAINT));
}

/**
 * used to check if selection is possible
 * (when we don't care if its face or vert)
 */
bool BKE_paint_select_elem_test(Object *ob)
{
  return (BKE_paint_select_vert_test(ob) || BKE_paint_select_face_test(ob));
}

void BKE_paint_cavity_curve_preset(Paint *p, int preset)
{
  CurveMap *cm = NULL;

  if (!p->cavity_curve) {
    p->cavity_curve = curvemapping_add(1, 0, 0, 1, 1);
  }

  cm = p->cavity_curve->cm;
  cm->flag &= ~CUMA_EXTEND_EXTRAPOLATE;

  p->cavity_curve->preset = preset;
  curvemap_reset(cm, &p->cavity_curve->clipr, p->cavity_curve->preset, CURVEMAP_SLOPE_POSITIVE);
  curvemapping_changed(p->cavity_curve, false);
}

eObjectMode BKE_paint_object_mode_from_paintmode(ePaintMode mode)
{
  switch (mode) {
    case PAINT_MODE_SCULPT:
      return OB_MODE_SCULPT;
    case PAINT_MODE_VERTEX:
      return OB_MODE_VERTEX_PAINT;
    case PAINT_MODE_WEIGHT:
      return OB_MODE_WEIGHT_PAINT;
    case PAINT_MODE_TEXTURE_2D:
    case PAINT_MODE_TEXTURE_3D:
      return OB_MODE_TEXTURE_PAINT;
    case PAINT_MODE_SCULPT_UV:
      return OB_MODE_EDIT;
    case PAINT_MODE_INVALID:
    default:
      return 0;
  }
}

/**
 * Call when entering each respective paint mode.
 */
bool BKE_paint_ensure(const ToolSettings *ts, struct Paint **r_paint)
{
  Paint *paint = NULL;
  if (*r_paint) {
    /* Note: 'ts->imapaint' is ignored, it's not allocated. */
    BLI_assert(ELEM(*r_paint,
                    &ts->gp_paint->paint,
                    &ts->sculpt->paint,
                    &ts->vpaint->paint,
                    &ts->wpaint->paint,
                    &ts->uvsculpt->paint));

#ifdef DEBUG
    struct Paint paint_test = **r_paint;
    BKE_paint_runtime_init(ts, *r_paint);
    /* Swap so debug doesn't hide errors when release fails. */
    SWAP(Paint, **r_paint, paint_test);
    BLI_assert(paint_test.runtime.ob_mode == (*r_paint)->runtime.ob_mode);
    BLI_assert(paint_test.runtime.tool_offset == (*r_paint)->runtime.tool_offset);
#endif
    return true;
  }

  if (((VPaint **)r_paint == &ts->vpaint) || ((VPaint **)r_paint == &ts->wpaint)) {
    VPaint *data = MEM_callocN(sizeof(*data), __func__);
    paint = &data->paint;
  }
  else if ((Sculpt **)r_paint == &ts->sculpt) {
    Sculpt *data = MEM_callocN(sizeof(*data), __func__);
    paint = &data->paint;

    /* Turn on X plane mirror symmetry by default */
    paint->symmetry_flags |= PAINT_SYMM_X;

    /* Make sure at least dyntopo subdivision is enabled */
    data->flags |= SCULPT_DYNTOPO_SUBDIVIDE | SCULPT_DYNTOPO_COLLAPSE;
  }
  else if ((GpPaint **)r_paint == &ts->gp_paint) {
    GpPaint *data = MEM_callocN(sizeof(*data), __func__);
    paint = &data->paint;
  }
  else if ((UvSculpt **)r_paint == &ts->uvsculpt) {
    UvSculpt *data = MEM_callocN(sizeof(*data), __func__);
    paint = &data->paint;
  }

  paint->flags |= PAINT_SHOW_BRUSH;

  *r_paint = paint;

  BKE_paint_runtime_init(ts, paint);

  return false;
}

void BKE_paint_init(Main *bmain, Scene *sce, ePaintMode mode, const char col[3])
{
  UnifiedPaintSettings *ups = &sce->toolsettings->unified_paint_settings;
  Paint *paint = BKE_paint_get_active_from_paintmode(sce, mode);

  /* If there's no brush, create one */
  if (PAINT_MODE_HAS_BRUSH(mode)) {
    Brush *brush = BKE_paint_brush(paint);
    if (brush == NULL) {
      eObjectMode ob_mode = BKE_paint_object_mode_from_paintmode(mode);
      brush = BKE_brush_first_search(bmain, ob_mode);
      if (!brush) {
        brush = BKE_brush_add(bmain, "Brush", ob_mode);
        id_us_min(&brush->id); /* fake user only */
      }
      BKE_paint_brush_set(paint, brush);
    }
  }

  memcpy(paint->paint_cursor_col, col, 3);
  paint->paint_cursor_col[3] = 128;
  ups->last_stroke_valid = false;
  zero_v3(ups->average_stroke_accum);
  ups->average_stroke_counter = 0;
  if (!paint->cavity_curve) {
    BKE_paint_cavity_curve_preset(paint, CURVE_PRESET_LINE);
  }
}

void BKE_paint_free(Paint *paint)
{
  curvemapping_free(paint->cavity_curve);
  MEM_SAFE_FREE(paint->tool_slots);
}

/* called when copying scene settings, so even if 'src' and 'tar' are the same
 * still do a id_us_plus(), rather then if we were copying between 2 existing
 * scenes where a matching value should decrease the existing user count as
 * with paint_brush_set() */
void BKE_paint_copy(Paint *src, Paint *tar, const int flag)
{
  tar->brush = src->brush;
  tar->cavity_curve = curvemapping_copy(src->cavity_curve);
  tar->tool_slots = MEM_dupallocN(src->tool_slots);

  if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
    id_us_plus((ID *)tar->brush);
    id_us_plus((ID *)tar->palette);
    if (src->tool_slots != NULL) {
      for (int i = 0; i < tar->tool_slots_len; i++) {
        id_us_plus((ID *)tar->tool_slots[i].brush);
      }
    }
  }
}

void BKE_paint_stroke_get_average(Scene *scene, Object *ob, float stroke[3])
{
  UnifiedPaintSettings *ups = &scene->toolsettings->unified_paint_settings;
  if (ups->last_stroke_valid && ups->average_stroke_counter > 0) {
    float fac = 1.0f / ups->average_stroke_counter;
    mul_v3_v3fl(stroke, ups->average_stroke_accum, fac);
  }
  else {
    copy_v3_v3(stroke, ob->obmat[3]);
  }
}

/* returns non-zero if any of the face's vertices
 * are hidden, zero otherwise */
bool paint_is_face_hidden(const MLoopTri *lt, const MVert *mvert, const MLoop *mloop)
{
  return ((mvert[mloop[lt->tri[0]].v].flag & ME_HIDE) ||
          (mvert[mloop[lt->tri[1]].v].flag & ME_HIDE) ||
          (mvert[mloop[lt->tri[2]].v].flag & ME_HIDE));
}

/* returns non-zero if any of the corners of the grid
 * face whose inner corner is at (x, y) are hidden,
 * zero otherwise */
bool paint_is_grid_face_hidden(const unsigned int *grid_hidden, int gridsize, int x, int y)
{
  /* skip face if any of its corners are hidden */
  return (BLI_BITMAP_TEST(grid_hidden, y * gridsize + x) ||
          BLI_BITMAP_TEST(grid_hidden, y * gridsize + x + 1) ||
          BLI_BITMAP_TEST(grid_hidden, (y + 1) * gridsize + x + 1) ||
          BLI_BITMAP_TEST(grid_hidden, (y + 1) * gridsize + x));
}

/* Return true if all vertices in the face are visible, false otherwise */
bool paint_is_bmesh_face_hidden(BMFace *f)
{
  BMLoop *l_iter;
  BMLoop *l_first;

  l_iter = l_first = BM_FACE_FIRST_LOOP(f);
  do {
    if (BM_elem_flag_test(l_iter->v, BM_ELEM_HIDDEN)) {
      return true;
    }
  } while ((l_iter = l_iter->next) != l_first);

  return false;
}

float paint_grid_paint_mask(const GridPaintMask *gpm, unsigned level, unsigned x, unsigned y)
{
  int factor = BKE_ccg_factor(level, gpm->level);
  int gridsize = BKE_ccg_gridsize(gpm->level);

  return gpm->data[(y * factor) * gridsize + (x * factor)];
}

/* threshold to move before updating the brush rotation */
#define RAKE_THRESHHOLD 20

void paint_update_brush_rake_rotation(UnifiedPaintSettings *ups, Brush *brush, float rotation)
{
  if (brush->mtex.brush_angle_mode & MTEX_ANGLE_RAKE) {
    ups->brush_rotation = rotation;
  }
  else {
    ups->brush_rotation = 0.0f;
  }

  if (brush->mask_mtex.brush_angle_mode & MTEX_ANGLE_RAKE) {
    ups->brush_rotation_sec = rotation;
  }
  else {
    ups->brush_rotation_sec = 0.0f;
  }
}

bool paint_calculate_rake_rotation(UnifiedPaintSettings *ups,
                                   Brush *brush,
                                   const float mouse_pos[2])
{
  bool ok = false;
  if ((brush->mtex.brush_angle_mode & MTEX_ANGLE_RAKE) ||
      (brush->mask_mtex.brush_angle_mode & MTEX_ANGLE_RAKE)) {
    const float r = RAKE_THRESHHOLD;
    float rotation;

    float dpos[2];
    sub_v2_v2v2(dpos, ups->last_rake, mouse_pos);

    if (len_squared_v2(dpos) >= r * r) {
      rotation = atan2f(dpos[0], dpos[1]);

      copy_v2_v2(ups->last_rake, mouse_pos);

      ups->last_rake_angle = rotation;

      paint_update_brush_rake_rotation(ups, brush, rotation);
      ok = true;
    }
    /* make sure we reset here to the last rotation to avoid accumulating
     * values in case a random rotation is also added */
    else {
      paint_update_brush_rake_rotation(ups, brush, ups->last_rake_angle);
      ok = false;
    }
  }
  else {
    ups->brush_rotation = ups->brush_rotation_sec = 0.0f;
    ok = true;
  }
  return ok;
}

void BKE_sculptsession_free_deformMats(SculptSession *ss)
{
  MEM_SAFE_FREE(ss->orig_cos);
  MEM_SAFE_FREE(ss->deform_cos);
  MEM_SAFE_FREE(ss->deform_imats);
}

void BKE_sculptsession_free_vwpaint_data(struct SculptSession *ss)
{
  struct SculptVertexPaintGeomMap *gmap = NULL;
  if (ss->mode_type == OB_MODE_VERTEX_PAINT) {
    gmap = &ss->mode.vpaint.gmap;

    MEM_SAFE_FREE(ss->mode.vpaint.previous_color);
  }
  else if (ss->mode_type == OB_MODE_WEIGHT_PAINT) {
    gmap = &ss->mode.wpaint.gmap;

    MEM_SAFE_FREE(ss->mode.wpaint.alpha_weight);
    if (ss->mode.wpaint.dvert_prev) {
      BKE_defvert_array_free_elems(ss->mode.wpaint.dvert_prev, ss->totvert);
      MEM_freeN(ss->mode.wpaint.dvert_prev);
      ss->mode.wpaint.dvert_prev = NULL;
    }
  }
  else {
    return;
  }
  MEM_SAFE_FREE(gmap->vert_to_loop);
  MEM_SAFE_FREE(gmap->vert_map_mem);
  MEM_SAFE_FREE(gmap->vert_to_poly);
  MEM_SAFE_FREE(gmap->poly_map_mem);
}

/* Write out the sculpt dynamic-topology BMesh to the Mesh */
static void sculptsession_bm_to_me_update_data_only(Object *ob, bool reorder)
{
  SculptSession *ss = ob->sculpt;

  if (ss->bm) {
    if (ob->data) {
      BMIter iter;
      BMFace *efa;
      BM_ITER_MESH (efa, &iter, ss->bm, BM_FACES_OF_MESH) {
        BM_elem_flag_set(efa, BM_ELEM_SMOOTH, ss->bm_smooth_shading);
      }
      if (reorder) {
        BM_log_mesh_elems_reorder(ss->bm, ss->bm_log);
      }
      BM_mesh_bm_to_me(NULL,
                       ss->bm,
                       ob->data,
                       (&(struct BMeshToMeshParams){
                           .calc_object_remap = false,
                       }));
    }
  }
}

void BKE_sculptsession_bm_to_me(Object *ob, bool reorder)
{
  if (ob && ob->sculpt) {
    sculptsession_bm_to_me_update_data_only(ob, reorder);

    /* Ensure the objects evaluated mesh doesn't hold onto arrays
     * now realloc'd in the mesh T34473. */
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  }
}

void BKE_sculptsession_bm_to_me_for_render(Object *object)
{
  if (object && object->sculpt) {
    if (object->sculpt->bm) {
      /* Ensure no points to old arrays are stored in DM
       *
       * Apparently, we could not use DEG_id_tag_update
       * here because this will lead to the while object
       * surface to disappear, so we'll release DM in place.
       */
      BKE_object_free_derived_caches(object);

      if (object->sculpt->pbvh) {
        BKE_pbvh_free(object->sculpt->pbvh);
        object->sculpt->pbvh = NULL;
      }

      sculptsession_bm_to_me_update_data_only(object, false);

      /* In contrast with sculptsession_bm_to_me no need in
       * DAG tag update here - derived mesh was freed and
       * old pointers are nowhere stored.
       */
    }
  }
}

void BKE_sculptsession_free(Object *ob)
{
  if (ob && ob->sculpt) {
    SculptSession *ss = ob->sculpt;

    if (ss->bm) {
      BKE_sculptsession_bm_to_me(ob, true);
      BM_mesh_free(ss->bm);
    }

    if (ss->pbvh) {
      BKE_pbvh_free(ss->pbvh);
    }
    MEM_SAFE_FREE(ss->pmap);
    MEM_SAFE_FREE(ss->pmap_mem);
    if (ss->bm_log) {
      BM_log_free(ss->bm_log);
    }

    if (ss->texcache) {
      MEM_freeN(ss->texcache);
    }

    if (ss->tex_pool) {
      BKE_image_pool_free(ss->tex_pool);
    }

    if (ss->layer_co) {
      MEM_freeN(ss->layer_co);
    }

    if (ss->orig_cos) {
      MEM_freeN(ss->orig_cos);
    }
    if (ss->deform_cos) {
      MEM_freeN(ss->deform_cos);
    }
    if (ss->deform_imats) {
      MEM_freeN(ss->deform_imats);
    }

    BKE_sculptsession_free_vwpaint_data(ob->sculpt);

    MEM_freeN(ss);

    ob->sculpt = NULL;
  }
}

/* Sculpt mode handles multires differently from regular meshes, but only if
 * it's the last modifier on the stack and it is not on the first level */
MultiresModifierData *BKE_sculpt_multires_active(Scene *scene, Object *ob)
{
  Mesh *me = (Mesh *)ob->data;
  ModifierData *md;
  VirtualModifierData virtualModifierData;

  if (ob->sculpt && ob->sculpt->bm) {
    /* can't combine multires and dynamic topology */
    return NULL;
  }

  if (!CustomData_get_layer(&me->ldata, CD_MDISPS)) {
    /* multires can't work without displacement layer */
    return NULL;
  }

  for (md = modifiers_getVirtualModifierList(ob, &virtualModifierData); md; md = md->next) {
    if (md->type == eModifierType_Multires) {
      MultiresModifierData *mmd = (MultiresModifierData *)md;

      if (!modifier_isEnabled(scene, md, eModifierMode_Realtime)) {
        continue;
      }

      if (mmd->sculptlvl > 0) {
        return mmd;
      }
      else {
        return NULL;
      }
    }
  }

  return NULL;
}

/* Checks if there are any supported deformation modifiers active */
static bool sculpt_modifiers_active(Scene *scene, Sculpt *sd, Object *ob)
{
  ModifierData *md;
  Mesh *me = (Mesh *)ob->data;
  MultiresModifierData *mmd = BKE_sculpt_multires_active(scene, ob);
  VirtualModifierData virtualModifierData;

  if (mmd || ob->sculpt->bm) {
    return false;
  }

  /* non-locked shape keys could be handled in the same way as deformed mesh */
  if ((ob->shapeflag & OB_SHAPE_LOCK) == 0 && me->key && ob->shapenr) {
    return true;
  }

  md = modifiers_getVirtualModifierList(ob, &virtualModifierData);

  /* exception for shape keys because we can edit those */
  for (; md; md = md->next) {
    const ModifierTypeInfo *mti = modifierType_getInfo(md->type);
    if (!modifier_isEnabled(scene, md, eModifierMode_Realtime)) {
      continue;
    }
    if (ELEM(md->type, eModifierType_ShapeKey, eModifierType_Multires)) {
      continue;
    }

    if (mti->type == eModifierTypeType_OnlyDeform) {
      return true;
    }
    else if ((sd->flags & SCULPT_ONLY_DEFORM) == 0) {
      return true;
    }
  }

  return false;
}

/**
 * \param need_mask: So that the evaluated mesh that is returned has mask data.
 */
void BKE_sculpt_update_mesh_elements(
    Depsgraph *depsgraph, Scene *scene, Sculpt *sd, Object *ob, bool need_pmap, bool need_mask)
{
  /* TODO(sergey): Make sure ob points to an original object. This is what it
   * is supposed to be pointing to. The issue is, currently draw code takes
   * care of PBVH creation, even though this is something up to dependency
   * graph.
   * Probably, we need to being back logic which was checking for sculpt mode
   * and (re)create PBVH if needed in that case, similar to how DerivedMesh
   * was handling this.
   */
  ob = DEG_get_original_object(ob);
  Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob);

  SculptSession *ss = ob->sculpt;
  Mesh *me = BKE_object_get_original_mesh(ob);
  MultiresModifierData *mmd = BKE_sculpt_multires_active(scene, ob);

  ss->modifiers_active = sculpt_modifiers_active(scene, sd, ob);
  ss->show_diffuse_color = (sd->flags & SCULPT_SHOW_DIFFUSE) != 0;
  ss->show_mask = (sd->flags & SCULPT_HIDE_MASK) == 0;

  ss->building_vp_handle = false;

  if (need_mask) {
    if (mmd == NULL) {
      if (!CustomData_has_layer(&me->vdata, CD_PAINT_MASK)) {
        BKE_sculpt_mask_layers_ensure(ob, NULL);
      }
    }
    else {
      if (!CustomData_has_layer(&me->ldata, CD_GRID_PAINT_MASK)) {
#if 1
        BKE_sculpt_mask_layers_ensure(ob, mmd);
#else
        /* If we wanted to support adding mask data while multi-res painting,
         * we would need to do this. */

        if ((ED_sculpt_mask_layers_ensure(ob, mmd) & ED_SCULPT_MASK_LAYER_CALC_LOOP)) {
          /* remake the derived mesh */
          ob->recalc |= ID_RECALC_GEOMETRY;
          BKE_object_handle_update(scene, ob);
        }
#endif
      }
    }
  }

  /* tessfaces aren't used and will become invalid */
  BKE_mesh_tessface_clear(me);

  ss->kb = (mmd == NULL) ? BKE_keyblock_from_object(ob) : NULL;

  Mesh *me_eval = mesh_get_eval_final(depsgraph, scene, ob_eval, &CD_MASK_BAREMESH);

  /* VWPaint require mesh info for loop lookup, so require sculpt mode here */
  if (mmd && ob->mode & OB_MODE_SCULPT) {
    ss->multires = mmd;
    ss->totvert = me_eval->totvert;
    ss->totpoly = me_eval->totpoly;
    ss->mvert = NULL;
    ss->mpoly = NULL;
    ss->mloop = NULL;
  }
  else {
    ss->totvert = me->totvert;
    ss->totpoly = me->totpoly;
    ss->mvert = me->mvert;
    ss->mpoly = me->mpoly;
    ss->mloop = me->mloop;
    ss->multires = NULL;
    ss->vmask = CustomData_get_layer(&me->vdata, CD_PAINT_MASK);
  }

  ss->subdiv_ccg = me_eval->runtime.subdiv_ccg;

  PBVH *pbvh = BKE_sculpt_object_pbvh_ensure(depsgraph, ob);
  BLI_assert(pbvh == ss->pbvh);
  UNUSED_VARS_NDEBUG(pbvh);
  MEM_SAFE_FREE(ss->pmap);
  MEM_SAFE_FREE(ss->pmap_mem);
  if (need_pmap && ob->type == OB_MESH) {
    BKE_mesh_vert_poly_map_create(
        &ss->pmap, &ss->pmap_mem, me->mpoly, me->mloop, me->totvert, me->totpoly, me->totloop);
  }

  pbvh_show_diffuse_color_set(ss->pbvh, ss->show_diffuse_color);
  pbvh_show_mask_set(ss->pbvh, ss->show_mask);

  if (ss->modifiers_active) {
    if (!ss->orig_cos) {
      Object *object_orig = DEG_get_original_object(ob);
      int a;

      BKE_sculptsession_free_deformMats(ss);

      ss->orig_cos = (ss->kb) ? BKE_keyblock_convert_to_vertcos(ob, ss->kb) :
                                BKE_mesh_vertexCos_get(me, NULL);

      BKE_crazyspace_build_sculpt(
          depsgraph, scene, object_orig, &ss->deform_imats, &ss->deform_cos);
      BKE_pbvh_apply_vertCos(ss->pbvh, ss->deform_cos, me->totvert);

      for (a = 0; a < me->totvert; ++a) {
        invert_m3(ss->deform_imats[a]);
      }
    }
  }
  else {
    BKE_sculptsession_free_deformMats(ss);
  }

  if (ss->kb != NULL && ss->deform_cos == NULL) {
    ss->deform_cos = BKE_keyblock_convert_to_vertcos(ob, ss->kb);
  }

  /* if pbvh is deformed, key block is already applied to it */
  if (ss->kb) {
    bool pbvh_deformed = BKE_pbvh_isDeformed(ss->pbvh);
    if (!pbvh_deformed || ss->deform_cos == NULL) {
      float(*vertCos)[3] = BKE_keyblock_convert_to_vertcos(ob, ss->kb);

      if (vertCos) {
        if (!pbvh_deformed) {
          /* apply shape keys coordinates to PBVH */
          BKE_pbvh_apply_vertCos(ss->pbvh, vertCos, me->totvert);
        }
        if (ss->deform_cos == NULL) {
          ss->deform_cos = vertCos;
        }
        if (vertCos != ss->deform_cos) {
          MEM_freeN(vertCos);
        }
      }
    }
  }

  /* 2.8x - avoid full mesh update! */
  BKE_mesh_batch_cache_dirty_tag(me, BKE_MESH_BATCH_DIRTY_SCULPT_COORDS);
}

int BKE_sculpt_mask_layers_ensure(Object *ob, MultiresModifierData *mmd)
{
  const float *paint_mask;
  Mesh *me = ob->data;
  int ret = 0;

  paint_mask = CustomData_get_layer(&me->vdata, CD_PAINT_MASK);

  /* if multires is active, create a grid paint mask layer if there
   * isn't one already */
  if (mmd && !CustomData_has_layer(&me->ldata, CD_GRID_PAINT_MASK)) {
    GridPaintMask *gmask;
    int level = max_ii(1, mmd->sculptlvl);
    int gridsize = BKE_ccg_gridsize(level);
    int gridarea = gridsize * gridsize;
    int i, j;

    gmask = CustomData_add_layer(&me->ldata, CD_GRID_PAINT_MASK, CD_CALLOC, NULL, me->totloop);

    for (i = 0; i < me->totloop; i++) {
      GridPaintMask *gpm = &gmask[i];

      gpm->level = level;
      gpm->data = MEM_callocN(sizeof(float) * gridarea, "GridPaintMask.data");
    }

    /* if vertices already have mask, copy into multires data */
    if (paint_mask) {
      for (i = 0; i < me->totpoly; i++) {
        const MPoly *p = &me->mpoly[i];
        float avg = 0;

        /* mask center */
        for (j = 0; j < p->totloop; j++) {
          const MLoop *l = &me->mloop[p->loopstart + j];
          avg += paint_mask[l->v];
        }
        avg /= (float)p->totloop;

        /* fill in multires mask corner */
        for (j = 0; j < p->totloop; j++) {
          GridPaintMask *gpm = &gmask[p->loopstart + j];
          const MLoop *l = &me->mloop[p->loopstart + j];
          const MLoop *prev = ME_POLY_LOOP_PREV(me->mloop, p, j);
          const MLoop *next = ME_POLY_LOOP_NEXT(me->mloop, p, j);

          gpm->data[0] = avg;
          gpm->data[1] = (paint_mask[l->v] + paint_mask[next->v]) * 0.5f;
          gpm->data[2] = (paint_mask[l->v] + paint_mask[prev->v]) * 0.5f;
          gpm->data[3] = paint_mask[l->v];
        }
      }
    }

    ret |= SCULPT_MASK_LAYER_CALC_LOOP;
  }

  /* create vertex paint mask layer if there isn't one already */
  if (!paint_mask) {
    CustomData_add_layer(&me->vdata, CD_PAINT_MASK, CD_CALLOC, NULL, me->totvert);
    ret |= SCULPT_MASK_LAYER_CALC_VERT;
  }

  return ret;
}

void BKE_sculpt_toolsettings_data_ensure(struct Scene *scene)
{
  BKE_paint_ensure(scene->toolsettings, (Paint **)&scene->toolsettings->sculpt);

  Sculpt *sd = scene->toolsettings->sculpt;
  if (!sd->detail_size) {
    sd->detail_size = 12;
  }
  if (!sd->detail_percent) {
    sd->detail_percent = 25;
  }
  if (sd->constant_detail == 0.0f) {
    sd->constant_detail = 3.0f;
  }

  /* Set sane default tiling offsets */
  if (!sd->paint.tile_offset[0]) {
    sd->paint.tile_offset[0] = 1.0f;
  }
  if (!sd->paint.tile_offset[1]) {
    sd->paint.tile_offset[1] = 1.0f;
  }
  if (!sd->paint.tile_offset[2]) {
    sd->paint.tile_offset[2] = 1.0f;
  }
}

static bool check_sculpt_object_deformed(Object *object, const bool for_construction)
{
  bool deformed = false;

  /* Active modifiers means extra deformation, which can't be handled correct
   * on birth of PBVH and sculpt "layer" levels, so use PBVH only for internal brush
   * stuff and show final evaluated mesh so user would see actual object shape.
   */
  deformed |= object->sculpt->modifiers_active;

  if (for_construction) {
    deformed |= object->sculpt->kb != NULL;
  }
  else {
    /* As in case with modifiers, we can't synchronize deformation made against
     * PBVH and non-locked keyblock, so also use PBVH only for brushes and
     * final DM to give final result to user.
     */
    deformed |= object->sculpt->kb && (object->shapeflag & OB_SHAPE_LOCK) == 0;
  }

  return deformed;
}

static PBVH *build_pbvh_for_dynamic_topology(Object *ob)
{
  PBVH *pbvh = BKE_pbvh_new();
  BKE_pbvh_build_bmesh(pbvh,
                       ob->sculpt->bm,
                       ob->sculpt->bm_smooth_shading,
                       ob->sculpt->bm_log,
                       ob->sculpt->cd_vert_node_offset,
                       ob->sculpt->cd_face_node_offset);
  pbvh_show_diffuse_color_set(pbvh, ob->sculpt->show_diffuse_color);
  pbvh_show_mask_set(pbvh, ob->sculpt->show_mask);
  return pbvh;
}

static PBVH *build_pbvh_from_regular_mesh(Object *ob, Mesh *me_eval_deform)
{
  Mesh *me = BKE_object_get_original_mesh(ob);
  const int looptris_num = poly_to_tri_count(me->totpoly, me->totloop);
  PBVH *pbvh = BKE_pbvh_new();

  MLoopTri *looptri = MEM_malloc_arrayN(looptris_num, sizeof(*looptri), __func__);

  BKE_mesh_recalc_looptri(me->mloop, me->mpoly, me->mvert, me->totloop, me->totpoly, looptri);

  BKE_pbvh_build_mesh(pbvh,
                      me->mpoly,
                      me->mloop,
                      me->mvert,
                      me->totvert,
                      &me->vdata,
                      &me->ldata,
                      looptri,
                      looptris_num);

  pbvh_show_diffuse_color_set(pbvh, ob->sculpt->show_diffuse_color);
  pbvh_show_mask_set(pbvh, ob->sculpt->show_mask);

  const bool is_deformed = check_sculpt_object_deformed(ob, true);
  if (is_deformed && me_eval_deform != NULL) {
    int totvert;
    float(*v_cos)[3] = BKE_mesh_vertexCos_get(me_eval_deform, &totvert);
    BKE_pbvh_apply_vertCos(pbvh, v_cos, totvert);
    MEM_freeN(v_cos);
  }

  return pbvh;
}

static PBVH *build_pbvh_from_ccg(Object *ob, SubdivCCG *subdiv_ccg)
{
  CCGKey key;
  BKE_subdiv_ccg_key_top_level(&key, subdiv_ccg);
  PBVH *pbvh = BKE_pbvh_new();
  BKE_pbvh_build_grids(pbvh,
                       subdiv_ccg->grids,
                       subdiv_ccg->num_grids,
                       &key,
                       (void **)subdiv_ccg->grid_faces,
                       subdiv_ccg->grid_flag_mats,
                       subdiv_ccg->grid_hidden);
  pbvh_show_diffuse_color_set(pbvh, ob->sculpt->show_diffuse_color);
  pbvh_show_mask_set(pbvh, ob->sculpt->show_mask);
  return pbvh;
}

PBVH *BKE_sculpt_object_pbvh_ensure(Depsgraph *depsgraph, Object *ob)
{
  if (ob == NULL || ob->sculpt == NULL) {
    return NULL;
  }
  PBVH *pbvh = ob->sculpt->pbvh;
  if (pbvh != NULL) {
    /* NOTE: It is possible that grids were re-allocated due to modifier
     * stack. Need to update those pointers. */
    if (BKE_pbvh_type(pbvh) == PBVH_GRIDS) {
      Object *object_eval = DEG_get_evaluated_object(depsgraph, ob);
      Mesh *mesh_eval = object_eval->data;
      SubdivCCG *subdiv_ccg = mesh_eval->runtime.subdiv_ccg;
      if (subdiv_ccg != NULL) {
        BKE_sculpt_bvh_update_from_ccg(pbvh, subdiv_ccg);
      }
    }
    return pbvh;
  }

  if (ob->sculpt->bm != NULL) {
    /* Sculpting on a BMesh (dynamic-topology) gets a special PBVH. */
    pbvh = build_pbvh_for_dynamic_topology(ob);
  }
  else {
    Object *object_eval = DEG_get_evaluated_object(depsgraph, ob);
    Mesh *mesh_eval = object_eval->data;
    if (mesh_eval->runtime.subdiv_ccg != NULL) {
      pbvh = build_pbvh_from_ccg(ob, mesh_eval->runtime.subdiv_ccg);
    }
    else if (ob->type == OB_MESH) {
      Mesh *me_eval_deform = mesh_get_eval_deform(
          depsgraph, DEG_get_evaluated_scene(depsgraph), object_eval, &CD_MASK_BAREMESH);
      pbvh = build_pbvh_from_regular_mesh(ob, me_eval_deform);
    }
  }

  ob->sculpt->pbvh = pbvh;
  return pbvh;
}

void BKE_sculpt_bvh_update_from_ccg(PBVH *pbvh, SubdivCCG *subdiv_ccg)
{
  BKE_pbvh_grids_update(pbvh,
                        subdiv_ccg->grids,
                        (void **)subdiv_ccg->grid_faces,
                        subdiv_ccg->grid_flag_mats,
                        subdiv_ccg->grid_hidden);
}
