/* SPDX-FileCopyrightText: 2009 by Nicholas Bishop. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cstdlib>
#include <cstring>
#include <optional>

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

#include "BLI_alloca.h"
#include "BLI_array.h"
#include "BLI_bitmap.h"
#include "BLI_hash.h"
#include "BLI_index_range.hh"
#include "BLI_listbase.h"
#include "BLI_math_color.h"
#include "BLI_math_matrix.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_vector.h"
#include "BLI_string_ref.hh"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.hh"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "BLT_translation.hh"

#include "BKE_attribute.h"
#include "BKE_attribute.hh"
#include "BKE_brush.hh"
#include "BKE_ccg.h"
#include "BKE_colortools.hh"
#include "BKE_context.hh"
#include "BKE_crazyspace.hh"
#include "BKE_deform.hh"
#include "BKE_global.hh"
#include "BKE_gpencil_legacy.h"
#include "BKE_idtype.hh"
#include "BKE_image.h"
#include "BKE_key.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_material.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.hh"
#include "BKE_mesh_runtime.hh"
#include "BKE_modifier.hh"
#include "BKE_multires.hh"
#include "BKE_object.hh"
#include "BKE_object_types.hh"
#include "BKE_paint.hh"
#include "BKE_pbvh_api.hh"
#include "BKE_scene.hh"
#include "BKE_sculpt.hh"
#include "BKE_subdiv_ccg.hh"
#include "BKE_subsurf.hh"
#include "BKE_undo_system.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "RNA_enum_types.hh"

#include "BLO_read_write.hh"

#include "bmesh.hh"
#include "bmesh_idmap.hh"
#include "bmesh_log.hh"

using blender::bke::AttrDomain;

// TODO: figure out bad cross module refs
namespace blender::ed::sculpt_paint::undo {
void ensure_bmlog(Object *ob);
}

using blender::float3;
using blender::IndexRange;
using blender::MutableSpan;
using blender::Span;
using blender::StringRef;
using blender::Vector;
using namespace blender;

static void sculpt_attribute_update_refs(Object *ob);
static SculptAttribute *sculpt_attribute_ensure_ex(Object *ob,
                                                   AttrDomain domain,
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

static void palette_copy_data(Main * /*bmain*/,
                              std::optional<Library *> /*owner_library*/,
                              ID *id_dst,
                              const ID *id_src,
                              const int /*flag*/)
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
    /*dependencies_id_types*/ 0,
    /*main_listbase_index*/ INDEX_ID_PAL,
    /*struct_size*/ sizeof(Palette),
    /*name*/ "Palette",
    /*name_plural*/ N_("palettes"),
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
    /*blend_read_after_liblink*/ nullptr,

    /*blend_read_undo_preserve*/ palette_undo_preserve,

    /*lib_override_apply_post*/ nullptr,
};

static void paint_curve_copy_data(Main * /*bmain*/,
                                  std::optional<Library *> /*owner_library*/,
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
    /*dependencies_id_types*/ 0,
    /*main_listbase_index*/ INDEX_ID_PC,
    /*struct_size*/ sizeof(PaintCurve),
    /*name*/ "PaintCurve",
    /*name_plural*/ N_("paint_curves"),
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
    /*blend_read_after_liblink*/ nullptr,

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

bool BKE_paint_ensure_from_paintmode(Scene *sce, PaintMode mode)
{
  ToolSettings *ts = sce->toolsettings;
  Paint **paint_ptr = nullptr;
  /* Some paint modes don't store paint settings as pointer, for these this can be set and
   * referenced by paint_ptr. */
  Paint *paint_tmp = nullptr;

  switch (mode) {
    case PaintMode::Sculpt:
      paint_ptr = (Paint **)&ts->sculpt;
      break;
    case PaintMode::Vertex:
      paint_ptr = (Paint **)&ts->vpaint;
      break;
    case PaintMode::Weight:
      paint_ptr = (Paint **)&ts->wpaint;
      break;
    case PaintMode::Texture2D:
    case PaintMode::Texture3D:
      paint_tmp = (Paint *)&ts->imapaint;
      paint_ptr = &paint_tmp;
      break;
    case PaintMode::SculptUV:
      paint_ptr = (Paint **)&ts->uvsculpt;
      break;
    case PaintMode::GPencil:
      paint_ptr = (Paint **)&ts->gp_paint;
      break;
    case PaintMode::VertexGPencil:
      paint_ptr = (Paint **)&ts->gp_vertexpaint;
      break;
    case PaintMode::SculptGPencil:
      paint_ptr = (Paint **)&ts->gp_sculptpaint;
      break;
    case PaintMode::WeightGPencil:
      paint_ptr = (Paint **)&ts->gp_weightpaint;
      break;
    case PaintMode::SculptCurves:
      paint_ptr = (Paint **)&ts->curves_sculpt;
      break;
    case PaintMode::Invalid:
      break;
  }
  if (paint_ptr) {
    BKE_paint_ensure(ts, paint_ptr);
    return true;
  }
  return false;
}

Paint *BKE_paint_get_active_from_paintmode(Scene *sce, PaintMode mode)
{
  if (sce) {
    ToolSettings *ts = sce->toolsettings;

    switch (mode) {
      case PaintMode::Sculpt:
        return &ts->sculpt->paint;
      case PaintMode::Vertex:
        return &ts->vpaint->paint;
      case PaintMode::Weight:
        return &ts->wpaint->paint;
      case PaintMode::Texture2D:
      case PaintMode::Texture3D:
        return &ts->imapaint.paint;
      case PaintMode::SculptUV:
        return &ts->uvsculpt->paint;
      case PaintMode::GPencil:
        return &ts->gp_paint->paint;
      case PaintMode::VertexGPencil:
        return &ts->gp_vertexpaint->paint;
      case PaintMode::SculptGPencil:
        return &ts->gp_sculptpaint->paint;
      case PaintMode::WeightGPencil:
        return &ts->gp_weightpaint->paint;
      case PaintMode::SculptCurves:
        return &ts->curves_sculpt->paint;
      case PaintMode::Invalid:
        return nullptr;
      default:
        return &ts->imapaint.paint;
    }
  }

  return nullptr;
}

const EnumPropertyItem *BKE_paint_get_tool_enum_from_paintmode(const PaintMode mode)
{
  switch (mode) {
    case PaintMode::Sculpt:
      return rna_enum_brush_sculpt_tool_items;
    case PaintMode::Vertex:
      return rna_enum_brush_vertex_tool_items;
    case PaintMode::Weight:
      return rna_enum_brush_weight_tool_items;
    case PaintMode::Texture2D:
    case PaintMode::Texture3D:
      return rna_enum_brush_image_tool_items;
    case PaintMode::SculptUV:
      return rna_enum_brush_uv_sculpt_tool_items;
    case PaintMode::GPencil:
      return rna_enum_brush_gpencil_types_items;
    case PaintMode::VertexGPencil:
      return rna_enum_brush_gpencil_vertex_types_items;
    case PaintMode::SculptGPencil:
      return rna_enum_brush_gpencil_sculpt_types_items;
    case PaintMode::WeightGPencil:
      return rna_enum_brush_gpencil_weight_types_items;
    case PaintMode::SculptCurves:
      return rna_enum_brush_curves_sculpt_tool_items;
    case PaintMode::Invalid:
      break;
  }
  return nullptr;
}

const char *BKE_paint_get_tool_prop_id_from_paintmode(const PaintMode mode)
{
  switch (mode) {
    case PaintMode::Sculpt:
      return "sculpt_tool";
    case PaintMode::Vertex:
      return "vertex_tool";
    case PaintMode::Weight:
      return "weight_tool";
    case PaintMode::Texture2D:
    case PaintMode::Texture3D:
      return "image_tool";
    case PaintMode::SculptUV:
      return "uv_sculpt_tool";
    case PaintMode::GPencil:
      return "gpencil_tool";
    case PaintMode::VertexGPencil:
      return "gpencil_vertex_tool";
    case PaintMode::SculptGPencil:
      return "gpencil_sculpt_tool";
    case PaintMode::WeightGPencil:
      return "gpencil_weight_tool";
    case PaintMode::SculptCurves:
      return "curves_sculpt_tool";
    case PaintMode::Invalid:
      break;
  }

  /* Invalid paint mode. */
  return nullptr;
}

const char *BKE_paint_get_tool_enum_translation_context_from_paintmode(const PaintMode mode)
{
  switch (mode) {
    case PaintMode::Sculpt:
    case PaintMode::GPencil:
    case PaintMode::Texture2D:
    case PaintMode::Texture3D:
      return BLT_I18NCONTEXT_ID_BRUSH;
    case PaintMode::Vertex:
    case PaintMode::Weight:
    case PaintMode::SculptUV:
    case PaintMode::VertexGPencil:
    case PaintMode::SculptGPencil:
    case PaintMode::WeightGPencil:
    case PaintMode::SculptCurves:
    case PaintMode::Invalid:
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

PaintMode BKE_paintmode_get_active_from_context(const bContext *C)
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
          return PaintMode::Texture2D;
        }
        if (sima->mode == SI_MODE_UV) {
          return PaintMode::SculptUV;
        }
      }
      else {
        return PaintMode::Texture2D;
      }
    }
    else if (obact) {
      switch (obact->mode) {
        case OB_MODE_SCULPT:
          return PaintMode::Sculpt;
        case OB_MODE_SCULPT_GPENCIL_LEGACY:
          return PaintMode::SculptGPencil;
        case OB_MODE_WEIGHT_GPENCIL_LEGACY:
          return PaintMode::WeightGPencil;
        case OB_MODE_VERTEX_PAINT:
          return PaintMode::Vertex;
        case OB_MODE_WEIGHT_PAINT:
          return PaintMode::Weight;
        case OB_MODE_TEXTURE_PAINT:
          return PaintMode::Texture3D;
        case OB_MODE_EDIT:
          return PaintMode::SculptUV;
        case OB_MODE_SCULPT_CURVES:
          return PaintMode::SculptCurves;
        case OB_MODE_PAINT_GREASE_PENCIL:
          return PaintMode::GPencil;
        default:
          return PaintMode::Texture2D;
      }
    }
    else {
      /* default to image paint */
      return PaintMode::Texture2D;
    }
  }

  return PaintMode::Invalid;
}

PaintMode BKE_paintmode_get_from_tool(const bToolRef *tref)
{
  if (tref->space_type == SPACE_VIEW3D) {
    switch (tref->mode) {
      case CTX_MODE_SCULPT:
        return PaintMode::Sculpt;
      case CTX_MODE_PAINT_VERTEX:
        return PaintMode::Vertex;
      case CTX_MODE_PAINT_WEIGHT:
        return PaintMode::Weight;
      case CTX_MODE_PAINT_GPENCIL_LEGACY:
        return PaintMode::GPencil;
      case CTX_MODE_PAINT_TEXTURE:
        return PaintMode::Texture3D;
      case CTX_MODE_VERTEX_GPENCIL_LEGACY:
        return PaintMode::VertexGPencil;
      case CTX_MODE_SCULPT_GPENCIL_LEGACY:
        return PaintMode::SculptGPencil;
      case CTX_MODE_WEIGHT_GPENCIL_LEGACY:
        return PaintMode::WeightGPencil;
      case CTX_MODE_SCULPT_CURVES:
        return PaintMode::SculptCurves;
      case CTX_MODE_PAINT_GREASE_PENCIL:
        return PaintMode::GPencil;
    }
  }
  else if (tref->space_type == SPACE_IMAGE) {
    switch (tref->mode) {
      case SI_MODE_PAINT:
        return PaintMode::Texture2D;
      case SI_MODE_UV:
        return PaintMode::SculptUV;
    }
  }

  return PaintMode::Invalid;
}

