/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "usd_writer_hair.hh"
#include "usd_hierarchy_iterator.hh"

#include <pxr/usd/usdGeom/basisCurves.h>
#include <pxr/usd/usdGeom/tokens.h>

#include "BKE_particle.h"

#include "BLI_math_matrix.hh"
#include "BLI_math_vector_types.hh"

#include "DNA_particle_types.h"

namespace blender::io::usd {

USDHairWriter::USDHairWriter(const USDExporterContext &ctx) : USDAbstractWriter(ctx) {}

void USDHairWriter::do_write(HierarchyContext &context)
{
  ParticleSystem *psys = context.particle_system;
  ParticleCacheKey **cache = psys->pathcache;
  if (cache == nullptr) {
    return;
  }

  pxr::UsdTimeCode time = get_export_time_code();
  pxr::UsdGeomBasisCurves curves = pxr::UsdGeomBasisCurves::Define(usd_export_context_.stage,
                                                                   usd_export_context_.usd_path);

  if (psys->part->flag & PART_HAIR_BSPLINE) {
    curves.CreateBasisAttr(pxr::VtValue(pxr::UsdGeomTokens->bspline));
    curves.CreateTypeAttr(pxr::VtValue(pxr::UsdGeomTokens->cubic));
  }
  else {
    curves.CreateBasisAttr(pxr::VtValue(pxr::UsdGeomTokens->catmullRom));
    curves.CreateTypeAttr(pxr::VtValue(pxr::UsdGeomTokens->linear));
    curves.CreateWrapAttr(pxr::VtValue(pxr::UsdGeomTokens->pinned));
  }

  pxr::VtArray<pxr::GfVec3f> points;
  pxr::VtIntArray curve_point_counts;
  curve_point_counts.reserve(psys->totpart);

  /* Reverse current transform since the Hair curves will be placed under the object's Xform and we
   * don't want a double-transform to happen. */
  const blender::float4x4 inv = blender::math::invert(context.object->object_to_world());

  ParticleCacheKey *strand;
  for (int strand_index = 0; strand_index < psys->totpart; ++strand_index) {
    strand = cache[strand_index];

    int point_count = strand->segments + 1;
    curve_point_counts.push_back(point_count);

    for (int point_index = 0; point_index < point_count; ++point_index, ++strand) {
      const float3 vert = blender::math::transform_point(inv, float3(strand->co));
      points.push_back(pxr::GfVec3f(vert.x, vert.y, vert.z));
    }
  }

  pxr::UsdAttribute attr_points = curves.CreatePointsAttr(pxr::VtValue(), true);
  pxr::UsdAttribute attr_vertex_counts = curves.CreateCurveVertexCountsAttr(pxr::VtValue(), true);
  if (!attr_points.HasValue()) {
    attr_points.Set(points, pxr::UsdTimeCode::Default());
    attr_vertex_counts.Set(curve_point_counts, pxr::UsdTimeCode::Default());
  }
  usd_value_writer_.SetAttribute(attr_points, pxr::VtValue(points), time);
  usd_value_writer_.SetAttribute(attr_vertex_counts, pxr::VtValue(curve_point_counts), time);

  if (psys->totpart > 0) {
    pxr::VtArray<pxr::GfVec3f> colors;
    colors.push_back(pxr::GfVec3f(cache[0]->col));
    curves.CreateDisplayColorAttr(pxr::VtValue(colors));
  }

  if (psys->part) {
    auto prim = curves.GetPrim();
    add_to_prim_map(prim.GetPath(), &psys->part->id);
    write_id_properties(prim, psys->part->id, time);
  }

  this->author_extent(curves, time);
}

bool USDHairWriter::check_is_animated(const HierarchyContext & /*context*/) const
{
  return true;
}

}  // namespace blender::io::usd
