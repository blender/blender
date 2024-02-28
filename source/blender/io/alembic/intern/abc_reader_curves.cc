/* SPDX-FileCopyrightText: 2016 Kévin Dietrich. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup balembic
 */

#include "abc_reader_curves.h"
#include "abc_axis_conversion.h"
#include "abc_util.h"

#include <cstdio>

#include "DNA_curves_types.h"
#include "DNA_object_types.h"

#include "BKE_attribute.hh"
#include "BKE_curve.hh"
#include "BKE_curves.hh"
#include "BKE_geometry_set.hh"
#include "BKE_object.hh"

#include "BLI_vector.hh"

#include "BLT_translation.hh"

using Alembic::Abc::FloatArraySamplePtr;
using Alembic::Abc::Int32ArraySamplePtr;
using Alembic::Abc::P3fArraySamplePtr;
using Alembic::Abc::PropertyHeader;
using Alembic::Abc::UcharArraySamplePtr;

using Alembic::AbcGeom::CurvePeriodicity;
using Alembic::AbcGeom::ICompoundProperty;
using Alembic::AbcGeom::ICurves;
using Alembic::AbcGeom::ICurvesSchema;
using Alembic::AbcGeom::IFloatGeomParam;
using Alembic::AbcGeom::IInt16Property;
using Alembic::AbcGeom::ISampleSelector;
using Alembic::AbcGeom::kWrapExisting;

namespace blender::io::alembic {
static int16_t get_curve_resolution(const ICurvesSchema &schema,
                                    const Alembic::Abc::ISampleSelector &sample_sel)
{
  ICompoundProperty user_props = schema.getUserProperties();
  if (!user_props) {
    return 1;
  }

  const PropertyHeader *header = user_props.getPropertyHeader(ABC_CURVE_RESOLUTION_U_PROPNAME);
  if (!header || !header->isScalar() || !IInt16Property::matches(*header)) {
    return 1;
  }

  IInt16Property resolu(user_props, header->getName());
  return resolu.getValue(sample_sel);
}

static int16_t get_curve_order(const Alembic::AbcGeom::CurveType abc_curve_type,
                               const UcharArraySamplePtr orders,
                               const size_t curve_index)
{
  switch (abc_curve_type) {
    case Alembic::AbcGeom::kCubic:
      return 4;
    case Alembic::AbcGeom::kVariableOrder:
      if (orders && orders->size() > curve_index) {
        return int16_t((*orders)[curve_index]);
      }
      ATTR_FALLTHROUGH;
    case Alembic::AbcGeom::kLinear:
    default:
      return 2;
  }
}

static int get_curve_overlap(const Alembic::AbcGeom::CurvePeriodicity periodicity,
                             const P3fArraySamplePtr positions,
                             const int idx,
                             const int num_verts,
                             const int16_t order)
{
  if (periodicity != Alembic::AbcGeom::kPeriodic) {
    /* kNonPeriodic is always assumed to have no overlap. */
    return 0;
  }

  /* Check the number of points which overlap, we don't have overlapping points in Blender, but
   * other software do use them to indicate that a curve is actually cyclic. Usually the number of
   * overlapping points is equal to the order/degree of the curve.
   */

  const int start = idx;
  const int end = idx + num_verts;
  int overlap = 0;

  const int safe_order = order <= num_verts ? order : num_verts;
  for (int j = start, k = end - safe_order; j < (start + safe_order); j++, k++) {
    const Imath::V3f &p1 = (*positions)[j];
    const Imath::V3f &p2 = (*positions)[k];

    if (p1 != p2) {
      break;
    }

    overlap++;
  }

  /* TODO: Special case, need to figure out how it coincides with knots. */
  if (overlap == 0 && num_verts > 2 && (*positions)[start] == (*positions)[end - 1]) {
    overlap = 1;
  }

  /* There is no real cycles. */
  return overlap;
}

static CurveType get_curve_type(const Alembic::AbcGeom::BasisType basis)
{
  switch (basis) {
    case Alembic::AbcGeom::kNoBasis:
      return CURVE_TYPE_POLY;
    case Alembic::AbcGeom::kBezierBasis:
      return CURVE_TYPE_BEZIER;
    case Alembic::AbcGeom::kBsplineBasis:
      return CURVE_TYPE_NURBS;
    case Alembic::AbcGeom::kCatmullromBasis:
      return CURVE_TYPE_CATMULL_ROM;
    case Alembic::AbcGeom::kHermiteBasis:
    case Alembic::AbcGeom::kPowerBasis:
      /* Those types are unknown to Blender, use a default poly type. */
      return CURVE_TYPE_POLY;
  }
  return CURVE_TYPE_POLY;
}

static bool curves_topology_changed(const bke::CurvesGeometry &curves,
                                    Span<int> preprocessed_offsets)
{
  if (curves.offsets() != preprocessed_offsets) {
    return true;
  }
  return false;
}

/* Preprocessed data to help and simplify converting curve data from Alembic to Blender.
 * As some operations may require to look up the Alembic sample multiple times, we just
 * do it once and cache the results in this.
 */
struct PreprocessedSampleData {
  /* This holds one value for each spline. This will be used to lookup the data at the right
   * indices, and will also be used to set #CurveGeometry.offsets. */
  Vector<int> offset_in_blender;
  /* This holds one value for each spline, and tells where in the Alembic curve sample the spline
   * actually starts, accounting for duplicate points indicating cyclicity. */
  Vector<int> offset_in_alembic;
  /* This holds one value for each spline to tell whether it is cyclic. */
  Vector<bool> curves_cyclic;
  /* This holds one value for each spline which define its order. */
  Vector<int8_t> curves_orders;