Brush *BKE_paint_brush(Paint *p)
{
  return p ? p->brush : nullptr;
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

uint BKE_paint_get_brush_tool_offset_from_paintmode(const PaintMode mode)
{
  switch (mode) {
    case PaintMode::Texture2D:
    case PaintMode::Texture3D:
      return offsetof(Brush, imagepaint_tool);
    case PaintMode::Sculpt:
      return offsetof(Brush, sculpt_tool);
    case PaintMode::Vertex:
      return offsetof(Brush, vertexpaint_tool);
    case PaintMode::Weight:
      return offsetof(Brush, weightpaint_tool);
    case PaintMode::SculptUV:
      return offsetof(Brush, uv_sculpt_tool);
    case PaintMode::GPencil:
      return offsetof(Brush, gpencil_tool);
    case PaintMode::VertexGPencil:
      return offsetof(Brush, gpencil_vertex_tool);
    case PaintMode::SculptGPencil:
      return offsetof(Brush, gpencil_sculpt_tool);
    case PaintMode::WeightGPencil:
      return offsetof(Brush, gpencil_weight_tool);
    case PaintMode::SculptCurves:
      return offsetof(Brush, curves_sculpt_tool);
    case PaintMode::Invalid:
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

bool BKE_paint_select_face_test(const Object *ob)
{
  return ((ob != nullptr) && (ob->type == OB_MESH) && (ob->data != nullptr) &&
          (((Mesh *)ob->data)->editflag & ME_EDIT_PAINT_FACE_SEL) &&
          (ob->mode & (OB_MODE_VERTEX_PAINT | OB_MODE_WEIGHT_PAINT | OB_MODE_TEXTURE_PAINT)));
}

bool BKE_paint_select_vert_test(const Object *ob)
{
  return ((ob != nullptr) && (ob->type == OB_MESH) && (ob->data != nullptr) &&
          (((Mesh *)ob->data)->editflag & ME_EDIT_PAINT_VERT_SEL) &&
          (ob->mode & OB_MODE_WEIGHT_PAINT || ob->mode & OB_MODE_VERTEX_PAINT));
}

bool BKE_paint_select_elem_test(const Object *ob)
{
  return (BKE_paint_select_vert_test(ob) || BKE_paint_select_face_test(ob));
}

bool BKE_paint_always_hide_test(const Object *ob)
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

eObjectMode BKE_paint_object_mode_from_paintmode(const PaintMode mode)
{
  switch (mode) {
    case PaintMode::Sculpt:
      return OB_MODE_SCULPT;
    case PaintMode::Vertex:
      return OB_MODE_VERTEX_PAINT;
    case PaintMode::Weight:
      return OB_MODE_WEIGHT_PAINT;
    case PaintMode::Texture2D:
    case PaintMode::Texture3D:
      return OB_MODE_TEXTURE_PAINT;
    case PaintMode::SculptUV:
      return OB_MODE_EDIT;
    case PaintMode::SculptCurves:
      return OB_MODE_SCULPT_CURVES;
    case PaintMode::GPencil:
      return OB_MODE_PAINT_GREASE_PENCIL;
    case PaintMode::Invalid:
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

  paint->tile_offset[0] = paint->tile_offset[1] = paint->tile_offset[2] = 1.0f;

  BKE_paint_runtime_init(ts, paint);

  return false;
}

void BKE_paint_init(Main *bmain, Scene *sce, PaintMode mode, const uchar col[3])
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

void BKE_paint_copy(const Paint *src, Paint *tar, int flag)
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

void BKE_paint_stroke_get_average(const Scene *scene, const Object *ob, float stroke[3])
{
  UnifiedPaintSettings *ups = &scene->toolsettings->unified_paint_settings;
  if (ups->last_stroke_valid && ups->average_stroke_counter > 0) {
    float fac = 1.0f / ups->average_stroke_counter;
    mul_v3_v3fl(stroke, ups->average_stroke_accum, fac);
  }
  else {
    copy_v3_v3(stroke, ob->object_to_world().location());
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

  p->paint_cursor = nullptr;
  BKE_paint_runtime_init(scene->toolsettings, p);
}

bool paint_is_grid_face_hidden(const blender::BoundedBitSpan grid_hidden,
                               int gridsize,
                               int x,
                               int y)
{
  /* Skip face if any of its corners are hidden. */
  return grid_hidden[y * gridsize + x] || grid_hidden[y * gridsize + x + 1] ||
         grid_hidden[(y + 1) * gridsize + x + 1] || grid_hidden[(y + 1) * gridsize + x];
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

static const bool paint_rake_rotation_active(const Brush &brush, PaintMode paint_mode)
{
  return paint_rake_rotation_active(brush.mtex) || paint_rake_rotation_active(brush.mask_mtex) ||
         BKE_brush_has_cube_tip(&brush, paint_mode);
}

bool paint_calculate_rake_rotation(UnifiedPaintSettings *ups,
                                   Brush *brush,
                                   const float mouse_pos[2],
                                   const float initial_mouse_pos[2],
                                   PaintMode paint_mode,
                                   bool stroke_has_started)
{
  bool ok = false;
  if (paint_rake_rotation_active(*brush, paint_mode)) {
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

    float r = paint_rake_rotation_spacing(ups, brush);

    /* Use a smaller limit if the stroke hasn't started to prevent excessive pre-roll. */
    if (!stroke_has_started) {
      r = min_ff(r, 4.0f);
    }

    float dpos[2];
    sub_v2_v2v2(dpos, mouse_pos, ups->last_rake);

    /* Limit how often we update the angle to prevent jitter. */
    if (len_squared_v2(dpos) >= r * r) {
      rotation = atan2f(dpos[1], dpos[0]) + float(0.5f * M_PI);

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

/**
 * Returns pointer to a CustomData associated with a given domain, if
 * one exists.  If not nullptr is returned (this may happen with e.g.
 * multires and #AttrDomain::Point).
 */
static CustomData *sculpt_get_cdata(Object *ob, AttrDomain domain)
{
  SculptSession *ss = ob->sculpt;

  if (ss->bm) {
    switch (domain) {
      case AttrDomain::Point:
        return &ss->bm->vdata;
      case AttrDomain::Edge:
        return &ss->bm->edata;
      case AttrDomain::Corner:
        return &ss->bm->ldata;
      case AttrDomain::Face:
        return &ss->bm->pdata;
      default:
        BLI_assert_unreachable();
        return nullptr;
    }
  }
  else {
    Mesh *me = BKE_object_get_original_mesh(ob);

    switch (domain) {
      case AttrDomain::Point:
        /* Cannot get vertex domain for multires grids. */
        if (ss->pbvh && BKE_pbvh_type(ss->pbvh) == PBVH_GRIDS) {
          return nullptr;
        }

        return &me->vert_data;
      case AttrDomain::Corner:
        return &me->corner_data;
      case AttrDomain::Edge:
        return &me->edge_data;
      case AttrDomain::Face:
        return &me->face_data;
      default:
        BLI_assert_unreachable();
        return nullptr;
    }
  }
}

static bool sculpt_boundary_flags_ensure(
    Object *ob, PBVH *pbvh, int totvert, int totedge, bool force_update = false)
{
  SculptSession *ss = ob->sculpt;
  bool ret = false;

  if (!ss->attrs.edge_boundary_flags) {
    SculptAttributeParams params = {0};
    params.nointerp = true;

    ss->attrs.edge_boundary_flags = sculpt_attribute_ensure_ex(
        ob,
        AttrDomain::Edge,
        CD_PROP_INT32,
        SCULPT_ATTRIBUTE_NAME(edge_boundary_flags),
        &params,
        BKE_pbvh_type(pbvh));

    force_update = true;
    ret = true;
  }

  if (!ss->attrs.boundary_flags) {
    SculptAttributeParams params = {0};
    params.nointerp = true;

    ss->attrs.boundary_flags = sculpt_attribute_ensure_ex(ob,
                                                          AttrDomain::Point,
                                                          CD_PROP_INT32,
                                                          SCULPT_ATTRIBUTE_NAME(boundary_flags),
                                                          &params,
                                                          BKE_pbvh_type(pbvh));

    force_update = true;
    ret = true;

    BKE_pbvh_set_boundary_flags(pbvh, static_cast<int *>(ss->attrs.boundary_flags->data));
  }

  if (force_update) {
    if (ss->bm) {
      BM_mesh_elem_table_ensure(ss->bm, BM_VERT | BM_EDGE);
    }

    for (int i = 0; i < totvert; i++) {
      PBVHVertRef vertex = BKE_pbvh_index_to_vertex(pbvh, i);
      BKE_sculpt_boundary_flag_update(ss, vertex);
      BKE_sculpt_boundary_flag_uv_update(ss, vertex);

      if (ss->pbvh) {
        blender::bke::pbvh::check_vert_boundary(ss->pbvh, vertex, ss->face_sets);
      }
    }

    for (int i = 0; i < totedge; i++) {
      PBVHEdgeRef edge = BKE_pbvh_index_to_edge(pbvh, i);
      BKE_sculpt_boundary_flag_update(ss, edge);
      BKE_sculpt_boundary_flag_uv_update(ss, edge);

      if (ss->pbvh) {
        blender::bke::pbvh::check_edge_boundary(ss->pbvh, edge, ss->face_sets);
      }
    }
  }

  BKE_pbvh_set_boundary_flags(pbvh, reinterpret_cast<int *>(ss->attrs.boundary_flags->data));

  return ret;
}

bool BKE_sculptsession_boundary_flags_ensure(Object *ob)
{
  return sculpt_boundary_flags_ensure(
      ob, ob->sculpt->pbvh, BKE_sculptsession_vertex_count(ob->sculpt), ob->sculpt->totedges);
}

void BKE_sculptsession_free_deformMats(SculptSession *ss)
{
  ss->orig_cos = {};
  ss->deform_cos = {};
  ss->deform_imats = {};
}

void BKE_sculptsession_free_vwpaint_data(SculptSession *ss)
{
  if (ss->mode_type == OB_MODE_WEIGHT_PAINT) {
    MEM_SAFE_FREE(ss->mode.wpaint.alpha_weight);
    if (ss->mode.wpaint.dvert_prev) {
      BKE_defvert_array_free_elems(ss->mode.wpaint.dvert_prev, ss->totvert);
      MEM_freeN(ss->mode.wpaint.dvert_prev);
      ss->mode.wpaint.dvert_prev = nullptr;
    }
  }
}

/**
 * Write out the sculpt dynamic-topology #BMesh to the #Mesh.
 */
static void sculptsession_bm_to_me_update_data_only(Object *ob, bool /*reorder*/)
{
  SculptSession *ss = ob->sculpt;

  if (ss->bm && ob->data) {
    BKE_sculptsession_update_attr_refs(ob);

    BMeshToMeshParams params = {};
    params.update_shapekey_indices = true;
    params.calc_object_remap = false;

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
    bke::pbvh::free(ss->pbvh);

    ss->needs_pbvh_rebuild = false;
    ss->pbvh = nullptr;
  }

  ss->vert_to_face_map = {};
  ss->edge_to_face_offsets = {};
  ss->edge_to_face_indices = {};
  ss->edge_to_face_map = {};
  ss->vert_to_edge_offsets = {};
  ss->vert_to_edge_indices = {};
  ss->vert_to_edge_map = {};

  MEM_SAFE_FREE(ss->preview_vert_list);
  ss->preview_vert_count = 0;

  ss->vertex_info.boundary.clear_and_shrink();

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

    if (ss->bm_log) {
      /* Does not free the actual entries, the undo system does that */
      BM_log_free(ss->bm_log);
      ss->bm_log = nullptr;
    }

    /* Destroy temporary attributes. */
    BKE_sculpt_attribute_destroy_temporary_all(ob);

    if (ss->bm) {
      BKE_sculptsession_bm_to_me(ob, true);
      BM_mesh_free(ss->bm);
      ss->bm = nullptr;
    }

    sculptsession_free_pbvh(ob);

    if (ss->bm_log) {
      BM_log_free(ss->bm_log);
    }

    if (ss->tex_pool) {
      BKE_image_pool_free(ss->tex_pool);
    }

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

  if (!CustomData_get_layer(&me->corner_data, CD_MDISPS)) {
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
          CustomData_add_layer(&me->corner_data, CD_MDISPS, CD_SET_DEFAULT, me->corners_num);
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

    if (mti->type == ModifierTypeType::OnlyDeform) {
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

    if (ob->sculpt->bm_log) {
      BM_log_set_idmap(ob->sculpt->bm_log, ob->sculpt->bm_idmap);
    }

    if (ob->sculpt->pbvh) {
      bke::pbvh::set_idmap(ob->sculpt->pbvh, ob->sculpt->bm_idmap);
    }

    /* Push id attributes into base mesh customdata layout. */
    BKE_sculptsession_update_attr_refs(ob);
    BKE_sculptsession_sync_attributes(ob, static_cast<Mesh *>(ob->data), true);
  }
  else {
    if (BM_idmap_check_attributes(ob->sculpt->bm_idmap)) {
      BKE_sculptsession_update_attr_refs(ob);
      BKE_sculptsession_sync_attributes(ob, static_cast<Mesh *>(ob->data), true);
    }
  }
}

void BKE_sculpt_distort_correction_set(Object *ob, eAttrCorrectMode value)
{
  ob->sculpt->distort_correction_mode = value;

  if (ob->sculpt->pbvh) {
    BKE_pbvh_distort_correction_set(ob->sculpt->pbvh, value);
  }
}

static void sculpt_check_face_areas(Object *ob, PBVH *pbvh)
{
  SculptSession *ss = ob->sculpt;

  if (!ss->attrs.face_areas) {
    SculptAttributeParams params = {0};

    params.nointerp = true;
    ss->attrs.face_areas = sculpt_attribute_ensure_ex(ob,
                                                      AttrDomain::Face,
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
      ob, AttrDomain::Point, CD_PROP_FLOAT3, SCULPT_ATTRIBUTE_NAME(persistent_co));
  ss->attrs.persistent_no = BKE_sculpt_attribute_get(
      ob, AttrDomain::Point, CD_PROP_FLOAT3, SCULPT_ATTRIBUTE_NAME(persistent_no));
  ss->attrs.persistent_disp = BKE_sculpt_attribute_get(
      ob, AttrDomain::Point, CD_PROP_FLOAT, SCULPT_ATTRIBUTE_NAME(persistent_disp));
}

static void sculpt_update_object(
    Depsgraph *depsgraph, Object *ob, Object *ob_eval, bool /*need_pmap*/, bool is_paint_tool)
{
  Scene *scene = DEG_get_input_scene(depsgraph);
  Sculpt *sd = scene->toolsettings->sculpt;
  UnifiedPaintSettings &ups = scene->toolsettings->unified_paint_settings;
  SculptSession *ss = ob->sculpt;
  Mesh *mesh = BKE_object_get_original_mesh(ob);
  Mesh *mesh_eval = BKE_object_get_evaluated_mesh(ob_eval);
  MultiresModifierData *mmd = sculpt_multires_modifier_get(scene, ob, true);
  const bool use_face_sets = (ob->mode & OB_MODE_SCULPT) != 0;

  BLI_assert(mesh_eval != nullptr);

  /* This is for handling a newly opened file with no object visible,
   * causing `mesh_eval == nullptr`. */
  if (mesh_eval == nullptr) {
    return;
  }

  Brush *brush = sd->paint.brush;
  ss->sharp_angle_limit = (!brush || ups.flag & UNIFIED_PAINT_FLAG_SHARP_ANGLE_LIMIT) ?
                              ups.sharp_angle_limit :
                              brush->sharp_angle_limit;
  ss->smooth_boundary_flag = eSculptBoundary(ups.smooth_boundary_flag);

  ss->depsgraph = depsgraph;

  ss->distort_correction_mode = eAttrCorrectMode(ups.distort_correction_mode);

  ss->deform_modifiers_active = sculpt_modifiers_active(scene, sd, ob);

  ss->building_vp_handle = false;

  ss->scene = scene;

  ss->shapekey_active = (mmd == nullptr) ? BKE_keyblock_from_object(ob) : nullptr;

  ss->material_index = (int *)CustomData_get_layer_named(
      &mesh->face_data, CD_PROP_INT32, "material_index");

  /* NOTE: Weight pPaint require mesh info for loop lookup, but it never uses multires code path,
   * so no extra checks is needed here. */
  if (mmd) {
    ss->multires.active = true;
    ss->multires.modifier = mmd;
    ss->multires.level = mmd->sculptlvl;
    ss->totvert = mesh->verts_num;
    ss->totloops = mesh->corners_num;
    ss->totedges = mesh->edges_num;
    ss->faces_num = mesh->faces_num;
    ss->totfaces = mesh->faces_num;

    /* These are assigned to the base mesh in Multires. This is needed because Face Sets
     * operators and tools use the Face Sets data from the base mesh when Multires is active. */
    ss->vert_positions = mesh->vert_positions_for_write();
    ss->faces = mesh->faces();
    ss->edges = mesh->edges();
    ss->corner_verts = mesh->corner_verts();
    ss->corner_edges = mesh->corner_edges();
  }
  else {
    ss->totvert = mesh->verts_num;
    ss->faces_num = mesh->faces_num;
    ss->totfaces = mesh->faces_num;
    ss->totloops = mesh->corners_num;
    ss->totedges = mesh->edges_num;

    ss->vert_positions = mesh->vert_positions_for_write();
    ss->edges = mesh->edges();
    ss->faces = mesh->faces();

    ss->vert_positions = mesh->vert_positions_for_write();

    ss->sharp_edge = (bool *)CustomData_get_layer_named_for_write(
        &mesh->edge_data, CD_PROP_BOOL, "sharp_edge", mesh->edges_num);
    ss->seam_edge = (bool *)CustomData_get_layer_named_for_write(
        &mesh->edge_data, CD_PROP_BOOL, ".uv_seam", mesh->edges_num);

    ss->vdata = &mesh->vert_data;
    ss->edata = &mesh->edge_data;
    ss->ldata = &mesh->corner_data;
    ss->pdata = &mesh->face_data;

    ss->corner_verts = mesh->corner_verts();
    ss->corner_edges = mesh->corner_edges();

    ss->multires.active = false;
    ss->multires.modifier = nullptr;
    ss->multires.level = 0;

    CustomDataLayer *layer;
    AttrDomain domain;

    if (BKE_pbvh_get_color_layer(nullptr, mesh, &layer, &domain)) {
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
      ss->vcol_domain = AttrDomain::Point;
    }
  }

  CustomData *ldata;
  if (ss->bm) {
    ldata = &ss->bm->ldata;
  }
  else {
    ldata = &mesh->corner_data;
  }

  ss->totuv = 0;
  for (int i : IndexRange(ldata->totlayer)) {
    CustomDataLayer &layer = ldata->layers[i];
    if (layer.type == CD_PROP_FLOAT2 && !(layer.flag & CD_FLAG_TEMPORARY)) {
      ss->totuv++;
    }
  }

  ss->hide_poly = (bool *)CustomData_get_layer_named_for_write(
      &mesh->face_data, CD_PROP_BOOL, ".hide_poly", mesh->faces_num);

  ss->subdiv_ccg = mesh_eval->runtime->subdiv_ccg.get();

  PBVH *pbvh = BKE_sculpt_object_pbvh_ensure(depsgraph, ob);
  sculpt_check_face_areas(ob, pbvh);

  if (ss->bm) {
    ss->totedges = ss->bm->totedge;
  }

  if (pbvh) {
    blender::bke::pbvh::sharp_limit_set(pbvh, ss->sharp_angle_limit);
  }

  /* Sculpt Face Sets. */
  if (use_face_sets) {
    int *face_sets = static_cast<int *>(CustomData_get_layer_named_for_write(
        &mesh->face_data, CD_PROP_INT32, ".sculpt_face_set", mesh->faces_num));

    if (face_sets) {
      /* Load into sculpt attribute system. */
      ss->face_sets = BKE_sculpt_face_sets_ensure(ob);
    }
    else {
      ss->face_sets = nullptr;
    }
  }
  else {
    ss->face_sets = nullptr;
  }

  sculpt_boundary_flags_ensure(ob, pbvh, BKE_sculptsession_vertex_count(ss), ss->totedges);

  BKE_pbvh_update_active_vcol(pbvh, mesh);

  if (BKE_pbvh_type(pbvh) == PBVH_FACES) {
    ss->poly_normals = blender::bke::pbvh::get_poly_normals(ss->pbvh);
  }
  else {
    ss->poly_normals = {};
  }

  ss->subdiv_ccg = mesh_eval->runtime->subdiv_ccg.get();

  BLI_assert(pbvh == ss->pbvh);
  UNUSED_VARS_NDEBUG(pbvh);

  if (ss->subdiv_ccg) {
    BKE_pbvh_subdiv_ccg_set(ss->pbvh, ss->subdiv_ccg);
  }

  BKE_pbvh_update_hide_attributes_from_mesh(ss->pbvh);

  sculpt_attribute_update_refs(ob);
  sculpt_update_persistent_base(ob);

  if (ob->type == OB_MESH) {
    ss->vert_to_face_map = mesh->vert_to_face_map();
  }

  if (ss->pbvh) {
    blender::bke::pbvh::set_pmap(ss->pbvh, ss->vert_to_face_map);
  }

  if (ss->deform_modifiers_active) {
    /* Painting doesn't need crazyspace, use already evaluated mesh coordinates if possible. */
    bool used_me_eval = false;

    if (ob->mode & (OB_MODE_VERTEX_PAINT | OB_MODE_WEIGHT_PAINT)) {
      Mesh *me_eval_deform = ob_eval->runtime->mesh_deform_eval;

      /* If the fully evaluated mesh has the same topology as the deform-only version, use it.
       * This matters because crazyspace evaluation is very restrictive and excludes even
       * modifiers
       * that simply recompute vertex weights (which can even include Geometry Nodes). */
      if (me_eval_deform->faces_num == mesh_eval->faces_num &&
          me_eval_deform->corners_num == mesh_eval->corners_num &&
          me_eval_deform->verts_num == mesh_eval->verts_num)
      {
        BKE_sculptsession_free_deformMats(ss);

        BLI_assert(me_eval_deform->totvert == mesh->verts_num);

        ss->deform_cos = mesh_eval->vert_positions();
        BKE_pbvh_vert_coords_apply(ss->pbvh, ss->deform_cos);

        used_me_eval = true;
      }
    }

    if (ss->orig_cos.is_empty() && !used_me_eval) {
      BKE_sculptsession_free_deformMats(ss);

      ss->orig_cos = (ss->shapekey_active) ?
                         Span(static_cast<const float3 *>(ss->shapekey_active->data),
                              mesh->verts_num) :
                         mesh->vert_positions();

      BKE_crazyspace_build_sculpt(depsgraph, scene, ob, ss->deform_imats, ss->deform_cos);
      BKE_pbvh_vert_coords_apply(ss->pbvh, ss->deform_cos);

      int a = 0;
      for (blender::float3x3 &matrix : ss->deform_imats) {
        float *co = ss->deform_cos[a];

        matrix = blender::math::invert(matrix);

        float ff = dot_v3v3(co, co);
        if (isnan(ff) || !isfinite(ff)) {
          printf("%s: nan1! %.4f %.4f %.4f\n", __func__, co[0], co[1], co[2]);
        }

        ff = blender::math::determinant(matrix);
        if (isnan(ff) || !isfinite(ff)) {
          printf("%s: nan2!\n", __func__);
        }

        a++;
      }
    }
  }
  else {
    BKE_sculptsession_free_deformMats(ss);
  }

  if (ss->shapekey_active != nullptr && ss->deform_cos.is_empty()) {
    ss->deform_cos = Span(static_cast<const float3 *>(ss->shapekey_active->data), mesh->verts_num);
  }

  /* if pbvh is deformed, key block is already applied to it */
  if (ss->shapekey_active) {
    bool pbvh_deformed = BKE_pbvh_is_deformed(ss->pbvh);
    if (!pbvh_deformed || ss->deform_cos.is_empty()) {
      const Span key_data(static_cast<const float3 *>(ss->shapekey_active->data), mesh->verts_num);

      if (key_data.data() != nullptr) {
        if (!pbvh_deformed) {
          /* apply shape keys coordinates to PBVH */
          BKE_pbvh_vert_coords_apply(ss->pbvh, key_data);
        }
        if (ss->deform_cos.is_empty()) {
          ss->deform_cos = key_data;
        }
      }
    }
  }

  int totvert = 0;

  switch (BKE_pbvh_type(pbvh)) {
    case PBVH_FACES:
      totvert = mesh->verts_num;
      break;
    case PBVH_BMESH:
      totvert = ss->bm ? ss->bm->totvert : mesh->verts_num;
      break;
    case PBVH_GRIDS:
      totvert = BKE_pbvh_get_grid_num_verts(ss->pbvh);
      break;
  }

  BKE_sculpt_init_flags_valence(ob, pbvh, totvert, false);

  if (ss->bm && mesh->key && ob->shapenr != ss->bm->shapenr) {
    KeyBlock *actkey = static_cast<KeyBlock *>(
        BLI_findlink(&mesh->key->block, ss->bm->shapenr - 1));
    KeyBlock *newkey = static_cast<KeyBlock *>(BLI_findlink(&mesh->key->block, ob->shapenr - 1));

    bool updatePBVH = false;

    if (!actkey) {
      printf("%s: failed to find active shapekey\n", __func__);
      if (!ss->bm->shapenr || !CustomData_has_layer(&ss->bm->vdata, CD_SHAPEKEY)) {
        printf("allocating shapekeys. . .\n");

        // need to allocate customdata for keys
        for (KeyBlock *key = (KeyBlock *)mesh->key->block.first; key; key = key->next) {

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
  }

  if (ss->bm_log && ss->pbvh) {
    bke::pbvh::set_idmap(ss->pbvh, ss->bm_idmap);
    BKE_pbvh_set_bm_log(ss->pbvh, ss->bm_log);
  }

  if (is_paint_tool) {
    if (ss->vcol_domain == AttrDomain::Corner) {
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
          !STREQ(paint_canvas_key, ss->last_paint_canvas_key))
      {
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
      Vector<PBVHNode *> nodes = blender::bke::pbvh::search_gather(ss->pbvh, {});

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

  if (ob_orig->sculpt) {
    BKE_sculptsession_sync_attributes(ob_orig, me_orig, false);
  }

  sculpt_update_object(depsgraph, ob_orig, ob_eval, false, false);
}

void BKE_sculpt_color_layer_create_if_needed(Object *object)
{
  using namespace blender;
  using namespace blender::bke;
  Mesh *orig_me = BKE_object_get_original_mesh(object);

  SculptAttribute attr = BKE_sculpt_find_attribute(object, orig_me->active_color_attribute);
  if (!attr.is_empty() && (CD_TYPE_AS_MASK(attr.proptype) & CD_MASK_COLOR_ALL) &&
      ELEM(attr.domain, AttrDomain::Point, AttrDomain::Corner))
  {
    return;
  }

  const std::string unique_name = BKE_id_attribute_calc_unique_name(orig_me->id, "Color");
  if (!orig_me->attributes_for_write().add(
          unique_name, AttrDomain::Point, CD_PROP_COLOR, AttributeInitDefaultValue()))
  {
    return;
  }

  BKE_id_attributes_active_color_set(&orig_me->id, unique_name.c_str());
  BKE_id_attributes_default_color_set(&orig_me->id, unique_name.c_str());
  DEG_id_tag_update(&orig_me->id, ID_RECALC_GEOMETRY_ALL_MODES);
  BKE_mesh_tessface_clear(orig_me);

  if (object->sculpt && object->sculpt->pbvh) {
    BKE_pbvh_update_active_vcol(object->sculpt->pbvh, orig_me);
  }

  /* Flush attribute into sculpt mesh. */
  BKE_sculptsession_sync_attributes(object, orig_me, false);
}

void BKE_sculpt_update_object_for_edit(Depsgraph *depsgraph, Object *ob_orig, bool is_paint_tool)
{
  /* Update from sculpt operators and undo, to update sculpt session
   * and PBVH after edits. */
  Object *ob_eval = DEG_get_evaluated_object(depsgraph, ob_orig);

  sculpt_update_object(depsgraph, ob_orig, ob_eval, true, is_paint_tool);
}

int *BKE_sculpt_face_sets_ensure(Object *ob)
{
  SculptSession *ss = ob->sculpt;

  if (!ss->attrs.face_set) {
    SculptAttributeParams params = {};
    params.permanent = true;

    CustomData *cdata = sculpt_get_cdata(ob, AttrDomain::Face);
    bool clear = CustomData_get_named_layer_index(
                     cdata, CD_PROP_INT32, SCULPT_ATTRIBUTE_NAME(face_set)) == -1;

    ss->attrs.face_set = BKE_sculpt_attribute_ensure(
        ob, AttrDomain::Face, CD_PROP_INT32, SCULPT_ATTRIBUTE_NAME(face_set), &params);

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

  int *face_sets = static_cast<int *>(ss->attrs.face_set->data);
  ss->face_sets = face_sets;

  return face_sets;
}

bool *BKE_sculpt_hide_poly_ensure(Object *ob)
{
  SculptAttributeParams params = {0};
  params.permanent = true;

  ob->sculpt->attrs.hide_poly = BKE_sculpt_attribute_ensure(
      ob, AttrDomain::Face, CD_PROP_BOOL, ".hide_poly", &params);

  bool *hide_poly = static_cast<bool *>(ob->sculpt->attrs.hide_poly->data);
  ob->sculpt->hide_poly = hide_poly;

  return hide_poly;
}

void BKE_sculpt_hide_poly_pointer_update(Object &object)
{
  Mesh &mesh = *static_cast<Mesh *>(object.data);
  object.sculpt->hide_poly = static_cast<bool *>(CustomData_get_layer_named_for_write(
      &mesh.face_data, CD_PROP_BOOL, ".hide_poly", mesh.faces_num));
}

void BKE_sculpt_mask_layers_ensure(Depsgraph *depsgraph,
                                   Main *bmain,
                                   Object *ob,
                                   MultiresModifierData *mmd)
{
  using namespace blender;
  using namespace blender::bke;
  Mesh *me = static_cast<Mesh *>(ob->data);
  const OffsetIndices faces = me->faces();
  const Span<int> corner_verts = me->corner_verts();
  MutableAttributeAccessor attributes = me->attributes_for_write();

  /* if multires is active, create a grid paint mask layer if there
   * isn't one already */
  if (mmd && !CustomData_has_layer(&me->corner_data, CD_GRID_PAINT_MASK)) {
    int level = max_ii(1, mmd->sculptlvl);
    int gridsize = BKE_ccg_gridsize(level);
    int gridarea = gridsize * gridsize;

    GridPaintMask *gmask = static_cast<GridPaintMask *>(CustomData_add_layer(
        &me->corner_data, CD_GRID_PAINT_MASK, CD_SET_DEFAULT, me->corners_num));

    for (int i = 0; i < me->corners_num; i++) {
      GridPaintMask *gpm = &gmask[i];

      gpm->level = level;
      gpm->data = static_cast<float *>(
          MEM_callocN(sizeof(float) * gridarea, "GridPaintMask.data"));
    }

    /* If vertices already have mask, copy into multires data. */
    if (const VArray<float> mask = *attributes.lookup<float>(".sculpt_mask", AttrDomain::Point)) {
      const VArraySpan<float> mask_span(mask);
      for (const int i : faces.index_range()) {
        const IndexRange face = faces[i];

        /* Mask center. */
        float avg = 0.0f;
        for (const int vert : corner_verts.slice(face)) {
          avg += mask_span[vert];
        }
        avg /= float(face.size());

        /* Fill in multires mask corner. */
        for (const int corner : face) {
          GridPaintMask *gpm = &gmask[corner];
          const int vert = corner_verts[corner];
          const int prev = corner_verts[mesh::face_corner_prev(face, vert)];
          const int next = corner_verts[mesh::face_corner_next(face, vert)];

          gpm->data[0] = avg;
          gpm->data[1] = (mask_span[vert] + mask_span[next]) * 0.5f;
          gpm->data[2] = (mask_span[vert] + mask_span[prev]) * 0.5f;
          gpm->data[3] = mask_span[vert];
        }
      }
    }
    /* The evaluated multires CCG must be updated to contain the new data. */
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    if (depsgraph) {
      BKE_scene_graph_evaluated_ensure(depsgraph, bmain);
    }
  }

  /* Create vertex paint mask layer if there isn't one already. */
  if (attributes.add<float>(".sculpt_mask", AttrDomain::Point, AttributeInitDefaultValue())) {
    /* The evaluated mesh must be updated to contain the new data. */
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  }

  if (ob->sculpt) {
    BKE_sculptsession_update_attr_refs(ob);
  }
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

  if (sd->dyntopo.detail_percent == 0.0f) {
    sd->dyntopo.detail_percent = defaults->detail_percent;
  }
  if (sd->dyntopo.constant_detail == 0.0f) {
    sd->dyntopo.constant_detail = defaults->constant_detail;
  }
  if (sd->dyntopo.detail_size == 0.0f) {
    sd->dyntopo.detail_size = defaults->detail_size;
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
      ".hide_poly", AttrDomain::Face, false);
  if (hide_poly.is_single() && !hide_poly.get_internal_single()) {
    BKE_subdiv_ccg_grid_hidden_free(*subdiv_ccg);
    return;
  }

  const OffsetIndices<int> faces = mesh->faces();

  const VArraySpan<bool> hide_poly_span(hide_poly);
  BitGroupVector<> &grid_hidden = BKE_subdiv_ccg_grid_hidden_ensure(*subdiv_ccg);
  threading::parallel_for(faces.index_range(), 1024, [&](const IndexRange range) {
    for (const int i : range) {
      const bool face_hidden = hide_poly_span[i];
      for (const int corner : faces[i]) {
        grid_hidden[corner].set_all(face_hidden);
      }
    }
  });
}

static PBVH *build_pbvh_for_dynamic_topology(Object *ob, bool update_flags_valence)
{
  SculptSession *ss = ob->sculpt;
  PBVH *pbvh = ss->pbvh = BKE_pbvh_new(PBVH_BMESH);

  BKE_pbvh_set_bmesh(pbvh, ss->bm);
  BM_mesh_elem_table_ensure(ss->bm, BM_VERT | BM_EDGE | BM_FACE);

  sculptsession_bmesh_add_layers(ob);
  sculpt_boundary_flags_ensure(ob, pbvh, ss->bm->totvert, ss->bm->totedge);
  BKE_sculpt_ensure_sculpt_layers(ob);

  if (update_flags_valence) {
    BKE_sculpt_init_flags_valence(ob, ss->pbvh, ss->bm->totvert, true);
  }

  BKE_sculptsession_update_attr_refs(ob);

  BKE_sculpt_ensure_origco(ob);
  sculpt_check_face_areas(ob, pbvh);

  BKE_sculpt_ensure_idmap(ob);
  blender::bke::pbvh::sharp_limit_set(pbvh, ss->sharp_angle_limit);

  bke::pbvh::build_bmesh(pbvh,
                         BKE_object_get_original_mesh(ob),
                         ss->bm,
                         ss->bm_log,
                         ss->bm_idmap,
                         ss->attrs.dyntopo_node_id_vertex->bmesh_cd_offset,
                         ss->attrs.dyntopo_node_id_face->bmesh_cd_offset,
                         ss->attrs.face_areas->bmesh_cd_offset,
                         ss->attrs.boundary_flags->bmesh_cd_offset,
                         ss->attrs.edge_boundary_flags->bmesh_cd_offset,
                         ss->attrs.flags ? ss->attrs.flags->bmesh_cd_offset : -1,
                         ss->attrs.valence ? ss->attrs.valence->bmesh_cd_offset : -1,
                         ss->attrs.orig_co ? ss->attrs.orig_co->bmesh_cd_offset : -1,
                         ss->attrs.orig_no ? ss->attrs.orig_no->bmesh_cd_offset : -1);

  if (ss->bm_log) {
    BKE_pbvh_set_bm_log(pbvh, ss->bm_log);
  }

  return pbvh;
}

static PBVH *build_pbvh_from_regular_mesh(Object *ob, Mesh *me_eval_deform)
{
  SculptSession *ss = ob->sculpt;
  Mesh *me = BKE_object_get_original_mesh(ob);

  if (ss->vert_to_face_map.is_empty()) {
    ss->vert_to_face_map = me->vert_to_face_map();
  }

  PBVH *pbvh = ob->sculpt->pbvh = bke::pbvh::build_mesh(me);

  BKE_sculptsession_update_attr_refs(ob);

  blender::bke::pbvh::set_pmap(ss->pbvh, ss->vert_to_face_map);
  BKE_sculpt_ensure_sculpt_layers(ob);
  BKE_sculpt_init_flags_valence(ob, pbvh, me->verts_num, true);
  BKE_sculpt_ensure_origco(ob);

  Mesh *mesh = static_cast<Mesh *>(ob->data);
  Span<float3> positions = mesh->vert_positions();
  Span<float3> normals = mesh->vert_normals();

  for (int i = 0; i < mesh->verts_num; i++) {
    blender::bke::paint::vertex_attr_set<float3>({i}, ss->attrs.orig_co, positions[i]);
    blender::bke::paint::vertex_attr_set<float3>({i}, ss->attrs.orig_no, normals[i]);
  }

  sculpt_check_face_areas(ob, pbvh);
  BKE_sculptsession_update_attr_refs(ob);

  blender::bke::sculpt::sculpt_vert_boundary_ensure(ob);

  blender::bke::pbvh::sharp_limit_set(pbvh, ss->sharp_angle_limit);

  const bool is_deformed = check_sculpt_object_deformed(ob, true);
  if (is_deformed && me_eval_deform != nullptr) {
    BKE_pbvh_vert_coords_apply(pbvh, me_eval_deform->vert_positions());
  }

  return pbvh;
}

static PBVH *build_pbvh_from_ccg(Object *ob, SubdivCCG *subdiv_ccg)
{
  SculptSession *ss = ob->sculpt;

  CCGKey key = BKE_subdiv_ccg_key_top_level(*subdiv_ccg);
  PBVH *pbvh = ob->sculpt->pbvh = BKE_pbvh_new(PBVH_GRIDS);

  Mesh *base_mesh = BKE_mesh_from_object(ob);

  BKE_sculpt_sync_face_visibility_to_grids(base_mesh, subdiv_ccg);

  BKE_sculptsession_update_attr_refs(ob);
  sculpt_check_face_areas(ob, pbvh);
  blender::bke::sculpt::sculpt_vert_boundary_ensure(ob);

  blender::bke::pbvh::build_grids(&key, base_mesh, subdiv_ccg);
  blender::bke::pbvh::sharp_limit_set(pbvh, ss->sharp_angle_limit);

  if (ss->vert_to_face_map.is_empty()) {
    ss->vert_to_face_map = base_mesh->vert_to_face_map();
  }

  blender::bke::pbvh::set_pmap(ss->pbvh, ss->vert_to_face_map);
  int totvert = BKE_pbvh_get_grid_num_verts(pbvh);
  BKE_sculpt_init_flags_valence(ob, pbvh, totvert, true);

  BKE_subdiv_ccg_start_face_grid_index_ensure(*ss->subdiv_ccg);

  return pbvh;
}

bool BKE_sculpt_init_flags_valence(Object *ob, struct PBVH *pbvh, int totvert, bool reset_flags)
{
  SculptSession *ss = ob->sculpt;

  if (!ss->attrs.flags) {
    BKE_sculpt_ensure_sculpt_layers(ob);

    reset_flags = true;
  }

  BKE_sculpt_ensure_origco(ob);
  sculpt_boundary_flags_ensure(ob, pbvh, totvert, ss->totedges);
  BKE_sculptsession_update_attr_refs(ob);

  if (reset_flags) {
    if (ss->bm) {
      int cd_flags = ss->attrs.flags->bmesh_cd_offset;
      BMVert *v;
      BMIter iter;

      BM_ITER_MESH (v, &iter, ss->bm, BM_VERTS_OF_MESH) {
        *BM_ELEM_CD_PTR<uint8_t *>(v, cd_flags) = SCULPTFLAG_NEED_VALENCE |
                                                  SCULPTFLAG_NEED_TRIANGULATE;
      }
    }
    else {
      uint8_t *flags = static_cast<uint8_t *>(ss->attrs.flags->data);

      for (int i = 0; i < totvert; i++) {
        flags[i] = SCULPTFLAG_NEED_VALENCE | SCULPTFLAG_NEED_TRIANGULATE;
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
  if (ob->sculpt == nullptr) {
    return nullptr;
  }

  SculptSession *ss = ob->sculpt;

  PBVH *pbvh = ss->pbvh;
  if (pbvh != nullptr) {
    blender::bke::pbvh::sharp_limit_set(pbvh, ss->sharp_angle_limit);

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
        SubdivCCG *subdiv_ccg = mesh_eval->runtime->subdiv_ccg.get();
        if (subdiv_ccg != nullptr) {
          BKE_sculpt_bvh_update_from_ccg(pbvh, subdiv_ccg);
        }
        break;
      }
      case PBVH_BMESH: {
        break;
      }
    }

    BKE_sculptsession_sync_attributes(ob, BKE_object_get_original_mesh(ob), false);
    BKE_pbvh_update_active_vcol(pbvh, BKE_object_get_original_mesh(ob));
    blender::bke::pbvh::set_pmap(pbvh, ob->sculpt->vert_to_face_map);

    return pbvh;
  }

  ss->islands_valid = false;

  if (ss->bm != nullptr) {
    /* Sculpting on a BMesh (dynamic-topology) gets a special PBVH. */
    pbvh = ss->pbvh = build_pbvh_for_dynamic_topology(ob, false);
  }
  else {
    /* Detect if we are loading from an undo memfile step. */
    Mesh *mesh_orig = BKE_object_get_original_mesh(ob);
    bool is_dyntopo = (mesh_orig->flag & ME_SCULPT_DYNAMIC_TOPOLOGY) &&
                      ss->mode_type == OB_MODE_SCULPT;

    if (is_dyntopo) {
      BMesh *bm = BKE_sculptsession_empty_bmesh_create();

      BMeshFromMeshParams params = {0};
      params.calc_face_normal = true;
      params.use_shapekey = true;
      params.active_shapekey = ob->shapenr;
      params.copy_temp_cdlayers = true;

      BM_mesh_bm_from_me(bm, mesh_orig, &params);
      BM_mesh_elem_table_ensure(bm, BM_VERT | BM_EDGE | BM_FACE);

      ss->bm = bm;

      BKE_sculpt_ensure_idmap(ob);
      blender::ed::sculpt_paint::undo::ensure_bmlog(ob);

      pbvh = ss->pbvh = build_pbvh_for_dynamic_topology(ob, true);

      if (!CustomData_has_layer_named(&ss->bm->vdata, CD_PROP_FLOAT, ".sculpt_mask")) {
        BM_data_layer_add_named(ss->bm, &ss->bm->vdata, CD_PROP_FLOAT, ".sculpt_mask");
        BKE_sculptsession_update_attr_refs(ob);
      }

      BKE_sculpt_ensure_origco(ob);
      BKE_sculpt_ensure_sculpt_layers(ob);

      BKE_sculpt_init_flags_valence(ob, pbvh, bm->totvert, true);
      blender::bke::paint::load_all_original(ob);
    }
    else {
      Object *object_eval = DEG_get_evaluated_object(depsgraph, ob);
      Mesh *mesh_eval = static_cast<Mesh *>(object_eval->data);
      if (mesh_eval->runtime->subdiv_ccg != nullptr) {
        pbvh = build_pbvh_from_ccg(ob, mesh_eval->runtime->subdiv_ccg.get());
      }
      else if (ob->type == OB_MESH) {
        Mesh *me_eval_deform = object_eval->runtime->mesh_deform_eval;
        pbvh = build_pbvh_from_regular_mesh(ob, me_eval_deform);
      }
    }
  }

  ss->pbvh = pbvh;
  blender::bke::pbvh::set_pmap(pbvh, ss->vert_to_face_map);

  sculpt_attribute_update_refs(ob);

  /* Forcibly flag all boundaries for update. */
  sculpt_boundary_flags_ensure(
      ob, pbvh, BKE_sculptsession_vertex_count(ss), ss->bm ? ss->bm->totedge : ss->totedges, true);

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

void BKE_sculpt_bvh_update_from_ccg(PBVH *pbvh, SubdivCCG *subdiv_ccg)
{
  const CCGKey key = BKE_subdiv_ccg_key_top_level(*subdiv_ccg);
  BKE_pbvh_grids_update(pbvh, &key);
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

static bool sculpt_attribute_stored_in_bmesh_builtin(const StringRef name)
{
  return BM_attribute_stored_in_bmesh_builtin(name);
}

static bool sync_ignore_layer(CustomDataLayer *layer)
{
  int badmask = CD_MASK_ORIGINDEX | CD_MASK_ORIGSPACE | CD_MASK_MFACE;

  bool bad = sculpt_attribute_stored_in_bmesh_builtin(layer->name);
  bad = bad || ((1 << layer->type) & badmask);
  bad = bad || (layer->flag & (CD_FLAG_TEMPORARY | CD_FLAG_NOCOPY));

  return bad;
}
/**
  Syncs customdata layers with internal bmesh, but ignores deleted layers.
*/
static void get_synced_attributes(CustomData *src_data,
                                  CustomData *dst_data,
                                  Vector<CustomDataLayer> &r_new,
                                  Vector<CustomDataLayer> &r_kill)
{
  for (int i : IndexRange(src_data->totlayer)) {
    CustomDataLayer *src_layer = src_data->layers + i;

    if (sync_ignore_layer(src_layer)) {
      continue;
    }

    if (CustomData_get_named_layer_index(
            dst_data, eCustomDataType(src_layer->type), src_layer->name) == -1)
    {
      r_new.append(*src_layer);
    }
  }

  for (int i : IndexRange(dst_data->totlayer)) {
    CustomDataLayer *dst_layer = dst_data->layers + i;

    if (sync_ignore_layer(dst_layer)) {
      continue;
    }

    if (CustomData_get_named_layer_index(
            src_data, eCustomDataType(dst_layer->type), dst_layer->name) == -1)
    {
      r_kill.append(*dst_layer);
    }
  }
}

static bool sync_attribute_actives(CustomData *src_data, CustomData *dst_data)
{
  bool modified = false;

  bool donemap[CD_NUMTYPES] = {0};

  for (int i : IndexRange(src_data->totlayer)) {
    CustomDataLayer *src_layer = src_data->layers + i;
    eCustomDataType type = eCustomDataType(src_layer->type);

    if (sync_ignore_layer(src_layer) || donemap[int(type)]) {
      continue;
    }

    /* Only do first layers of each type, active refs will be propagated to
     * the other ones later.
     */
    donemap[src_layer->type] = true;

    /* Find first layer of type. */
    int baseidx = CustomData_get_layer_index(dst_data, type);

    if (baseidx < 0) {
      modified |= true;
      continue;
    }

    CustomDataLayer *dst_layer = dst_data->layers + baseidx;

    int idx = CustomData_get_named_layer_index(dst_data, type, src_layer[src_layer->active].name);
    if (idx >= 0) {
      modified |= idx - baseidx != dst_layer->active;
      dst_layer->active = idx - baseidx;
    }
    else {
      modified |= dst_layer->active != 0;
      dst_layer->active = 0;
    }

    idx = CustomData_get_named_layer_index(dst_data, type, src_layer[src_layer->active_rnd].name);
    if (idx >= 0) {
      modified |= idx - baseidx != dst_layer->active_rnd;
      dst_layer->active_rnd = idx - baseidx;
    }
    else {
      modified |= dst_layer->active_rnd != 0;
      dst_layer->active_rnd = 0;
    }

    idx = CustomData_get_named_layer_index(dst_data, type, src_layer[src_layer->active_mask].name);
    if (idx >= 0) {
      modified |= idx - baseidx != dst_layer->active_mask;
      dst_layer->active_mask = idx - baseidx;
    }
    else {
      modified |= dst_layer->active_mask != 0;
      dst_layer->active_mask = 0;
    }

    idx = CustomData_get_named_layer_index(
        dst_data, type, src_layer[src_layer->active_clone].name);
    if (idx >= 0) {
      modified |= idx - baseidx != dst_layer->active_clone;
      dst_layer->active_clone = idx - baseidx;
    }
    else {
      modified |= dst_layer->active_clone != 0;
      dst_layer->active_clone = 0;
    }
  }

  if (modified) {
    CustomDataLayer *base_layer = dst_data->layers;

    for (int i = 0; i < dst_data->totlayer; i++) {
      CustomDataLayer *dst_layer = dst_data->layers;

      if (dst_layer->type != base_layer->type) {
        base_layer = dst_layer;
      }

      dst_layer->active = base_layer->active;
      dst_layer->active_clone = base_layer->active_clone;
      dst_layer->active_mask = base_layer->active_mask;
      dst_layer->active_rnd = base_layer->active_rnd;
    }
  }

  return modified;
}

void BKE_sculptsession_sync_attributes(struct Object *ob, struct Mesh *me, bool load_to_mesh)
{
  SculptSession *ss = ob->sculpt;

  if (!ss) {
    return;
  }
  else if (!ss->bm) {
    if (!load_to_mesh) {
      BKE_sculptsession_update_attr_refs(ob);
    }
    return;
  }

  bool modified = false;

  BMesh *bm = ss->bm;

  CustomData *cdme[4] = {&me->vert_data, &me->edge_data, &me->corner_data, &me->face_data};
  CustomData *cdbm[4] = {&bm->vdata, &bm->edata, &bm->ldata, &bm->pdata};

  if (!load_to_mesh) {
    for (int i = 0; i < 4; i++) {
      Vector<CustomDataLayer> new_layers, kill_layers;

      get_synced_attributes(cdme[i], cdbm[i], new_layers, kill_layers);

      for (CustomDataLayer &layer : kill_layers) {
        BM_data_layer_free_named(bm, cdbm[i], layer.name);
        modified = true;
      }

      Vector<BMCustomLayerReq> new_bm_layers;
      for (CustomDataLayer &layer : new_layers) {
        new_bm_layers.append({layer.type, layer.name, layer.flag});
      }

      BM_data_layers_ensure(bm, cdbm[i], new_bm_layers.data(), new_bm_layers.size());

      modified |= new_bm_layers.size() > 0;
      modified |= sync_attribute_actives(cdme[i], cdbm[i]);
    }
  }
  else {
    for (int i = 0; i < 4; i++) {
      Vector<CustomDataLayer> new_layers, kill_layers;

      get_synced_attributes(cdbm[i], cdme[i], new_layers, kill_layers);

      int totelem;
      switch (i) {
        case 0:
          totelem = me->verts_num;
          break;
        case 1:
          totelem = me->edges_num;
          break;
        case 2:
          totelem = me->corners_num;
          break;
        case 3:
          totelem = me->faces_num;
          break;
      }

      for (CustomDataLayer &layer : new_layers) {
        CustomData_add_layer_named(
            cdme[i], eCustomDataType(layer.type), CD_CONSTRUCT, totelem, layer.name);
        modified = true;
      }

      for (CustomDataLayer &layer : kill_layers) {
        CustomData_free_layer_named(cdme[i], layer.name, totelem);
        modified = true;
      }

      modified |= sync_attribute_actives(cdbm[i], cdme[i]);
    }

    if (me->default_color_attribute &&
        !BKE_id_attributes_color_find(&me->id, me->active_color_attribute))
    {
      MEM_SAFE_FREE(me->active_color_attribute);
    }
    if (me->default_color_attribute &&
        !BKE_id_attributes_color_find(&me->id, me->default_color_attribute))
    {
      MEM_SAFE_FREE(me->default_color_attribute);
    }
  }

  if (modified) {
    printf("%s: Attribute layout changed! %s\n",
           __func__,
           load_to_mesh ? "Loading to mesh" : "Loading from mesh");
  }

  if (!load_to_mesh) {
    BKE_sculptsession_update_attr_refs(ob);
  }
};

BMesh *BKE_sculptsession_empty_bmesh_create()
{
  BMAllocTemplate allocsize;

  allocsize.totvert = 2048 * 1;
  allocsize.totface = 2048 * 16;
  allocsize.totloop = 4196 * 16;
  allocsize.totedge = 2048 * 16;

  BMeshCreateParams params = {0};

  params.use_toolflags = false;

  BMesh *bm = BM_mesh_create(&allocsize, &params);

  return bm;
}

static int sculpt_attr_elem_count_get(Object *ob, AttrDomain domain)
{
  SculptSession *ss = ob->sculpt;

  switch (domain) {
    case AttrDomain::Point:
      /* Cannot rely on prescence of ss->pbvh. */

      if (ss->bm) {
        return ss->bm->totvert;
      }
      else if (ss->subdiv_ccg) {
        CCGKey key = BKE_subdiv_ccg_key_top_level(*ss->subdiv_ccg);
        return ss->subdiv_ccg->grids.size() * key.grid_area;
      }
      else {
        Mesh *me = BKE_object_get_original_mesh(ob);

        return me->verts_num;
      }
      break;
    case AttrDomain::Face:
      return ss->totfaces;
    case AttrDomain::Edge:
      return ss->bm ? ss->bm->totedge : BKE_object_get_original_mesh(ob)->edges_num;
    default:
      BLI_assert_unreachable();
      return 0;
  }
}

static bool sculpt_attribute_create(SculptSession *ss,
                                    Object *ob,
                                    AttrDomain domain,
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
  if (pbvhtype == PBVH_GRIDS && domain == AttrDomain::Point) {
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
        case AttrDomain::Point:
          cdata = &ss->bm->vdata;
          break;
        case AttrDomain::Edge:
          cdata = &ss->bm->edata;
          break;
        case AttrDomain::Face:
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

      if (!permanent) {
        cdata->layers[index].flag |= CD_FLAG_TEMPORARY | CD_FLAG_NOCOPY;
      }
      else {
        /* Push attribute into the base mesh. */
        BKE_sculptsession_sync_attributes(ob, static_cast<Mesh *>(ob->data), true);
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
        case AttrDomain::Point:
          cdata = &me->vert_data;
          break;
        case AttrDomain::Face:
          cdata = &me->face_data;
          break;
        case AttrDomain::Edge:
          cdata = &me->edge_data;
          break;
        default:
          out->used = false;
          return false;
      }

      if (CustomData_get_named_layer_index(cdata, proptype, name) == -1) {
        CustomData_add_layer_named(cdata, proptype, CD_SET_DEFAULT, totelem, name);
      }
      int index = CustomData_get_named_layer_index(cdata, proptype, name);

      if (!permanent) {
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

static bool sculpt_attr_update(Object *ob, SculptAttribute *attr)
{
  SculptSession *ss = ob->sculpt;
  int elem_num = sculpt_attr_elem_count_get(ob, attr->domain);

  bool bad = false;

  if (attr->data) {
    bad = attr->elem_num != elem_num;
  }

  /* Check if we are a coerced simple array and shouldn't be. */
  bad |= (attr->simple_array && !attr->params.simple_array) &&
         !(ss->pbvh && BKE_pbvh_type(ss->pbvh) == PBVH_GRIDS && attr->domain == AttrDomain::Point);

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
        attr->data = CustomData_get_layer_named_for_write(
            cdata, attr->proptype, attr->name, elem_num);
      }
    }

    if (layer_index != -1) {
      if (attr->params.nocopy) {
        cdata->layers[layer_index].flag |= CD_FLAG_ELEM_NOCOPY;
      }
      else {
        cdata->layers[layer_index].flag &= ~CD_FLAG_ELEM_NOCOPY;
      }

      if (attr->params.nointerp) {
        cdata->layers[layer_index].flag |= CD_FLAG_ELEM_NOINTERP;
      }
      else {
        cdata->layers[layer_index].flag &= ~CD_FLAG_ELEM_NOINTERP;
      }

      if (attr->params.permanent) {
        cdata->layers[layer_index].flag &= ~(CD_FLAG_TEMPORARY | CD_FLAG_NOCOPY);
      }
      else {
        cdata->layers[layer_index].flag |= CD_FLAG_TEMPORARY | CD_FLAG_NOCOPY;
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
                                                AttrDomain domain,
                                                eCustomDataType proptype,
                                                const char *name)
{
  for (int i = 0; i < SCULPT_MAX_ATTRIBUTES; i++) {
    SculptAttribute *attr = ss->temp_attributes + i;

    if (attr->used && STREQ(attr->name, name) && attr->proptype == proptype &&
        attr->domain == domain)
    {

      return attr;
    }
  }

  return nullptr;
}

bool BKE_sculpt_attribute_exists(Object *ob,
                                 AttrDomain domain,
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
                                          AttrDomain domain,
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

  if (!ss->pbvh || (BKE_pbvh_type(ss->pbvh) == PBVH_GRIDS && domain == AttrDomain::Point)) {
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

      if (ss->pbvh && (BKE_pbvh_type(ss->pbvh) == PBVH_FACES ||
                       (BKE_pbvh_type(ss->pbvh) == PBVH_GRIDS &&
                        ELEM(domain, AttrDomain::Face, AttrDomain::Edge))))
      {
        attr->data = CustomData_get_layer_named_for_write(
            cdata, attr->proptype, attr->name, totelem);
      }

      attr->params.nointerp = cdata->layers[index].flag & CD_FLAG_ELEM_NOINTERP;
      attr->params.nocopy = cdata->layers[index].flag & CD_FLAG_ELEM_NOCOPY;
      attr->params.permanent = !(cdata->layers[index].flag & CD_FLAG_TEMPORARY);
      attr->used = true;
      attr->domain = domain;
      attr->proptype = proptype;
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
                                                   AttrDomain domain,
                                                   eCustomDataType proptype,
                                                   const char *name,
                                                   const SculptAttributeParams *params,
                                                   PBVHType pbvhtype)
{
  SculptSession *ss = ob->sculpt;
  SculptAttribute *attr = BKE_sculpt_attribute_get(ob, domain, proptype, name);

  if (attr) {
    attr->params.nocopy = params->nocopy;
    attr->params.nointerp = params->nointerp;
    attr->params.permanent = params->permanent;

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
                                             AttrDomain domain,
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
    int cd_edge_boundary = ss->attrs.edge_boundary_flags ?
                               ss->attrs.edge_boundary_flags->bmesh_cd_offset :
                               -1;
    int cd_dyntopo_vert = ss->attrs.dyntopo_node_id_vertex ?
                              ss->attrs.dyntopo_node_id_vertex->bmesh_cd_offset :
                              -1;
    int cd_dyntopo_face = ss->attrs.dyntopo_node_id_face ?
                              ss->attrs.dyntopo_node_id_face->bmesh_cd_offset :
                              -1;
    int cd_flag = ss->attrs.flags ? ss->attrs.flags->bmesh_cd_offset : -1;
    int cd_valence = ss->attrs.valence ? ss->attrs.valence->bmesh_cd_offset : -1;

    bke::pbvh::set_idmap(ss->pbvh, ss->bm_idmap);
    bke::pbvh::update_offsets(ss->pbvh,
                              cd_dyntopo_vert,
                              cd_dyntopo_face,
                              cd_face_area,
                              cd_boundary_flags,
                              cd_edge_boundary,
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

  params.nocopy = true;
  params.nointerp = true;

  if (!ss->attrs.face_areas) {
    SculptAttributeParams params = {0};
    ss->attrs.face_areas = sculpt_attribute_ensure_ex(ob,
                                                      AttrDomain::Face,
                                                      CD_PROP_FLOAT2,
                                                      SCULPT_ATTRIBUTE_NAME(face_areas),
                                                      &params,
                                                      PBVH_BMESH);
  }

  if (!ss->attrs.dyntopo_node_id_vertex) {
    ss->attrs.dyntopo_node_id_vertex = sculpt_attribute_ensure_ex(
        ob,
        AttrDomain::Point,
        CD_PROP_INT32,
        SCULPT_ATTRIBUTE_NAME(dyntopo_node_id_vertex),
        &params,
        PBVH_BMESH);
  }

  if (!ss->attrs.dyntopo_node_id_face) {
    ss->attrs.dyntopo_node_id_face = sculpt_attribute_ensure_ex(
        ob,
        AttrDomain::Face,
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
    case AttrDomain::Point:
      itertype = BM_VERTS_OF_MESH;
      break;
    case AttrDomain::Edge:
      itertype = BM_EDGES_OF_MESH;
      break;
    case AttrDomain::Face:
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
        if (attr->domain == AttrDomain::Point) {
          sculpt_clear_attribute_bmesh<BMVert>(ss->bm, attr);
        }
        else if (attr->domain == AttrDomain::Edge) {
          sculpt_clear_attribute_bmesh<BMEdge>(ss->bm, attr);
        }
        else if (attr->domain == AttrDomain::Face) {
          sculpt_clear_attribute_bmesh<BMFace>(ss->bm, attr);
        }
        continue;
      }

      BKE_sculpt_attribute_destroy(ob, attr);
    }
  }
}

static void update_bmesh_offsets(Object *ob)
{
  Mesh *me = BKE_object_get_original_mesh(ob);
  SculptSession *ss = ob->sculpt;

  ss->cd_vert_node_offset = ss->attrs.dyntopo_node_id_vertex ?
                                ss->attrs.dyntopo_node_id_vertex->bmesh_cd_offset :
                                -1;
  ss->cd_face_node_offset = ss->attrs.dyntopo_node_id_face ?
                                ss->attrs.dyntopo_node_id_face->bmesh_cd_offset :
                                -1;
  ss->cd_origco_offset = ss->attrs.orig_co ? ss->attrs.orig_co->bmesh_cd_offset : -1;
  ss->cd_origno_offset = ss->attrs.orig_no ? ss->attrs.orig_no->bmesh_cd_offset : -1;
  ss->cd_origvcol_offset = ss->attrs.orig_color ? ss->attrs.orig_color->bmesh_cd_offset : -1;

  CustomDataLayer *layer = BKE_id_attribute_search_for_write(
      &me->id,
      BKE_id_attributes_active_color_name(&me->id),
      CD_MASK_COLOR_ALL,
      ATTR_DOMAIN_MASK_POINT | ATTR_DOMAIN_MASK_CORNER);
  if (layer) {
    AttrDomain domain = BKE_id_attribute_domain(&me->id, layer);
    CustomData *cdata = sculpt_get_cdata(ob, domain);

    int layer_i = CustomData_get_named_layer_index(
        cdata, eCustomDataType(layer->type), layer->name);

    ss->cd_vcol_offset = layer_i != -1 ? cdata->layers[layer_i].offset : -1;
  }
  else {
    ss->cd_vcol_offset = -1;
  }

  ss->cd_vert_mask_offset = CustomData_get_offset_named(
      &ss->bm->vdata, CD_PROP_FLOAT, ".sculpt_mask");
  ss->cd_faceset_offset = CustomData_get_offset_named(
      &ss->bm->pdata, CD_PROP_INT32, ".sculpt_face_set");
  ss->cd_face_areas = ss->attrs.face_areas ? ss->attrs.face_areas->bmesh_cd_offset : -1;

  int cd_boundary_flags = ss->attrs.boundary_flags ? ss->attrs.boundary_flags->bmesh_cd_offset :
                                                     -1;
  int cd_edge_boundary = ss->attrs.edge_boundary_flags ?
                             ss->attrs.edge_boundary_flags->bmesh_cd_offset :
                             -1;

  if (ss->pbvh) {
    bke::pbvh::update_offsets(ss->pbvh,
                              ss->cd_vert_node_offset,
                              ss->cd_face_node_offset,
                              ss->cd_face_areas,
                              cd_boundary_flags,
                              cd_edge_boundary,
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

  /* Run twice, in case sculpt_attr_update had to recreate a layer and messed up #BMesh
   * offsets.
   */
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
    update_bmesh_offsets(ob);
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

SculptAttribute BKE_sculpt_find_attribute(Object *ob, const char *name)
{
  if (!name) {
    SculptAttribute attr = {};
    return attr;
  }

  SculptSession *ss = ob->sculpt;

  CustomData *cdatas[4];
  AttrDomain domains[4] = {
      AttrDomain::Point, AttrDomain::Edge, AttrDomain::Corner, AttrDomain::Face};

  if (ss->bm) {
    cdatas[0] = &ss->bm->vdata;
    cdatas[1] = &ss->bm->edata;
    cdatas[2] = &ss->bm->ldata;
    cdatas[3] = &ss->bm->pdata;
  }
  else {
    Mesh *me = static_cast<Mesh *>(ob->data);

    cdatas[0] = &me->vert_data;
    cdatas[1] = &me->edge_data;
    cdatas[2] = &me->corner_data;
    cdatas[3] = &me->face_data;
  }

  bool is_grids = ss->pbvh && BKE_pbvh_type(ss->pbvh) == PBVH_GRIDS;

  for (int i = 0; i < 4; i++) {
    AttrDomain domain = domains[i];
    if (domain == AttrDomain::Point && is_grids) {
      for (int i = 0; i < ARRAY_SIZE(ss->temp_attributes); i++) {
        SculptAttribute *attr = &ss->temp_attributes[i];

        if (attr->used && attr->domain == AttrDomain::Point && STREQ(attr->name, name)) {
          return *attr;
        }
      }

      continue;
    }

    CustomData *data = cdatas[i];
    for (int j = 0; j < data->totlayer; j++) {
      CustomDataLayer &layer = data->layers[j];

      if (STREQ(layer.name, name) && (CD_TYPE_AS_MASK(layer.type) & CD_MASK_PROP_ALL)) {
        SculptAttribute *attr = BKE_sculpt_attribute_get(
            ob, domain, eCustomDataType(layer.type), name);
        SculptAttribute ret;

        ret = *attr;
        BKE_sculpt_attribute_release_ref(ob, attr);

        return ret;
      }
    }
  }

  SculptAttribute unused = {};
  return unused;
}

void BKE_sculpt_attribute_release_ref(Object *ob, SculptAttribute *attr)
{
  SculptSession *ss = ob->sculpt;

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

  /* Simple_array mode attributes are owned by SculptAttribute. */
  if (attr->simple_array) {
    MEM_SAFE_FREE(attr->data);
  }

  attr->data = nullptr;
  attr->used = false;
}

bool BKE_sculpt_attribute_destroy(Object *ob, SculptAttribute *attr)
{
  if (!attr || !attr->used) {
    return false;
  }

  SculptSession *ss = ob->sculpt;
  AttrDomain domain = attr->domain;

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
  ;
  if (attr->simple_array) {
    MEM_SAFE_FREE(attr->data);
  }
  else if (ss->bm) {
    if (attr->data_for_bmesh) {
      BM_data_layer_free_named(ss->bm, sculpt_get_cdata(ob, attr->domain), attr->name);
    }
  }
  else {
    CustomData *cdata = nullptr;
    int totelem = 0;

    switch (domain) {
      case AttrDomain::Point:
        cdata = ss->bm ? &ss->bm->vdata : &me->vert_data;
        totelem = ss->totvert;
        break;
      case AttrDomain::Edge:
        cdata = ss->bm ? &ss->bm->edata : &me->edge_data;
        totelem = BKE_object_get_original_mesh(ob)->edges_num;
        break;
      case AttrDomain::Face:
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
    int layer_i = CustomData_get_named_layer_index(
        cdata, eCustomDataType(attr->proptype), attr->name);
    if (layer_i != 0) {
      CustomData_free_layer(cdata, attr->proptype, totelem, layer_i);
    }
  }

  attr->data = nullptr;
  attr->used = false;

  sculpt_attribute_update_refs(ob);

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
        ob, AttrDomain::Point, CD_PROP_FLOAT3, SCULPT_ATTRIBUTE_NAME(orig_co), &params);
  }
  if (!ss->attrs.orig_no) {
    ss->attrs.orig_no = BKE_sculpt_attribute_ensure(
        ob, AttrDomain::Point, CD_PROP_FLOAT3, SCULPT_ATTRIBUTE_NAME(orig_no), &params);
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
        ob, AttrDomain::Point, CD_PROP_FLOAT3, SCULPT_ATTRIBUTE_NAME(curvature_dir), &params);
  }
}

void BKE_sculpt_ensure_origmask(struct Object *ob)
{
  SculptAttributeParams params = {};
  if (!ob->sculpt->attrs.orig_mask) {
    ob->sculpt->attrs.orig_mask = BKE_sculpt_attribute_ensure(
        ob, AttrDomain::Point, CD_PROP_FLOAT, SCULPT_ATTRIBUTE_NAME(orig_mask), &params);
  }
}
void BKE_sculpt_ensure_origcolor(struct Object *ob)
{
  SculptAttributeParams params = {};
  if (!ob->sculpt->attrs.orig_color) {
    ob->sculpt->attrs.orig_color = BKE_sculpt_attribute_ensure(
        ob, AttrDomain::Point, CD_PROP_COLOR, SCULPT_ATTRIBUTE_NAME(orig_color), &params);
  }
}

void BKE_sculpt_ensure_origfset(struct Object *ob)
{
  SculptAttributeParams params = {};
  if (!ob->sculpt->attrs.orig_fsets) {
    ob->sculpt->attrs.orig_fsets = BKE_sculpt_attribute_ensure(
        ob, AttrDomain::Face, CD_PROP_INT32, SCULPT_ATTRIBUTE_NAME(orig_fsets), &params);
  }
}

void BKE_sculpt_ensure_sculpt_layers(struct Object *ob)
{
  SculptAttributeParams params = {};
  params.nointerp = params.nocopy = true;

  if (!ob->sculpt->attrs.flags) {
    ob->sculpt->attrs.flags = BKE_sculpt_attribute_ensure(
        ob, AttrDomain::Point, CD_PROP_INT8, SCULPT_ATTRIBUTE_NAME(flags), &params);
  }
  if (!ob->sculpt->attrs.valence) {
    ob->sculpt->attrs.valence = BKE_sculpt_attribute_ensure(
        ob, AttrDomain::Point, CD_PROP_INT32, SCULPT_ATTRIBUTE_NAME(valence), &params);
  }
  if (!ob->sculpt->attrs.stroke_id) {
    ob->sculpt->attrs.stroke_id = BKE_sculpt_attribute_ensure(
        ob, AttrDomain::Point, CD_PROP_INT32, SCULPT_ATTRIBUTE_NAME(stroke_id), &params);
  }

  if (ob->sculpt->pbvh) {
    blender::bke::pbvh::set_flags_valence(ob->sculpt->pbvh,
                                          static_cast<uint8_t *>(ob->sculpt->attrs.flags->data),
                                          static_cast<int *>(ob->sculpt->attrs.valence->data));
  }
}

void BKE_sculpt_set_bmesh(Object *ob, BMesh *bm, bool free_existing)
{
  SculptSession *ss = ob->sculpt;

  if (bm == ss->bm) {
    return;
  }

  /* Destroy existing idmap. */
  if (ss->bm_idmap) {
    BM_idmap_destroy(ss->bm_idmap);
    ss->bm_idmap = nullptr;
  }

  if (!ss->bm) {
    ss->bm = bm;
    return;
  }

  /* Free existing bmesh. */
  if (ss->bm && free_existing) {
    BM_mesh_free(ss->bm);
    ss->bm = nullptr;
  }

  /* Destroy toolflags if they exist (will reallocate the bmesh). */
  BM_mesh_toolflags_set(bm, false);

  /* Ensure element indices & lookup tables are up to date. */
  bm->elem_index_dirty = BM_VERT | BM_EDGE | BM_FACE;
  bm->elem_table_dirty = BM_VERT | BM_EDGE | BM_FACE;
  BM_mesh_elem_table_ensure(bm, BM_VERT | BM_EDGE | BM_FACE);
  BM_mesh_elem_index_ensure(bm, BM_VERT | BM_EDGE | BM_FACE);

  /* Set new bmesh. */
  ss->bm = bm;

  /* Invalidate any existing bmesh attributes. */
  SculptAttribute **attrs = reinterpret_cast<SculptAttribute **>(&ss->attrs);
  int attrs_num = int(sizeof(ss->attrs) / sizeof(void *));
  for (int i = 0; i < attrs_num; i++) {
    if (attrs[i] && attrs[i]->data_for_bmesh) {
      attrs[i]->used = false;
      attrs[i] = nullptr;
    }
  }

  /* Check for any stray attributes in the pool that weren't stored in ss->attrs. */
  for (int i = 0; i < SCULPT_MAX_ATTRIBUTES; i++) {
    SculptAttribute *attr = ss->temp_attributes + i;
    if (attr->used && attr->data_for_bmesh) {
      attr->used = false;
    }
  }

  if (!ss->pbvh) {
    /* Do nothing, no pbvh to rebuild. */
    return;
  }

  /* Destroy and rebuild pbvh */
  bke::pbvh::free(ss->pbvh);
  ss->pbvh = nullptr;

  ss->pbvh = build_pbvh_for_dynamic_topology(ob, true);
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
      const float *mask = nullptr;

      switch (BKE_pbvh_type(ss->pbvh)) {
        case PBVH_FACES:
          mask = ss->vmask ? &ss->vmask[vertex.i] : nullptr;
          break;
        case PBVH_BMESH: {
          BMVert *v;
          int cd_mask = ss->cd_vert_mask_offset;

          v = (BMVert *)vertex.i;
          mask = cd_mask != -1 ? static_cast<float *>(BM_ELEM_CD_GET_VOID_P(v, cd_mask)) : nullptr;
          break;
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
          break;
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

namespace blender::bke::sculpt {
void sculpt_vert_boundary_ensure(Object *object)
{
  using namespace blender;
  SculptSession *ss = object->sculpt;

  /* PBVH_BMESH now handles boundaries itself. */
  if (ss->bm || !ss->vertex_info.boundary.is_empty()) {
    if (!ss->bm && ss->pbvh) {
      blender::bke::pbvh::set_vert_boundary_map(ss->pbvh, &ss->vertex_info.boundary);
    }

    return;
  }

  Mesh *base_mesh = BKE_mesh_from_object(object);
  const blender::Span<int2> edges = base_mesh->edges();
  const OffsetIndices polys = base_mesh->faces();
  const Span<int> corner_edges = base_mesh->corner_edges();

  ss->vertex_info.boundary.resize(base_mesh->verts_num);
  int *adjacent_faces_edge_count = static_cast<int *>(
      MEM_calloc_arrayN(base_mesh->edges_num, sizeof(int), "Adjacent face edge count"));

  for (const int i : polys.index_range()) {
    for (const int edge : corner_edges.slice(polys[i])) {
      adjacent_faces_edge_count[edge]++;
    }
  }

  for (const int e : edges.index_range()) {
    if (adjacent_faces_edge_count[e] < 2) {
      const int2 &edge = edges[e];
      BLI_BITMAP_SET(ss->vertex_info.boundary, edge[0], true);
      BLI_BITMAP_SET(ss->vertex_info.boundary, edge[1], true);
    }
  }

  if (ss->pbvh) {
    blender::bke::pbvh::set_vert_boundary_map(ss->pbvh, &ss->vertex_info.boundary);
  }

  MEM_freeN(adjacent_faces_edge_count);
}

float calc_uv_snap_limit(BMLoop *l, int cd_uv)
{
  BMVert *v = l->v;

  float avg_len = 0.0f;
  int avg_tot = 0;

  BMEdge *e = v->e;
  do {
    BMLoop *l2 = e->l;

    if (!l2) {
      continue;
    }

    do {
      float *uv1 = BM_ELEM_CD_PTR<float *>(l2, cd_uv);
      float *uv2 = BM_ELEM_CD_PTR<float *>(l2->next, cd_uv);

      avg_len += len_v2v2(uv1, uv2);
      avg_tot++;
    } while ((l2 = l2->radial_next) != e->l);
  } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);

  if (avg_tot > 0) {
    /* 1/10th of average uv edge length. */
    return (avg_len / avg_tot) * 0.1f;
  }
  else {
    return 0.005f;
  }
}

/* Angle test. loop_is_corner calls this if the chart count test fails. */
static bool loop_is_corner_angle(BMLoop *l,
                                 int cd_offset,
                                 const float limit,
                                 const float angle_limit,
                                 const CustomData * /*ldata*/)
{
  BMVert *v = l->v;
  BMEdge *e = v->e;

  float2 uv_value = *BM_ELEM_CD_PTR<float2 *>(l, cd_offset);

  BMLoop *outer1 = nullptr, *outer2 = nullptr;
  float2 outer1_value;

#ifdef TEST_UV_CORNER_CALC
  int cd_pin = -1, cd_sel = -1;

  bool test_mode = false;
  // test_mode = l->v->head.hflag & BM_ELEM_SELECT;

  if (ldata && test_mode) {
    char name[512];
    for (int i = 0; i < ldata->totlayer; i++) {
      CustomDataLayer *layer = ldata->layers + i;
      if (layer->offset == cd_offset) {
        sprintf(name, ".pn.%s", layer->name);
        cd_pin = CustomData_get_offset_named(ldata, CD_PROP_BOOL, name);

        sprintf(name, ".vs.%s", layer->name);
        cd_sel = CustomData_get_offset_named(ldata, CD_PROP_BOOL, name);
      }
    }

    if (cd_sel != -1) {
      test_mode = BM_ELEM_CD_GET_BOOL(l, cd_sel);
    }
  }

  if (test_mode) {
    printf("%s: start\n", __func__);
  }
#endif

  do {
    BMLoop *l2 = e->l;
    if (!l2) {
      continue;
    }

    do {
      BMLoop *uv_l2 = l2->v == v ? l2 : l2->next;
      float2 uv_value2 = *BM_ELEM_CD_PTR<float2 *>(uv_l2, cd_offset);

      BMLoop *other_uv_l2 = l2->v == v ? l2->next : l2;
      float2 other_uv_value2 = *BM_ELEM_CD_PTR<float2 *>(other_uv_l2, cd_offset);

      if (math::distance_squared(uv_value, uv_value2) > limit * limit) {
        continue;
      }

      bool outer = l2 == l2->radial_next;

      BMLoop *l3 = l2->radial_next;
      while (l3 != l2) {
        BMLoop *other_uv_l3 = l3->v == v ? l3->next : l3;
        float2 other_uv_value3 = *BM_ELEM_CD_PTR<float2 *>(other_uv_l3, cd_offset);

        if (math::distance_squared(other_uv_value2, other_uv_value3) > limit * limit) {
#ifdef TEST_UV_CORNER_CALC
          if (test_mode && cd_pin != -1) {
            // BM_ELEM_CD_SET_BOOL(other_uv_l2, cd_pin, true);
            // BM_ELEM_CD_SET_BOOL(other_uv_l3, cd_pin, true);
          }
#endif

          BMLoop *uv_l3 = l3->v == v ? l3 : l3->next;
          float2 uv_value3 = *BM_ELEM_CD_PTR<float2 *>(uv_l3, cd_offset);

          /* other_uv_value might be valid for one of the two arms, check. */
          if (math::distance_squared(uv_value, uv_value3) <= limit * limit) {
#ifdef TEST_UV_CORNER_CALC
            if (test_mode) {
              printf("%s: case 1\n", __func__);
            }
#endif

            outer1 = other_uv_l2;
            outer2 = other_uv_l3;
            goto outer_break;
          }

          outer = true;
          break;
        }

        l3 = l3->radial_next;
      }

      if (outer) {
#ifdef TEST_UV_CORNER_CALC
        if (test_mode && cd_pin != -1) {
          // BM_ELEM_CD_SET_BOOL(other_uv_l2, cd_pin, true);
        }
#endif

        if (!outer1) {
          outer1 = other_uv_l2;
          outer1_value = other_uv_value2;
        }
        else if (other_uv_l2 != outer2 &&
                 math::distance_squared(outer1_value, other_uv_value2) > limit * limit)
        {
          outer2 = other_uv_l2;

#ifdef TEST_UV_CORNER_CALC
          if (test_mode) {
            printf("%s: case 2\n", __func__);
          }
#endif
          goto outer_break;
        }
      }
    } while ((l2 = l2->radial_next) != e->l);
  } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);

outer_break:

#ifdef TEST_UV_CORNER_CALC
  if (test_mode) {
    printf("%s: l: %p, l->v: %p, outer1: %p, outer2: %p\n", __func__, l, l->v, outer1, outer2);
  }
#endif

  if (!outer1 || !outer2) {
    return false;
  }

#ifdef TEST_UV_CORNER_CALC
  if (test_mode && cd_pin != -1) {
    // BM_ELEM_CD_SET_BOOL(outer1, cd_pin, true);
    // BM_ELEM_CD_SET_BOOL(outer2, cd_pin, true);
  }
#endif

  float2 t1 = *BM_ELEM_CD_PTR<float2 *>(outer1, cd_offset) - uv_value;
  float2 t2 = *BM_ELEM_CD_PTR<float2 *>(outer2, cd_offset) - uv_value;

  normalize_v2(t1);
  normalize_v2(t2);

  if (dot_v2v2(t1, t2) < 0.0f) {
    negate_v2(t2);
  }

  float angle = math::safe_acos(dot_v2v2(t1, t2));

#ifdef TEST_UV_CORNER_CALC
  if (test_mode) {
    printf("%s: angle: %.5f\n", __func__, angle);
  }
#endif

  bool ret = angle > angle_limit;

#ifdef TEST_UV_CORNER_CALC
  if (ret) {
    // l->v->head.hflag |= BM_ELEM_SELECT;
  }
#endif

  return ret;
}

bool loop_is_corner(BMLoop *l, int cd_uv, float limit, const CustomData *ldata)
{
  BMVert *v = l->v;

  float2 value = *BM_ELEM_CD_PTR<float2 *>(l, cd_uv);

  Vector<BMLoop *, 16> ls;
  Vector<int, 16> keys;

  BMEdge *e = v->e;
  do {
    BMLoop *l2 = e->l;

    if (!l2) {
      continue;
    }

    do {
      BMLoop *l3 = l2->v == v ? l2 : l2->next;
      if (!ls.contains(l3)) {
        ls.append(l3);
      }
    } while ((l2 = l2->radial_next) != e->l);
  } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);

  const float scale = 1.0f / limit;

  for (BMLoop *l2 : ls) {
    float2 value2 = *BM_ELEM_CD_PTR<float2 *>(l2, cd_uv);
    float2 dv = value2 - value;

    double f = dv[0] * dv[0] + dv[1] * dv[1];
    int key = int(f * scale);
    if (!keys.contains(key)) {
      keys.append(key);
    }
  }

  bool ret = keys.size() > 2;

  if (!ret) {
    float angle_limit = 60.0f / 180.0f * M_PI;

    return loop_is_corner_angle(l, cd_uv, limit, angle_limit, ldata);
  }

#ifdef TEST_UV_CORNER_CALC
  if (ret) {
    // l->v->head.hflag |= BM_ELEM_SELECT;
  }
#endif

  return ret;
}

namespace detail {
static void corner_interp(CustomDataLayer * /*layer*/,
                          BMVert *v,
                          BMLoop *l,
                          Span<BMLoop *> loops,
                          Span<float> ws,
                          int cd_offset,
                          float factor,
                          float2 &new_value)
{
  float *ws2 = (float *)BLI_array_alloca(ws2, loops.size() + 1);
  float2 sum = {};
  float totsum = 0.0f;

  float2 value = *BM_ELEM_CD_PTR<float2 *>(l, cd_offset);

  float limit, limit_sqr;

  limit = calc_uv_snap_limit(l, cd_offset);
  limit_sqr = limit * limit;

  /* Sum over uv verts surrounding l connected to the same chart. */
  for (int i : loops.index_range()) {
    BMLoop *l2 = loops[i];
    BMLoop *l3;

    /* Find UV in l2's face that's owned by v. */
    if (l2->v == v) {
      l3 = l2;
    }
    else if (l2->next->v == v) {
      l3 = l2->next;
    }
    else {
      l3 = l2->prev;
    }

    float2 value3 = *BM_ELEM_CD_PTR<float2 *>(l3, cd_offset);

    if (math::distance_squared(value, value3) <= limit_sqr) {
      float2 value2 = *BM_ELEM_CD_PTR<float2 *>(l2, cd_offset);
      sum += value2 * ws[i];
      totsum += ws[i];
    }
  }

  if (totsum == 0.0f) {
    return;
  }

  sum /= totsum;
  new_value = value + (sum - value) * factor;
}

/* Interpolates loops surrounding a vertex, splitting any UV map by
 * island as appropriate and enforcing proper boundary conditions.
 */
static void interp_face_corners_intern(PBVH * /*pbvh*/,
                                       BMVert *v,
                                       Span<BMLoop *> loops,
                                       Span<float> ws,
                                       float factor,
                                       int cd_vert_boundary,
                                       Span<BMLoop *> ls,
                                       CustomDataLayer *layer)
{
  Vector<bool, 24> corners;
  Array<float2, 24> new_values(ls.size());

  /* Build (semantic) corner tags. */
  for (BMLoop *l : ls) {
    /* Do not calculate the corner state here, use stored corner flag.
     *
     * The corner would normally be calculated like so:
     *     corner = loop_is_corner(l, layer->offset);
     */
    bool corner = BM_ELEM_CD_GET_INT(v, cd_vert_boundary) & SCULPT_CORNER_UV;

    corners.append(corner);
  }

  /* Interpolate loops. */
  for (int i : ls.index_range()) {
    BMLoop *l = ls[i];

    if (!corners[i]) {
      corner_interp(layer, v, l, loops, ws, layer->offset, factor, new_values[i]);
    }
  }

  for (int i : ls.index_range()) {
    if (!corners[i]) {
      *BM_ELEM_CD_PTR<float2 *>(ls[i], layer->offset) = new_values[i];
    }
  }
}
}  // namespace detail

void interp_face_corners(PBVH *pbvh,
                         PBVHVertRef vertex,
                         Span<BMLoop *> loops,
                         Span<float> ws,
                         float factor,
                         int cd_vert_boundary)
{
  if (BKE_pbvh_type(pbvh) != PBVH_BMESH) {
    return; /* Only for PBVH_BMESH. */
  }

  BMesh *bm = BKE_pbvh_get_bmesh(pbvh);
  BMVert *v = reinterpret_cast<BMVert *>(vertex.i);
  BMEdge *e = v->e;
  BMLoop *l = e->l;
  CustomData *cdata = &bm->ldata;

  Vector<BMLoop *, 32> ls;

  /* Tag loops around vertex. */
  do {
    l = e->l;

    if (!l) {
      continue;
    }

    do {
      BMLoop *l2 = l->v == v ? l : l->next;
      BM_elem_flag_enable(l2, BM_ELEM_TAG);
    } while ((l = l->radial_next) != e->l);
  } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);

  /* Build loop list. */
  do {
    l = e->l;

    if (!l) {
      continue;
    }

    do {
      BMLoop *l2 = l->v == v ? l : l->next;
      if (BM_elem_flag_test(l2, BM_ELEM_TAG)) {
        BM_elem_flag_disable(l2, BM_ELEM_TAG);
        ls.append(l2);
      }
    } while ((l = l->radial_next) != e->l);
  } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);

  Vector<CustomDataLayer *, 16> layers;
  for (int layer_i : IndexRange(cdata->totlayer)) {
    CustomDataLayer *layer = cdata->layers + layer_i;

    if (layer->type != CD_PROP_FLOAT2 ||
        (layer->flag & (CD_FLAG_ELEM_NOINTERP | CD_FLAG_TEMPORARY)))
    {
      continue;
    }

    layers.append(layer);
  }

  /* Interpolate. */
  if (layers.size() > 0 && loops.size() > 1) {
    // VertLoopSnapper corner_snap = {Span<BMLoop *>(ls), Span<CustomDataLayer *>(layers)};

    for (CustomDataLayer *layer : layers) {
      detail::interp_face_corners_intern(pbvh, v, loops, ws, factor, cd_vert_boundary, ls, layer);
    }

    /* Snap. */
    // corner_snap.snap();
  }
}
}  // namespace blender::bke::sculpt
