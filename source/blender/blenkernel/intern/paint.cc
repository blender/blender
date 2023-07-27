/* SPDX-FileCopyrightText: 2009 by Nicholas Bishop. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_brush_types.h"
#include "DNA_defaults.h"
#include "DNA_gpencil_legacy_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"
#include "DNA_workspace_types.h"

#include "BLI_bitmap.h"
#include "BLI_hash.h"
#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_string_utf8.h"
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
#include "BKE_pbvh_api.hh"
#include "BKE_scene.h"
#include "BKE_subdiv_ccg.h"
#include "BKE_subsurf.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "RNA_enum_types.h"

#include "BLO_read_write.h"

#include "bmesh.h"

using blender::float3;
using blender::MutableSpan;
using blender::Span;
using blender::Vector;

static void sculpt_attribute_update_refs(Object *ob);
static SculptAttribute *sculpt_attribute_ensure_ex(Object *ob,
                                                   eAttrDomain domain,
                                                   eCustomDataType proptype,
                                                   const char *name,
                                                   const SculptAttributeParams *params,
                                                   PBVHType pbvhtype,
                                                   bool flat_array_for_bmesh);
static void sculptsession_bmesh_add_layers(Object *ob);

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

void BKE_paint_invalidate_overlay_all()
{
  overlay_flags |= (PAINT_OVERLAY_INVALID_TEXTURE_SECONDARY |
                    PAINT_OVERLAY_INVALID_TEXTURE_PRIMARY | PAINT_OVERLAY_INVALID_CURVE);
}

ePaintOverlayControlFlags BKE_paint_get_overlay_flags()
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

const char *BKE_paint_get_tool_enum_translation_context_from_paintmode(ePaintMode mode)
{
  switch (mode) {
    case PAINT_MODE_SCULPT:
    case PAINT_MODE_GPENCIL:
    case PAINT_MODE_TEXTURE_2D:
    case PAINT_MODE_TEXTURE_3D:
      return BLT_I18NCONTEXT_ID_BRUSH;
    case PAINT_MODE_VERTEX:
    case PAINT_MODE_WEIGHT:
    case PAINT_MODE_SCULPT_UV:
    case PAINT_MODE_VERTEX_GPENCIL:
    case PAINT_MODE_SCULPT_GPENCIL:
    case PAINT_MODE_WEIGHT_GPENCIL:
    case PAINT_MODE_SCULPT_CURVES:
    case PAINT_MODE_INVALID:
      break;
  }

  /* Invalid paint mode. */
  return BLT_I18NCONTEXT_DEFAULT;
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
        case OB_MODE_PAINT_GPENCIL_LEGACY:
          return &ts->gp_paint->paint;
        case OB_MODE_VERTEX_GPENCIL_LEGACY:
          return &ts->gp_vertexpaint->paint;
        case OB_MODE_SCULPT_GPENCIL_LEGACY:
          return &ts->gp_sculptpaint->paint;
        case OB_MODE_WEIGHT_GPENCIL_LEGACY:
          return &ts->gp_weightpaint->paint;
        case OB_MODE_SCULPT_CURVES:
          return &ts->curves_sculpt->paint;
        case OB_MODE_PAINT_GREASE_PENCIL:
          return &ts->gp_paint->paint;
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
      case CTX_MODE_PAINT_GPENCIL_LEGACY:
        return PAINT_MODE_GPENCIL;
      case CTX_MODE_PAINT_TEXTURE:
        return PAINT_MODE_TEXTURE_3D;
      case CTX_MODE_VERTEX_GPENCIL_LEGACY:
        return PAINT_MODE_VERTEX_GPENCIL;
      case CTX_MODE_SCULPT_GPENCIL_LEGACY:
        return PAINT_MODE_SCULPT_GPENCIL;
      case CTX_MODE_WEIGHT_GPENCIL_LEGACY:
        return PAINT_MODE_WEIGHT_GPENCIL;
      case CTX_MODE_SCULPT_CURVES:
        return PAINT_MODE_SCULPT_CURVES;
      case CTX_MODE_PAINT_GREASE_PENCIL:
        return PAINT_MODE_GPENCIL;
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
  return (Brush *)BKE_paint_brush_for_read((const Paint *)p);
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
    if (U.experimental.use_grease_pencil_version3) {
      paint->runtime.ob_mode = OB_MODE_PAINT_GREASE_PENCIL;
    }
    else {
      paint->runtime.ob_mode = OB_MODE_PAINT_GPENCIL_LEGACY;
    }
  }
  else if (ts->gp_vertexpaint && paint == &ts->gp_vertexpaint->paint) {
    paint->runtime.tool_offset = offsetof(Brush, gpencil_vertex_tool);
    paint->runtime.ob_mode = OB_MODE_VERTEX_GPENCIL_LEGACY;
  }
  else if (ts->gp_sculptpaint && paint == &ts->gp_sculptpaint->paint) {
    paint->runtime.tool_offset = offsetof(Brush, gpencil_sculpt_tool);
    paint->runtime.ob_mode = OB_MODE_SCULPT_GPENCIL_LEGACY;
  }
  else if (ts->gp_weightpaint && paint == &ts->gp_weightpaint->paint) {
    paint->runtime.tool_offset = offsetof(Brush, gpencil_weight_tool);
    paint->runtime.ob_mode = OB_MODE_WEIGHT_GPENCIL_LEGACY;
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

    *data = *DNA_struct_default_get(Sculpt);

    paint = &data->paint;
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
    BLO_read_id_address(reader, &sce->id, &p->brush);
    for (int i = 0; i < p->tool_slots_len; i++) {
      if (p->tool_slots[i].brush != nullptr) {
        BLO_read_id_address(reader, &sce->id, &p->tool_slots[i].brush);
      }
    }
    BLO_read_id_address(reader, &sce->id, &p->palette);
    p->paint_cursor = nullptr;

    BKE_paint_runtime_init(sce->toolsettings, p);
  }
}

