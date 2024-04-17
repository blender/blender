/* SPDX-FileCopyrightText: 2016 Kévin Dietrich. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup balembic
 */

#include <functional>
#include <memory>

#include "abc_writer_curves.h"
#include "intern/abc_axis_conversion.h"

#include "BLI_array_utils.hh"
#include "BLI_offset_indices.hh"

#include "DNA_curve_types.h"
#include "DNA_object_types.h"

#include "BKE_curve_legacy_convert.hh"
#include "BKE_curve_to_mesh.hh"
#include "BKE_curves.hh"
#include "BKE_lib_id.hh"
#include "BKE_mesh.hh"
#include "BKE_object.hh"

#include "CLG_log.h"
static CLG_LogRef LOG = {"io.alembic"};

using Alembic::AbcGeom::OCompoundProperty;
using Alembic::AbcGeom::OCurves;
using Alembic::AbcGeom::OCurvesSchema;
using Alembic::AbcGeom::OInt16Property;
using Alembic::AbcGeom::ON3fGeomParam;
using Alembic::AbcGeom::OV2fGeomParam;

namespace blender::io::alembic {

const std::string ABC_CURVE_RESOLUTION_U_PROPNAME("blender:resolution");

static inline Imath::V3f to_yup_V3f(float3 v)
{
  Imath::V3f p;
  copy_yup_from_zup(p.getValue(), v);
  return p;
}

ABCCurveWriter::ABCCurveWriter(const ABCWriterConstructorArgs &args) : ABCAbstractWriter(args) {}

void ABCCurveWriter::create_alembic_objects(const HierarchyContext *context)
{
  CLOG_INFO(&LOG, 2, "exporting %s", args_.abc_path.c_str());
  abc_curve_ = OCurves(args_.abc_parent, args_.abc_name, timesample_index_);
  abc_curve_schema_ = abc_curve_.getSchema();

  /* TODO: Blender supports per-curve resolutions but we're only using the first curve's data
   * here. Investigate using OInt16ArrayProperty to write out all the data but do so efficiently.
   * e.g. Write just a single value if all curves share the same resolution etc. */

  int resolution_u = 1;
  switch (context->object->type) {
    case OB_CURVES_LEGACY: {
      Curve *curves_id = static_cast<Curve *>(context->object->data);
      resolution_u = curves_id->resolu;
      break;
    }
    case OB_CURVES: {
      Curves *curves_id = static_cast<Curves *>(context->object->data);
      const bke::CurvesGeometry &curves = curves_id->geometry.wrap();
      resolution_u = curves.resolution().first();
      break;
    }
  }

  OCompoundProperty user_props = abc_curve_schema_.getUserProperties();
  OInt16Property user_prop_resolu(user_props, ABC_CURVE_RESOLUTION_U_PROPNAME);
  user_prop_resolu.set(resolution_u);
}

Alembic::Abc::OObject ABCCurveWriter::get_alembic_object() const
{
  return abc_curve_;
}

Alembic::Abc::OCompoundProperty ABCCurveWriter::abc_prop_for_custom_props()
{
  return abc_schema_prop_for_custom_props(abc_curve_schema_);
}

void ABCCurveWriter::do_write(HierarchyContext &context)
{
  const Curves *curves_id;
  std::unique_ptr<Curves, std::function<void(Curves *)>> converted_curves;

  switch (context.object->type) {
    case OB_CURVES_LEGACY: {
      const Curve *legacy_curve = static_cast<Curve *>(context.object->data);
      converted_curves = std::unique_ptr<Curves, std::function<void(Curves *)>>(
          bke::curve_legacy_to_curves(*legacy_curve), [](Curves *c) { BKE_id_free(nullptr, c); });
      curves_id = converted_curves.get();
      break;
    }
    case OB_CURVES:
      curves_id = static_cast<Curves *>(context.object->data);
      break;
    default:
      BLI_assert_unreachable();
      return;
  }

  const bke::CurvesGeometry &curves = curves_id->geometry.wrap();
  if (curves.points_num() == 0) {
    return;
  }

  /* Alembic only supports 1 curve type / periodicity combination per object. Enforce this here.
   * See: Alembic source code for OCurves.h as no documentation explicitly exists for this. */
  const std::array<int, CURVE_TYPES_NUM> &curve_type_counts = curves.curve_type_counts();
  const int number_of_curve_types = std::count_if(curve_type_counts.begin(),
                                                  curve_type_counts.end(),
                                                  [](const int count) { return count > 0; });
  if (number_of_curve_types > 1) {
    CLOG_WARN(&LOG, "Cannot export mixed curve types in the same Curves object");
    return;
  }

  if (array_utils::booleans_mix_calc(curves.cyclic()) == array_utils::BooleanMix::Mixed) {
    CLOG_WARN(&LOG, "Cannot export mixed cyclic and non-cyclic curves in the same Curves object");
    return;
  }

  const bool is_cyclic = curves.cyclic().first();
  Alembic::AbcGeom::BasisType curve_basis = Alembic::AbcGeom::kNoBasis;
  Alembic::AbcGeom::CurveType curve_type = Alembic::AbcGeom::kVariableOrder;
  Alembic::AbcGeom::CurvePeriodicity periodicity = is_cyclic ? Alembic::AbcGeom::kPeriodic :
                                                               Alembic::AbcGeom::kNonPeriodic;
  const CurveType blender_curve_type = CurveType(curves.curve_types().first());
  switch (blender_curve_type) {
    case CURVE_TYPE_POLY:
      curve_basis = Alembic::AbcGeom::kNoBasis;
      curve_type = Alembic::AbcGeom::kVariableOrder;
      break;
    case CURVE_TYPE_CATMULL_ROM:
      curve_basis = Alembic::AbcGeom::kCatmullromBasis;
      curve_type = Alembic::AbcGeom::kVariableOrder;
      break;
    case CURVE_TYPE_BEZIER:
      curve_basis = Alembic::AbcGeom::kBezierBasis;
      curve_type = Alembic::AbcGeom::kCubic;
      break;
    case CURVE_TYPE_NURBS:
      curve_basis = Alembic::AbcGeom::kBsplineBasis;
      curve_type = Alembic::AbcGeom::kVariableOrder;
      break;
  }

  std::vector<Imath::V3f> verts;
  std::vector<int32_t> vert_counts;
  std::vector<float> widths;
  std::vector<float> weights;
  std::vector<float> knots;
  std::vector<uint8_t> orders;

  const Span<float3> positions = curves.positions();
  const Span<float> nurbs_weights = curves.nurbs_weights();
  const VArray<int8_t> nurbs_orders = curves.nurbs_orders();
  const bke::AttributeAccessor curve_attributes = curves.attributes();
  const VArray<float> radii = *curve_attributes.lookup_or_default<float>(
      "radius", bke::AttrDomain::Point, 0.01f);

  vert_counts.resize(curves.curves_num());
  const OffsetIndices points_by_curve = curves.points_by_curve();
  if (blender_curve_type == CURVE_TYPE_BEZIER) {
    const Span<float3> handles_l = curves.handle_positions_left();
    const Span<float3> handles_r = curves.handle_positions_right();

    for (const int i_curve : curves.curves_range()) {
      const IndexRange points = points_by_curve[i_curve];
      const size_t current_vert_count = verts.size();

      const int start_point_index = points.first();
      const int last_point_index = points.last();

      /* Vert order in the bezier curve representation is:
       * [
       *   control point 0(+ width), right handle 0, left handle 1,
       *   control point 1(+ width), right handle 1, left handle 2,
       *   control point 2(+ width), ...
       * ] */
      for (const int i_point : points.drop_back(1)) {
        verts.push_back(to_yup_V3f(positions[i_point]));
        widths.push_back(radii[i_point] * 2.0f);

        verts.push_back(to_yup_V3f(handles_r[i_point]));
        verts.push_back(to_yup_V3f(handles_l[i_point + 1]));
      }

      /* The last vert in the array doesn't need a right handle because the curve stops
       * at that point. */
      verts.push_back(to_yup_V3f(positions[last_point_index]));
      widths.push_back(radii[last_point_index] * 2.0f);

      /* If the curve is cyclic, include the right handle of the last point and the
       * left handle of the first point. */
      if (is_cyclic) {
        verts.push_back(to_yup_V3f(handles_r[last_point_index]));
        verts.push_back(to_yup_V3f(handles_l[start_point_index]));
      }

      vert_counts[i_curve] = verts.size() - current_vert_count;
    }
  }
  else {
    verts.resize(curves.points_num());
    widths.resize(curves.points_num());
    for (const int i_point : curves.points_range()) {
      verts[i_point] = to_yup_V3f(positions[i_point]);
      widths[i_point] = radii[i_point] * 2.0f;
    }

    if (blender_curve_type == CURVE_TYPE_NURBS) {
      weights.resize(curves.points_num());
      std::copy_n(nurbs_weights.data(), weights.size(), weights.data());

      orders.resize(curves.curves_num());
      for (const int i_curve : curves.curves_range()) {
        orders[i_curve] = nurbs_orders[i_curve];
      }
    }

    offset_indices::copy_group_sizes(points_by_curve, points_by_curve.index_range(), vert_counts);
  }

  Alembic::AbcGeom::OFloatGeomParam::Sample width_sample;
  width_sample.setVals(widths);

  OCurvesSchema::Sample sample(verts,
                               vert_counts,
                               curve_type,
                               periodicity,
                               width_sample,
                               OV2fGeomParam::Sample(), /* UVs */
                               ON3fGeomParam::Sample(), /* normals */
                               curve_basis,
                               weights,
                               orders,
                               knots);

  update_bounding_box(context.object);
  sample.setSelfBounds(bounding_box_);
  abc_curve_schema_.set(sample);
}

ABCCurveMeshWriter::ABCCurveMeshWriter(const ABCWriterConstructorArgs &args)
    : ABCGenericMeshWriter(args)
{
}

Mesh *ABCCurveMeshWriter::get_export_mesh(Object *object_eval, bool &r_needsfree)
{
  switch (object_eval->type) {
    case OB_CURVES_LEGACY: {
      Mesh *mesh_eval = BKE_object_get_evaluated_mesh(object_eval);
      if (mesh_eval != nullptr) {
        /* Mesh_eval only exists when generative modifiers are in use. */
        r_needsfree = false;
        return mesh_eval;
      }

      r_needsfree = true;
      return BKE_mesh_new_nomain_from_curve(object_eval);
    }

    case OB_CURVES:
      const bke::AnonymousAttributePropagationInfo propagation_info;
      Curves *curves = static_cast<Curves *>(object_eval->data);
      r_needsfree = true;
      return bke::curve_to_wire_mesh(curves->geometry.wrap(), propagation_info);
  }

  return nullptr;
}

}  // namespace blender::io::alembic