  /* True if any values of `curves_overlaps` is true. If so, we will need to copy the
   * `curves_overlaps` to an attribute on the Blender curves. */
  bool do_cyclic = false;

  /* Only one curve type for the whole objects. */
  CurveType curve_type;

  /* Store the pointers during preprocess so we do not have to look up the sample twice. */
  P3fArraySamplePtr positions = nullptr;
  FloatArraySamplePtr weights = nullptr;
  FloatArraySamplePtr radii = nullptr;
};

/* Compute topological information about the curves. We do this step mainly to properly account
 * for curves overlaps which imply different offsets between Blender and Alembic, but also to
 * validate the data and cache some values. */
static std::optional<PreprocessedSampleData> preprocess_sample(StringRefNull iobject_name,
                                                               const ICurvesSchema &schema,
                                                               const ISampleSelector sample_sel)
{

  ICurvesSchema::Sample smp;
  try {
    smp = schema.getValue(sample_sel);
  }
  catch (Alembic::Util::Exception &ex) {
    printf("Alembic: error reading curve sample for '%s/%s' at time %f: %s\n",
           iobject_name.c_str(),
           schema.getName().c_str(),
           sample_sel.getRequestedTime(),
           ex.what());
    return {};
  }

  /* Note: although Alembic can store knots, we do not read them as the functionality is not
   * exposed by the Blender's Curves API yet. */
  const Int32ArraySamplePtr per_curve_vertices_count = smp.getCurvesNumVertices();
  const P3fArraySamplePtr positions = smp.getPositions();
  const FloatArraySamplePtr weights = smp.getPositionWeights();
  const CurvePeriodicity periodicity = smp.getWrap();
  const UcharArraySamplePtr orders = smp.getOrders();

  const IFloatGeomParam widths_param = schema.getWidthsParam();
  FloatArraySamplePtr radii;
  if (widths_param.valid()) {
    IFloatGeomParam::Sample wsample = widths_param.getExpandedValue(sample_sel);
    radii = wsample.getVals();
  }

  const int curve_count = per_curve_vertices_count->size();

  PreprocessedSampleData data;
  /* Add 1 as these store offsets with the actual value being `offset[i + 1] - offset[i]`. */
  data.offset_in_blender.resize(curve_count + 1);
  data.offset_in_alembic.resize(curve_count + 1);
  data.curves_cyclic.resize(curve_count);
  data.curve_type = get_curve_type(smp.getBasis());

  if (data.curve_type == CURVE_TYPE_NURBS) {
    data.curves_orders.resize(curve_count);
  }

  /* Compute topological information. */

  int blender_offset = 0;
  int alembic_offset = 0;
  for (size_t i = 0; i < curve_count; i++) {
    const int vertices_count = (*per_curve_vertices_count)[i];

    const int curve_order = get_curve_order(smp.getType(), orders, i);

    /* Check if the curve is cyclic. */
    const int overlap = get_curve_overlap(
        periodicity, positions, alembic_offset, vertices_count, curve_order);

    data.offset_in_blender[i] = blender_offset;
    data.offset_in_alembic[i] = alembic_offset;
    data.curves_cyclic[i] = overlap != 0;

    if (data.curve_type == CURVE_TYPE_NURBS) {
      data.curves_orders[i] = curve_order;
    }

    data.do_cyclic |= data.curves_cyclic[i];
    blender_offset += (overlap >= vertices_count) ? vertices_count : (vertices_count - overlap);
    alembic_offset += vertices_count;
  }
  data.offset_in_blender[curve_count] = blender_offset;
  data.offset_in_alembic[curve_count] = alembic_offset;

  /* Store relevant pointers. */

  data.positions = positions;

  if (weights && weights->size() > 1) {
    data.weights = weights;
  }

  if (radii && radii->size() > 1) {
    data.radii = radii;
  }

  return data;
}

AbcCurveReader::AbcCurveReader(const Alembic::Abc::IObject &object, ImportSettings &settings)
    : AbcObjectReader(object, settings)
{
  ICurves abc_curves(object, kWrapExisting);
  m_curves_schema = abc_curves.getSchema();

  get_min_max_time(m_iobject, m_curves_schema, m_min_time, m_max_time);
}

bool AbcCurveReader::valid() const
{
  return m_curves_schema.valid();
}

bool AbcCurveReader::accepts_object_type(
    const Alembic::AbcCoreAbstract::ObjectHeader &alembic_header,
    const Object *const ob,
    const char **err_str) const
{
  if (!Alembic::AbcGeom::ICurves::matches(alembic_header)) {
    *err_str = RPT_(
        "Object type mismatch, Alembic object path pointed to Curves when importing, but not "
        "anymore.");
    return false;
  }

  if (ob->type != OB_CURVES) {
    *err_str = RPT_("Object type mismatch, Alembic object path points to Curves.");
    return false;
  }

  return true;
}

void AbcCurveReader::readObjectData(Main *bmain, const Alembic::Abc::ISampleSelector &sample_sel)
{
  Curves *curves = static_cast<Curves *>(BKE_curves_add(bmain, m_data_name.c_str()));

  m_object = BKE_object_add_only_object(bmain, OB_CURVES, m_object_name.c_str());
  m_object->data = curves;

  read_curves_sample(curves, m_curves_schema, sample_sel);

  if (m_settings->always_add_cache_reader || has_animations(m_curves_schema, m_settings)) {
    addCacheModifier();
  }
}

void AbcCurveReader::read_curves_sample(Curves *curves_id,
                                        const ICurvesSchema &schema,
                                        const ISampleSelector &sample_sel)
{
  std::optional<PreprocessedSampleData> opt_preprocess = preprocess_sample(
      m_iobject.getFullName(), schema, sample_sel);
  if (!opt_preprocess) {
    return;
  }

  const PreprocessedSampleData &data = opt_preprocess.value();

  const int point_count = data.offset_in_blender.last();
  const int curve_count = data.offset_in_blender.size() - 1;

  bke::CurvesGeometry &curves = curves_id->geometry.wrap();

  if (curves_topology_changed(curves, data.offset_in_blender)) {
    curves.resize(point_count, curve_count);
    curves.offsets_for_write().copy_from(data.offset_in_blender);
  }

  curves.fill_curve_types(data.curve_type);

  if (data.curve_type != CURVE_TYPE_POLY) {
    curves.resolution_for_write().fill(get_curve_resolution(schema, sample_sel));
  }

  MutableSpan<float3> curves_positions = curves.positions_for_write();
  for (const int i_curve : curves.curves_range()) {
    int position_offset = data.offset_in_alembic[i_curve];
    for (const int i_point : curves.points_by_curve()[i_curve]) {
      const Imath::V3f &pos = (*data.positions)[position_offset++];
      copy_zup_from_yup(curves_positions[i_point], pos.getValue());
    }
  }

  if (data.do_cyclic) {
    curves.cyclic_for_write().copy_from(data.curves_cyclic);
    curves.handle_types_left_for_write().fill(BEZIER_HANDLE_AUTO);
    curves.handle_types_right_for_write().fill(BEZIER_HANDLE_AUTO);
  }

  if (data.radii) {
    bke::SpanAttributeWriter<float> radii =
        curves.attributes_for_write().lookup_or_add_for_write_span<float>("radius",
                                                                          bke::AttrDomain::Point);

    for (const int i_curve : curves.curves_range()) {
      int position_offset = data.offset_in_alembic[i_curve];
      for (const int i_point : curves.points_by_curve()[i_curve]) {
        radii.span[i_point] = (*data.radii)[position_offset++];
      }
    }

    radii.finish();
  }

  if (data.curve_type == CURVE_TYPE_NURBS) {
    curves.nurbs_orders_for_write().copy_from(data.curves_orders);

    if (data.weights) {
      MutableSpan<float> curves_weights = curves.nurbs_weights_for_write();
      Span<float> data_weights_span = {data.weights->get(), int64_t(data.weights->size())};
      for (const int i_curve : curves.curves_range()) {
        const int alembic_offset = data.offset_in_alembic[i_curve];
        const IndexRange points = curves.points_by_curve()[i_curve];
        curves_weights.slice(points).copy_from(
            data_weights_span.slice(alembic_offset, points.size()));
      }
    }
  }
}

void AbcCurveReader::read_geometry(bke::GeometrySet &geometry_set,
                                   const Alembic::Abc::ISampleSelector &sample_sel,
                                   int /*read_flag*/,
                                   const char * /*velocity_name*/,
                                   const float /*velocity_scale*/,
                                   const char ** /*err_str*/)
{
  Curves *curves = geometry_set.get_curves_for_write();

  read_curves_sample(curves, m_curves_schema, sample_sel);
}

}  // namespace blender::io::alembic
