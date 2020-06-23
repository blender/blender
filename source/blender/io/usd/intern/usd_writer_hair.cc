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
 *
 * The Original Code is Copyright (C) 2019 Blender Foundation.
 * All rights reserved.
 */
#include "usd_writer_hair.h"
#include "usd_hierarchy_iterator.h"

#include <pxr/usd/usdGeom/basisCurves.h>
#include <pxr/usd/usdGeom/tokens.h>

#include "BKE_particle.h"

#include "DNA_particle_types.h"

namespace blender {
namespace io {
namespace usd {

USDHairWriter::USDHairWriter(const USDExporterContext &ctx) : USDAbstractWriter(ctx)
{
}

void USDHairWriter::do_write(HierarchyContext &context)
{
  ParticleSystem *psys = context.particle_system;
  ParticleCacheKey **cache = psys->pathcache;
  if (cache == nullptr) {
    return;
  }

  pxr::UsdTimeCode timecode = get_export_time_code();
  pxr::UsdGeomBasisCurves curves = pxr::UsdGeomBasisCurves::Define(usd_export_context_.stage,
                                                                   usd_export_context_.usd_path);

  // TODO(Sybren): deal with (psys->part->flag & PART_HAIR_BSPLINE)
  curves.CreateBasisAttr(pxr::VtValue(pxr::UsdGeomTokens->bspline));
  curves.CreateTypeAttr(pxr::VtValue(pxr::UsdGeomTokens->cubic));

  pxr::VtArray<pxr::GfVec3f> points;
  pxr::VtIntArray curve_point_counts;
  curve_point_counts.reserve(psys->totpart);

  ParticleCacheKey *strand;
  for (int strand_index = 0; strand_index < psys->totpart; ++strand_index) {
    strand = cache[strand_index];

    int point_count = strand->segments + 1;
    curve_point_counts.push_back(point_count);

    for (int point_index = 0; point_index < point_count; ++point_index, ++strand) {
      points.push_back(pxr::GfVec3f(strand->co));
    }
  }

  pxr::UsdAttribute attr_points = curves.CreatePointsAttr(pxr::VtValue(), true);
  pxr::UsdAttribute attr_vertex_counts = curves.CreateCurveVertexCountsAttr(pxr::VtValue(), true);
  if (!attr_points.HasValue()) {
    attr_points.Set(points, pxr::UsdTimeCode::Default());
    attr_vertex_counts.Set(curve_point_counts, pxr::UsdTimeCode::Default());
  }
  usd_value_writer_.SetAttribute(attr_points, pxr::VtValue(points), timecode);
  usd_value_writer_.SetAttribute(attr_vertex_counts, pxr::VtValue(curve_point_counts), timecode);

  if (psys->totpart > 0) {
    pxr::VtArray<pxr::GfVec3f> colors;
    colors.push_back(pxr::GfVec3f(cache[0]->col));
    curves.CreateDisplayColorAttr(pxr::VtValue(colors));
  }
}

bool USDHairWriter::check_is_animated(const HierarchyContext &) const
{
  return true;
}

}  // namespace usd
}  // namespace io
}  // namespace blender
