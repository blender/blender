/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2009 by Nicholas Bishop. All rights reserved. */

/** \file
 * \ingroup bke
 */

#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_brush_types.h"
#include "DNA_defaults.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"
#include "DNA_workspace_types.h"

#include "BLI_array.h"
#include "BLI_bitmap.h"
#include "BLI_hash.h"
#include "BLI_index_range.hh"
#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_string_ref.hh"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.h"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "BLT_translation.h"

#include "BKE_attribute.h"
#include "BKE_attribute.hh"
#include "BKE_brush.h"
#include "BKE_ccg.h"
#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_crazyspace.h"
#include "BKE_deform.h"
#include "BKE_global.h"
#include "BKE_gpencil_legacy.h"
#include "BKE_idtype.h"
#include "BKE_image.h"
#include "BKE_key.h"
#include "BKE_layer.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.h"
#include "BKE_mesh_runtime.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_pbvh.h"
#include "BKE_scene.h"
#include "BKE_sculpt.hh"
#include "BKE_subdiv_ccg.h"
#include "BKE_subsurf.h"
#include "BKE_undo_system.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "RNA_enum_types.h"

#include "BLO_read_write.h"

#include "../../bmesh/intern/bmesh_idmap.h"
#include "bmesh.h"
#include "bmesh_log.h"

// TODO: figure out bad cross module refs
void SCULPT_undo_ensure_bmlog(Object *ob);

using blender::float3;
using blender::IndexRange;
using blender::MutableSpan;
using blender::Span;
using blender::StringRef;
using blender::Vector;

static void sculpt_attribute_update_refs(Object *ob);
static SculptAttribute *sculpt_attribute_ensure_ex(Object *ob,
                                                   eAttrDomain domain,
                                                   eCustomDataType proptype,
                                                   const char *name,
                                                   const SculptAttributeParams *params,
                                                   PBVHType pbvhtype);
static void sculptsession_bmesh_add_layers(Object *ob);

using blender::MutableSpan;
using blender::Span;
using blender::Vector;

static void palette_init_data(ID *id)
{
  Palette *palette = (Palette *)id;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(palette, id));

  /* Enable fake user by default. */
  id_fake_user_set(&palette->id);
}

static void palette_copy_data(Main * /*bmain*/, ID *id_dst, const ID *id_src, const int /*flag*/)
{
  Palette *palette_dst = (Palette *)id_dst;
  const Palette *palette_src = (const Palette *)id_src;

  BLI_duplicatelist(&palette_dst->colors, &palette_src->colors);
}

static void palette_free_data(ID *id)
{
  Palette *palette = (Palette *)id;

  BLI_freelistN(&palette->colors);
}

static void palette_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  Palette *palette = (Palette *)id;

  BLO_write_id_struct(writer, Palette, id_address, &palette->id);
  BKE_id_blend_write(writer, &palette->id);

  BLO_write_struct_list(writer, PaletteColor, &palette->colors);
}

static void palette_blend_read_data(BlendDataReader *reader, ID *id)
{
  Palette *palette = (Palette *)id;
  BLO_read_list(reader, &palette->colors);
}

static void palette_undo_preserve(BlendLibReader * /*reader*/, ID *id_new, ID *id_old)
{
  /* Whole Palette is preserved across undo-steps, and it has no extra pointer, simple. */
  /* NOTE: We do not care about potential internal references to self here, Palette has none. */
  /* NOTE: We do not swap IDProperties, as dealing with potential ID pointers in those would be
   *       fairly delicate. */
  BKE_lib_id_swap(nullptr, id_new, id_old, false, 0);
  std::swap(id_new->properties, id_old->properties);
}

IDTypeInfo IDType_ID_PAL = {
    /*id_code*/ ID_PAL,
    /*id_filter*/ FILTER_ID_PAL,
    /*main_listbase_index*/ INDEX_ID_PAL,
    /*struct_size*/ sizeof(Palette),
    /*name*/ "Palette",
    /*name_plural*/ "palettes",
    /*translation_context*/ BLT_I18NCONTEXT_ID_PALETTE,
    /*flags*/ IDTYPE_FLAGS_NO_ANIMDATA,
    /*asset_type_info*/ nullptr,

    /*init_data*/ palette_init_data,
    /*copy_data*/ palette_copy_data,
    /*free_data*/ palette_free_data,
    /*make_local*/ nullptr,
    /*foreach_id*/ nullptr,
    /*foreach_cache*/ nullptr,
    /*foreach_path*/ nullptr,
    /*owner_pointer_get*/ nullptr,

    /*blend_write*/ palette_blend_write,
    /*blend_read_data*/ palette_blend_read_data,
    /*blend_read_lib*/ nullptr,
    /*blend_read_expand*/ nullptr,

    /*blend_read_undo_preserve*/ palette_undo_preserve,

    /*lib_override_apply_post*/ nullptr,
};

static void paint_curve_copy_data(Main * /*bmain*/,
                                  ID *id_dst,
                                  const ID *id_src,
                                  const int /*flag*/)
{
  PaintCurve *paint_curve_dst = (PaintCurve *)id_dst;
  const PaintCurve *paint_curve_src = (const PaintCurve *)id_src;

  if (paint_curve_src->tot_points != 0) {
    paint_curve_dst->points = static_cast<PaintCurvePoint *>(
        MEM_dupallocN(paint_curve_src->points));
  }
}

static void paint_curve_free_data(ID *id)
{
  PaintCurve *paint_curve = (PaintCurve *)id;

  MEM_SAFE_FREE(paint_curve->points);
  paint_curve->tot_points = 0;
}

static void paint_curve_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  PaintCurve *pc = (PaintCurve *)id;

  BLO_write_id_struct(writer, PaintCurve, id_address, &pc->id);
  BKE_id_blend_write(writer, &pc->id);

  BLO_write_struct_array(writer, PaintCurvePoint, pc->tot_points, pc->points);
}

static void paint_curve_blend_read_data(BlendDataReader *reader, ID *id)
{
  PaintCurve *pc = (PaintCurve *)id;
  BLO_read_data_address(reader, &pc->points);
}

IDTypeInfo IDType_ID_PC = {
    /*id_code*/ ID_PC,
    /*id_filter*/ FILTER_ID_PC,
    /*main_listbase_index*/ INDEX_ID_PC,
    /*struct_size*/ sizeof(PaintCurve),
    /*name*/ "PaintCurve",
    /*name_plural*/ "paint_curves",
    /*translation_context*/ BLT_I18NCONTEXT_ID_PAINTCURVE,
    /*flags*/ IDTYPE_FLAGS_NO_ANIMDATA,
    /*asset_type_info*/ nullptr,

    /*init_data*/ nullptr,
    /*copy_data*/ paint_curve_copy_data,
    /*free_data*/ paint_curve_free_data,
    /*make_local*/ nullptr,
    /*foreach_id*/ nullptr,
    /*foreach_cache*/ nullptr,
    /*foreach_path*/ nullptr,
    /*owner_pointer_get*/ nullptr,

    /*blend_write*/ paint_curve_blend_write,
    /*blend_read_data*/ paint_curve_blend_read_data,
    /*blend_read_lib*/ nullptr,
    /*blend_read_expand*/ nullptr,

    /*blend_read_undo_preserve*/ nullptr,

    /*lib_override_apply_post*/ nullptr,
};

const uchar PAINT_CURSOR_SCULPT[3] = {255, 100, 100};
const uchar PAINT_CURSOR_VERTEX_PAINT[3] = {255, 255, 255};
const uchar PAINT_CURSOR_WEIGHT_PAINT[3] = {200, 200, 255};
const uchar PAINT_CURSOR_TEXTURE_PAINT[3] = {255, 255, 255};
const uchar PAINT_CURSOR_SCULPT_CURVES[3] = {255, 100, 100};

static ePaintOverlayControlFlags overlay_flags = (ePaintOverlayControlFlags)0;

void BKE_paint_invalidate_overlay_tex(Scene *scene, ViewLayer *view_layer, const Tex *tex)
{
  Paint *p = BKE_paint_get_active(scene, view_layer);
  if (!p) {
    return;
  }

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
  if (p == nullptr) {
    return;
  }

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

ePaintOverlayControlFlags BKE_paint_get_overlay_flags(void)
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

void BKE_paint_reset_overlay_invalid(ePaintOverlayControlFlags flag)
{
  overlay_flags &= ~(flag);
}

bool BKE_paint_ensure_from_paintmode(Scene *sce, ePaintMode mode)
{
  ToolSettings *ts = sce->toolsettings;
  Paint **paint_ptr = nullptr;
  /* Some paint modes don't store paint settings as pointer, for these this can be set and
   * referenced by paint_ptr. */
  Paint *paint_tmp = nullptr;

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
      paint_tmp = (Paint *)&ts->imapaint;
      paint_ptr = &paint_tmp;
      break;
    case PAINT_MODE_SCULPT_UV:
      paint_ptr = (Paint **)&ts->uvsculpt;
      break;
    case PAINT_MODE_GPENCIL:
      paint_ptr = (Paint **)&ts->gp_paint;
      break;
    case PAINT_MODE_VERTEX_GPENCIL:
      paint_ptr = (Paint **)&ts->gp_vertexpaint;
      break;
    case PAINT_MODE_SCULPT_GPENCIL:
      paint_ptr = (Paint **)&ts->gp_sculptpaint;
      break;
    case PAINT_MODE_WEIGHT_GPENCIL:
      paint_ptr = (Paint **)&ts->gp_weightpaint;
      break;
    case PAINT_MODE_SCULPT_CURVES:
      paint_ptr = (Paint **)&ts->curves_sculpt;
      break;
    case PAINT_MODE_INVALID:
      break;
  }
  if (paint_ptr) {
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
      case PAINT_MODE_VERTEX_GPENCIL:
        return &ts->gp_vertexpaint->paint;
      case PAINT_MODE_SCULPT_GPENCIL:
        return &ts->gp_sculptpaint->paint;
      case PAINT_MODE_WEIGHT_GPENCIL:
        return &ts->gp_weightpaint->paint;
      case PAINT_MODE_SCULPT_CURVES:
        return &ts->curves_sculpt->paint;
      case PAINT_MODE_INVALID:
        return nullptr;
      default:
        return &ts->imapaint.paint;
    }
  }

  return nullptr;
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
    case PAINT_MODE_GPENCIL:
      return rna_enum_brush_gpencil_types_items;
    case PAINT_MODE_VERTEX_GPENCIL:
      return rna_enum_brush_gpencil_vertex_types_items;
    case PAINT_MODE_SCULPT_GPENCIL:
      return rna_enum_brush_gpencil_sculpt_types_items;
    case PAINT_MODE_WEIGHT_GPENCIL:
      return rna_enum_brush_gpencil_weight_types_items;
    case PAINT_MODE_SCULPT_CURVES:
      return rna_enum_brush_curves_sculpt_tool_items;
    case PAINT_MODE_INVALID:
      break;
  }
  return nullptr;
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
    case PAINT_MODE_VERTEX_GPENCIL:
      return "gpencil_vertex_tool";
    case PAINT_MODE_SCULPT_GPENCIL:
      return "gpencil_sculpt_tool";
    case PAINT_MODE_WEIGHT_GPENCIL:
      return "gpencil_weight_tool";
    case PAINT_MODE_SCULPT_CURVES:
      return "curves_sculpt_tool";
    case PAINT_MODE_INVALID:
      break;
  }

  /* Invalid paint mode. */
  return nullptr;
}

Paint *BKE_paint_get_active(Scene *sce, ViewLayer *view_layer)
{
  if (sce && view_layer) {
    ToolSettings *ts = sce->toolsettings;
    BKE_view_layer_synced_ensure(sce, view_layer);
    Object *actob = BKE_view_layer_active_object_get(view_layer);

    if (actob) {
      switch (actob->mode) {
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
        case OB_MODE_VERTEX_GPENCIL:
          return &ts->gp_vertexpaint->paint;
        case OB_MODE_SCULPT_GPENCIL:
          return &ts->gp_sculptpaint->paint;
        case OB_MODE_WEIGHT_GPENCIL:
          return &ts->gp_weightpaint->paint;
        case OB_MODE_SCULPT_CURVES:
          return &ts->curves_sculpt->paint;
        case OB_MODE_EDIT:
          return ts->uvsculpt ? &ts->uvsculpt->paint : nullptr;
        default:
          break;
      }
    }

    /* default to image paint */
    return &ts->imapaint.paint;
  }

  return nullptr;
}