bool paint_is_face_hidden(const int *looptri_faces, const bool *hide_poly, const int tri_index)
{
  if (!hide_poly) {
    return false;
  }
  return hide_poly[looptri_faces[tri_index]];
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

float paint_grid_paint_mask(const GridPaintMask *gpm, uint level, uint x, uint y)
{
  int factor = BKE_ccg_factor(level, gpm->level);
  int gridsize = BKE_ccg_gridsize(gpm->level);

  return gpm->data[(y * factor) * gridsize + (x * factor)];
}

/* Threshold to move before updating the brush rotation, reduces jitter. */
static float paint_rake_rotation_spacing(UnifiedPaintSettings * /*ups*/, Brush *brush)
{
  return brush->sculpt_tool == SCULPT_TOOL_CLAY_STRIPS ? 1.0f : 20.0f;
}

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
                                   ePaintMode paint_mode,
                                   bool stroke_has_started)
{
  bool ok = false;
  if (paint_rake_rotation_active(*brush, paint_mode)) {
    float r = paint_rake_rotation_spacing(ups, brush);
    float rotation;

    /* Use a smaller limit if the stroke hasn't started to prevent excessive pre-roll. */
    if (!stroke_has_started) {
      r = min_ff(r, 4.0f);
    }

    float dpos[2];
    sub_v2_v2v2(dpos, ups->last_rake, mouse_pos);

    /* Limit how often we update the angle to prevent jitter. */
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
  gmap->vert_to_loop_offsets = {};
  gmap->vert_to_loop_indices = {};
  gmap->vert_to_loop = {};
  gmap->vert_to_face_offsets = {};
  gmap->vert_to_face_indices = {};
  gmap->vert_to_face = {};
}

/**
 * Write out the sculpt dynamic-topology #BMesh to the #Mesh.
 */
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
      BMeshToMeshParams params{};
      params.calc_object_remap = false;
      BM_mesh_bm_to_me(nullptr, ss->bm, static_cast<Mesh *>(ob->data), &params);
    }
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
    BKE_pbvh_free(ss->pbvh);
    ss->pbvh = nullptr;
  }

  ss->vert_to_face_offsets = {};
  ss->vert_to_face_indices = {};
  ss->pmap = {};
  ss->edge_to_face_offsets = {};
  ss->edge_to_face_indices = {};
  ss->epmap = {};
  ss->vert_to_edge_offsets = {};
  ss->vert_to_edge_indices = {};
  ss->vemap = {};

  MEM_SAFE_FREE(ss->preview_vert_list);
  ss->preview_vert_count = 0;

  MEM_SAFE_FREE(ss->vertex_info.boundary);

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

    BKE_sculpt_attribute_destroy_temporary_all(ob);

    if (ss->bm) {
      BKE_sculptsession_bm_to_me(ob, true);
      BM_mesh_free(ss->bm);
    }

    sculptsession_free_pbvh(ob);

    if (ss->bm_log) {
      BM_log_free(ss->bm_log);
    }

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

    MEM_delete(ss);

    ob->sculpt = nullptr;
  }
}

