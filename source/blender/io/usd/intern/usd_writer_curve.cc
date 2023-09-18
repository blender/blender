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
#include "usd_writer_curve.h"
#include "usd_hierarchy_iterator.h"

#include <pxr/usd/usdGeom/basisCurves.h>
#include <pxr/usd/usdGeom/curves.h>
#include <pxr/usd/usdGeom/nurbsCurves.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>

#include "BKE_curve.h"
#include "BKE_material.h"

#include "BLI_math_geom.h"

#include "DNA_curve_types.h"

#include "WM_api.h"
#include "WM_types.h"

namespace blender::io::usd {

USDCurveWriter::USDCurveWriter(const USDExporterContext &ctx) : USDAbstractWriter(ctx) {}

void USDCurveWriter::do_write(HierarchyContext &context)
{
  // Because blender allows vector handles and auto handles all bezier curves are bezier
  // An optimization could be made to set usd type to linear if all controls are vector

  Curve *curve = static_cast<Curve *>(context.object->data);

  pxr::VtArray<pxr::GfVec3f> verts;
  pxr::VtArray<float> widths;

  std::vector<int> vert_counts;
  std::vector<float> weights;
  std::vector<float> knots;
  std::vector<uint8_t> orders;

  pxr::VtIntArray curve_point_counts;

  pxr::UsdTimeCode timecode = get_export_time_code();
  pxr::UsdGeomCurves curves;

  Nurb *nurbs = static_cast<Nurb *>(curve->nurb.first);
  if (nurbs == nullptr)
    return;

  int curveType = nurbs->type;

  for (; nurbs; nurbs = nurbs->next) {
    if (nurbs->type != curveType) {
      // We dont yet support writing curves with multiple types of curve data
      WM_reportf(RPT_WARNING, "Cannot export mixed curves");
      return;
    }
  }

  if (curveType == CU_NURBS) {
    curves = (usd_export_context_.export_params.export_as_overs) ?
                 pxr::UsdGeomNurbsCurves(
                     usd_export_context_.stage->OverridePrim(usd_export_context_.usd_path)) :
                 pxr::UsdGeomNurbsCurves::Define(usd_export_context_.stage,
                                                 usd_export_context_.usd_path);
  }
  else {
    curves = (usd_export_context_.export_params.export_as_overs) ?
                 pxr::UsdGeomBasisCurves(
                     usd_export_context_.stage->OverridePrim(usd_export_context_.usd_path)) :
                 pxr::UsdGeomBasisCurves::Define(usd_export_context_.stage,
                                                 usd_export_context_.usd_path);

    pxr::UsdGeomBasisCurves(curves).CreateWrapAttr(pxr::VtValue(pxr::UsdGeomTokens->nonperiodic));
  }

  nurbs = static_cast<Nurb *>(curve->nurb.first);
  for (; nurbs; nurbs = nurbs->next) {
    bool is_cyclic = false;

    if ((nurbs->flagu & CU_NURB_CYCLIC) != 0) {
      // Not needed for this conversion
      // curves.CreateWrapAttr(pxr::VtValue(pxr::UsdGeomTokens->periodic));
      is_cyclic = true;
    }

    if (nurbs->bp) {

      if (nurbs->type != CU_NURBS) {
        pxr::UsdGeomBasisCurves(curves).CreateBasisAttr(pxr::VtValue(pxr::UsdGeomTokens->bezier));
        pxr::UsdGeomBasisCurves(curves).CreateTypeAttr(pxr::VtValue(pxr::UsdGeomTokens->linear));
      }

      const long long totpoint = nurbs->pntsu * nurbs->pntsv;

      const BPoint *point = nurbs->bp;
      curve_point_counts.push_back(totpoint);

      for (int i = 0; i < totpoint; i++, point++) {
        verts.push_back(pxr::GfVec3f(point->vec));
        weights.push_back(point->vec[3]);
        widths.push_back(point->radius * curve->bevel_radius * 2.0f);
      }
    }
    else if (nurbs->bezt) {
      if (nurbs->type != CU_NURBS) {
        pxr::UsdGeomBasisCurves(curves).CreateBasisAttr(pxr::VtValue(pxr::UsdGeomTokens->bezier));
        pxr::UsdGeomBasisCurves(curves).CreateTypeAttr(pxr::VtValue(pxr::UsdGeomTokens->cubic));
      }

      const int totpoint = nurbs->pntsu;

      const BezTriple *bezier = nurbs->bezt;
      curve_point_counts.push_back((totpoint * 3) - (is_cyclic ? -1 : 2));

      /* TODO(kevin): store info about handles, Alembic doesn't have this. */
      for (int i = 0; i < totpoint; i++, bezier++) {

        if (i > 0) {
          verts.push_back(pxr::GfVec3f(bezier->vec[0]));
          widths.push_back(bezier->radius * curve->bevel_radius * 2.0f);
        }

        verts.push_back(pxr::GfVec3f(bezier->vec[1]));
        widths.push_back(bezier->radius * curve->bevel_radius * 2.0f);

        if (i < totpoint - 1 || is_cyclic) {
          verts.push_back(pxr::GfVec3f(bezier->vec[2]));
          widths.push_back(bezier->radius * curve->bevel_radius * 2.0f);
        }
      }

      if (is_cyclic) {
        verts.push_back(pxr::GfVec3f(nurbs->bezt->vec[0]));
        widths.push_back(nurbs->bezt->radius * curve->bevel_radius * 2.0f);

        verts.push_back(pxr::GfVec3f(nurbs->bezt->vec[1]));
        widths.push_back(nurbs->bezt->radius * curve->bevel_radius * 2.0f);
      }
    }
    // TODO: Implement knots
    // if (nurbs->knotsu != NULL) {
    //   const size_t num_knots = KNOTSU(nurbs);

    //   /* Add an extra knot at the beginning and end of the array since most apps
    //    * require/expect them. */
    //   knots.resize(num_knots + 2);

    //   for (int i = 0; i < num_knots; i++) {
    //     knots[i + 1] = nurbs->knotsu[i];
    //   }

    //   if ((nurbs->flagu & CU_NURB_CYCLIC) != 0) {
    //     knots[0] = nurbs->knotsu[0];
    //     knots[num_knots - 1] = nurbs->knotsu[num_knots - 1];
    //   }
    //   else {
    //     knots[0] = (2.0f * nurbs->knotsu[0] - nurbs->knotsu[1]);
    //     knots[num_knots - 1] = (2.0f * nurbs->knotsu[num_knots - 1] -
    //                             nurbs->knotsu[num_knots - 2]);
    //   }
    // }

    // orders.push_back(nurbs->orderu);
    // vert_counts.push_back(verts.size());
  }

  pxr::UsdAttribute attr_points = curves.CreatePointsAttr(pxr::VtValue(), true);
  pxr::UsdAttribute attr_vertex_counts = curves.CreateCurveVertexCountsAttr(pxr::VtValue(), true);
  pxr::UsdAttribute attr_widths = curves.CreateWidthsAttr(pxr::VtValue(), true);

  // NOTE (Marcelo Sercheli): Code to set values at default time was removed since
  // `timecode` will be default time in case of non-animation exports. For animated
  // exports, USD will inter/extrapolate values linearly.
  usd_value_writer_.SetAttribute(attr_points, pxr::VtValue(verts), timecode);
  usd_value_writer_.SetAttribute(attr_vertex_counts, pxr::VtValue(curve_point_counts), timecode);
  usd_value_writer_.SetAttribute(attr_widths, pxr::VtValue(widths), timecode);

  // USDGeomBasisCurves only allow binding one material to each basis curve.
  // In order to support Blender's curve material assignment we probably
  // need to create multiple Basis Curves per mat_nr
  assign_materials(context, curves);

  if (usd_export_context_.export_params.export_custom_properties && curve && curve->id.properties)
  {
    auto prim = curves.GetPrim();
    write_id_properties(prim, curve->id, timecode);
  }
}

bool USDCurveWriter::check_is_animated(const HierarchyContext &) const
{
  return true;
}

void USDCurveWriter::assign_materials(const HierarchyContext &context,
                                      pxr::UsdGeomCurves usd_curve)
{
  if (context.object->totcol == 0) {
    return;
  }

  bool curve_material_bound = false;
  for (short mat_num = 0; mat_num < context.object->totcol; mat_num++) {
    Material *material = BKE_object_material_get(context.object, mat_num + 1);
    if (material == nullptr) {
      continue;
    }

    pxr::UsdShadeMaterialBindingAPI api = pxr::UsdShadeMaterialBindingAPI(usd_curve.GetPrim());
    pxr::UsdShadeMaterial usd_material = ensure_usd_material(context, material);
    api.Bind(usd_material);

    /* USD seems to support neither per-material nor per-face-group double-sidedness, so we just
     * use the flag from the first non-empty material slot. */
    usd_curve.CreateDoubleSidedAttr(
        pxr::VtValue((material->blend_flag & MA_BL_CULL_BACKFACE) == 0));

    curve_material_bound = true;
    break;
  }

  if (!curve_material_bound) {
    /* Blender defaults to double-sided, but USD to single-sided. */
    usd_curve.CreateDoubleSidedAttr(pxr::VtValue(true));
  }

  if (!curve_material_bound) {
    /* Either all material slots were empty or there is only one material in use. As geometry
     * subsets are only written when actually used to assign a material, and the mesh already has
     * the material assigned, there is no need to continue. */
    return;
  }
}

}  // namespace blender::io::usd
