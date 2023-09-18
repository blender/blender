/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "usd_writer_hair.h"
#include "usd_hierarchy_iterator.h"

#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>

#include "BKE_material.h"

#include "BKE_particle.h"

#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"

#include "DNA_particle_types.h"

namespace blender::io::usd {

USDHairWriter::USDHairWriter(const USDExporterContext &ctx) : USDAbstractWriter(ctx) {}

// This was copied from source/intern/cycles/blender/blender_curves.cpp
static float shaperadius(float shape, float root, float tip, float time)
{
  assert(time >= 0.0f);
  assert(time <= 1.0f);
  float radius = 1.0f - time;

  if (shape != 0.0f) {
    if (shape < 0.0f)
      radius = powf(radius, 1.0f + shape);
    else
      radius = powf(radius, 1.0f / (1.0f - shape));
  }
  return (radius * (root - tip)) + tip;
}

void USDHairWriter::do_write(HierarchyContext &context)
{
  /* Get untransformed vertices, there's a xform under the hair. */
  float inv_mat[4][4];
  invert_m4_m4_safe(inv_mat, context.object->object_to_world);

  ParticleSystem *psys = context.particle_system;
  ParticleCacheKey **cache = psys->pathcache;
  if (cache == nullptr) {
    return;
  }

  pxr::UsdTimeCode timecode = get_export_time_code();
  pxr::UsdGeomBasisCurves curves =
      (usd_export_context_.export_params.export_as_overs) ?
          pxr::UsdGeomBasisCurves(
              usd_export_context_.stage->OverridePrim(usd_export_context_.usd_path)) :
          pxr::UsdGeomBasisCurves::Define(usd_export_context_.stage, usd_export_context_.usd_path);

  if (psys->part->flag & PART_HAIR_BSPLINE) {
    curves.CreateTypeAttr(pxr::VtValue(pxr::UsdGeomTokens->cubic));
    curves.CreateBasisAttr(pxr::VtValue(pxr::UsdGeomTokens->bspline));
  }
  else {
    curves.CreateTypeAttr(pxr::VtValue(pxr::UsdGeomTokens->linear));
    curves.CreateBasisAttr(pxr::VtValue(pxr::UsdGeomTokens->bezier));
  }

  curves.CreateWrapAttr(pxr::VtValue(pxr::UsdGeomTokens->nonperiodic));

  pxr::VtArray<pxr::GfVec3f> points;
  pxr::VtArray<float> widths;
  pxr::VtIntArray curve_point_counts;
  curve_point_counts.reserve(psys->totpart);

  float hair_root_rad = psys->part->rad_root * psys->part->rad_scale * 0.5f;
  float hair_tip_rad = psys->part->rad_tip * psys->part->rad_scale * 0.5f;
  float hair_shape = psys->part->shape;

  bool close_tip = (psys->part->shape_flag & PART_SHAPE_CLOSE_TIP) != 0;

  ParticleCacheKey *strand;
  for (int strand_index = 0; strand_index < psys->totpart; ++strand_index) {
    strand = cache[strand_index];

    int point_count = strand->segments + 1;
    curve_point_counts.push_back(point_count);

    for (int point_index = 0; point_index < point_count; ++point_index, ++strand) {
      float vert[3];
      copy_v3_v3(vert, strand->co);
      mul_m4_v3(inv_mat, vert);
      points.push_back(pxr::GfVec3f(vert));
      float t = (float)point_index / (float)(point_count - 1);
      // if(point_index == point_count - 1) t = 0.95f;
      float root_rad = (close_tip && point_index == point_count - 1) ? 0 : hair_tip_rad;
      widths.push_back(shaperadius(hair_shape, hair_root_rad, root_rad, t) * 2.0f);
    }
  }

  if (usd_export_context_.export_params.export_child_particles) {
    ParticleCacheKey **child_cache = psys->childcache;
    if (child_cache != nullptr) {
      for (int strand_index = 0; strand_index < psys->totchild; ++strand_index) {
        strand = child_cache[strand_index];

        int point_count = strand->segments + 1;
        curve_point_counts.push_back(point_count);

        for (int point_index = 0; point_index < point_count; ++point_index, ++strand) {
          float vert[3];
          copy_v3_v3(vert, strand->co);
          mul_m4_v3(inv_mat, vert);
          points.push_back(pxr::GfVec3f(vert));
          float t = (float)point_index / (float)(point_count - 1);
          t = clamp_f(t, 0.0f, 0.95f);
          widths.push_back(shaperadius(hair_shape, hair_root_rad, hair_tip_rad, t) * 2.0f);
        }
      }
    }
  }

  pxr::UsdAttribute attr_points = curves.CreatePointsAttr(pxr::VtValue(), true);
  pxr::UsdAttribute attr_vertex_counts = curves.CreateCurveVertexCountsAttr(pxr::VtValue(), true);
  pxr::UsdAttribute attr_widths = curves.CreateWidthsAttr(pxr::VtValue(), true);

  // NOTE (Marcelo Sercheli): Code to set values at default time was removed since
  // `timecode` will be default time in case of non-animation exports. For animated
  // exports, USD will inter/extrapolate values linearly.
  usd_value_writer_.SetAttribute(attr_points, pxr::VtValue(points), timecode);
  usd_value_writer_.SetAttribute(attr_vertex_counts, pxr::VtValue(curve_point_counts), timecode);
  usd_value_writer_.SetAttribute(attr_widths, pxr::VtValue(widths), timecode);

  if (psys->totpart > 0) {
    pxr::VtArray<pxr::GfVec3f> colors;
    colors.push_back(pxr::GfVec3f(cache[0]->col));
    curves.CreateDisplayColorAttr(pxr::VtValue(colors));
  }

  if (usd_export_context_.export_params.export_materials) {
    assign_material(context, curves);
  }

  if (usd_export_context_.export_params.export_custom_properties && psys->part) {
    auto prim = curves.GetPrim();
    write_id_properties(prim, psys->part->id, timecode);
  }

  this->author_extent(timecode, curves);
}

bool USDHairWriter::check_is_animated(const HierarchyContext & /*context*/) const
{
  return true;
}

void USDHairWriter::assign_material(const HierarchyContext &context,
                                    pxr::UsdGeomBasisCurves usd_curve)
{
  ParticleSystem *psys = context.particle_system;
  // In newer Blender builds this becomes: BKE_object_material_get
  Material *material = BKE_object_material_get(context.object, psys->part->omat);

  if (material == nullptr) {
    return;
  }

  pxr::UsdShadeMaterialBindingAPI api = pxr::UsdShadeMaterialBindingAPI(usd_curve.GetPrim());
  pxr::UsdShadeMaterial usd_material = ensure_usd_material(context, material);
  api.Bind(usd_material);

  /* USD seems to support neither per-material nor per-face-group double-sidedness, so we just
   * use the flag from the first non-empty material slot. */
  usd_curve.CreateDoubleSidedAttr(pxr::VtValue((material->blend_flag & MA_BL_CULL_BACKFACE) == 0));
}

}  // namespace blender::io::usd