static MultiresModifierData *sculpt_multires_modifier_get(const Scene *scene,
                                                          Object *ob,
                                                          const bool auto_create_mdisps)
{
  Mesh *me = (Mesh *)ob->data;
  ModifierData *md;
  VirtualModifierData virtual_modifier_data;

  if (ob->sculpt && ob->sculpt->bm) {
    /* Can't combine multires and dynamic topology. */
    return nullptr;
  }

  bool need_mdisps = false;

  if (!CustomData_get_layer(&me->loop_data, CD_MDISPS)) {
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

  for (md = BKE_modifiers_get_virtual_modifierlist(ob, &virtual_modifier_data); md; md = md->next)
  {
    if (md->type == eModifierType_Multires) {
      MultiresModifierData *mmd = (MultiresModifierData *)md;

      if (!BKE_modifier_is_enabled(scene, md, eModifierMode_Realtime)) {
        continue;
      }

      if (mmd->sculptlvl > 0 && !(mmd->flags & eMultiresModifierFlag_UseSculptBaseMesh)) {
        if (need_mdisps) {
          CustomData_add_layer(&me->loop_data, CD_MDISPS, CD_SET_DEFAULT, me->totloop);
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
  VirtualModifierData virtual_modifier_data;

  if (ob->sculpt->bm || BKE_sculpt_multires_active(scene, ob)) {
    return false;
  }

  /* Non-locked shape keys could be handled in the same way as deformed mesh. */
  if ((ob->shapeflag & OB_SHAPE_LOCK) == 0 && me->key && ob->shapenr) {
    return true;
  }

  md = BKE_modifiers_get_virtual_modifierlist(ob, &virtual_modifier_data);

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

  /* This is for handling a newly opened file with no object visible,
   * causing `me_eval == nullptr`. */
  if (me_eval == nullptr) {
    return;
  }

  ss->depsgraph = depsgraph;

  ss->deform_modifiers_active = sculpt_modifiers_active(scene, sd, ob);

  ss->building_vp_handle = false;

  ss->scene = scene;

  ss->shapekey_active = (mmd == nullptr) ? BKE_keyblock_from_object(ob) : nullptr;

  /* NOTE: Weight pPaint require mesh info for loop lookup, but it never uses multires code path,
   * so no extra checks is needed here. */
  if (mmd) {
    ss->multires.active = true;
    ss->multires.modifier = mmd;
    ss->multires.level = mmd->sculptlvl;
    ss->totvert = me_eval->totvert;
    ss->faces_num = me_eval->faces_num;
    ss->totfaces = me->faces_num;

    /* These are assigned to the base mesh in Multires. This is needed because Face Sets operators
     * and tools use the Face Sets data from the base mesh when Multires is active. */
    ss->vert_positions = me->vert_positions_for_write();
    ss->faces = me->faces();
    ss->corner_verts = me->corner_verts();
  }
  else {
    ss->totvert = me->totvert;
    ss->faces_num = me->faces_num;
    ss->totfaces = me->faces_num;
    ss->vert_positions = me->vert_positions_for_write();
    ss->faces = me->faces();
    ss->corner_verts = me->corner_verts();
    ss->multires.active = false;
    ss->multires.modifier = nullptr;
    ss->multires.level = 0;
    ss->vmask = static_cast<float *>(
        CustomData_get_layer_for_write(&me->vert_data, CD_PAINT_MASK, me->totvert));

    CustomDataLayer *layer;
    eAttrDomain domain;
    if (BKE_pbvh_get_color_layer(me, &layer, &domain)) {
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

  /* Sculpt Face Sets. */
  if (use_face_sets) {
    ss->face_sets = static_cast<int *>(CustomData_get_layer_named_for_write(
        &me->face_data, CD_PROP_INT32, ".sculpt_face_set", me->faces_num));
  }
  else {
    ss->face_sets = nullptr;
  }

  ss->hide_poly = (bool *)CustomData_get_layer_named_for_write(
      &me->face_data, CD_PROP_BOOL, ".hide_poly", me->faces_num);

  ss->subdiv_ccg = me_eval->runtime->subdiv_ccg;

  PBVH *pbvh = BKE_sculpt_object_pbvh_ensure(depsgraph, ob);
  BLI_assert(pbvh == ss->pbvh);
  UNUSED_VARS_NDEBUG(pbvh);

  BKE_pbvh_subdiv_cgg_set(ss->pbvh, ss->subdiv_ccg);
  BKE_pbvh_face_sets_set(ss->pbvh, ss->face_sets);
  BKE_pbvh_update_hide_attributes_from_mesh(ss->pbvh);

  BKE_pbvh_face_sets_color_set(ss->pbvh, me->face_sets_color_seed, me->face_sets_color_default);

  sculpt_attribute_update_refs(ob);
  sculpt_update_persistent_base(ob);

  if (ob->type == OB_MESH && ss->pmap.is_empty()) {
    ss->pmap = blender::bke::mesh::build_vert_to_face_map(me->faces(),
                                                          me->corner_verts(),
                                                          me->totvert,
                                                          ss->vert_to_face_offsets,
                                                          ss->vert_to_face_indices);
  }

  if (ss->pbvh) {
    BKE_pbvh_pmap_set(ss->pbvh, ss->pmap);
  }

  if (ss->deform_modifiers_active) {
    /* Painting doesn't need crazyspace, use already evaluated mesh coordinates if possible. */
    bool used_me_eval = false;

    if (ob->mode & (OB_MODE_VERTEX_PAINT | OB_MODE_WEIGHT_PAINT)) {
      Mesh *me_eval_deform = ob_eval->runtime.mesh_deform_eval;

      /* If the fully evaluated mesh has the same topology as the deform-only version, use it.
       * This matters because crazyspace evaluation is very restrictive and excludes even modifiers
       * that simply recompute vertex weights (which can even include Geometry Nodes). */
      if (me_eval_deform->faces_num == me_eval->faces_num &&
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

void BKE_sculpt_update_object_before_eval(Object *ob_eval)
{
  /* Update before mesh evaluation in the dependency graph. */
  SculptSession *ss = ob_eval->sculpt;

  if (ss && ss->building_vp_handle == false) {
    if (!ss->cache && !ss->filter_cache && !ss->expand_cache) {
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

  sculpt_update_object(depsgraph, ob_orig, ob_eval, false, false);
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
  BKE_id_attributes_default_color_set(&orig_me->id, unique_name);
  DEG_id_tag_update(&orig_me->id, ID_RECALC_GEOMETRY_ALL_MODES);
  BKE_mesh_tessface_clear(orig_me);

  if (object->sculpt && object->sculpt->pbvh) {
    BKE_pbvh_update_active_vcol(object->sculpt->pbvh, orig_me);
  }
}

void BKE_sculpt_update_object_for_edit(
    Depsgraph *depsgraph, Object *ob_orig, bool need_pmap, bool /*need_mask*/, bool is_paint_tool)
{
  BLI_assert(ob_orig == DEG_get_original_object(ob_orig));

  Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob_orig);

  sculpt_update_object(depsgraph, ob_orig, ob_eval, need_pmap, is_paint_tool);
}

int *BKE_sculpt_face_sets_ensure(Object *ob)
{
  SculptSession *ss = ob->sculpt;
  Mesh *mesh = static_cast<Mesh *>(ob->data);

  using namespace blender;
  using namespace blender::bke;
  MutableAttributeAccessor attributes = mesh->attributes_for_write();
  if (!attributes.contains(".sculpt_face_set")) {
    SpanAttributeWriter<int> face_sets = attributes.lookup_or_add_for_write_only_span<int>(
        ".sculpt_face_set", ATTR_DOMAIN_FACE);
    face_sets.span.fill(1);
    mesh->face_sets_color_default = 1;
    face_sets.finish();
  }

  int *face_sets = static_cast<int *>(CustomData_get_layer_named_for_write(
      &mesh->face_data, CD_PROP_INT32, ".sculpt_face_set", mesh->faces_num));

  if (ss->pbvh && ELEM(BKE_pbvh_type(ss->pbvh), PBVH_FACES, PBVH_GRIDS)) {
    BKE_pbvh_face_sets_set(ss->pbvh, face_sets);
  }

  return face_sets;
}

bool *BKE_sculpt_hide_poly_ensure(Mesh *mesh)
{
  bool *hide_poly = static_cast<bool *>(CustomData_get_layer_named_for_write(
      &mesh->face_data, CD_PROP_BOOL, ".hide_poly", mesh->faces_num));
  if (hide_poly != nullptr) {
    return hide_poly;
  }
  return static_cast<bool *>(CustomData_add_layer_named(
      &mesh->face_data, CD_PROP_BOOL, CD_SET_DEFAULT, mesh->faces_num, ".hide_poly"));
}

int BKE_sculpt_mask_layers_ensure(Depsgraph *depsgraph,
                                  Main *bmain,
                                  Object *ob,
                                  MultiresModifierData *mmd)
{
  Mesh *me = static_cast<Mesh *>(ob->data);
  const blender::OffsetIndices faces = me->faces();
  const Span<int> corner_verts = me->corner_verts();
  int ret = 0;

  const float *paint_mask = static_cast<const float *>(
      CustomData_get_layer(&me->vert_data, CD_PAINT_MASK));

  /* if multires is active, create a grid paint mask layer if there
   * isn't one already */
  if (mmd && !CustomData_has_layer(&me->loop_data, CD_GRID_PAINT_MASK)) {
    GridPaintMask *gmask;
    int level = max_ii(1, mmd->sculptlvl);
    int gridsize = BKE_ccg_gridsize(level);
    int gridarea = gridsize * gridsize;

    gmask = static_cast<GridPaintMask *>(
        CustomData_add_layer(&me->loop_data, CD_GRID_PAINT_MASK, CD_SET_DEFAULT, me->totloop));

    for (int i = 0; i < me->totloop; i++) {
      GridPaintMask *gpm = &gmask[i];

      gpm->level = level;
      gpm->data = static_cast<float *>(
          MEM_callocN(sizeof(float) * gridarea, "GridPaintMask.data"));
    }

    /* If vertices already have mask, copy into multires data. */
    if (paint_mask) {
      for (const int i : faces.index_range()) {
        const blender::IndexRange face = faces[i];

        /* Mask center. */
        float avg = 0.0f;
        for (const int vert : corner_verts.slice(face)) {
          avg += paint_mask[vert];
        }
        avg /= float(face.size());

        /* Fill in multires mask corner. */
        for (const int corner : face) {
          GridPaintMask *gpm = &gmask[corner];
          const int vert = corner_verts[corner];
          const int prev = corner_verts[blender::bke::mesh::face_corner_prev(face, vert)];
          const int next = corner_verts[blender::bke::mesh::face_corner_next(face, vert)];

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
    CustomData_add_layer(&me->vert_data, CD_PAINT_MASK, CD_SET_DEFAULT, me->totvert);
    /* The evaluated mesh must be updated to contain the new data. */
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    ret |= SCULPT_MASK_LAYER_CALC_VERT;
  }

  return ret;
}

void BKE_sculpt_toolsettings_data_ensure(Scene *scene)
{
  BKE_paint_ensure(scene->toolsettings, (Paint **)&scene->toolsettings->sculpt);

  Sculpt *sd = scene->toolsettings->sculpt;

  const Sculpt *defaults = DNA_struct_default_get(Sculpt);

  /* We have file versioning code here for historical
   * reasons.  Don't add more checks here, do it properly
   * in blenloader.
   */
  if (sd->automasking_start_normal_limit == 0.0f) {
    sd->automasking_start_normal_limit = defaults->automasking_start_normal_limit;
    sd->automasking_start_normal_falloff = defaults->automasking_start_normal_falloff;

    sd->automasking_view_normal_limit = defaults->automasking_view_normal_limit;
    sd->automasking_view_normal_falloff = defaults->automasking_view_normal_limit;
  }

  if (sd->detail_percent == 0.0f) {
    sd->detail_percent = defaults->detail_percent;
  }
  if (sd->constant_detail == 0.0f) {
    sd->constant_detail = defaults->constant_detail;
  }
  if (sd->detail_size == 0.0f) {
    sd->detail_size = defaults->detail_size;
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
     * there is not bitmap for the grid. This is because missing grid_hidden implies grid is fully
     * visible. */
    if (is_hidden) {
      BKE_subdiv_ccg_grid_hidden_ensure(subdiv_ccg, i);
    }

    BLI_bitmap *gh = subdiv_ccg->grid_hidden[i];
    if (gh) {
      BLI_bitmap_set_all(gh, is_hidden, key.grid_area);
    }
  }
}

static PBVH *build_pbvh_for_dynamic_topology(Object *ob)
{
  PBVH *pbvh = ob->sculpt->pbvh = BKE_pbvh_new(PBVH_BMESH);

  sculptsession_bmesh_add_layers(ob);

  BKE_pbvh_build_bmesh(pbvh,
                       ob->sculpt->bm,
                       ob->sculpt->bm_smooth_shading,
                       ob->sculpt->bm_log,
                       ob->sculpt->attrs.dyntopo_node_id_vertex->bmesh_cd_offset,
                       ob->sculpt->attrs.dyntopo_node_id_face->bmesh_cd_offset);
  return pbvh;
}

static PBVH *build_pbvh_from_regular_mesh(Object *ob, Mesh *me_eval_deform)
{
  Mesh *me = BKE_object_get_original_mesh(ob);
  PBVH *pbvh = BKE_pbvh_new(PBVH_FACES);

  BKE_pbvh_build_mesh(pbvh, me);

  const bool is_deformed = check_sculpt_object_deformed(ob, true);
  if (is_deformed && me_eval_deform != nullptr) {
    BKE_pbvh_vert_coords_apply(
        pbvh,
        reinterpret_cast<const float(*)[3]>(me_eval_deform->vert_positions().data()),
        me_eval_deform->totvert);
  }

  return pbvh;
}

static PBVH *build_pbvh_from_ccg(Object *ob, SubdivCCG *subdiv_ccg)
{
  CCGKey key;
  BKE_subdiv_ccg_key_top_level(&key, subdiv_ccg);
  PBVH *pbvh = BKE_pbvh_new(PBVH_GRIDS);

  Mesh *base_mesh = BKE_mesh_from_object(ob);
  BKE_sculpt_sync_face_visibility_to_grids(base_mesh, subdiv_ccg);

  BKE_pbvh_build_grids(pbvh,
                       subdiv_ccg->grids,
                       subdiv_ccg->num_grids,
                       &key,
                       (void **)subdiv_ccg->grid_faces,
                       subdiv_ccg->grid_flag_mats,
                       subdiv_ccg->grid_hidden,
                       base_mesh,
                       subdiv_ccg);
  return pbvh;
}

PBVH *BKE_sculpt_object_pbvh_ensure(Depsgraph *depsgraph, Object *ob)
{
  if (ob == nullptr || ob->sculpt == nullptr) {
    return nullptr;
  }

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

    BKE_pbvh_update_active_vcol(pbvh, BKE_object_get_original_mesh(ob));
    BKE_pbvh_pmap_set(pbvh, ob->sculpt->pmap);

    return pbvh;
  }

  ob->sculpt->islands_valid = false;

  if (ob->sculpt->bm != nullptr) {
    /* Sculpting on a BMesh (dynamic-topology) gets a special PBVH. */
    pbvh = build_pbvh_for_dynamic_topology(ob);
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

  BKE_pbvh_pmap_set(pbvh, ob->sculpt->pmap);
  ob->sculpt->pbvh = pbvh;

  sculpt_attribute_update_refs(ob);
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

  if (BKE_pbvh_type(ss->pbvh) == PBVH_FACES) {
    /* Regular mesh only draws from PBVH without modifiers and shape keys, or for
     * external engines that do not have access to the PBVH like Eevee does. */
    const bool external_engine = rv3d && rv3d->view_render != nullptr;
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

/**
 * Returns pointer to a CustomData associated with a given domain, if
 * one exists.  If not nullptr is returned (this may happen with e.g.
 * multires and #ATTR_DOMAIN_POINT).
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

        return &me->vert_data;
      case ATTR_DOMAIN_FACE:
        return &me->face_data;
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
      return BKE_sculptsession_vertex_count(ss);
      break;
    case ATTR_DOMAIN_FACE:
      return ss->totfaces;
      break;
    default:
      BLI_assert_unreachable();
      return 0;
  }
}

static bool sculpt_attribute_create(SculptSession *ss,
                                    Object *ob,
                                    eAttrDomain domain,
                                    eCustomDataType proptype,
                                    const char *name,
                                    SculptAttribute *out,
                                    const SculptAttributeParams *params,
                                    PBVHType pbvhtype,
                                    bool flat_array_for_bmesh)
{
  Mesh *me = BKE_object_get_original_mesh(ob);

  bool simple_array = params->simple_array;
  bool permanent = params->permanent;

  out->params = *params;
  out->proptype = proptype;
  out->domain = domain;
  STRNCPY_UTF8(out->name, name);

  /* Force non-CustomData simple_array mode if not PBVH_FACES. */
  if (pbvhtype == PBVH_GRIDS || (pbvhtype == PBVH_BMESH && flat_array_for_bmesh)) {
    if (permanent) {
      printf(
          "%s: error: tried to make permanent customdata in multires or bmesh mode; will make "
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

  out->simple_array = false;

  switch (BKE_pbvh_type(ss->pbvh)) {
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

      BLI_assert(CustomData_get_named_layer_index(cdata, proptype, name) == -1);

      BM_data_layer_add_named(ss->bm, cdata, proptype, name);
      int index = CustomData_get_named_layer_index(cdata, proptype, name);

      if (!permanent) {
        cdata->layers[index].flag |= CD_FLAG_TEMPORARY | CD_FLAG_NOCOPY;
      }

      out->data = nullptr;
      out->layer = cdata->layers + index;
      out->bmesh_cd_offset = out->layer->offset;
      out->elem_size = CustomData_sizeof(proptype);
      break;
    }
    case PBVH_FACES: {
      CustomData *cdata = nullptr;

      switch (domain) {
        case ATTR_DOMAIN_POINT:
          cdata = &me->vert_data;
          break;
        case ATTR_DOMAIN_FACE:
          cdata = &me->face_data;
          break;
        default:
          out->used = false;
          return false;
      }

      BLI_assert(CustomData_get_named_layer_index(cdata, proptype, name) == -1);

      CustomData_add_layer_named(cdata, proptype, CD_SET_DEFAULT, totelem, name);
      int index = CustomData_get_named_layer_index(cdata, proptype, name);

      if (!permanent) {
        cdata->layers[index].flag |= CD_FLAG_TEMPORARY | CD_FLAG_NOCOPY;
      }

      out->layer = cdata->layers + index;
      out->data = out->layer->data;
      out->data_for_bmesh = false;
      out->bmesh_cd_offset = -1;
      out->elem_size = CustomData_get_elem_size(out->layer);

      break;
    }
    case PBVH_GRIDS: {
      /* GRIDS should have been handled as simple arrays. */
      BLI_assert_unreachable();
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

static bool sculpt_attr_update(Object *ob, SculptAttribute *attr)
{
  SculptSession *ss = ob->sculpt;
  int elem_num = sculpt_attr_elem_count_get(ob, attr->domain);

  bool bad = false;

  if (attr->data) {
    bad = attr->elem_num != elem_num;
  }

  /* Check if we are a coerced simple array and shouldn't be. */
  bad |= attr->simple_array && !attr->params.simple_array &&
         !ELEM(BKE_pbvh_type(ss->pbvh), PBVH_GRIDS, PBVH_BMESH);

  CustomData *cdata = sculpt_get_cdata(ob, attr->domain);
  if (cdata && !attr->simple_array) {
    int layer_index = CustomData_get_named_layer_index(cdata, attr->proptype, attr->name);

    bad |= layer_index == -1;
    bad |= (ss->bm != nullptr) != attr->data_for_bmesh;

    if (!bad) {
      if (attr->data_for_bmesh) {
        attr->bmesh_cd_offset = cdata->layers[layer_index].offset;
      }
      else {
        attr->data = cdata->layers[layer_index].data;
      }
    }
  }

  if (bad) {
    if (attr->simple_array) {
      MEM_SAFE_FREE(attr->data);
    }

    sculpt_attribute_create(ss,
                            ob,
                            attr->domain,
                            attr->proptype,
                            attr->name,
                            attr,
                            &attr->params,
                            BKE_pbvh_type(ss->pbvh),
                            attr->data_for_bmesh);
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

SculptAttribute *BKE_sculpt_attribute_get(Object *ob,
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

  /* Does attribute exist in CustomData layout? */
  CustomData *cdata = sculpt_get_cdata(ob, domain);
  if (cdata) {
    int index = CustomData_get_named_layer_index(cdata, proptype, name);

    if (index != -1) {
      int totelem = 0;

      switch (domain) {
        case ATTR_DOMAIN_POINT:
          totelem = BKE_sculptsession_vertex_count(ss);
          break;
        case ATTR_DOMAIN_FACE:
          totelem = ss->totfaces;
          break;
        default:
          BLI_assert_unreachable();
          break;
      }

      attr = sculpt_alloc_attr(ss);

      attr->used = true;
      attr->domain = domain;
      attr->proptype = proptype;
      attr->data = cdata->layers[index].data;
      attr->bmesh_cd_offset = cdata->layers[index].offset;
      attr->elem_num = totelem;
      attr->layer = cdata->layers + index;
      attr->elem_size = CustomData_get_elem_size(attr->layer);

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
                                                   PBVHType pbvhtype,
                                                   bool flat_array_for_bmesh)
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
  sculpt_attribute_create(
      ss, ob, domain, proptype, name, attr, params, pbvhtype, flat_array_for_bmesh);
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
      ob, domain, proptype, name, &temp_params, BKE_pbvh_type(ob->sculpt->pbvh), true);
}

static void sculptsession_bmesh_attr_update_internal(Object *ob)
{
  SculptSession *ss = ob->sculpt;

  sculptsession_bmesh_add_layers(ob);

  if (ss->pbvh) {
    BKE_pbvh_update_bmesh_offsets(ss->pbvh,
                                  ob->sculpt->attrs.dyntopo_node_id_vertex->bmesh_cd_offset,
                                  ob->sculpt->attrs.dyntopo_node_id_face->bmesh_cd_offset);
  }
}

static void sculptsession_bmesh_add_layers(Object *ob)
{
  SculptSession *ss = ob->sculpt;
  SculptAttributeParams params = {0};

  ss->attrs.dyntopo_node_id_vertex = sculpt_attribute_ensure_ex(
      ob,
      ATTR_DOMAIN_POINT,
      CD_PROP_INT32,
      SCULPT_ATTRIBUTE_NAME(dyntopo_node_id_vertex),
      &params,
      PBVH_BMESH,
      false);

  ss->attrs.dyntopo_node_id_face = sculpt_attribute_ensure_ex(
      ob,
      ATTR_DOMAIN_FACE,
      CD_PROP_INT32,
      SCULPT_ATTRIBUTE_NAME(dyntopo_node_id_face),
      &params,
      PBVH_BMESH,
      false);
}

void BKE_sculpt_attributes_destroy_temporary_stroke(Object *ob)
{
  SculptSession *ss = ob->sculpt;

  for (int i = 0; i < SCULPT_MAX_ATTRIBUTES; i++) {
    SculptAttribute *attr = ss->temp_attributes + i;

    if (attr->params.stroke_only) {
      BKE_sculpt_attribute_destroy(ob, attr);
    }
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
    CustomData *cdata = attr->domain == ATTR_DOMAIN_POINT ? &ss->bm->vdata : &ss->bm->pdata;

    BM_data_layer_free_named(ss->bm, cdata, attr->name);
  }
  else {
    CustomData *cdata = nullptr;
    int totelem = 0;

    switch (domain) {
      case ATTR_DOMAIN_POINT:
        cdata = ss->bm ? &ss->bm->vdata : &me->vert_data;
        totelem = ss->totvert;
        break;
      case ATTR_DOMAIN_FACE:
        cdata = ss->bm ? &ss->bm->pdata : &me->face_data;
        totelem = ss->totfaces;
        break;
      default:
        BLI_assert_unreachable();
        return false;
    }

    /* We may have been called after destroying ss->bm in which case attr->layer
     * might be invalid.
     */
    int layer_i = CustomData_get_named_layer_index(cdata, attr->proptype, attr->name);
    if (layer_i != 0) {
      CustomData_free_layer(cdata, attr->proptype, totelem, layer_i);
    }

    sculpt_attribute_update_refs(ob);
  }

  attr->data = nullptr;
  attr->used = false;

  return true;
}