Paint *BKE_paint_get_active_from_context(const bContext *C)
{
  Scene *sce = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  SpaceImage *sima;

  if (sce && view_layer) {
    ToolSettings *ts = sce->toolsettings;
    BKE_view_layer_synced_ensure(sce, view_layer);
    Object *obact = BKE_view_layer_active_object_get(view_layer);

    if ((sima = CTX_wm_space_image(C)) != nullptr) {
      if (obact && obact->mode == OB_MODE_EDIT) {
        if (sima->mode == SI_MODE_PAINT) {
          return &ts->imapaint.paint;
        }
        if (sima->mode == SI_MODE_UV) {
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

  return nullptr;
}

ePaintMode BKE_paintmode_get_active_from_context(const bContext *C)
{
  Scene *sce = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  SpaceImage *sima;

  if (sce && view_layer) {
    BKE_view_layer_synced_ensure(sce, view_layer);
    Object *obact = BKE_view_layer_active_object_get(view_layer);

    if ((sima = CTX_wm_space_image(C)) != nullptr) {
      if (obact && obact->mode == OB_MODE_EDIT) {
        if (sima->mode == SI_MODE_PAINT) {
          return PAINT_MODE_TEXTURE_2D;
        }
        if (sima->mode == SI_MODE_UV) {
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
        case OB_MODE_SCULPT_CURVES:
          return PAINT_MODE_SCULPT_CURVES;
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

ePaintMode BKE_paintmode_get_from_tool(const bToolRef *tref)
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
      case CTX_MODE_VERTEX_GPENCIL:
        return PAINT_MODE_VERTEX_GPENCIL;
      case CTX_MODE_SCULPT_GPENCIL:
        return PAINT_MODE_SCULPT_GPENCIL;
      case CTX_MODE_WEIGHT_GPENCIL:
        return PAINT_MODE_WEIGHT_GPENCIL;
      case CTX_MODE_SCULPT_CURVES:
        return PAINT_MODE_SCULPT_CURVES;
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
  return p ? (p->brush_eval ? p->brush_eval : p->brush) : nullptr;
}

const Brush *BKE_paint_brush_for_read(const Paint *p)
{
  return p ? p->brush : nullptr;
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
  else if (ts->sculpt && paint == &ts->sculpt->paint) {
    paint->runtime.tool_offset = offsetof(Brush, sculpt_tool);
    paint->runtime.ob_mode = OB_MODE_SCULPT;
  }
  else if (ts->vpaint && paint == &ts->vpaint->paint) {
    paint->runtime.tool_offset = offsetof(Brush, vertexpaint_tool);
    paint->runtime.ob_mode = OB_MODE_VERTEX_PAINT;
  }
  else if (ts->wpaint && paint == &ts->wpaint->paint) {
    paint->runtime.tool_offset = offsetof(Brush, weightpaint_tool);
    paint->runtime.ob_mode = OB_MODE_WEIGHT_PAINT;
  }
  else if (ts->uvsculpt && paint == &ts->uvsculpt->paint) {
    paint->runtime.tool_offset = offsetof(Brush, uv_sculpt_tool);
    paint->runtime.ob_mode = OB_MODE_EDIT;
  }
  else if (ts->gp_paint && paint == &ts->gp_paint->paint) {
    paint->runtime.tool_offset = offsetof(Brush, gpencil_tool);
    paint->runtime.ob_mode = OB_MODE_PAINT_GPENCIL;
  }
  else if (ts->gp_vertexpaint && paint == &ts->gp_vertexpaint->paint) {
    paint->runtime.tool_offset = offsetof(Brush, gpencil_vertex_tool);
    paint->runtime.ob_mode = OB_MODE_VERTEX_GPENCIL;
  }
  else if (ts->gp_sculptpaint && paint == &ts->gp_sculptpaint->paint) {
    paint->runtime.tool_offset = offsetof(Brush, gpencil_sculpt_tool);
    paint->runtime.ob_mode = OB_MODE_SCULPT_GPENCIL;
  }
  else if (ts->gp_weightpaint && paint == &ts->gp_weightpaint->paint) {
    paint->runtime.tool_offset = offsetof(Brush, gpencil_weight_tool);
    paint->runtime.ob_mode = OB_MODE_WEIGHT_GPENCIL;
  }
  else if (ts->curves_sculpt && paint == &ts->curves_sculpt->paint) {
    paint->runtime.tool_offset = offsetof(Brush, curves_sculpt_tool);
    paint->runtime.ob_mode = OB_MODE_SCULPT_CURVES;
  }
  else {
    BLI_assert_unreachable();
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
    case PAINT_MODE_VERTEX_GPENCIL:
      return offsetof(Brush, gpencil_vertex_tool);
    case PAINT_MODE_SCULPT_GPENCIL:
      return offsetof(Brush, gpencil_sculpt_tool);
    case PAINT_MODE_WEIGHT_GPENCIL:
      return offsetof(Brush, gpencil_weight_tool);
    case PAINT_MODE_SCULPT_CURVES:
      return offsetof(Brush, curves_sculpt_tool);
    case PAINT_MODE_INVALID:
      break; /* We don't use these yet. */
  }
  return 0;
}

PaintCurve *BKE_paint_curve_add(Main *bmain, const char *name)
{
  PaintCurve *pc = static_cast<PaintCurve *>(BKE_id_new(bmain, ID_PC, name));
  return pc;
}

Palette *BKE_paint_palette(Paint *p)
{
  return p ? p->palette : nullptr;
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

void BKE_palette_color_remove(Palette *palette, PaletteColor *color)
{
  if (BLI_listbase_count_at_most(&palette->colors, palette->active_color) == palette->active_color)
  {
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
  Palette *palette = static_cast<Palette *>(BKE_id_new(bmain, ID_PAL, name));
  return palette;
}

PaletteColor *BKE_palette_color_add(Palette *palette)
{
  PaletteColor *color = MEM_cnew<PaletteColor>(__func__);
  BLI_addtail(&palette->colors, color);
  return color;
}

bool BKE_palette_is_empty(const Palette *palette)
{
  return BLI_listbase_is_empty(&palette->colors);
}

/* helper function to sort using qsort */
static int palettecolor_compare_hsv(const void *a1, const void *a2)
{
  const tPaletteColorHSV *ps1 = static_cast<const tPaletteColorHSV *>(a1);
  const tPaletteColorHSV *ps2 = static_cast<const tPaletteColorHSV *>(a2);

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

/* helper function to sort using qsort */
static int palettecolor_compare_svh(const void *a1, const void *a2)
{
  const tPaletteColorHSV *ps1 = static_cast<const tPaletteColorHSV *>(a1);
  const tPaletteColorHSV *ps2 = static_cast<const tPaletteColorHSV *>(a2);

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

static int palettecolor_compare_vhs(const void *a1, const void *a2)
{
  const tPaletteColorHSV *ps1 = static_cast<const tPaletteColorHSV *>(a1);
  const tPaletteColorHSV *ps2 = static_cast<const tPaletteColorHSV *>(a2);

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

static int palettecolor_compare_luminance(const void *a1, const void *a2)
{
  const tPaletteColorHSV *ps1 = static_cast<const tPaletteColorHSV *>(a1);
  const tPaletteColorHSV *ps2 = static_cast<const tPaletteColorHSV *>(a2);

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

void BKE_palette_sort_hsv(tPaletteColorHSV *color_array, const int totcol)
{
  /* Sort by Hue, Saturation and Value. */
  qsort(color_array, totcol, sizeof(tPaletteColorHSV), palettecolor_compare_hsv);
}

void BKE_palette_sort_svh(tPaletteColorHSV *color_array, const int totcol)
{
  /* Sort by Saturation, Value and Hue. */
  qsort(color_array, totcol, sizeof(tPaletteColorHSV), palettecolor_compare_svh);
}

void BKE_palette_sort_vhs(tPaletteColorHSV *color_array, const int totcol)
{
  /* Sort by Saturation, Value and Hue. */
  qsort(color_array, totcol, sizeof(tPaletteColorHSV), palettecolor_compare_vhs);
}

void BKE_palette_sort_luminance(tPaletteColorHSV *color_array, const int totcol)
{
  /* Sort by Luminance (calculated with the average, enough for sorting). */
  qsort(color_array, totcol, sizeof(tPaletteColorHSV), palettecolor_compare_luminance);
}

bool BKE_palette_from_hash(Main *bmain, GHash *color_table, const char *name, const bool linear)
{
  tPaletteColorHSV *color_array = nullptr;
  tPaletteColorHSV *col_elm = nullptr;
  bool done = false;

  const int totpal = BLI_ghash_len(color_table);

  if (totpal > 0) {
    color_array = static_cast<tPaletteColorHSV *>(
        MEM_calloc_arrayN(totpal, sizeof(tPaletteColorHSV), __func__));
    /* Put all colors in an array. */
    GHashIterator gh_iter;
    int t = 0;
    GHASH_ITER (gh_iter, color_table) {
      const uint col = POINTER_AS_INT(BLI_ghashIterator_getValue(&gh_iter));
      float r, g, b;
      float h, s, v;
      cpack_to_rgb(col, &r, &g, &b);
      rgb_to_hsv(r, g, b, &h, &s, &v);

      col_elm = &color_array[t];
      col_elm->rgb[0] = r;
      col_elm->rgb[1] = g;
      col_elm->rgb[2] = b;
      col_elm->h = h;
      col_elm->s = s;
      col_elm->v = v;
      t++;
    }
  }

  /* Create the Palette. */
  if (totpal > 0) {
    /* Sort by Hue and saturation. */
    BKE_palette_sort_hsv(color_array, totpal);

    Palette *palette = BKE_palette_add(bmain, name);
    if (palette) {
      for (int i = 0; i < totpal; i++) {
        col_elm = &color_array[i];
        PaletteColor *palcol = BKE_palette_color_add(palette);
        if (palcol) {
          copy_v3_v3(palcol->rgb, col_elm->rgb);
          if (linear) {
            linearrgb_to_srgb_v3_v3(palcol->rgb, palcol->rgb);
          }
        }
      }
      done = true;
    }
  }
  else {
    done = false;
  }

  if (totpal > 0) {
    MEM_SAFE_FREE(color_array);
  }

  return done;
}

bool BKE_paint_select_face_test(Object *ob)
{
  return ((ob != nullptr) && (ob->type == OB_MESH) && (ob->data != nullptr) &&
          (((Mesh *)ob->data)->editflag & ME_EDIT_PAINT_FACE_SEL) &&
          (ob->mode & (OB_MODE_VERTEX_PAINT | OB_MODE_WEIGHT_PAINT | OB_MODE_TEXTURE_PAINT)));
}

bool BKE_paint_select_vert_test(Object *ob)
{
  return ((ob != nullptr) && (ob->type == OB_MESH) && (ob->data != nullptr) &&
          (((Mesh *)ob->data)->editflag & ME_EDIT_PAINT_VERT_SEL) &&
          (ob->mode & OB_MODE_WEIGHT_PAINT || ob->mode & OB_MODE_VERTEX_PAINT));
}

bool BKE_paint_select_elem_test(Object *ob)
{
  return (BKE_paint_select_vert_test(ob) || BKE_paint_select_face_test(ob));
}

bool BKE_paint_always_hide_test(Object *ob)
{
  return ((ob != nullptr) && (ob->type == OB_MESH) && (ob->data != nullptr) &&
          (ob->mode & OB_MODE_WEIGHT_PAINT || ob->mode & OB_MODE_VERTEX_PAINT));
}

void BKE_paint_cavity_curve_preset(Paint *p, int preset)
{
  CurveMapping *cumap = nullptr;
  CurveMap *cuma = nullptr;

  if (!p->cavity_curve) {
    p->cavity_curve = BKE_curvemapping_add(1, 0, 0, 1, 1);
  }
  cumap = p->cavity_curve;
  cumap->flag &= ~CUMA_EXTEND_EXTRAPOLATE;
  cumap->preset = preset;

  cuma = cumap->cm;
  BKE_curvemap_reset(cuma, &cumap->clipr, cumap->preset, CURVEMAP_SLOPE_POSITIVE);
  BKE_curvemapping_changed(cumap, false);
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
    case PAINT_MODE_SCULPT_CURVES:
      return OB_MODE_SCULPT_CURVES;
    case PAINT_MODE_INVALID:
    default:
      return OB_MODE_OBJECT;
  }
}

bool BKE_paint_ensure(ToolSettings *ts, Paint **r_paint)
{
  Paint *paint = nullptr;
  if (*r_paint) {
    /* Tool offset should never be 0 for initialized paint settings, so it's a reliable way to
     * check if already initialized. */
    if ((*r_paint)->runtime.tool_offset == 0) {
      /* Currently only image painting is initialized this way, others have to be allocated. */
      BLI_assert(ELEM(*r_paint, (Paint *)&ts->imapaint));

      BKE_paint_runtime_init(ts, *r_paint);
    }
    else {
      BLI_assert(ELEM(*r_paint,
                      /* Cast is annoying, but prevent nullptr-pointer access. */
                      (Paint *)ts->gp_paint,
                      (Paint *)ts->gp_vertexpaint,
                      (Paint *)ts->gp_sculptpaint,
                      (Paint *)ts->gp_weightpaint,
                      (Paint *)ts->sculpt,
                      (Paint *)ts->vpaint,
                      (Paint *)ts->wpaint,
                      (Paint *)ts->uvsculpt,
                      (Paint *)ts->curves_sculpt,
                      (Paint *)&ts->imapaint));
#ifdef DEBUG
      Paint paint_test = **r_paint;
      BKE_paint_runtime_init(ts, *r_paint);
      /* Swap so debug doesn't hide errors when release fails. */
      std::swap(**r_paint, paint_test);
      BLI_assert(paint_test.runtime.ob_mode == (*r_paint)->runtime.ob_mode);
      BLI_assert(paint_test.runtime.tool_offset == (*r_paint)->runtime.tool_offset);
#endif
    }
    return true;
  }

  if (((VPaint **)r_paint == &ts->vpaint) || ((VPaint **)r_paint == &ts->wpaint)) {
    VPaint *data = MEM_cnew<VPaint>(__func__);
    paint = &data->paint;
  }
  else if ((Sculpt **)r_paint == &ts->sculpt) {
    Sculpt *data = MEM_cnew<Sculpt>(__func__);
    paint = &data->paint;

    /* Turn on X plane mirror symmetry by default */
    paint->symmetry_flags |= PAINT_SYMM_X;

    /* Make sure at least dyntopo subdivision is enabled */
    data->flags |= SCULPT_DYNTOPO_SUBDIVIDE | SCULPT_DYNTOPO_COLLAPSE | SCULPT_DYNTOPO_ENABLED;
  }
  else if ((GpPaint **)r_paint == &ts->gp_paint) {
    GpPaint *data = MEM_cnew<GpPaint>(__func__);
    paint = &data->paint;
  }
  else if ((GpVertexPaint **)r_paint == &ts->gp_vertexpaint) {
    GpVertexPaint *data = MEM_cnew<GpVertexPaint>(__func__);
    paint = &data->paint;
  }
  else if ((GpSculptPaint **)r_paint == &ts->gp_sculptpaint) {
    GpSculptPaint *data = MEM_cnew<GpSculptPaint>(__func__);
    paint = &data->paint;
  }
  else if ((GpWeightPaint **)r_paint == &ts->gp_weightpaint) {
    GpWeightPaint *data = MEM_cnew<GpWeightPaint>(__func__);
    paint = &data->paint;
  }
  else if ((UvSculpt **)r_paint == &ts->uvsculpt) {
    UvSculpt *data = MEM_cnew<UvSculpt>(__func__);
    paint = &data->paint;
  }
  else if ((CurvesSculpt **)r_paint == &ts->curves_sculpt) {
    CurvesSculpt *data = MEM_cnew<CurvesSculpt>(__func__);
    paint = &data->paint;
  }
  else if (*r_paint == &ts->imapaint.paint) {
    paint = &ts->imapaint.paint;
  }

  paint->flags |= PAINT_SHOW_BRUSH;

  *r_paint = paint;

  BKE_paint_runtime_init(ts, paint);

  return false;
}

void BKE_paint_init(Main *bmain, Scene *sce, ePaintMode mode, const uchar col[3])
{
  UnifiedPaintSettings *ups = &sce->toolsettings->unified_paint_settings;
  Paint *paint = BKE_paint_get_active_from_paintmode(sce, mode);

  BKE_paint_ensure_from_paintmode(sce, mode);

  /* If there's no brush, create one */
  if (PAINT_MODE_HAS_BRUSH(mode)) {
    Brush *brush = BKE_paint_brush(paint);
    if (brush == nullptr) {
      eObjectMode ob_mode = BKE_paint_object_mode_from_paintmode(mode);
      brush = BKE_brush_first_search(bmain, ob_mode);
      if (!brush) {
        brush = BKE_brush_add(bmain, "Brush", ob_mode);
        id_us_min(&brush->id); /* Fake user only. */
      }
      BKE_paint_brush_set(paint, brush);
    }
  }

  copy_v3_v3_uchar(paint->paint_cursor_col, col);
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
  BKE_curvemapping_free(paint->cavity_curve);
  MEM_SAFE_FREE(paint->tool_slots);
}

void BKE_paint_copy(Paint *src, Paint *tar, const int flag)
{
  tar->brush = src->brush;
  tar->cavity_curve = BKE_curvemapping_copy(src->cavity_curve);
  tar->tool_slots = static_cast<PaintToolSlot *>(MEM_dupallocN(src->tool_slots));

  if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
    id_us_plus((ID *)tar->brush);
    id_us_plus((ID *)tar->palette);
    if (src->tool_slots != nullptr) {
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
    copy_v3_v3(stroke, ob->object_to_world[3]);
  }
}

void BKE_paint_blend_write(BlendWriter *writer, Paint *p)
{
  if (p->cavity_curve) {
    BKE_curvemapping_blend_write(writer, p->cavity_curve);
  }
  BLO_write_struct_array(writer, PaintToolSlot, p->tool_slots_len, p->tool_slots);
}

void BKE_paint_blend_read_data(BlendDataReader *reader, const Scene *scene, Paint *p)
{
  if (p->num_input_samples < 1) {
    p->num_input_samples = 1;
  }

  BLO_read_data_address(reader, &p->cavity_curve);
  if (p->cavity_curve) {
    BKE_curvemapping_blend_read(reader, p->cavity_curve);
  }
  else {
    BKE_paint_cavity_curve_preset(p, CURVE_PRESET_LINE);
  }

  BLO_read_data_address(reader, &p->tool_slots);

  /* Workaround for invalid data written in older versions. */
  const size_t expected_size = sizeof(PaintToolSlot) * p->tool_slots_len;
  if (p->tool_slots && MEM_allocN_len(p->tool_slots) < expected_size) {
    MEM_freeN(p->tool_slots);
    p->tool_slots = static_cast<PaintToolSlot *>(MEM_callocN(expected_size, "PaintToolSlot"));
  }

  BKE_paint_runtime_init(scene->toolsettings, p);
}

void BKE_paint_blend_read_lib(BlendLibReader *reader, Scene *sce, Paint *p)
{
  if (p) {
    BLO_read_id_address(reader, sce->id.lib, &p->brush);
    for (int i = 0; i < p->tool_slots_len; i++) {
      if (p->tool_slots[i].brush != nullptr) {
        BLO_read_id_address(reader, sce->id.lib, &p->tool_slots[i].brush);
      }
    }
    BLO_read_id_address(reader, sce->id.lib, &p->palette);
    p->paint_cursor = nullptr;

    BKE_paint_runtime_init(sce->toolsettings, p);
  }
}

bool paint_is_face_hidden(const int *looptri_polys, const bool *hide_poly, const int tri_index)
{
  if (!hide_poly) {
    return false;
  }
  return hide_poly[looptri_polys[tri_index]];
}

bool paint_is_grid_face_hidden(const uint *grid_hidden, int gridsize, int x, int y)
{
  /* Skip face if any of its corners are hidden. */
  return (BLI_BITMAP_TEST(grid_hidden, y * gridsize + x) ||
          BLI_BITMAP_TEST(grid_hidden, y * gridsize + x + 1) ||
          BLI_BITMAP_TEST(grid_hidden, (y + 1) * gridsize + x + 1) ||
          BLI_BITMAP_TEST(grid_hidden, (y + 1) * gridsize + x));
}

bool paint_is_bmesh_face_hidden(BMFace *f)
{
  return BM_elem_flag_test(f, BM_ELEM_HIDDEN);
}

float paint_grid_paint_mask(const GridPaintMask *gpm, uint level, uint x, uint y)
{
  int factor = BKE_ccg_factor(level, gpm->level);
  int gridsize = BKE_ccg_gridsize(gpm->level);

  return gpm->data[(y * factor) * gridsize + (x * factor)];
}

/* Threshold to move before updating the brush rotation. */
#define RAKE_THRESHHOLD 20

void paint_update_brush_rake_rotation(UnifiedPaintSettings *ups, Brush *brush, float rotation)
{
  ups->brush_rotation = rotation;

  if (brush->mask_mtex.brush_angle_mode & MTEX_ANGLE_RAKE) {
    ups->brush_rotation_sec = rotation;
  }
  else {
    ups->brush_rotation_sec = 0.0f;
  }
}

static bool paint_rake_rotation_active(const MTex &mtex)
{
  return mtex.tex && mtex.brush_angle_mode & MTEX_ANGLE_RAKE;
}

static const bool paint_rake_rotation_active(const Brush &brush, ePaintMode paint_mode)
{
  return paint_rake_rotation_active(brush.mtex) || paint_rake_rotation_active(brush.mask_mtex) ||
         BKE_brush_has_cube_tip(&brush, paint_mode);
}

bool paint_calculate_rake_rotation(UnifiedPaintSettings *ups,
                                   Brush *brush,
                                   const float mouse_pos[2],
                                   const float initial_mouse_pos[2],
                                   ePaintMode paint_mode)
{
  bool ok = false;
  if (paint_rake_rotation_active(*brush, paint_mode)) {
    const float r = RAKE_THRESHHOLD;
    float rotation;

    if (brush->flag & BRUSH_DRAG_DOT) {
      const float dx = mouse_pos[0] - initial_mouse_pos[0];
      const float dy = mouse_pos[1] - initial_mouse_pos[1];

      if (dx * dx + dy * dy > 0.5f) {
        ups->brush_rotation = ups->brush_rotation_sec = atan2f(dx, dy) + (float)M_PI;
        return true;
      }
      else {
        return false;
      }
    }

    float dpos[2];
    sub_v2_v2v2(dpos, ups->last_rake, mouse_pos);

    if (len_squared_v2(dpos) >= r * r) {
      rotation = atan2f(dpos[0], dpos[1]);

      copy_v2_v2(ups->last_rake, mouse_pos);

      ups->last_rake_angle = rotation;

      paint_update_brush_rake_rotation(ups, brush, rotation);
      ok = true;
    }
    /* Make sure we reset here to the last rotation to avoid accumulating
     * values in case a random rotation is also added. */
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

static bool sculpt_boundary_flags_ensure(Object *ob, PBVH *pbvh, int totvert)
{
  SculptSession *ss = ob->sculpt;
  bool ret = false;

  if (!ss->attrs.boundary_flags) {
    SculptAttributeParams params = {0};

    params.nointerp = true;
    // params.nocopy = true;

    ss->attrs.boundary_flags = sculpt_attribute_ensure_ex(ob,
                                                          ATTR_DOMAIN_POINT,
                                                          CD_PROP_INT32,
                                                          SCULPT_ATTRIBUTE_NAME(boundary_flags),
                                                          &params,
                                                          BKE_pbvh_type(pbvh));

    for (int i = 0; i < totvert; i++) {
      PBVHVertRef vertex = BKE_pbvh_index_to_vertex(pbvh, i);
      BKE_sculpt_boundary_flag_update(ss, vertex);
    }

    ret = true;
  }

  BKE_pbvh_set_boundary_flags(pbvh, reinterpret_cast<int *>(ss->attrs.boundary_flags->data));

  return ret;
}

bool BKE_sculptsession_boundary_flags_ensure(Object *ob)
{
  return sculpt_boundary_flags_ensure(
      ob, ob->sculpt->pbvh, BKE_sculptsession_vertex_count(ob->sculpt));
}

void BKE_sculptsession_free_deformMats(SculptSession *ss)
{
  MEM_SAFE_FREE(ss->orig_cos);
  MEM_SAFE_FREE(ss->deform_cos);
  MEM_SAFE_FREE(ss->deform_imats);
}

void BKE_sculptsession_free_vwpaint_data(SculptSession *ss)
{
  SculptVertexPaintGeomMap *gmap = nullptr;
  if (ss->mode_type == OB_MODE_VERTEX_PAINT) {
    gmap = &ss->mode.vpaint.gmap;
  }
  else if (ss->mode_type == OB_MODE_WEIGHT_PAINT) {
    gmap = &ss->mode.wpaint.gmap;

    MEM_SAFE_FREE(ss->mode.wpaint.alpha_weight);
    if (ss->mode.wpaint.dvert_prev) {
      BKE_defvert_array_free_elems(ss->mode.wpaint.dvert_prev, ss->totvert);
      MEM_freeN(ss->mode.wpaint.dvert_prev);
      ss->mode.wpaint.dvert_prev = nullptr;
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

/**
 * Write out the sculpt dynamic-topology #BMesh to the #Mesh.
 */
static void sculptsession_bm_to_me_update_data_only(Object *ob, bool /*reorder*/)
{
  SculptSession *ss = ob->sculpt;

  if (ss->bm && ob->data) {
    BKE_sculptsession_update_attr_refs(ob);

    BMeshToMeshParams params = {0};
    params.update_shapekey_indices = true;

    BM_mesh_bm_to_me(nullptr, ss->bm, static_cast<Mesh *>(ob->data), &params);
  }
}

void BKE_sculptsession_bm_to_me(Object *ob, bool reorder)
{
  if (ob && ob->sculpt) {
    sculptsession_bm_to_me_update_data_only(ob, reorder);

    /* Ensure the objects evaluated mesh doesn't hold onto arrays
     * now realloc'd in the mesh #34473. */
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  }
}

static void sculptsession_free_pbvh(Object *object)
{
  SculptSession *ss = object->sculpt;

  if (!ss) {
    return;
  }

  if (ss->pbvh) {
    if (ss->pmap) {
      BKE_pbvh_pmap_release(ss->pmap);
      ss->pmap = nullptr;
    }

    BKE_pbvh_free(ss->pbvh);

    ss->needs_pbvh_rebuild = false;
    ss->pbvh = nullptr;
  }

  MEM_SAFE_FREE(ss->epmap);
  MEM_SAFE_FREE(ss->epmap_mem);

  MEM_SAFE_FREE(ss->vemap);
  MEM_SAFE_FREE(ss->vemap_mem);

  MEM_SAFE_FREE(ss->preview_vert_list);
  ss->preview_vert_count = 0;

  MEM_SAFE_FREE(ss->vertex_info.boundary);
  MEM_SAFE_FREE(ss->vertex_info.symmetrize_map);

  MEM_SAFE_FREE(ss->fake_neighbors.fake_neighbor_index);
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

    if (ss->bm_idmap) {
      BM_idmap_destroy(ss->bm_idmap);
      ss->bm_idmap = nullptr;
    }

    if (ss->bm_log && BM_log_free(ss->bm_log, true)) {
      ss->bm_log = nullptr;
    }

    /*try to save current mesh*/
    BKE_sculpt_attribute_destroy_temporary_all(ob);

    if (ss->bm) {
      BKE_sculptsession_bm_to_me(ob, true);
      ss->bm = nullptr;
      // BM_mesh_free(ss->bm);
    }

    CustomData_free(&ss->temp_vdata, ss->temp_vdata_elems);
    CustomData_free(&ss->temp_pdata, ss->temp_pdata_elems);

    if (!ss->pbvh) {
      BKE_pbvh_pmap_release(ss->pmap);
      ss->pmap = nullptr;
    }

    sculptsession_free_pbvh(ob);

    MEM_SAFE_FREE(ss->epmap);
    MEM_SAFE_FREE(ss->epmap_mem);

    MEM_SAFE_FREE(ss->vemap);
    MEM_SAFE_FREE(ss->vemap_mem);

    if (ss->tex_pool) {
      BKE_image_pool_free(ss->tex_pool);
    }

    MEM_SAFE_FREE(ss->orig_cos);
    MEM_SAFE_FREE(ss->deform_cos);
    MEM_SAFE_FREE(ss->deform_imats);

    if (ss->pose_ik_chain_preview) {
      for (int i = 0; i < ss->pose_ik_chain_preview->tot_segments; i++) {
        MEM_SAFE_FREE(ss->pose_ik_chain_preview->segments[i].weights);
      }
      MEM_SAFE_FREE(ss->pose_ik_chain_preview->segments);
      MEM_SAFE_FREE(ss->pose_ik_chain_preview);
    }

    if (ss->boundary_preview) {
      MEM_SAFE_FREE(ss->boundary_preview->verts);
      MEM_SAFE_FREE(ss->boundary_preview->edges);
      MEM_SAFE_FREE(ss->boundary_preview->distance);
      MEM_SAFE_FREE(ss->boundary_preview->edit_info);
      MEM_SAFE_FREE(ss->boundary_preview);
    }

    BKE_sculptsession_free_vwpaint_data(ob->sculpt);

    MEM_SAFE_FREE(ss->last_paint_canvas_key);

    MEM_freeN(ss);

    ob->sculpt = nullptr;
  }
}

static MultiresModifierData *sculpt_multires_modifier_get(const Scene *scene,
                                                          Object *ob,
                                                          const bool auto_create_mdisps)
{
  Mesh *me = (Mesh *)ob->data;
  ModifierData *md;
  VirtualModifierData virtualModifierData;

  if (ob->sculpt && ob->sculpt->bm) {
    /* Can't combine multires and dynamic topology. */
    return nullptr;
  }

  bool need_mdisps = false;

  if (!CustomData_get_layer(&me->ldata, CD_MDISPS)) {
    if (!auto_create_mdisps) {
      /* Multires can't work without displacement layer. */
      return nullptr;
    }
    need_mdisps = true;
  }

  /* Weight paint operates on original vertices, and needs to treat multires as regular modifier
   * to make it so that PBVH vertices are at the multires surface. */
  if ((ob->mode & OB_MODE_SCULPT) == 0) {
    return nullptr;
  }

  for (md = BKE_modifiers_get_virtual_modifierlist(ob, &virtualModifierData); md; md = md->next) {
    if (md->type == eModifierType_Multires) {
      MultiresModifierData *mmd = (MultiresModifierData *)md;

      if (!BKE_modifier_is_enabled(scene, md, eModifierMode_Realtime)) {
        continue;
      }

      if (mmd->sculptlvl > 0 && !(mmd->flags & eMultiresModifierFlag_UseSculptBaseMesh)) {
        if (need_mdisps) {
          CustomData_add_layer(&me->ldata, CD_MDISPS, CD_SET_DEFAULT, me->totloop);
        }

        return mmd;
      }

      return nullptr;
    }
  }

  return nullptr;
}

MultiresModifierData *BKE_sculpt_multires_active(const Scene *scene, Object *ob)
{
  return sculpt_multires_modifier_get(scene, ob, false);
}

/* Checks if there are any supported deformation modifiers active */
static bool sculpt_modifiers_active(Scene *scene, Sculpt *sd, Object *ob)
{
  ModifierData *md;
  Mesh *me = (Mesh *)ob->data;
  VirtualModifierData virtualModifierData;

  if (ob->sculpt->bm || BKE_sculpt_multires_active(scene, ob)) {
    return false;
  }

  /* Non-locked shape keys could be handled in the same way as deformed mesh. */
  if ((ob->shapeflag & OB_SHAPE_LOCK) == 0 && me->key && ob->shapenr) {
    return true;
  }

  md = BKE_modifiers_get_virtual_modifierlist(ob, &virtualModifierData);

  /* Exception for shape keys because we can edit those. */
  for (; md; md = md->next) {
    const ModifierTypeInfo *mti = BKE_modifier_get_info(static_cast<ModifierType>(md->type));
    if (!BKE_modifier_is_enabled(scene, md, eModifierMode_Realtime)) {
      continue;
    }
    if (md->type == eModifierType_Multires && (ob->mode & OB_MODE_SCULPT)) {
      MultiresModifierData *mmd = (MultiresModifierData *)md;
      if (!(mmd->flags & eMultiresModifierFlag_UseSculptBaseMesh)) {
        continue;
      }
    }
    if (md->type == eModifierType_ShapeKey) {
      continue;
    }

    if (mti->type == eModifierTypeType_OnlyDeform) {
      return true;
    }
    if ((sd->flags & SCULPT_ONLY_DEFORM) == 0) {
      return true;
    }
  }

  return false;
}

void BKE_sculpt_ensure_idmap(Object *ob)
{
  if (!ob->sculpt->bm_idmap) {
    ob->sculpt->bm_idmap = BM_idmap_new(ob->sculpt->bm, BM_VERT | BM_EDGE | BM_FACE);
    BM_idmap_check_ids(ob->sculpt->bm_idmap);

    BKE_sculptsession_update_attr_refs(ob);
  }
}

char BKE_get_fset_boundary_symflag(Object *object)
{
  const Mesh *mesh = BKE_mesh_from_object(object);
  return mesh->flag & ME_SCULPT_MIRROR_FSET_BOUNDARIES ? mesh->symmetry : 0;
}

void BKE_sculptsession_ignore_uvs_set(Object *ob, bool value)
{
  ob->sculpt->ignore_uvs = value;

  if (ob->sculpt->pbvh) {
    BKE_pbvh_ignore_uvs_set(ob->sculpt->pbvh, value);
  }
}

static void sculpt_check_face_areas(Object *ob, PBVH *pbvh)
{
  SculptSession *ss = ob->sculpt;

  if (!ss->attrs.face_areas) {
    SculptAttributeParams params = {0};
    ss->attrs.face_areas = sculpt_attribute_ensure_ex(ob,
                                                      ATTR_DOMAIN_FACE,
                                                      CD_PROP_FLOAT2,
                                                      SCULPT_ATTRIBUTE_NAME(face_areas),
                                                      &params,
                                                      BKE_pbvh_type(pbvh));
  }
}

/* Helper function to keep persistent base attribute references up to
 * date.  This is a bit more tricky since they persist across strokes.
 */
static void sculpt_update_persistent_base(Object *ob)
{
  SculptSession *ss = ob->sculpt;

  ss->attrs.persistent_co = BKE_sculpt_attribute_get(
      ob, ATTR_DOMAIN_POINT, CD_PROP_FLOAT3, SCULPT_ATTRIBUTE_NAME(persistent_co));
  ss->attrs.persistent_no = BKE_sculpt_attribute_get(
      ob, ATTR_DOMAIN_POINT, CD_PROP_FLOAT3, SCULPT_ATTRIBUTE_NAME(persistent_no));
  ss->attrs.persistent_disp = BKE_sculpt_attribute_get(
      ob, ATTR_DOMAIN_POINT, CD_PROP_FLOAT, SCULPT_ATTRIBUTE_NAME(persistent_disp));
}

static void sculpt_update_object(
    Depsgraph *depsgraph, Object *ob, Object *ob_eval, bool /*need_pmap*/, bool is_paint_tool)
{
  Scene *scene = DEG_get_input_scene(depsgraph);
  Sculpt *sd = scene->toolsettings->sculpt;
  SculptSession *ss = ob->sculpt;
  Mesh *me = BKE_object_get_original_mesh(ob);
  Mesh *me_eval = BKE_object_get_evaluated_mesh(ob_eval);
  MultiresModifierData *mmd = sculpt_multires_modifier_get(scene, ob, true);
  const bool use_face_sets = (ob->mode & OB_MODE_SCULPT) != 0;

  BLI_assert(me_eval != nullptr);

  /* This is for handling a newly opened file with no object visible, causing me_eval==NULL. */
  if (me_eval == nullptr) {
    return;
  }

  ss->depsgraph = depsgraph;

  ss->bm_smooth_shading = scene->toolsettings->sculpt->flags & SCULPT_DYNTOPO_SMOOTH_SHADING;
  ss->ignore_uvs = me->flag & ME_SCULPT_IGNORE_UVS;

  ss->deform_modifiers_active = sculpt_modifiers_active(scene, sd, ob);

  ss->building_vp_handle = false;

  ss->scene = scene;
  if (scene->toolsettings) {
    ss->save_temp_layers = scene->toolsettings->save_temp_layers;
  }

  ss->boundary_symmetry = (int)BKE_get_fset_boundary_symflag(ob);
  ss->shapekey_active = (mmd == nullptr) ? BKE_keyblock_from_object(ob) : nullptr;

  ss->material_index = (int *)CustomData_get_layer_named(
      &me->pdata, CD_PROP_INT32, "material_index");

  /* NOTE: Weight pPaint require mesh info for loop lookup, but it never uses multires code path,
   * so no extra checks is needed here. */
  if (mmd) {
    ss->multires.active = true;
    ss->multires.modifier = mmd;
    ss->multires.level = mmd->sculptlvl;
    ss->totvert = me_eval->totvert;
    ss->totpoly = me_eval->totpoly;
    ss->totfaces = me->totpoly;
    ss->totloops = me->totloop;
    ss->totedges = me->totedge;

    /* These are assigned to the base mesh in Multires. This is needed because Face Sets operators
     * and tools use the Face Sets data from the base mesh when Multires is active. */
    ss->vert_positions = BKE_mesh_vert_positions_for_write(me);
    ss->polys = me->polys();
    ss->edges = me->edges();
    ss->corner_verts = me->corner_verts();
    ss->corner_edges = me->corner_edges();
  }
  else {
    ss->totvert = me->totvert;
    ss->totpoly = me->totpoly;
    ss->totfaces = me->totpoly;
    ss->totloops = me->totloop;
    ss->totedges = me->totedge;
    ss->vert_positions = BKE_mesh_vert_positions_for_write(me);

    ss->sharp_edge = (bool *)CustomData_get_layer_named_for_write(
        &me->edata, CD_PROP_BOOL, "sharp_edge", me->totedge);
    ss->seam_edge = (bool *)CustomData_get_layer_named_for_write(
        &me->edata, CD_PROP_BOOL, ".uv_seam", me->totedge);

    ss->vdata = &me->vdata;
    ss->edata = &me->edata;
    ss->ldata = &me->ldata;
    ss->pdata = &me->pdata;

    ss->polys = me->polys();
    ss->edges = me->edges();
    ss->corner_verts = me->corner_verts();
    ss->corner_edges = me->corner_edges();

    ss->multires.active = false;
    ss->multires.modifier = nullptr;
    ss->multires.level = 0;
    ss->vmask = static_cast<float *>(
        CustomData_get_layer_for_write(&me->vdata, CD_PAINT_MASK, me->totvert));

    CustomDataLayer *layer;
    eAttrDomain domain;

    /* Do not pass ss->pbvh to BKE_pbvh_get_color_layer,
     * if it does exist it might not be PBVH_GRIDS.
     */
    if (BKE_pbvh_get_color_layer(nullptr, me, &layer, &domain)) {
      if (layer->type == CD_PROP_COLOR) {
        ss->vcol = static_cast<MPropCol *>(layer->data);
      }
      else {
        ss->mcol = static_cast<MLoopCol *>(layer->data);
      }

      ss->vcol_domain = domain;
      ss->vcol_type = static_cast<eCustomDataType>(layer->type);
    }
    else {
      ss->vcol = nullptr;
      ss->mcol = nullptr;

      ss->vcol_type = (eCustomDataType)-1;
      ss->vcol_domain = ATTR_DOMAIN_POINT;
    }
  }

  ss->hide_poly = (bool *)CustomData_get_layer_named_for_write(
      &me->pdata, CD_PROP_BOOL, ".hide_poly", me->totpoly);

  ss->subdiv_ccg = me_eval->runtime->subdiv_ccg;
  ss->fast_draw = (scene->toolsettings->sculpt->flags & SCULPT_FAST_DRAW) != 0;

  PBVH *pbvh = BKE_sculpt_object_pbvh_ensure(depsgraph, ob);
  sculpt_check_face_areas(ob, pbvh);

  /* Sculpt Face Sets. */
  if (use_face_sets) {
    int *face_sets = static_cast<int *>(CustomData_get_layer_named_for_write(
        &me->pdata, CD_PROP_INT32, ".sculpt_face_set", me->totpoly));

    if (face_sets) {
      /* Load into sculpt attribute system. */
      ss->face_sets = BKE_sculpt_face_sets_ensure(ob);
    }
    else {
      ss->face_sets = nullptr;
      if (ss->pbvh && !ss->bm) {
        BKE_pbvh_face_sets_set(ss->pbvh, nullptr);
      }
    }
  }
  else {
    ss->face_sets = nullptr;
    if (ss->pbvh && !ss->bm) {
      BKE_pbvh_face_sets_set(ss->pbvh, nullptr);
    }
  }

  sculpt_boundary_flags_ensure(ob, pbvh, BKE_sculptsession_vertex_count(ss));

  BKE_pbvh_update_active_vcol(pbvh, me);

  if (BKE_pbvh_type(pbvh) == PBVH_FACES) {
    ss->vert_normals = BKE_pbvh_get_vert_normals(ss->pbvh);
  }
  else {
    ss->vert_normals = nullptr;
  }

  BLI_assert(pbvh == ss->pbvh);
  UNUSED_VARS_NDEBUG(pbvh);

  if (ss->subdiv_ccg) {
    BKE_pbvh_subdiv_ccg_set(ss->pbvh, ss->subdiv_ccg);
  }

  BKE_pbvh_update_hide_attributes_from_mesh(ss->pbvh);

  BKE_pbvh_face_sets_color_set(ss->pbvh, me->face_sets_color_seed, me->face_sets_color_default);

  sculpt_attribute_update_refs(ob);
  sculpt_update_persistent_base(ob);

  if (ob->type == OB_MESH && !ss->pmap) {
    if (!ss->pmap && ss->pbvh) {
      ss->pmap = BKE_pbvh_get_pmap(ss->pbvh);

      if (ss->pmap) {
        BKE_pbvh_pmap_aquire(ss->pmap);
      }
    }

    if (!ss->pmap) {
      ss->pmap = BKE_pbvh_make_pmap(me);

      if (ss->pbvh) {
        BKE_pbvh_set_pmap(ss->pbvh, ss->pmap);
      }
    }
  }

  if (ss->deform_modifiers_active) {
    /* Painting doesn't need crazyspace, use already evaluated mesh coordinates if possible. */
    bool used_me_eval = false;

    if (ob->mode & (OB_MODE_VERTEX_PAINT | OB_MODE_WEIGHT_PAINT)) {
      Mesh *me_eval_deform = ob_eval->runtime.mesh_deform_eval;

      /* If the fully evaluated mesh has the same topology as the deform-only version, use it.
       * This matters because crazyspace evaluation is very restrictive and excludes even modifiers
       * that simply recompute vertex weights (which can even include Geometry Nodes). */
      if (me_eval_deform->totpoly == me_eval->totpoly &&
          me_eval_deform->totloop == me_eval->totloop &&
          me_eval_deform->totvert == me_eval->totvert)
      {
        BKE_sculptsession_free_deformMats(ss);

        BLI_assert(me_eval_deform->totvert == me->totvert);

        ss->deform_cos = BKE_mesh_vert_coords_alloc(me_eval, nullptr);
        BKE_pbvh_vert_coords_apply(ss->pbvh, ss->deform_cos, me->totvert);

        used_me_eval = true;
      }
    }

    if (!ss->orig_cos && !used_me_eval) {
      int a;

      BKE_sculptsession_free_deformMats(ss);

      ss->orig_cos = (ss->shapekey_active) ?
                         BKE_keyblock_convert_to_vertcos(ob, ss->shapekey_active) :
                         BKE_mesh_vert_coords_alloc(me, nullptr);

      BKE_crazyspace_build_sculpt(depsgraph, scene, ob, &ss->deform_imats, &ss->deform_cos);
      BKE_pbvh_vert_coords_apply(ss->pbvh, ss->deform_cos, me->totvert);

      for (a = 0; a < me->totvert; a++) {
        invert_m3(ss->deform_imats[a]);

        float *co = ss->deform_cos[a];
        float ff = dot_v3v3(co, co);
        if (isnan(ff) || !isfinite(ff)) {
          printf("%s: nan1! %.4f %.4f %.4f\n", __func__, co[0], co[1], co[2]);
        }

        ff = determinant_m3_array(ss->deform_imats[a]);

        if (isnan(ff) || !isfinite(ff)) {
          printf("%s: nan2!\n", __func__);
        }
      }
    }
  }
  else {
    BKE_sculptsession_free_deformMats(ss);
  }

  if (ss->shapekey_active != nullptr && ss->deform_cos == nullptr) {
    ss->deform_cos = BKE_keyblock_convert_to_vertcos(ob, ss->shapekey_active);
  }

  /* if pbvh is deformed, key block is already applied to it */
  if (ss->shapekey_active) {
    bool pbvh_deformed = BKE_pbvh_is_deformed(ss->pbvh);
    if (!pbvh_deformed || ss->deform_cos == nullptr) {
      float(*vertCos)[3] = BKE_keyblock_convert_to_vertcos(ob, ss->shapekey_active);

      if (vertCos) {
        if (!pbvh_deformed) {
          /* apply shape keys coordinates to PBVH */
          BKE_pbvh_vert_coords_apply(ss->pbvh, vertCos, me->totvert);
        }
        if (ss->deform_cos == nullptr) {
          ss->deform_cos = vertCos;
        }
        if (vertCos != ss->deform_cos) {
          MEM_freeN(vertCos);
        }
      }
    }
  }

  int totvert = 0;

  switch (BKE_pbvh_type(pbvh)) {
    case PBVH_FACES:
      totvert = me->totvert;
      break;
    case PBVH_BMESH:
      totvert = ss->bm ? ss->bm->totvert : me->totvert;
      break;
    case PBVH_GRIDS:
      totvert = BKE_pbvh_get_grid_num_verts(ss->pbvh);
      break;
  }

  BKE_sculpt_init_flags_valence(ob, pbvh, totvert, false);

  if (ss->bm && me->key && ob->shapenr != ss->bm->shapenr) {
    KeyBlock *actkey = static_cast<KeyBlock *>(BLI_findlink(&me->key->block, ss->bm->shapenr - 1));
    KeyBlock *newkey = static_cast<KeyBlock *>(BLI_findlink(&me->key->block, ob->shapenr - 1));

    bool updatePBVH = false;

    if (!actkey) {
      printf("%s: failed to find active shapekey\n", __func__);
      if (!ss->bm->shapenr || !CustomData_has_layer(&ss->bm->vdata, CD_SHAPEKEY)) {
        printf("allocating shapekeys. . .\n");

        // need to allocate customdata for keys
        for (KeyBlock *key = (KeyBlock *)me->key->block.first; key; key = key->next) {

          int idx = CustomData_get_named_layer_index(&ss->bm->vdata, CD_SHAPEKEY, key->name);

          if (idx == -1) {
            BM_data_layer_add_named(ss->bm, &ss->bm->vdata, CD_SHAPEKEY, key->name);
            BKE_sculptsession_update_attr_refs(ob);

            idx = CustomData_get_named_layer_index(&ss->bm->vdata, CD_SHAPEKEY, key->name);
            ss->bm->vdata.layers[idx].uid = key->uid;
          }

          int cd_shapeco = ss->bm->vdata.layers[idx].offset;
          BMVert *v;
          BMIter iter;

          BM_ITER_MESH (v, &iter, ss->bm, BM_VERTS_OF_MESH) {
            float *keyco = (float *)BM_ELEM_CD_GET_VOID_P(v, cd_shapeco);

            copy_v3_v3(keyco, v->co);
          }
        }
      }

      updatePBVH = true;
      ss->bm->shapenr = ob->shapenr;
    }

    if (!newkey) {
      printf("%s: failed to find new active shapekey\n", __func__);
    }

    if (actkey && newkey) {
      int cd_co1 = CustomData_get_named_layer_index(&ss->bm->vdata, CD_SHAPEKEY, actkey->name);
      int cd_co2 = CustomData_get_named_layer_index(&ss->bm->vdata, CD_SHAPEKEY, newkey->name);

      BMVert *v;
      BMIter iter;

      if (cd_co1 == -1) {  // non-recoverable error
        printf("%s: failed to find active shapekey in customdata.\n", __func__);
        return;
      }
      else if (cd_co2 == -1) {
        printf("%s: failed to find new shapekey in customdata; allocating . . .\n", __func__);

        BM_data_layer_add_named(ss->bm, &ss->bm->vdata, CD_SHAPEKEY, newkey->name);
        int idx = CustomData_get_named_layer_index(&ss->bm->vdata, CD_SHAPEKEY, newkey->name);

        int cd_co = ss->bm->vdata.layers[idx].offset;
        ss->bm->vdata.layers[idx].uid = newkey->uid;

        BKE_sculptsession_update_attr_refs(ob);

        BM_ITER_MESH (v, &iter, ss->bm, BM_VERTS_OF_MESH) {
          float *keyco = (float *)BM_ELEM_CD_GET_VOID_P(v, cd_co);
          copy_v3_v3(keyco, v->co);
        }

        cd_co2 = idx;
      }

      cd_co1 = ss->bm->vdata.layers[cd_co1].offset;
      cd_co2 = ss->bm->vdata.layers[cd_co2].offset;

      BM_ITER_MESH (v, &iter, ss->bm, BM_VERTS_OF_MESH) {
        float *co1 = (float *)BM_ELEM_CD_GET_VOID_P(v, cd_co1);
        float *co2 = (float *)BM_ELEM_CD_GET_VOID_P(v, cd_co2);

        copy_v3_v3(co1, v->co);
        copy_v3_v3(v->co, co2);
      }

      ss->bm->shapenr = ob->shapenr;

      updatePBVH = true;
    }

    if (updatePBVH && ss->pbvh) {
      Vector<PBVHNode *> nodes = blender::bke::pbvh::get_flagged_nodes(ss->pbvh, PBVH_Leaf);

      for (PBVHNode *node : nodes) {
        BKE_pbvh_node_mark_update(node);
        BKE_pbvh_vert_tag_update_normal_tri_area(node);
      }
    }
    /* We could be more precise when we have access to the active tool. */
    const bool use_paint_slots = (ob->mode & OB_MODE_SCULPT) != 0;
    if (use_paint_slots) {
      BKE_texpaint_slots_refresh_object(scene, ob);
    }
  }

  if (is_paint_tool) {
    if (ss->vcol_domain == ATTR_DOMAIN_CORNER) {
      /* Ensure pbvh nodes have loop indices; the sculpt undo system
       * needs them for color attributes.
       */
      BKE_pbvh_ensure_node_loops(ss->pbvh);
    }

    /*
     * We should rebuild the PBVH_pixels when painting canvas changes.
     *
     * The relevant changes are stored/encoded in the paint canvas key.
     * These include the active uv map, and resolutions.
     */
    if (U.experimental.use_sculpt_texture_paint && ss->pbvh) {
      char *paint_canvas_key = BKE_paint_canvas_key_get(&scene->toolsettings->paint_mode, ob);
      if (ss->last_paint_canvas_key == nullptr ||
          !STREQ(paint_canvas_key, ss->last_paint_canvas_key)) {
        MEM_SAFE_FREE(ss->last_paint_canvas_key);
        ss->last_paint_canvas_key = paint_canvas_key;
        BKE_pbvh_mark_rebuild_pixels(ss->pbvh);
      }

      /*
       * We should rebuild the PBVH_pixels when painting canvas changes.
       *
       * The relevant changes are stored/encoded in the paint canvas key.
       * These include the active uv map, and resolutions.
       */
      if (U.experimental.use_sculpt_texture_paint && ss->pbvh) {
        char *paint_canvas_key = BKE_paint_canvas_key_get(&scene->toolsettings->paint_mode, ob);
        if (ss->last_paint_canvas_key == nullptr ||
            !STREQ(paint_canvas_key, ss->last_paint_canvas_key)) {
          MEM_SAFE_FREE(ss->last_paint_canvas_key);
          ss->last_paint_canvas_key = paint_canvas_key;
          BKE_pbvh_mark_rebuild_pixels(ss->pbvh);
        }
        else {
          MEM_freeN(paint_canvas_key);
        }
      }

      /* We could be more precise when we have access to the active tool. */
      const bool use_paint_slots = (ob->mode & OB_MODE_SCULPT) != 0;
      if (use_paint_slots) {
        BKE_texpaint_slots_refresh_object(scene, ob);
      }
    }
  }

  if (ss->pbvh) {
    blender::bke::pbvh::set_flags_valence(ss->pbvh,
                                          static_cast<uint8_t *>(ss->attrs.flags->data),
                                          static_cast<int *>(ss->attrs.valence->data));
  }
}

void BKE_sculpt_update_object_before_eval(Object *ob_eval)
{
  /* Update before mesh evaluation in the dependency graph. */
  SculptSession *ss = ob_eval->sculpt;

  if (ss && (ss->building_vp_handle == false || ss->needs_pbvh_rebuild)) {
    if (ss->needs_pbvh_rebuild || (!ss->cache && !ss->filter_cache && !ss->expand_cache)) {
      /* We free pbvh on changes, except in the middle of drawing a stroke
       * since it can't deal with changing PVBH node organization, we hope
       * topology does not change in the meantime .. weak. */
      sculptsession_free_pbvh(ob_eval);

      BKE_sculptsession_free_deformMats(ob_eval->sculpt);

      /* In vertex/weight paint, force maps to be rebuilt. */
      BKE_sculptsession_free_vwpaint_data(ob_eval->sculpt);
    }
    else if (ss->pbvh) {
      Vector<PBVHNode *> nodes = blender::bke::pbvh::search_gather(ss->pbvh, nullptr, nullptr);

      for (PBVHNode *node : nodes) {
        BKE_pbvh_node_mark_update(node);
      }
    }
  }
}

void BKE_sculpt_update_object_after_eval(Depsgraph *depsgraph, Object *ob_eval)
{
  /* Update after mesh evaluation in the dependency graph, to rebuild PBVH or
   * other data when modifiers change the mesh. */
  Object *ob_orig = DEG_get_original_object(ob_eval);
  Mesh *me_orig = BKE_object_get_original_mesh(ob_orig);

  sculpt_update_object(depsgraph, ob_orig, ob_eval, false, false);
  BKE_sculptsession_sync_attributes(ob_orig, me_orig);
}

void BKE_sculpt_color_layer_create_if_needed(Object *object)
{
  using namespace blender;
  using namespace blender::bke;
  Mesh *orig_me = BKE_object_get_original_mesh(object);

  if (orig_me->attributes().contains(orig_me->active_color_attribute)) {
    return;
  }

  char unique_name[MAX_CUSTOMDATA_LAYER_NAME];
  BKE_id_attribute_calc_unique_name(&orig_me->id, "Color", unique_name);
  if (!orig_me->attributes_for_write().add(
          unique_name, ATTR_DOMAIN_POINT, CD_PROP_COLOR, AttributeInitDefaultValue()))
  {
    return;
  }

  BKE_id_attributes_active_color_set(&orig_me->id, unique_name);
  DEG_id_tag_update(&orig_me->id, ID_RECALC_GEOMETRY_ALL_MODES);
  BKE_mesh_tessface_clear(orig_me);

  if (object->sculpt && object->sculpt->pbvh) {
    BKE_pbvh_update_active_vcol(object->sculpt->pbvh, orig_me);
  }

  BKE_sculptsession_update_attr_refs(object);
}

void BKE_sculpt_update_object_for_edit(
    Depsgraph *depsgraph, Object *ob_orig, bool need_pmap, bool /*need_mask*/, bool is_paint_tool)
{
  /* Update from sculpt operators and undo, to update sculpt session
   * and PBVH after edits. */
  Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob_orig);

  sculpt_update_object(depsgraph, ob_orig, ob_eval, need_pmap, is_paint_tool);
}

ATTR_NO_OPT int *BKE_sculpt_face_sets_ensure(Object *ob)
{
  SculptSession *ss = ob->sculpt;

  if (!ss->attrs.face_set) {
    Mesh *mesh = static_cast<Mesh *>(ob->data);
    SculptAttributeParams params = {};
    params.permanent = true;

    CustomData *cdata = ss->bm ? &ss->bm->pdata : &mesh->pdata;
    bool clear = CustomData_get_named_layer_index(
                     cdata, CD_PROP_INT32, SCULPT_ATTRIBUTE_NAME(face_set)) == -1;

    ss->attrs.face_set = BKE_sculpt_attribute_ensure(
        ob, ATTR_DOMAIN_FACE, CD_PROP_INT32, SCULPT_ATTRIBUTE_NAME(face_set), &params);

    if (clear) {
      if (ss->bm) {
        BMFace *f;
        BMIter iter;
        int cd_faceset = ss->attrs.face_set->bmesh_cd_offset;

        BM_ITER_MESH (f, &iter, ss->bm, BM_FACES_OF_MESH) {
          BM_ELEM_CD_SET_INT(f, cd_faceset, 1);
        }
      }
      else {
        int *face_sets = static_cast<int *>(ss->attrs.face_set->data);

        for (int i : IndexRange(ss->totfaces)) {
          face_sets[i] = 1;
        }
      }

      Mesh *mesh = static_cast<Mesh *>(ob->data);
      mesh->face_sets_color_default = 1;
    }
  }

  if (ss->pbvh && !ss->bm) {
    BKE_pbvh_face_sets_set(ss->pbvh, static_cast<int *>(ss->attrs.face_set->data));
  }

  return static_cast<int *>(ss->attrs.face_set->data);
}

bool *BKE_sculpt_hide_poly_ensure(Object *ob)
{
  SculptAttributeParams params = {0};
  params.permanent = true;

  ob->sculpt->attrs.hide_poly = BKE_sculpt_attribute_ensure(
      ob, ATTR_DOMAIN_FACE, CD_PROP_BOOL, ".hide_poly", &params);

  return ob->sculpt->hide_poly = static_cast<bool *>(ob->sculpt->attrs.hide_poly->data);
}

int BKE_sculpt_mask_layers_ensure(Depsgraph *depsgraph,
                                  Main *bmain,
                                  Object *ob,
                                  MultiresModifierData *mmd)
{
  Mesh *me = static_cast<Mesh *>(ob->data);
  const blender::OffsetIndices polys = me->polys();
  const Span<int> corner_verts = me->corner_verts();
  int ret = 0;

  const float *paint_mask = static_cast<const float *>(
      CustomData_get_layer(&me->vdata, CD_PAINT_MASK));

  /* if multires is active, create a grid paint mask layer if there
   * isn't one already */
  if (mmd && !CustomData_has_layer(&me->ldata, CD_GRID_PAINT_MASK)) {
    GridPaintMask *gmask;
    int level = max_ii(1, mmd->sculptlvl);
    int gridsize = BKE_ccg_gridsize(level);
    int gridarea = gridsize * gridsize;

    gmask = static_cast<GridPaintMask *>(
        CustomData_add_layer(&me->ldata, CD_GRID_PAINT_MASK, CD_SET_DEFAULT, me->totloop));

    for (int i = 0; i < me->totloop; i++) {
      GridPaintMask *gpm = &gmask[i];

      gpm->level = level;
      gpm->data = static_cast<float *>(
          MEM_callocN(sizeof(float) * gridarea, "GridPaintMask.data"));
    }

    /* If vertices already have mask, copy into multires data. */
    if (paint_mask) {
      for (const int i : polys.index_range()) {
        const IndexRange poly = polys[i];

        /* Mask center. */
        float avg = 0.0f;
        for (const int vert : corner_verts.slice(poly)) {
          avg += paint_mask[vert];
        }
        avg /= float(poly.size());

        /* Fill in multires mask corner. */
        for (const int corner : poly) {
          GridPaintMask *gpm = &gmask[corner];
          const int vert = corner_verts[corner];
          const int prev = corner_verts[blender::bke::mesh::poly_corner_prev(poly, vert)];
          const int next = corner_verts[blender::bke::mesh::poly_corner_next(poly, vert)];

          gpm->data[0] = avg;
          gpm->data[1] = (paint_mask[vert] + paint_mask[next]) * 0.5f;
          gpm->data[2] = (paint_mask[vert] + paint_mask[prev]) * 0.5f;
          gpm->data[3] = paint_mask[vert];
        }
      }
    }
    /* The evaluated multires CCG must be updated to contain the new data. */
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    if (depsgraph) {
      BKE_scene_graph_evaluated_ensure(depsgraph, bmain);
    }

    ret |= SCULPT_MASK_LAYER_CALC_LOOP;
  }

  /* Create vertex paint mask layer if there isn't one already. */
  if (!paint_mask) {
    CustomData_add_layer(&me->vdata, CD_PAINT_MASK, CD_SET_DEFAULT, me->totvert);
    /* The evaluated mesh must be updated to contain the new data. */
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    ret |= SCULPT_MASK_LAYER_CALC_VERT;
  }

  if (ob->sculpt) {
    BKE_sculptsession_update_attr_refs(ob);
  }

  return ret;
}

void BKE_sculpt_toolsettings_data_ensure(Scene *scene)
{
  BKE_paint_ensure(scene->toolsettings, (Paint **)&scene->toolsettings->sculpt);

  Sculpt *sd = scene->toolsettings->sculpt;
  if (!sd->detail_size) {
    sd->detail_size = 8.0f;
  }

  /* We check these flags here in case versioning code fails. */
  if (!sd->detail_range || !sd->dyntopo.spacing) {
    sd->flags |= SCULPT_DYNTOPO_ENABLED;
  }

  if (!sd->detail_range) {
    sd->detail_range = 0.4f;
  }

  if (!sd->detail_percent) {
    sd->detail_percent = 25;
  }

  if (!sd->dyntopo.constant_detail) {
    sd->dyntopo = *DNA_struct_default_get(DynTopoSettings);
  }

  if (!sd->automasking_start_normal_limit) {
    sd->automasking_start_normal_limit = 20.0f / 180.0f * M_PI;
    sd->automasking_start_normal_falloff = 0.25f;

    sd->automasking_view_normal_limit = 90.0f / 180.0f * M_PI;
    sd->automasking_view_normal_falloff = 0.25f;
  }

  /* Set sane default tiling offsets. */
  if (!sd->paint.tile_offset[0]) {
    sd->paint.tile_offset[0] = 1.0f;
  }
  if (!sd->paint.tile_offset[1]) {
    sd->paint.tile_offset[1] = 1.0f;
  }
  if (!sd->paint.tile_offset[2]) {
    sd->paint.tile_offset[2] = 1.0f;
  }
  if (!sd->automasking_cavity_curve || !sd->automasking_cavity_curve_op) {
    BKE_sculpt_check_cavity_curves(sd);
  }
}

static bool check_sculpt_object_deformed(Object *object, const bool for_construction)
{
  bool deformed = false;

  /* Active modifiers means extra deformation, which can't be handled correct
   * on birth of PBVH and sculpt "layer" levels, so use PBVH only for internal brush
   * stuff and show final evaluated mesh so user would see actual object shape. */
  deformed |= object->sculpt->deform_modifiers_active;

  if (for_construction) {
    deformed |= object->sculpt->shapekey_active != nullptr;
  }
  else {
    /* As in case with modifiers, we can't synchronize deformation made against
     * PBVH and non-locked keyblock, so also use PBVH only for brushes and
     * final DM to give final result to user. */
    deformed |= object->sculpt->shapekey_active && (object->shapeflag & OB_SHAPE_LOCK) == 0;
  }

  return deformed;
}

void BKE_sculpt_sync_face_visibility_to_grids(Mesh *mesh, SubdivCCG *subdiv_ccg)
{
  using namespace blender;
  using namespace blender::bke;
  if (!subdiv_ccg) {
    return;
  }

  const AttributeAccessor attributes = mesh->attributes();
  const VArray<bool> hide_poly = *attributes.lookup_or_default<bool>(
      ".hide_poly", ATTR_DOMAIN_FACE, false);
  if (hide_poly.is_single() && !hide_poly.get_internal_single()) {
    /* Nothing is hidden, so we can just remove all visibility bitmaps. */
    for (const int i : IndexRange(subdiv_ccg->num_grids)) {
      BKE_subdiv_ccg_grid_hidden_free(subdiv_ccg, i);
    }
    return;
  }

  const VArraySpan<bool> hide_poly_span(hide_poly);
  CCGKey key;
  BKE_subdiv_ccg_key_top_level(&key, subdiv_ccg);
  for (int i = 0; i < mesh->totloop; i++) {
    const int face_index = BKE_subdiv_ccg_grid_to_face_index(subdiv_ccg, i);
    const bool is_hidden = hide_poly_span[face_index];

    /* Avoid creating and modifying the grid_hidden bitmap if the base mesh face is visible and
     * there is not bitmap for the grid. This is because missing grid_hidden implies grid is
     * fully visible. */
    if (is_hidden) {
      BKE_subdiv_ccg_grid_hidden_ensure(subdiv_ccg, i);
    }

    BLI_bitmap *gh = subdiv_ccg->grid_hidden[i];
    if (gh) {
      BLI_bitmap_set_all(gh, is_hidden, key.grid_area);
    }
  }
}

static PBVH *build_pbvh_for_dynamic_topology(Object *ob, bool /*update_sculptverts*/)
{
  PBVH *pbvh = ob->sculpt->pbvh = BKE_pbvh_new(PBVH_BMESH);
  BKE_pbvh_set_bmesh(pbvh, ob->sculpt->bm);

  BM_mesh_elem_table_ensure(ob->sculpt->bm, BM_VERT | BM_EDGE | BM_FACE);

  BKE_sculptsession_update_attr_refs(ob);
  sculpt_boundary_flags_ensure(ob, pbvh, ob->sculpt->bm->totvert);
  sculpt_check_face_areas(ob, pbvh);

  BKE_sculpt_ensure_idmap(ob);

  sculptsession_bmesh_add_layers(ob);
  BKE_pbvh_build_bmesh(pbvh,
                       BKE_object_get_original_mesh(ob),
                       ob->sculpt->bm,
                       ob->sculpt->bm_smooth_shading,
                       ob->sculpt->bm_log,
                       ob->sculpt->bm_idmap,
                       ob->sculpt->attrs.dyntopo_node_id_vertex->bmesh_cd_offset,
                       ob->sculpt->attrs.dyntopo_node_id_face->bmesh_cd_offset,
                       ob->sculpt->attrs.face_areas->bmesh_cd_offset,
                       ob->sculpt->attrs.boundary_flags->bmesh_cd_offset,
                       ob->sculpt->attrs.flags ? ob->sculpt->attrs.flags->bmesh_cd_offset : -1,
                       ob->sculpt->attrs.valence ? ob->sculpt->attrs.valence->bmesh_cd_offset : -1,
                       ob->sculpt->attrs.orig_co ? ob->sculpt->attrs.orig_co->bmesh_cd_offset : -1,
                       ob->sculpt->attrs.orig_no ? ob->sculpt->attrs.orig_no->bmesh_cd_offset : -1,
                       ob->sculpt->fast_draw);

  if (ob->sculpt->bm_log) {
    BKE_pbvh_set_bm_log(pbvh, ob->sculpt->bm_log);
  }

  BKE_pbvh_set_symmetry(pbvh, 0, (int)BKE_get_fset_boundary_symflag(ob));

  return pbvh;
}

static PBVH *build_pbvh_from_regular_mesh(Object *ob, Mesh *me_eval_deform)
{
  SculptSession *ss = ob->sculpt;
  Mesh *me = BKE_object_get_original_mesh(ob);

  if (!ss->pmap) {
    ss->pmap = BKE_pbvh_make_pmap(me);
  }

  PBVH *pbvh = ob->sculpt->pbvh = BKE_pbvh_new(PBVH_FACES);

  BKE_sculpt_init_flags_valence(ob, pbvh, me->totvert, true);
  sculpt_check_face_areas(ob, pbvh);
  BKE_sculptsession_update_attr_refs(ob);
  BKE_pbvh_set_pmap(pbvh, ss->pmap);

  BKE_pbvh_build_mesh(pbvh, me);
  BKE_pbvh_fast_draw_set(pbvh, ss->fast_draw);

  const bool is_deformed = check_sculpt_object_deformed(ob, true);
  if (is_deformed && me_eval_deform != nullptr) {
    BKE_pbvh_vert_coords_apply(
        pbvh, BKE_mesh_vert_positions(me_eval_deform), me_eval_deform->totvert);
  }

  return pbvh;
}

static PBVH *build_pbvh_from_ccg(Object *ob, SubdivCCG *subdiv_ccg)
{
  SculptSession *ss = ob->sculpt;

  CCGKey key;
  BKE_subdiv_ccg_key_top_level(&key, subdiv_ccg);
  PBVH *pbvh = ob->sculpt->pbvh = BKE_pbvh_new(PBVH_GRIDS);

  Mesh *base_mesh = BKE_mesh_from_object(ob);

  BKE_sculpt_sync_face_visibility_to_grids(base_mesh, subdiv_ccg);

  BKE_sculptsession_update_attr_refs(ob);
  sculpt_check_face_areas(ob, pbvh);

  BKE_pbvh_build_grids(pbvh,
                       subdiv_ccg->grids,
                       subdiv_ccg->num_grids,
                       &key,
                       (void **)subdiv_ccg->grid_faces,
                       subdiv_ccg->grid_flag_mats,
                       subdiv_ccg->grid_hidden,
                       ob->sculpt->fast_draw,
                       (float *)ss->attrs.face_areas->data,
                       base_mesh,
                       subdiv_ccg);

  CustomData_reset(&ob->sculpt->temp_vdata);
  CustomData_reset(&ob->sculpt->temp_pdata);

  ss->temp_vdata_elems = BKE_pbvh_get_grid_num_verts(pbvh);
  ss->temp_pdata_elems = ss->totfaces;

  if (!ss->pmap) {
    ss->pmap = BKE_pbvh_make_pmap(BKE_object_get_original_mesh(ob));
  }

  BKE_pbvh_set_pmap(pbvh, ss->pmap);

  int totvert = BKE_pbvh_get_grid_num_verts(pbvh);
  BKE_sculpt_init_flags_valence(ob, pbvh, totvert, true);

  return pbvh;
}

extern "C" bool BKE_sculpt_init_flags_valence(Object *ob,
                                              struct PBVH *pbvh,
                                              int totvert,
                                              bool reset_flags)
{
  SculptSession *ss = ob->sculpt;

  if (!ss->attrs.flags) {
    BKE_sculpt_ensure_sculpt_layers(ob);

    reset_flags = true;
  }

  BKE_sculpt_ensure_origco(ob);
  sculpt_boundary_flags_ensure(ob, pbvh, totvert);

  if (reset_flags) {
    if (ss->bm) {
      int cd_flags = ss->attrs.flags->bmesh_cd_offset;
      BMVert *v;
      BMIter iter;

      BM_ITER_MESH (v, &iter, ss->bm, BM_VERTS_OF_MESH) {
        *BM_ELEM_CD_PTR<uint8_t *>(v, cd_flags) = SCULPTFLAG_NEED_VALENCE |
                                                  SCULPTFLAG_NEED_TRIANGULATE |
                                                  SCULPTFLAG_NEED_DISK_SORT;
      }
    }
    else {
      uint8_t *flags = static_cast<uint8_t *>(ss->attrs.flags->data);

      for (int i = 0; i < totvert; i++) {
        flags[i] = SCULPTFLAG_NEED_VALENCE | SCULPTFLAG_NEED_TRIANGULATE |
                   SCULPTFLAG_NEED_DISK_SORT;
      }
    }
  }

  blender::bke::pbvh::set_flags_valence(ss->pbvh,
                                        static_cast<uint8_t *>(ss->attrs.flags->data),
                                        static_cast<int *>(ss->attrs.valence->data));

  return false;
}

PBVH *BKE_sculpt_object_pbvh_ensure(Depsgraph *depsgraph, Object *ob)
{
  if (ob == nullptr || ob->sculpt == nullptr) {
    return nullptr;
  }

  Scene *scene = DEG_get_input_scene(depsgraph);

  PBVH *pbvh = ob->sculpt->pbvh;
  if (pbvh != nullptr) {
    /* NOTE: It is possible that pointers to grids or other geometry data changed. Need to update
     * those pointers. */
    const PBVHType pbvh_type = BKE_pbvh_type(pbvh);
    switch (pbvh_type) {
      case PBVH_FACES: {
        BKE_pbvh_update_mesh_pointers(pbvh, BKE_object_get_original_mesh(ob));
        break;
      }
      case PBVH_GRIDS: {
        Object *object_eval = DEG_get_evaluated_object(depsgraph, ob);
        Mesh *mesh_eval = static_cast<Mesh *>(object_eval->data);
        SubdivCCG *subdiv_ccg = mesh_eval->runtime->subdiv_ccg;
        if (subdiv_ccg != nullptr) {
          BKE_sculpt_bvh_update_from_ccg(pbvh, subdiv_ccg);
        }
        break;
      }
      case PBVH_BMESH: {
        break;
      }
    }

    BKE_sculptsession_sync_attributes(ob, BKE_object_get_original_mesh(ob));
    BKE_pbvh_update_active_vcol(pbvh, BKE_object_get_original_mesh(ob));

    return pbvh;
  }

  ob->sculpt->islands_valid = false;

  if (ob->sculpt->bm != nullptr) {
    /* Sculpting on a BMesh (dynamic-topology) gets a special PBVH. */
    pbvh = build_pbvh_for_dynamic_topology(ob, false);

    ob->sculpt->pbvh = pbvh;
  }
  else {
    /* Detect if we are loading from an undo memfile step. */
    Mesh *mesh_orig = BKE_object_get_original_mesh(ob);
    bool is_dyntopo = (mesh_orig->flag & ME_SCULPT_DYNAMIC_TOPOLOGY);

    if (is_dyntopo) {
      BMesh *bm = BKE_sculptsession_empty_bmesh_create();

      BMeshFromMeshParams params = {0};
      params.calc_face_normal = true;
      params.use_shapekey = true;
      params.active_shapekey = ob->shapenr;
      params.create_shapekey_layers = true;
      params.ignore_id_layers = false;
      params.copy_temp_cdlayers = true;

      BM_mesh_bm_from_me(bm, mesh_orig, &params);

      ob->sculpt->bm = bm;

      SCULPT_undo_ensure_bmlog(ob);

      /* Note build_pbvh_for_dynamic_topology respects the pbvh cache. */
      pbvh = build_pbvh_for_dynamic_topology(ob, true);

      BKE_sculptsession_update_attr_refs(ob);
    }
    else {
      Object *object_eval = DEG_get_evaluated_object(depsgraph, ob);
      Mesh *mesh_eval = static_cast<Mesh *>(object_eval->data);
      if (mesh_eval->runtime->subdiv_ccg != nullptr) {
        pbvh = build_pbvh_from_ccg(ob, mesh_eval->runtime->subdiv_ccg);
      }
      else if (ob->type == OB_MESH) {
        Mesh *me_eval_deform = object_eval->runtime.mesh_deform_eval;
        pbvh = build_pbvh_from_regular_mesh(ob, me_eval_deform);
      }
    }
  }

  if (!ob->sculpt->pmap) {
    ob->sculpt->pmap = BKE_pbvh_make_pmap(BKE_object_get_original_mesh(ob));
  }

  BKE_pbvh_set_pmap(pbvh, ob->sculpt->pmap);
  ob->sculpt->pbvh = pbvh;

  sculpt_attribute_update_refs(ob);

  if (pbvh) {
    SCULPT_update_flat_vcol_shading(ob, scene);
  }

  return pbvh;
}

PBVH *BKE_object_sculpt_pbvh_get(Object *object)
{
  if (!object->sculpt) {
    return nullptr;
  }
  return object->sculpt->pbvh;
}

bool BKE_object_sculpt_use_dyntopo(const Object *object)
{
  return object->sculpt && object->sculpt->bm;
}

void BKE_object_sculpt_dyntopo_smooth_shading_set(Object *object, const bool value)
{
  object->sculpt->bm_smooth_shading = value;
}

void BKE_object_sculpt_fast_draw_set(Object *object, const bool value)
{
  object->sculpt->fast_draw = value;
  if (object->sculpt->pbvh) {
    BKE_pbvh_fast_draw_set(object->sculpt->pbvh, value);
  }
}

void BKE_sculpt_bvh_update_from_ccg(PBVH *pbvh, SubdivCCG *subdiv_ccg)
{
  CCGKey key;
  BKE_subdiv_ccg_key_top_level(&key, subdiv_ccg);

  BKE_pbvh_grids_update(pbvh,
                        subdiv_ccg->grids,
                        (void **)subdiv_ccg->grid_faces,
                        subdiv_ccg->grid_flag_mats,
                        subdiv_ccg->grid_hidden,
                        &key);
}

bool BKE_sculptsession_use_pbvh_draw(const Object *ob, const RegionView3D *rv3d)
{
  SculptSession *ss = ob->sculpt;
  if (ss == nullptr || ss->pbvh == nullptr || ss->mode_type != OB_MODE_SCULPT) {
    return false;
  }

#if 0
  if (BKE_pbvh_type(ss->pbvh) == PBVH_GRIDS) {
    return !(v3d && (v3d->shading.type > OB_SOLID));
  }
#endif

  if (BKE_pbvh_type(ss->pbvh) == PBVH_FACES) {
    /* Regular mesh only draws from PBVH without modifiers and shape keys, or for
     * external engines that do not have access to the PBVH like Eevee does. */
    const bool external_engine = rv3d && rv3d->render_engine != nullptr;
    return !(ss->shapekey_active || ss->deform_modifiers_active || external_engine);
  }

  /* Multires and dyntopo always draw directly from the PBVH. */
  return true;
}

/* Returns the Face Set random color for rendering in the overlay given its ID and a color seed. */
#define GOLDEN_RATIO_CONJUGATE 0.618033988749895f
void BKE_paint_face_set_overlay_color_get(const int face_set, const int seed, uchar r_color[4])
{
  float rgba[4];
  float random_mod_hue = GOLDEN_RATIO_CONJUGATE * (face_set + (seed % 10));
  random_mod_hue = random_mod_hue - floorf(random_mod_hue);
  const float random_mod_sat = BLI_hash_int_01(face_set + seed + 1);
  const float random_mod_val = BLI_hash_int_01(face_set + seed + 2);
  hsv_to_rgb(random_mod_hue,
             0.6f + (random_mod_sat * 0.25f),
             1.0f - (random_mod_val * 0.35f),
             &rgba[0],
             &rgba[1],
             &rgba[2]);
  rgba_float_to_uchar(r_color, rgba);
}

int BKE_sculptsession_vertex_count(const SculptSession *ss)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
      return ss->totvert;
    case PBVH_BMESH:
      return BM_mesh_elem_count(ss->bm, BM_VERT);
    case PBVH_GRIDS:
      return BKE_pbvh_get_grid_num_verts(ss->pbvh);
  }

  return 0;
}

static bool sculpt_attribute_stored_in_bmesh_builtin(const StringRef name)
{
  return BM_attribute_stored_in_bmesh_builtin(name);
}

/**
  Syncs customdata layers with internal bmesh, but ignores deleted layers.
*/
void BKE_sculptsession_sync_attributes(struct Object *ob, struct Mesh *me)
{
  SculptSession *ss = ob->sculpt;

  if (!ss) {
    return;
  }
  else if (!ss->bm) {
    BKE_sculptsession_update_attr_refs(ob);
    return;
  }

  bool modified = false;
  BMesh *bm = ss->bm;

  CustomData *cd1[4] = {&me->vdata, &me->edata, &me->ldata, &me->pdata};
  CustomData *cd2[4] = {&bm->vdata, &bm->edata, &bm->ldata, &bm->pdata};
  int badmask = CD_MASK_ORIGINDEX | CD_MASK_ORIGSPACE | CD_MASK_MFACE;

  for (int i = 0; i < 4; i++) {
    Vector<CustomDataLayer *> newlayers;

    CustomData *data1 = cd1[i];
    CustomData *data2 = cd2[i];

    if (!data1->layers) {
      modified |= data2->layers != nullptr;
      continue;
    }

    for (int j = 0; j < data1->totlayer; j++) {
      CustomDataLayer *cl1 = data1->layers + j;

      if (sculpt_attribute_stored_in_bmesh_builtin(cl1->name)) {
        continue;
      }
      if ((1 << cl1->type) & badmask) {
        continue;
      }

      int idx = CustomData_get_named_layer_index(data2, eCustomDataType(cl1->type), cl1->name);
      if (idx < 0) {
        newlayers.append(cl1);
      }
    }

    for (CustomDataLayer *layer : newlayers) {
      BM_data_layer_add_named(bm, data2, layer->type, layer->name);
      modified = true;
    }

    /* sync various ids */
    for (int j = 0; j < data1->totlayer; j++) {
      CustomDataLayer *cl1 = data1->layers + j;

      if (sculpt_attribute_stored_in_bmesh_builtin(cl1->name)) {
        continue;
      }
      if ((1 << cl1->type) & badmask) {
        continue;
      }

      int idx = CustomData_get_named_layer_index(data2, eCustomDataType(cl1->type), cl1->name);

      if (idx == -1) {
        continue;
      }

      CustomDataLayer *cl2 = data2->layers + idx;

      cl2->anonymous_id = cl1->anonymous_id;
      cl2->uid = cl1->uid;
    }

    bool typemap[CD_NUMTYPES] = {0};

    for (int j = 0; j < data1->totlayer; j++) {
      CustomDataLayer *cl1 = data1->layers + j;

      if (sculpt_attribute_stored_in_bmesh_builtin(cl1->name)) {
        continue;
      }
      if ((1 << cl1->type) & badmask) {
        continue;
      }

      if (typemap[cl1->type]) {
        continue;
      }

      typemap[cl1->type] = true;

      // find first layer
      int baseidx = CustomData_get_layer_index(data2, eCustomDataType(cl1->type));

      if (baseidx < 0) {
        modified |= true;
        continue;
      }

      CustomDataLayer *cl2 = data2->layers + baseidx;

      int idx = CustomData_get_named_layer_index(
          data2, eCustomDataType(cl1->type), cl1[cl1->active].name);
      if (idx >= 0) {
        modified |= idx - baseidx != cl2->active;
        cl2->active = idx - baseidx;
      }

      idx = CustomData_get_named_layer_index(
          data2, eCustomDataType(cl1->type), cl1[cl1->active_rnd].name);
      if (idx >= 0) {
        modified |= idx - baseidx != cl2->active_rnd;
        cl2->active_rnd = idx - baseidx;
      }

      idx = CustomData_get_named_layer_index(
          data2, eCustomDataType(cl1->type), cl1[cl1->active_mask].name);
      if (idx >= 0) {
        modified |= idx - baseidx != cl2->active_mask;
        cl2->active_mask = idx - baseidx;
      }

      idx = CustomData_get_named_layer_index(
          data2, eCustomDataType(cl1->type), cl1[cl1->active_clone].name);
      if (idx >= 0) {
        modified |= idx - baseidx != cl2->active_clone;
        cl2->active_clone = idx - baseidx;
      }
    }
  }

  if (modified && ss->bm) {
    CustomData_regen_active_refs(&ss->bm->vdata);
    CustomData_regen_active_refs(&ss->bm->edata);
    CustomData_regen_active_refs(&ss->bm->ldata);
    CustomData_regen_active_refs(&ss->bm->pdata);
  }

  BKE_sculptsession_update_attr_refs(ob);
}

BMesh *BKE_sculptsession_empty_bmesh_create()
{
  BMAllocTemplate allocsize;

  allocsize.totvert = 2048 * 1;
  allocsize.totface = 2048 * 16;
  allocsize.totloop = 4196 * 16;
  allocsize.totedge = 2048 * 16;

  BMeshCreateParams params = {0};

  params.use_toolflags = false;
  params.create_unique_ids = true;
  params.id_elem_mask = BM_VERT | BM_EDGE | BM_FACE;
  params.id_map = true;
  params.temporary_ids = false;

  params.no_reuse_ids = false;

  BMesh *bm = BM_mesh_create(&allocsize, &params);

  return bm;
}

/** Returns pointer to a CustomData associated with a given domain, if
 * one exists.  If not nullptr is returned (this may happen with e.g.
 * multires and ATTR_DOMAIN_POINT).
 */
static CustomData *sculpt_get_cdata(Object *ob, eAttrDomain domain)
{
  SculptSession *ss = ob->sculpt;

  if (ss->bm) {
    switch (domain) {
      case ATTR_DOMAIN_POINT:
        return &ss->bm->vdata;
      case ATTR_DOMAIN_FACE:
        return &ss->bm->pdata;
      default:
        BLI_assert_unreachable();
        return nullptr;
    }
  }
  else {
    Mesh *me = BKE_object_get_original_mesh(ob);

    switch (domain) {
      case ATTR_DOMAIN_POINT:
        /* Cannot get vertex domain for multires grids. */
        if (ss->pbvh && BKE_pbvh_type(ss->pbvh) == PBVH_GRIDS) {
          return nullptr;
        }

        return &me->vdata;
      case ATTR_DOMAIN_FACE:
        return &me->pdata;
      default:
        BLI_assert_unreachable();
        return nullptr;
    }
  }
}

static int sculpt_attr_elem_count_get(Object *ob, eAttrDomain domain)
{
  SculptSession *ss = ob->sculpt;

  switch (domain) {
    case ATTR_DOMAIN_POINT:
      /* Cannot rely on prescence of ss->pbvh. */

      if (ss->bm) {
        return ss->bm->totvert;
      }
      else if (ss->subdiv_ccg) {
        CCGKey key;
        BKE_subdiv_ccg_key_top_level(&key, ss->subdiv_ccg);
        return ss->subdiv_ccg->num_grids * key.grid_area;
      }
      else {
        Mesh *me = BKE_object_get_original_mesh(ob);

        return me->totvert;
      }
      break;
    case ATTR_DOMAIN_FACE:
      return ss->totfaces;
      break;
    default:
      BLI_assert_unreachable();
      return 0;
  }
}

ATTR_NO_OPT static bool sculpt_attribute_create(SculptSession *ss,
                                                Object *ob,
                                                eAttrDomain domain,
                                                eCustomDataType proptype,
                                                const char *name,
                                                SculptAttribute *out,
                                                const SculptAttributeParams *params,
                                                PBVHType pbvhtype)
{
  Mesh *me = BKE_object_get_original_mesh(ob);

  bool simple_array = params->simple_array;
  bool permanent = params->permanent;

  out->params = *params;
  out->proptype = proptype;
  out->domain = domain;
  STRNCPY_UTF8(out->name, name);

  /* Force non-CustomData simple_array mode if not PBVH_FACES. */
  if (pbvhtype == PBVH_GRIDS && domain == ATTR_DOMAIN_POINT) {
    if (permanent) {
      printf(
          "%s: error: tried to make permanent customdata in multires; will make "
          "local "
          "array "
          "instead.\n",
          __func__);
      permanent = (out->params.permanent = false);
    }

    simple_array = true;
  }

  BLI_assert(!(simple_array && permanent));

  int totelem = sculpt_attr_elem_count_get(ob, domain);

  if (simple_array) {
    int elemsize = CustomData_sizeof(proptype);

    out->data = MEM_calloc_arrayN(totelem, elemsize, __func__);

    out->data_for_bmesh = ss->bm != nullptr;
    out->simple_array = true;
    out->bmesh_cd_offset = -1;
    out->layer = nullptr;
    out->elem_size = elemsize;
    out->used = true;
    out->elem_num = totelem;

    return true;
  }

  switch (pbvhtype) {
    out->simple_array = false;

    case PBVH_BMESH: {
      CustomData *cdata = nullptr;
      out->data_for_bmesh = true;

      switch (domain) {
        case ATTR_DOMAIN_POINT:
          cdata = &ss->bm->vdata;
          break;
        case ATTR_DOMAIN_FACE:
          cdata = &ss->bm->pdata;
          break;
        default:
          out->used = false;
          return false;
      }

      if (CustomData_get_named_layer_index(cdata, proptype, name) == -1) {
        BM_data_layer_add_named(ss->bm, cdata, proptype, name);
      }
      int index = CustomData_get_named_layer_index(cdata, proptype, name);

      if (!permanent && !ss->save_temp_layers) {
        cdata->layers[index].flag |= CD_FLAG_TEMPORARY | CD_FLAG_NOCOPY;
      }

      out->data = nullptr;
      out->layer = cdata->layers + index;
      out->bmesh_cd_offset = out->layer->offset;
      out->elem_size = CustomData_sizeof(proptype);
      break;
    }
    case PBVH_GRIDS:
    case PBVH_FACES: {
      CustomData *cdata = nullptr;

      switch (domain) {
        case ATTR_DOMAIN_POINT:
          cdata = &me->vdata;
          break;
        case ATTR_DOMAIN_FACE:
          cdata = &me->pdata;
          break;
        default:
          out->used = false;
          return false;
      }

      if (CustomData_get_named_layer_index(cdata, proptype, name) == -1) {
        CustomData_add_layer_named(cdata, proptype, CD_SET_DEFAULT, totelem, name);
      }
      int index = CustomData_get_named_layer_index(cdata, proptype, name);

      if (!permanent && !ss->save_temp_layers) {
        cdata->layers[index].flag |= CD_FLAG_TEMPORARY | CD_FLAG_NOCOPY;
      }

      out->layer = cdata->layers + index;
      out->data = out->layer->data;
      out->data_for_bmesh = false;
      out->bmesh_cd_offset = -1;
      out->elem_size = CustomData_sizeof(proptype);

      break;
    }
    default:
      BLI_assert_unreachable();
      break;
  }

  out->used = true;
  out->elem_num = totelem;

  return true;
}

ATTR_NO_OPT static bool sculpt_attr_update(Object *ob, SculptAttribute *attr)
{
  SculptSession *ss = ob->sculpt;
  int elem_num = sculpt_attr_elem_count_get(ob, attr->domain);

  bool bad = false;

  if (attr->data) {
    bad = attr->elem_num != elem_num;
  }

  /* Check if we are a coerced simple array and shouldn't be. */
  bad |= (attr->simple_array && !attr->params.simple_array) &&
         !(BKE_pbvh_type(ss->pbvh) == PBVH_GRIDS && attr->domain == ATTR_DOMAIN_POINT);

  CustomData *cdata = sculpt_get_cdata(ob, attr->domain);
  if (cdata && !attr->simple_array) {
    int layer_index = CustomData_get_named_layer_index(cdata, attr->proptype, attr->name);

    bad |= layer_index == -1;
    bad |= (ss->bm != nullptr) != attr->data_for_bmesh;

    if (!bad) {
      if (attr->data_for_bmesh) {
        attr->bmesh_cd_offset = cdata->layers[layer_index].offset;
        attr->data = nullptr;
      }
      else {
        attr->data = cdata->layers[layer_index].data;
      }
    }
  }

  PBVHType pbvhtype;
  if (ss->pbvh) {
    pbvhtype = BKE_pbvh_type(ss->pbvh);
  }
  else if (ss->bm) {
    pbvhtype = PBVH_BMESH;
  }
  else if (ss->subdiv_ccg) {
    pbvhtype = PBVH_GRIDS;
  }
  else {
    pbvhtype = PBVH_FACES;
  }

  if (bad) {
    if (attr->simple_array) {
      MEM_SAFE_FREE(attr->data);
    }

    if (pbvhtype != PBVH_GRIDS && (attr->simple_array && !attr->params.simple_array)) {
      attr->simple_array = false;
    }

    sculpt_attribute_create(
        ss, ob, attr->domain, attr->proptype, attr->name, attr, &attr->params, pbvhtype);
  }

  return bad;
}

static SculptAttribute *sculpt_get_cached_layer(SculptSession *ss,
                                                eAttrDomain domain,
                                                eCustomDataType proptype,
                                                const char *name)
{
  for (int i = 0; i < SCULPT_MAX_ATTRIBUTES; i++) {
    SculptAttribute *attr = ss->temp_attributes + i;

    if (attr->used && STREQ(attr->name, name) && attr->proptype == proptype &&
        attr->domain == domain) {

      return attr;
    }
  }

  return nullptr;
}

bool BKE_sculpt_attribute_exists(Object *ob,
                                 eAttrDomain domain,
                                 eCustomDataType proptype,
                                 const char *name)
{
  SculptSession *ss = ob->sculpt;
  SculptAttribute *attr = sculpt_get_cached_layer(ss, domain, proptype, name);

  if (attr) {
    return true;
  }

  CustomData *cdata = sculpt_get_cdata(ob, domain);
  return CustomData_get_named_layer_index(cdata, proptype, name) != -1;
}

static SculptAttribute *sculpt_alloc_attr(SculptSession *ss)
{
  for (int i = 0; i < SCULPT_MAX_ATTRIBUTES; i++) {
    if (!ss->temp_attributes[i].used) {
      memset((void *)(ss->temp_attributes + i), 0, sizeof(SculptAttribute));
      ss->temp_attributes[i].used = true;

      return ss->temp_attributes + i;
    }
  }

  BLI_assert_unreachable();
  return nullptr;
}

SculptAttribute *BKE_sculpt_attribute_get(struct Object *ob,
                                          eAttrDomain domain,
                                          eCustomDataType proptype,
                                          const char *name)
{
  SculptSession *ss = ob->sculpt;

  /* See if attribute is cached in ss->temp_attributes. */
  SculptAttribute *attr = sculpt_get_cached_layer(ss, domain, proptype, name);

  if (attr) {
    if (sculpt_attr_update(ob, attr)) {
      sculpt_attribute_update_refs(ob);
    }

    return attr;
  }

  if (!ss->pbvh || (BKE_pbvh_type(ss->pbvh) == PBVH_GRIDS && domain == ATTR_DOMAIN_POINT)) {
    /* Don't pull from customdata for PBVH_GRIDS and vertex domain.
     * Multires vertex attributes don't go through CustomData.
     */
    return nullptr;
  }

  /* Does attribute exist in CustomData layout? */
  CustomData *cdata = sculpt_get_cdata(ob, domain);
  if (cdata) {
    int index = CustomData_get_named_layer_index(cdata, proptype, name);

    if (index != -1) {
      int totelem = sculpt_attr_elem_count_get(ob, domain);

      attr = sculpt_alloc_attr(ss);

      if (BKE_pbvh_type(ss->pbvh) == PBVH_FACES ||
          (BKE_pbvh_type(ss->pbvh) == PBVH_GRIDS && domain == ATTR_DOMAIN_FACE))
      {
        attr->data = cdata->layers[index].data;
      }

      attr->params.permanent = !(cdata->layers[index].flag & CD_FLAG_TEMPORARY);
      attr->used = true;
      attr->domain = domain;
      attr->proptype = proptype;
      attr->data = cdata->layers[index].data;
      attr->bmesh_cd_offset = cdata->layers[index].offset;
      attr->elem_num = totelem;
      attr->layer = cdata->layers + index;
      attr->elem_size = CustomData_sizeof(proptype);
      attr->data_for_bmesh = ss->bm && attr->bmesh_cd_offset != -1;

      STRNCPY_UTF8(attr->name, name);
      return attr;
    }
  }

  return nullptr;
}

static SculptAttribute *sculpt_attribute_ensure_ex(Object *ob,
                                                   eAttrDomain domain,
                                                   eCustomDataType proptype,
                                                   const char *name,
                                                   const SculptAttributeParams *params,
                                                   PBVHType pbvhtype)
{
  SculptSession *ss = ob->sculpt;
  SculptAttribute *attr = BKE_sculpt_attribute_get(ob, domain, proptype, name);

  if (attr) {
    sculpt_attr_update(ob, attr);

    /* Since "stroke_only" is not a CustomData flag we have
     * to sync its parameter setting manually. Fixes #104618.
     */
    attr->params.stroke_only = params->stroke_only;

    return attr;
  }

  attr = sculpt_alloc_attr(ss);

  /* Create attribute. */
  sculpt_attribute_create(ss, ob, domain, proptype, name, attr, params, pbvhtype);
  sculpt_attribute_update_refs(ob);

  return attr;
}

SculptAttribute *BKE_sculpt_attribute_ensure(Object *ob,
                                             eAttrDomain domain,
                                             eCustomDataType proptype,
                                             const char *name,
                                             const SculptAttributeParams *params)
{
  SculptAttributeParams temp_params = *params;

  return sculpt_attribute_ensure_ex(
      ob, domain, proptype, name, &temp_params, BKE_pbvh_type(ob->sculpt->pbvh));
}

static void sculptsession_bmesh_attr_update_internal(Object *ob)
{
  SculptSession *ss = ob->sculpt;

  sculptsession_bmesh_add_layers(ob);
  if (ss->bm_idmap) {
    BM_idmap_check_attributes(ss->bm_idmap);
  }

  if (ss->pbvh) {
    int cd_face_area = ss->attrs.face_areas ? ss->attrs.face_areas->bmesh_cd_offset : -1;
    int cd_boundary_flags = ss->attrs.boundary_flags ? ss->attrs.boundary_flags->bmesh_cd_offset :
                                                       -1;
    int cd_dyntopo_vert = ss->attrs.dyntopo_node_id_vertex ?
                              ss->attrs.dyntopo_node_id_vertex->bmesh_cd_offset :
                              -1;
    int cd_dyntopo_face = ss->attrs.dyntopo_node_id_face ?
                              ss->attrs.dyntopo_node_id_face->bmesh_cd_offset :
                              -1;
    int cd_flag = ss->attrs.flags ? ss->attrs.flags->bmesh_cd_offset : -1;
    int cd_valence = ss->attrs.valence ? ss->attrs.valence->bmesh_cd_offset : -1;

    BKE_pbvh_set_idmap(ss->pbvh, ss->bm_idmap);
    BKE_pbvh_update_offsets(ss->pbvh,
                            cd_dyntopo_vert,
                            cd_dyntopo_face,
                            cd_face_area,
                            cd_boundary_flags,
                            cd_flag,
                            cd_valence,
                            ss->attrs.orig_co ? ss->attrs.orig_co->bmesh_cd_offset : -1,
                            ss->attrs.orig_no ? ss->attrs.orig_no->bmesh_cd_offset : -1,
                            ss->attrs.curvature_dir ? ss->attrs.curvature_dir->bmesh_cd_offset :
                                                      -1);
  }
}

static void sculptsession_bmesh_add_layers(Object *ob)
{
  SculptSession *ss = ob->sculpt;
  SculptAttributeParams params = {0};

  if (!ss->attrs.face_areas) {
    SculptAttributeParams params = {0};
    ss->attrs.face_areas = sculpt_attribute_ensure_ex(ob,
                                                      ATTR_DOMAIN_FACE,
                                                      CD_PROP_FLOAT2,
                                                      SCULPT_ATTRIBUTE_NAME(face_areas),
                                                      &params,
                                                      PBVH_BMESH);
  }

  if (!ss->attrs.dyntopo_node_id_vertex) {
    ss->attrs.dyntopo_node_id_vertex = sculpt_attribute_ensure_ex(
        ob,
        ATTR_DOMAIN_POINT,
        CD_PROP_INT32,
        SCULPT_ATTRIBUTE_NAME(dyntopo_node_id_vertex),
        &params,
        PBVH_BMESH);
  }

  if (!ss->attrs.dyntopo_node_id_face) {
    ss->attrs.dyntopo_node_id_face = sculpt_attribute_ensure_ex(
        ob,
        ATTR_DOMAIN_FACE,
        CD_PROP_INT32,
        SCULPT_ATTRIBUTE_NAME(dyntopo_node_id_face),
        &params,
        PBVH_BMESH);
  }

  ss->cd_vert_node_offset = ss->attrs.dyntopo_node_id_vertex->bmesh_cd_offset;
  ss->cd_face_node_offset = ss->attrs.dyntopo_node_id_face->bmesh_cd_offset;
  ss->cd_face_areas = ss->attrs.face_areas ? ss->attrs.face_areas->bmesh_cd_offset : -1;
}

template<typename T> static void sculpt_clear_attribute_bmesh(BMesh *bm, SculptAttribute *attr)
{
  BMIter iter;
  int itertype;

  switch (attr->domain) {
    case ATTR_DOMAIN_POINT:
      itertype = BM_VERTS_OF_MESH;
      break;
    case ATTR_DOMAIN_EDGE:
      itertype = BM_EDGES_OF_MESH;
      break;
    case ATTR_DOMAIN_FACE:
      itertype = BM_FACES_OF_MESH;
      break;
    default:
      BLI_assert_unreachable();
      return;
  }

  int size = CustomData_sizeof(attr->proptype);

  T *elem;
  BM_ITER_MESH (elem, &iter, bm, itertype) {
    memset(POINTER_OFFSET(elem->head.data, attr->bmesh_cd_offset), 0, size);
  }
}

void BKE_sculpt_attributes_destroy_temporary_stroke(Object *ob)
{
  SculptSession *ss = ob->sculpt;

  for (int i = 0; i < SCULPT_MAX_ATTRIBUTES; i++) {
    SculptAttribute *attr = ss->temp_attributes + i;

    if (attr->params.stroke_only) {
      /* Don't free BMesh attribute as it is quite expensive;
       * note that temporary attributes are still freed on
       * exiting sculpt mode.
       *
       * Attributes allocated as simple arrays are fine however.
       */

      if (!attr->params.simple_array && ss->bm) {
        /* Zero the attribute in an attempt to emulate the behavior of releasing it. */
        if (attr->domain == ATTR_DOMAIN_POINT) {
          sculpt_clear_attribute_bmesh<BMVert>(ss->bm, attr);
        }
        else if (attr->domain == ATTR_DOMAIN_EDGE) {
          sculpt_clear_attribute_bmesh<BMEdge>(ss->bm, attr);
        }
        else if (attr->domain == ATTR_DOMAIN_FACE) {
          sculpt_clear_attribute_bmesh<BMFace>(ss->bm, attr);
        }
        continue;
      }

      BKE_sculpt_attribute_destroy(ob, attr);
    }
  }
}

static void update_bmesh_offsets(Mesh *me, SculptSession *ss)
{
  ss->cd_vert_node_offset = ss->attrs.dyntopo_node_id_vertex ?
                                ss->attrs.dyntopo_node_id_vertex->bmesh_cd_offset :
                                -1;
  ss->cd_face_node_offset = ss->attrs.dyntopo_node_id_face ?
                                ss->attrs.dyntopo_node_id_face->bmesh_cd_offset :
                                -1;

  CustomDataLayer *layer = BKE_id_attribute_search(&me->id,
                                                   BKE_id_attributes_active_color_name(&me->id),
                                                   CD_MASK_COLOR_ALL,
                                                   ATTR_DOMAIN_MASK_POINT |
                                                       ATTR_DOMAIN_MASK_CORNER);
  if (layer) {
    eAttrDomain domain = BKE_id_attribute_domain(&me->id, layer);

    CustomData *cdata = domain == ATTR_DOMAIN_POINT ? &ss->bm->vdata : &ss->bm->ldata;
    int layer_i = CustomData_get_named_layer_index(
        cdata, eCustomDataType(layer->type), layer->name);

    ss->cd_vcol_offset = layer_i != -1 ? cdata->layers[layer_i].offset : -1;
  }
  else {
    ss->cd_vcol_offset = -1;
  }

  ss->cd_vert_mask_offset = CustomData_get_offset(&ss->bm->vdata, CD_PAINT_MASK);
  ss->cd_faceset_offset = CustomData_get_offset_named(
      &ss->bm->pdata, CD_PROP_INT32, ".sculpt_face_set");
  ss->cd_face_areas = ss->attrs.face_areas ? ss->attrs.face_areas->bmesh_cd_offset : -1;

  int cd_boundary_flags = ss->attrs.boundary_flags ? ss->attrs.boundary_flags->bmesh_cd_offset :
                                                     -1;

  if (ss->pbvh) {
    BKE_pbvh_update_offsets(ss->pbvh,
                            ss->cd_vert_node_offset,
                            ss->cd_face_node_offset,
                            ss->cd_face_areas,
                            cd_boundary_flags,
                            ss->attrs.flags ? ss->attrs.flags->bmesh_cd_offset : -1,
                            ss->attrs.valence ? ss->attrs.valence->bmesh_cd_offset : -1,
                            ss->attrs.orig_co ? ss->attrs.orig_co->bmesh_cd_offset : -1,
                            ss->attrs.orig_no ? ss->attrs.orig_no->bmesh_cd_offset : -1,
                            ss->attrs.curvature_dir ? ss->attrs.curvature_dir->bmesh_cd_offset :
                                                      -1);
  }
}

static void sculpt_attribute_update_refs(Object *ob)
{
  SculptSession *ss = ob->sculpt;

  /* Run twice, in case sculpt_attr_update had to recreate a layer and messed up #BMesh offsets. */
  for (int i = 0; i < 2; i++) {
    for (int j = 0; j < SCULPT_MAX_ATTRIBUTES; j++) {
      SculptAttribute *attr = ss->temp_attributes + j;

      if (attr->used) {
        sculpt_attr_update(ob, attr);
      }
    }

    if (ss->bm) {
      sculptsession_bmesh_attr_update_internal(ob);
    }
  }

  Mesh *me = BKE_object_get_original_mesh(ob);

  if (ss->pbvh) {
    BKE_pbvh_update_active_vcol(ss->pbvh, me);
  }

  if (ss->attrs.face_areas && ss->pbvh) {
    BKE_pbvh_set_face_areas(ss->pbvh, (float *)ss->attrs.face_areas->data);
  }

  if (ss->bm) {
    update_bmesh_offsets(me, ss);
  }
  else if (ss->pbvh) {
    if (ss->attrs.orig_co && ss->attrs.orig_no) {
      const int verts_count = BKE_sculptsession_vertex_count(ss);
      blender::bke::pbvh::set_original(
          ob->sculpt->pbvh,
          {static_cast<float3 *>(ss->attrs.orig_co->data), verts_count},
          {static_cast<float3 *>(ss->attrs.orig_no->data), verts_count});
    }
  }
}

void BKE_sculptsession_update_attr_refs(Object *ob)
{
  sculpt_attribute_update_refs(ob);
}

void BKE_sculpt_attribute_destroy_temporary_all(Object *ob)
{
  SculptSession *ss = ob->sculpt;

  for (int i = 0; i < SCULPT_MAX_ATTRIBUTES; i++) {
    SculptAttribute *attr = ss->temp_attributes + i;

    if (attr->used && !attr->params.permanent) {
      BKE_sculpt_attribute_destroy(ob, attr);
    }
  }
}

bool BKE_sculpt_attribute_destroy(Object *ob, SculptAttribute *attr)
{
  if (!attr || !attr->used) {
    return false;
  }

  SculptSession *ss = ob->sculpt;
  eAttrDomain domain = attr->domain;

  BLI_assert(attr->used);

  /* Remove from convenience pointer struct. */
  SculptAttribute **ptrs = (SculptAttribute **)&ss->attrs;
  int ptrs_num = sizeof(ss->attrs) / sizeof(void *);

  for (int i = 0; i < ptrs_num; i++) {
    if (ptrs[i] == attr) {
      ptrs[i] = nullptr;
    }
  }

  /* Remove from internal temp_attributes array. */
  for (int i = 0; i < SCULPT_MAX_ATTRIBUTES; i++) {
    SculptAttribute *attr2 = ss->temp_attributes + i;

    if (STREQ(attr2->name, attr->name) && attr2->domain == attr->domain &&
        attr2->proptype == attr->proptype)
    {

      attr2->used = false;
    }
  }

  Mesh *me = BKE_object_get_original_mesh(ob);

  if (attr->simple_array) {
    MEM_SAFE_FREE(attr->data);
  }
  else if (ss->bm) {
    if (attr->data_for_bmesh) {
      CustomData *cdata = attr->domain == ATTR_DOMAIN_POINT ? &ss->bm->vdata : &ss->bm->pdata;

      BM_data_layer_free_named(ss->bm, cdata, attr->name);
    }
  }
  else {
    CustomData *cdata = nullptr;
    int totelem = 0;

    switch (domain) {
      case ATTR_DOMAIN_POINT:
        cdata = ss->bm ? &ss->bm->vdata : &me->vdata;
        totelem = ss->totvert;
        break;
      case ATTR_DOMAIN_FACE:
        cdata = ss->bm ? &ss->bm->pdata : &me->pdata;
        totelem = ss->totfaces;
        break;
      default:
        BLI_assert_unreachable();
        return false;
    }

    /* We may have been called after destroying ss->bm in which case attr->layer
     * might be invalid.
     */
    int layer_i = CustomData_get_named_layer_index(
        cdata, eCustomDataType(attr->proptype), attr->name);
    if (layer_i != 0) {
      CustomData_free_layer(cdata, attr->proptype, totelem, layer_i);
    }

    sculpt_attribute_update_refs(ob);
  }

  attr->data = nullptr;
  attr->used = false;

  return true;
}

bool BKE_sculpt_has_persistent_base(SculptSession *ss)
{
  if (ss->bm) {
    return CustomData_get_named_layer_index(
               &ss->bm->vdata, CD_PROP_FLOAT3, SCULPT_ATTRIBUTE_NAME(persistent_co)) != -1;
  }
  else if (ss->vdata) {
    return CustomData_get_named_layer_index(
               ss->vdata, CD_PROP_FLOAT3, SCULPT_ATTRIBUTE_NAME(persistent_co)) != -1;
  }

  /* Detect multires. */
  return ss->attrs.persistent_co;
}

void BKE_sculpt_ensure_origco(struct Object *ob)
{
  SculptSession *ss = ob->sculpt;
  SculptAttributeParams params = {};

  if (!ss->attrs.orig_co) {
    ss->attrs.orig_co = BKE_sculpt_attribute_ensure(
        ob, ATTR_DOMAIN_POINT, CD_PROP_FLOAT3, SCULPT_ATTRIBUTE_NAME(orig_co), &params);
  }
  if (!ss->attrs.orig_no) {
    ss->attrs.orig_no = BKE_sculpt_attribute_ensure(
        ob, ATTR_DOMAIN_POINT, CD_PROP_FLOAT3, SCULPT_ATTRIBUTE_NAME(orig_no), &params);
  }

  if (ss->pbvh) {
    const int verts_count = BKE_sculptsession_vertex_count(ss);
    blender::bke::pbvh::set_original(
        ob->sculpt->pbvh,
        {static_cast<float3 *>(ss->attrs.orig_co->data), verts_count},
        {static_cast<float3 *>(ss->attrs.orig_no->data), verts_count});
  }
}

void BKE_sculpt_ensure_curvature_dir(struct Object *ob)
{
  SculptAttributeParams params = {};
  if (!ob->sculpt->attrs.curvature_dir) {
    ob->sculpt->attrs.curvature_dir = BKE_sculpt_attribute_ensure(
        ob, ATTR_DOMAIN_POINT, CD_PROP_FLOAT3, SCULPT_ATTRIBUTE_NAME(curvature_dir), &params);
  }
}

void BKE_sculpt_ensure_origmask(struct Object *ob)
{
  SculptAttributeParams params = {};
  if (!ob->sculpt->attrs.orig_mask) {
    ob->sculpt->attrs.orig_mask = BKE_sculpt_attribute_ensure(
        ob, ATTR_DOMAIN_POINT, CD_PROP_FLOAT, SCULPT_ATTRIBUTE_NAME(orig_mask), &params);
  }
}
void BKE_sculpt_ensure_origcolor(struct Object *ob)
{
  SculptAttributeParams params = {};
  if (!ob->sculpt->attrs.orig_color) {
    ob->sculpt->attrs.orig_color = BKE_sculpt_attribute_ensure(
        ob, ATTR_DOMAIN_POINT, CD_PROP_COLOR, SCULPT_ATTRIBUTE_NAME(orig_color), &params);
  }
}

void BKE_sculpt_ensure_origfset(struct Object *ob)
{
  SculptAttributeParams params = {};
  if (!ob->sculpt->attrs.orig_fsets) {
    ob->sculpt->attrs.orig_fsets = BKE_sculpt_attribute_ensure(
        ob, ATTR_DOMAIN_FACE, CD_PROP_INT32, SCULPT_ATTRIBUTE_NAME(orig_fsets), &params);
  }
}

void BKE_sculpt_ensure_sculpt_layers(struct Object *ob)
{
  SculptAttributeParams params = {};

  if (!ob->sculpt->attrs.flags) {
    ob->sculpt->attrs.flags = BKE_sculpt_attribute_ensure(
        ob, ATTR_DOMAIN_POINT, CD_PROP_INT8, SCULPT_ATTRIBUTE_NAME(flags), &params);
  }
  if (!ob->sculpt->attrs.valence) {
    ob->sculpt->attrs.valence = BKE_sculpt_attribute_ensure(
        ob, ATTR_DOMAIN_POINT, CD_PROP_INT32, SCULPT_ATTRIBUTE_NAME(valence), &params);
  }
  if (!ob->sculpt->attrs.stroke_id) {
    ob->sculpt->attrs.stroke_id = BKE_sculpt_attribute_ensure(
        ob, ATTR_DOMAIN_POINT, CD_PROP_INT32, SCULPT_ATTRIBUTE_NAME(stroke_id), &params);
  }

  if (ob->sculpt->pbvh) {
    blender::bke::pbvh::set_flags_valence(ob->sculpt->pbvh,
                                          static_cast<uint8_t *>(ob->sculpt->attrs.flags->data),
                                          static_cast<int *>(ob->sculpt->attrs.valence->data));
  }
}

namespace blender::bke::paint {
bool get_original_vertex(SculptSession *ss,
                         PBVHVertRef vertex,
                         float **r_co,
                         float **r_no,
                         float **r_color,
                         float **r_mask)
{
  bool retval = false;

  if (sculpt::stroke_id_test(ss, vertex, STROKEID_USER_ORIGINAL)) {
    if (ss->attrs.orig_co) {
      const float *co = nullptr, *no = nullptr;

      switch (BKE_pbvh_type(ss->pbvh)) {
        case PBVH_BMESH: {
          BMVert *v = reinterpret_cast<BMVert *>(vertex.i);
          co = v->co;
          no = v->no;
          break;
        }
        case PBVH_FACES:
          if (ss->shapekey_active || ss->deform_modifiers_active) {
            co = BKE_pbvh_get_vert_positions(ss->pbvh)[vertex.i];
          }
          else {
            co = ss->vert_positions[vertex.i];
          }

          no = BKE_pbvh_get_vert_normals(ss->pbvh)[vertex.i];
          break;
        case PBVH_GRIDS:
          const CCGKey *key = BKE_pbvh_get_grid_key(ss->pbvh);
          const int grid_index = vertex.i / key->grid_area;
          const int vertex_index = vertex.i - grid_index * key->grid_area;
          CCGElem *elem = BKE_pbvh_get_grids(ss->pbvh)[grid_index];

          co = CCG_elem_co(key, CCG_elem_offset(key, elem, vertex_index));
          no = CCG_elem_no(key, CCG_elem_offset(key, elem, vertex_index));

          break;
      }

      copy_v3_v3(vertex_attr_ptr<float>(vertex, ss->attrs.orig_co), co);
      copy_v3_v3(vertex_attr_ptr<float>(vertex, ss->attrs.orig_no), no);
    }

    bool have_colors = BKE_pbvh_type(ss->pbvh) != PBVH_GRIDS &&
                       ((ss->bm && ss->cd_vcol_offset != -1) || ss->vcol || ss->mcol);

    if (ss->attrs.orig_color && have_colors) {
      BKE_pbvh_vertex_color_get(
          ss->pbvh, vertex, vertex_attr_ptr<float>(vertex, ss->attrs.orig_color));
    }

    if (ss->attrs.orig_mask) {
      float *mask = nullptr;
      switch (BKE_pbvh_type(ss->pbvh)) {
        case PBVH_FACES:
          mask = ss->vmask ? &ss->vmask[vertex.i] : nullptr;
        case PBVH_BMESH: {
          BMVert *v;
          int cd_mask = CustomData_get_offset(&ss->bm->vdata, CD_PAINT_MASK);

          v = (BMVert *)vertex.i;
          mask = cd_mask != -1 ? static_cast<float *>(BM_ELEM_CD_GET_VOID_P(v, cd_mask)) : nullptr;
        }
        case PBVH_GRIDS: {
          const CCGKey *key = BKE_pbvh_get_grid_key(ss->pbvh);

          if (key->mask_offset == -1) {
            mask = nullptr;
          }
          else {
            const int grid_index = vertex.i / key->grid_area;
            const int vertex_index = vertex.i - grid_index * key->grid_area;
            CCGElem *elem = BKE_pbvh_get_grids(ss->pbvh)[grid_index];
            mask = CCG_elem_mask(key, CCG_elem_offset(key, elem, vertex_index));
          }
        }
      }

      if (mask) {
        vertex_attr_set<float>(vertex, ss->attrs.orig_mask, *mask);
      }
    }

    retval = true;
  }

  if (r_co && ss->attrs.orig_co) {
    *r_co = vertex_attr_ptr<float>(vertex, ss->attrs.orig_co);
  }
  if (r_no && ss->attrs.orig_no) {
    *r_no = vertex_attr_ptr<float>(vertex, ss->attrs.orig_no);
  }
  if (r_color && ss->attrs.orig_color) {
    *r_color = vertex_attr_ptr<float>(vertex, ss->attrs.orig_color);
  }
  if (r_mask && ss->attrs.orig_mask) {
    *r_mask = vertex_attr_ptr<float>(vertex, ss->attrs.orig_mask);
  }

  return retval;
}

void load_all_original(Object *ob)
{
  SculptSession *ss = ob->sculpt;

  int verts_count = BKE_sculptsession_vertex_count(ss);
  for (int i : IndexRange(verts_count)) {
    PBVHVertRef vertex = BKE_pbvh_index_to_vertex(ss->pbvh, i);

    blender::bke::sculpt::stroke_id_clear(ss, vertex, STROKEID_USER_ORIGINAL);
    get_original_vertex(ss, vertex, nullptr, nullptr, nullptr, nullptr);
  }
}

}  // namespace blender::bke::paint
