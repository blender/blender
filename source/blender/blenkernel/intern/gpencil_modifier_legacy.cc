/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "BLI_assert.h"
#include "BLI_listbase.h"

#include "MEM_guardedalloc.h"

#include "DNA_colorband_types.h"
#include "DNA_gpencil_modifier_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_texture_types.h"

#include "BKE_colortools.hh"
#include "BKE_deform.hh"
#include "BKE_gpencil_modifier_legacy.h"
#include "BKE_lattice.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_modifier.hh"
#include "BKE_screen.hh"
#include "BKE_shrinkwrap.hh"

#include "BLO_read_write.hh"

namespace blender {

/* Check if the type value is valid. */
static bool gpencil_modifier_type_valid(const int type)
{
  return type > 0 && type < NUM_GREASEPENCIL_MODIFIER_TYPES;
}

/**
 * Free internal modifier data variables, this function should
 * not free the md variable itself.
 */
static void gpencil_modifier_free_data(GpencilModifierData *md)
{
  switch (GpencilModifierType(md->type)) {
    case eGpencilModifierType_Noise: {
      NoiseGpencilModifierData *gpmd = reinterpret_cast<NoiseGpencilModifierData *>(md);

      if (gpmd->curve_intensity) {
        BKE_curvemapping_free(gpmd->curve_intensity);
      }
      break;
    }
    case eGpencilModifierType_Thick: {
      ThickGpencilModifierData *gpmd = reinterpret_cast<ThickGpencilModifierData *>(md);

      if (gpmd->curve_thickness) {
        BKE_curvemapping_free(gpmd->curve_thickness);
      }
      break;
    }
    case eGpencilModifierType_Tint: {
      TintGpencilModifierData *mmd = reinterpret_cast<TintGpencilModifierData *>(md);

      MEM_SAFE_DELETE(mmd->colorband);
      if (mmd->curve_intensity) {
        BKE_curvemapping_free(mmd->curve_intensity);
      }
      break;
    }
    case eGpencilModifierType_Opacity: {
      OpacityGpencilModifierData *gpmd = reinterpret_cast<OpacityGpencilModifierData *>(md);

      if (gpmd->curve_intensity) {
        BKE_curvemapping_free(gpmd->curve_intensity);
      }
      break;
    }
    case eGpencilModifierType_Color: {
      ColorGpencilModifierData *gpmd = reinterpret_cast<ColorGpencilModifierData *>(md);

      if (gpmd->curve_intensity) {
        BKE_curvemapping_free(gpmd->curve_intensity);
      }
      break;
    }
    case eGpencilModifierType_Lattice: {
      LatticeGpencilModifierData *mmd = reinterpret_cast<LatticeGpencilModifierData *>(md);
      LatticeDeformData *ldata = static_cast<LatticeDeformData *>(mmd->cache_data);

      /* free deform data */
      if (ldata) {
        BKE_lattice_deform_data_destroy(ldata);
      }
      break;
    }
    case eGpencilModifierType_Smooth: {
      SmoothGpencilModifierData *gpmd = reinterpret_cast<SmoothGpencilModifierData *>(md);

      if (gpmd->curve_intensity) {
        BKE_curvemapping_free(gpmd->curve_intensity);
      }
      break;
    }
    case eGpencilModifierType_Hook: {
      HookGpencilModifierData *mmd = reinterpret_cast<HookGpencilModifierData *>(md);

      if (mmd->curfalloff) {
        BKE_curvemapping_free(mmd->curfalloff);
      }
      break;
    }
    case eGpencilModifierType_Time: {
      TimeGpencilModifierData *gpmd = reinterpret_cast<TimeGpencilModifierData *>(md);

      MEM_SAFE_DELETE(gpmd->segments);
      break;
    }
    case eGpencilModifierType_Dash: {
      DashGpencilModifierData *dmd = reinterpret_cast<DashGpencilModifierData *>(md);

      MEM_SAFE_DELETE(dmd->segments);
      break;
    }
    case eGpencilModifierType_Shrinkwrap: {
      ShrinkwrapGpencilModifierData *mmd = reinterpret_cast<ShrinkwrapGpencilModifierData *>(md);

      if (mmd->cache_data) {
        BKE_shrinkwrap_free_tree(mmd->cache_data);
        MEM_delete(mmd->cache_data);
      }
      break;
    }

    case eGpencilModifierType_None:
    case eGpencilModifierType_Subdiv:
    case eGpencilModifierType_Array:
    case eGpencilModifierType_Build:
    case eGpencilModifierType_Simplify:
    case eGpencilModifierType_Offset:
    case eGpencilModifierType_Mirror:
    case eGpencilModifierType_Multiply:
    case eGpencilModifierType_Texture:
    case eGpencilModifierType_Lineart:
    case eGpencilModifierType_Length:
    case eGpencilModifierType_WeightProximity:
    case eGpencilModifierType_WeightAngle:
    case eGpencilModifierType_Envelope:
    case eGpencilModifierType_Outline:
    case eGpencilModifierType_Armature:
      break;
    case NUM_GREASEPENCIL_MODIFIER_TYPES:
      BLI_assert_unreachable();
      break;
  }
}

/**
 * Should call the given walk function with a pointer to each ID
 * pointer (i.e. each data-block pointer) that the modifier data
 * stores. This is used for linking on file load and for
 * unlinking data-blocks or forwarding data-block references.
 */
static void gpencil_modifier_foreach_ID_link(GpencilModifierData *md,
                                             Object *ob,
                                             GreasePencilIDWalkFunc walk,
                                             void *user_data)
{
  switch (GpencilModifierType(md->type)) {
    case eGpencilModifierType_None: {
      break;
    }
    case eGpencilModifierType_Noise: {
      NoiseGpencilModifierData *mmd = reinterpret_cast<NoiseGpencilModifierData *>(md);

      walk(user_data, ob, reinterpret_cast<ID **>(&mmd->material), IDWALK_CB_USER);
      break;
    }
    case eGpencilModifierType_Subdiv: {
      SubdivGpencilModifierData *mmd = reinterpret_cast<SubdivGpencilModifierData *>(md);

      walk(user_data, ob, reinterpret_cast<ID **>(&mmd->material), IDWALK_CB_USER);
      break;
    }
    case eGpencilModifierType_Thick: {
      ThickGpencilModifierData *mmd = reinterpret_cast<ThickGpencilModifierData *>(md);

      walk(user_data, ob, reinterpret_cast<ID **>(&mmd->material), IDWALK_CB_USER);
      break;
    }
    case eGpencilModifierType_Tint: {
      TintGpencilModifierData *mmd = reinterpret_cast<TintGpencilModifierData *>(md);

      walk(user_data, ob, reinterpret_cast<ID **>(&mmd->material), IDWALK_CB_USER);
      walk(user_data, ob, reinterpret_cast<ID **>(&mmd->object), IDWALK_CB_NOP);
      break;
    }
    case eGpencilModifierType_Array: {
      ArrayGpencilModifierData *mmd = reinterpret_cast<ArrayGpencilModifierData *>(md);

      walk(user_data, ob, reinterpret_cast<ID **>(&mmd->material), IDWALK_CB_USER);
      walk(user_data, ob, reinterpret_cast<ID **>(&mmd->object), IDWALK_CB_NOP);
      break;
    }
    case eGpencilModifierType_Build: {
      BuildGpencilModifierData *mmd = reinterpret_cast<BuildGpencilModifierData *>(md);

      walk(user_data, ob, reinterpret_cast<ID **>(&mmd->object), IDWALK_CB_NOP);
      break;
    }
    case eGpencilModifierType_Opacity: {
      OpacityGpencilModifierData *mmd = reinterpret_cast<OpacityGpencilModifierData *>(md);

      walk(user_data, ob, reinterpret_cast<ID **>(&mmd->material), IDWALK_CB_USER);
      break;
    }
    case eGpencilModifierType_Color: {
      ColorGpencilModifierData *mmd = reinterpret_cast<ColorGpencilModifierData *>(md);

      walk(user_data, ob, reinterpret_cast<ID **>(&mmd->material), IDWALK_CB_USER);
      break;
    }
    case eGpencilModifierType_Lattice: {
      LatticeGpencilModifierData *mmd = reinterpret_cast<LatticeGpencilModifierData *>(md);

      walk(user_data, ob, reinterpret_cast<ID **>(&mmd->material), IDWALK_CB_USER);
      walk(user_data, ob, reinterpret_cast<ID **>(&mmd->object), IDWALK_CB_NOP);
      break;
    }
    case eGpencilModifierType_Simplify: {
      SimplifyGpencilModifierData *mmd = reinterpret_cast<SimplifyGpencilModifierData *>(md);

      walk(user_data, ob, reinterpret_cast<ID **>(&mmd->material), IDWALK_CB_USER);
      break;
    }
    case eGpencilModifierType_Smooth: {
      SmoothGpencilModifierData *mmd = reinterpret_cast<SmoothGpencilModifierData *>(md);

      walk(user_data, ob, reinterpret_cast<ID **>(&mmd->material), IDWALK_CB_USER);
      break;
    }
    case eGpencilModifierType_Hook: {
      HookGpencilModifierData *mmd = reinterpret_cast<HookGpencilModifierData *>(md);

      walk(user_data, ob, reinterpret_cast<ID **>(&mmd->material), IDWALK_CB_USER);
      walk(user_data, ob, reinterpret_cast<ID **>(&mmd->object), IDWALK_CB_NOP);
      break;
    }
    case eGpencilModifierType_Offset: {
      OffsetGpencilModifierData *mmd = reinterpret_cast<OffsetGpencilModifierData *>(md);

      walk(user_data, ob, reinterpret_cast<ID **>(&mmd->material), IDWALK_CB_USER);
      break;
    }
    case eGpencilModifierType_Mirror: {
      MirrorGpencilModifierData *mmd = reinterpret_cast<MirrorGpencilModifierData *>(md);

      walk(user_data, ob, reinterpret_cast<ID **>(&mmd->material), IDWALK_CB_USER);
      walk(user_data, ob, reinterpret_cast<ID **>(&mmd->object), IDWALK_CB_NOP);
      break;
    }
    case eGpencilModifierType_Armature: {
      ArmatureGpencilModifierData *mmd = reinterpret_cast<ArmatureGpencilModifierData *>(md);

      walk(user_data, ob, reinterpret_cast<ID **>(&mmd->object), IDWALK_CB_NOP);
      break;
    }
    case eGpencilModifierType_Time: {
      TimeGpencilModifierData *mmd = reinterpret_cast<TimeGpencilModifierData *>(md);

      walk(user_data, ob, reinterpret_cast<ID **>(&mmd->material), IDWALK_CB_USER);
      break;
    }
    case eGpencilModifierType_Multiply: {
      MultiplyGpencilModifierData *mmd = reinterpret_cast<MultiplyGpencilModifierData *>(md);

      walk(user_data, ob, reinterpret_cast<ID **>(&mmd->material), IDWALK_CB_USER);
      break;
    }
    case eGpencilModifierType_Texture: {
      TextureGpencilModifierData *mmd = reinterpret_cast<TextureGpencilModifierData *>(md);

      walk(user_data, ob, reinterpret_cast<ID **>(&mmd->material), IDWALK_CB_USER);
      break;
    }
    case eGpencilModifierType_Lineart: {
      LineartGpencilModifierData *lmd = reinterpret_cast<LineartGpencilModifierData *>(md);

      walk(user_data, ob, reinterpret_cast<ID **>(&lmd->target_material), IDWALK_CB_USER);
      walk(user_data, ob, reinterpret_cast<ID **>(&lmd->source_collection), IDWALK_CB_NOP);

      walk(user_data, ob, reinterpret_cast<ID **>(&lmd->source_object), IDWALK_CB_NOP);
      walk(user_data, ob, reinterpret_cast<ID **>(&lmd->source_camera), IDWALK_CB_NOP);
      walk(user_data, ob, reinterpret_cast<ID **>(&lmd->light_contour_object), IDWALK_CB_NOP);
      break;
    }
    case eGpencilModifierType_Length: {
      LengthGpencilModifierData *mmd = reinterpret_cast<LengthGpencilModifierData *>(md);

      walk(user_data, ob, reinterpret_cast<ID **>(&mmd->material), IDWALK_CB_USER);
      break;
    }
    case eGpencilModifierType_WeightProximity: {
      WeightProxGpencilModifierData *mmd = reinterpret_cast<WeightProxGpencilModifierData *>(md);

      walk(user_data, ob, reinterpret_cast<ID **>(&mmd->material), IDWALK_CB_USER);
      walk(user_data, ob, reinterpret_cast<ID **>(&mmd->object), IDWALK_CB_NOP);
      break;
    }
    case eGpencilModifierType_Dash: {
      DashGpencilModifierData *mmd = reinterpret_cast<DashGpencilModifierData *>(md);

      walk(user_data, ob, reinterpret_cast<ID **>(&mmd->material), IDWALK_CB_USER);
      break;
    }
    case eGpencilModifierType_WeightAngle: {
      WeightAngleGpencilModifierData *mmd = reinterpret_cast<WeightAngleGpencilModifierData *>(md);

      walk(user_data, ob, reinterpret_cast<ID **>(&mmd->material), IDWALK_CB_USER);
      break;
    }
    case eGpencilModifierType_Shrinkwrap: {
      ShrinkwrapGpencilModifierData *mmd = reinterpret_cast<ShrinkwrapGpencilModifierData *>(md);

      walk(user_data, ob, reinterpret_cast<ID **>(&mmd->target), IDWALK_CB_NOP);
      walk(user_data, ob, reinterpret_cast<ID **>(&mmd->aux_target), IDWALK_CB_NOP);
      walk(user_data, ob, reinterpret_cast<ID **>(&mmd->material), IDWALK_CB_USER);
      break;
    }
    case eGpencilModifierType_Envelope: {
      EnvelopeGpencilModifierData *mmd = reinterpret_cast<EnvelopeGpencilModifierData *>(md);

      walk(user_data, ob, reinterpret_cast<ID **>(&mmd->material), IDWALK_CB_USER);
      break;
    }
    case eGpencilModifierType_Outline: {
      OutlineGpencilModifierData *mmd = reinterpret_cast<OutlineGpencilModifierData *>(md);

      walk(user_data, ob, reinterpret_cast<ID **>(&mmd->material), IDWALK_CB_USER);
      walk(user_data, ob, reinterpret_cast<ID **>(&mmd->outline_material), IDWALK_CB_USER);
      walk(user_data, ob, reinterpret_cast<ID **>(&mmd->object), IDWALK_CB_NOP);
      break;
    }
    case NUM_GREASEPENCIL_MODIFIER_TYPES:
      BLI_assert_unreachable();
      break;
  }
}

/* *************************************************** */
/* Modifier Methods - Evaluation Loops, etc. */

static void modifier_free_data_id_us_cb(void * /*user_data*/,
                                        Object * /*ob*/,
                                        ID **idpoin,
                                        const LibraryForeachIDCallbackFlag cb_flag)
{
  ID *id = *idpoin;
  if (id != nullptr && (cb_flag & IDWALK_CB_USER) != 0) {
    id_us_min(id);
  }
}

void BKE_gpencil_modifier_free_ex(GpencilModifierData *md, const int flag)
{
  if ((flag & LIB_ID_CREATE_NO_USER_REFCOUNT) == 0) {
    gpencil_modifier_foreach_ID_link(md, nullptr, modifier_free_data_id_us_cb, nullptr);
  }

  gpencil_modifier_free_data(md);
  if (md->error) {
    MEM_delete(md->error);
  }

  MEM_delete(md);
}

void BKE_gpencil_modifier_free(GpencilModifierData *md)
{
  BKE_gpencil_modifier_free_ex(md, 0);
}

void BKE_gpencil_modifiers_foreach_ID_link(Object *ob,
                                           GreasePencilIDWalkFunc walk,
                                           void *user_data)
{
  GpencilModifierData *md = static_cast<GpencilModifierData *>(ob->greasepencil_modifiers.first);

  for (; md; md = md->next) {
    gpencil_modifier_foreach_ID_link(md, ob, walk, user_data);
  }
}

void BKE_gpencil_modifier_blend_read_data(BlendDataReader *reader,
                                          ListBaseT<GpencilModifierData> *lb,
                                          Object *ob)
{
  BLO_read_struct_list(reader, GpencilModifierData, lb);

  for (GpencilModifierData &md : *lb) {
    md.error = nullptr;

    /* if modifiers disappear, or for upward compatibility */
    if (!gpencil_modifier_type_valid(md.type)) {
      md.type = eModifierType_None;
    }

    /* If linking from a library, clear 'local' library override flag. */
    if (ID_IS_LINKED(ob)) {
      md.flag &= ~eGpencilModifierFlag_OverrideLibrary_Local;
    }

    if (md.type == eGpencilModifierType_Lattice) {
      LatticeGpencilModifierData *gpmd = reinterpret_cast<LatticeGpencilModifierData *>(&md);
      gpmd->cache_data = nullptr;
    }
    else if (md.type == eGpencilModifierType_Hook) {
      HookGpencilModifierData *hmd = reinterpret_cast<HookGpencilModifierData *>(&md);

      BLO_read_struct(reader, CurveMapping, &hmd->curfalloff);
      if (hmd->curfalloff) {
        BKE_curvemapping_blend_read(reader, hmd->curfalloff);
        BKE_curvemapping_init(hmd->curfalloff);
      }
    }
    else if (md.type == eGpencilModifierType_Noise) {
      NoiseGpencilModifierData *gpmd = reinterpret_cast<NoiseGpencilModifierData *>(&md);

      BLO_read_struct(reader, CurveMapping, &gpmd->curve_intensity);
      if (gpmd->curve_intensity) {
        BKE_curvemapping_blend_read(reader, gpmd->curve_intensity);
        /* Initialize the curve. Maybe this could be moved to modifier logic. */
        BKE_curvemapping_init(gpmd->curve_intensity);
      }
    }
    else if (md.type == eGpencilModifierType_Thick) {
      ThickGpencilModifierData *gpmd = reinterpret_cast<ThickGpencilModifierData *>(&md);

      BLO_read_struct(reader, CurveMapping, &gpmd->curve_thickness);
      if (gpmd->curve_thickness) {
        BKE_curvemapping_blend_read(reader, gpmd->curve_thickness);
        BKE_curvemapping_init(gpmd->curve_thickness);
      }
    }
    else if (md.type == eGpencilModifierType_Tint) {
      TintGpencilModifierData *gpmd = reinterpret_cast<TintGpencilModifierData *>(&md);
      BLO_read_struct(reader, ColorBand, &gpmd->colorband);
      BLO_read_struct(reader, CurveMapping, &gpmd->curve_intensity);
      if (gpmd->curve_intensity) {
        BKE_curvemapping_blend_read(reader, gpmd->curve_intensity);
        BKE_curvemapping_init(gpmd->curve_intensity);
      }
    }
    else if (md.type == eGpencilModifierType_Smooth) {
      SmoothGpencilModifierData *gpmd = reinterpret_cast<SmoothGpencilModifierData *>(&md);
      BLO_read_struct(reader, CurveMapping, &gpmd->curve_intensity);
      if (gpmd->curve_intensity) {
        BKE_curvemapping_blend_read(reader, gpmd->curve_intensity);
        BKE_curvemapping_init(gpmd->curve_intensity);
      }
    }
    else if (md.type == eGpencilModifierType_Color) {
      ColorGpencilModifierData *gpmd = reinterpret_cast<ColorGpencilModifierData *>(&md);
      BLO_read_struct(reader, CurveMapping, &gpmd->curve_intensity);
      if (gpmd->curve_intensity) {
        BKE_curvemapping_blend_read(reader, gpmd->curve_intensity);
        BKE_curvemapping_init(gpmd->curve_intensity);
      }
    }
    else if (md.type == eGpencilModifierType_Opacity) {
      OpacityGpencilModifierData *gpmd = reinterpret_cast<OpacityGpencilModifierData *>(&md);
      BLO_read_struct(reader, CurveMapping, &gpmd->curve_intensity);
      if (gpmd->curve_intensity) {
        BKE_curvemapping_blend_read(reader, gpmd->curve_intensity);
        BKE_curvemapping_init(gpmd->curve_intensity);
      }
    }
    else if (md.type == eGpencilModifierType_Dash) {
      DashGpencilModifierData *gpmd = reinterpret_cast<DashGpencilModifierData *>(&md);
      BLO_read_struct_array(
          reader, DashGpencilModifierSegment, gpmd->segments_len, &gpmd->segments);
      for (int i = 0; i < gpmd->segments_len; i++) {
        gpmd->segments[i].dmd = gpmd;
      }
    }
    else if (md.type == eGpencilModifierType_Time) {
      TimeGpencilModifierData *gpmd = reinterpret_cast<TimeGpencilModifierData *>(&md);
      BLO_read_struct_array(
          reader, TimeGpencilModifierSegment, gpmd->segments_len, &gpmd->segments);
      for (int i = 0; i < gpmd->segments_len; i++) {
        gpmd->segments[i].gpmd = gpmd;
      }
    }
    if (md.type == eGpencilModifierType_Shrinkwrap) {
      ShrinkwrapGpencilModifierData *gpmd = reinterpret_cast<ShrinkwrapGpencilModifierData *>(&md);
      gpmd->cache_data = nullptr;
    }
  }
}

}  // namespace blender
