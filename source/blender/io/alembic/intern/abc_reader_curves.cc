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
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

#include "BKE_attribute.hh"
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
    return 0;
  }

  const PropertyHeader *header = user_props.getPropertyHeader(ABC_CURVE_RESOLUTION_U_PROPNAME);
  if (!header || !header->isScalar() || !IInt16Property::matches(*header)) {
    return 0;
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

static int8_t get_knot_mode(const Alembic::AbcGeom::CurveType abc_curve_type)
{
  if (abc_curve_type == Alembic::AbcGeom::kCubic) {
    return NURBS_KNOT_MODE_ENDPOINT;
  }

  return NURBS_KNOT_MODE_NORMAL;
}

static int get_curve_overlap(const P3fArraySamplePtr positions,
                             const int idx,
                             const int num_verts,
                             const int16_t order)
{
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

static inline int bezier_point_count(int alembic_count, bool is_cyclic)
{
  return is_cyclic ? (alembic_count / 3) : ((alembic_count / 3) + 1);
}

static inline float3 to_zup_float3(Imath::V3f v)
{
  float3 p;
  copy_zup_from_yup(p, v.getValue());
  return p;
}

static bool curves_topology_changed(const bke::CurvesGeometry &curves,
                                    Span<int> preprocessed_offsets)
{
  if (curves.offsets() != preprocessed_offsets) {
    return true;
  }
  return false;
}

template<typename SampleType>
static bool samples_have_same_topology(const SampleType &sample, const SampleType &ceil_sample)
{
  const P3fArraySamplePtr positions = sample.getPositions();
  const Int32ArraySamplePtr per_curve_vertices_count = sample.getCurvesNumVertices();

  const P3fArraySamplePtr ceil_positions = ceil_sample.getPositions();
  const Int32ArraySamplePtr ceil_per_curve_vertices_count = ceil_sample.getCurvesNumVertices();

  /* It the counters are different, we can be sure the topology is different. */
  const bool different_counters = positions->size() != ceil_positions->size() ||
                                  per_curve_vertices_count->size() !=
                                      ceil_per_curve_vertices_count->size();
  if (different_counters) {
    return false;
  }

  /* Otherwise check the curve vertex counts. */
  if (memcmp(per_curve_vertices_count->get(),
             ceil_per_curve_vertices_count->get(),
             per_curve_vertices_count->size() * sizeof(int)))
  {
    return false;
  }

  return true;
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
  CurveType curve_type = CURVE_TYPE_POLY;
  int8_t knot_mode = 0;

  /* Optional settings for reading interpolated vertices. If present, `ceil_positions` has to be
   * valid. */
  std::optional<SampleInterpolationSettings> interpolation_settings;

  /* Store the pointers during preprocess so we do not have to look up the sample twice. */
  P3fArraySamplePtr positions = nullptr;
  P3fArraySamplePtr ceil_positions = nullptr;
  FloatArraySamplePtr weights = nullptr;
  FloatArraySamplePtr radii = nullptr;
};

/* Compute topological information about the curves. We do this step mainly to properly account
 * for curves overlaps which imply different offsets between Blender and Alembic, but also to
 * validate the data and cache some values. */
static std::optional<PreprocessedSampleData> preprocess_sample(StringRefNull iobject_name,
                                                               bool use_interpolation,
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

  /* NOTE: although Alembic can store knots, we do not read them as the functionality is not
   * exposed by the Blender's Curves API yet. */
  const Int32ArraySamplePtr per_curve_vertices_count = smp.getCurvesNumVertices();
  const P3fArraySamplePtr positions = smp.getPositions();
  const FloatArraySamplePtr weights = smp.getPositionWeights();
  const CurvePeriodicity periodicity = smp.getWrap();
  const UcharArraySamplePtr orders = smp.getOrders();

  if (positions->size() == 0) {
    return {};
  }

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
  data.knot_mode = get_knot_mode(smp.getType());
  data.do_cyclic = periodicity == Alembic::AbcGeom::kPeriodic;

  /* If #kVariableOrder is set then we must have order data. If not, this sample is suspect.
   * Interpret the data as linear as a fallback. See #126324 for one such example.
   * See also: Alembic source code in `ICurves.h`, #ICurvesSchema::Sample::valid() */
  if (smp.getType() == Alembic::AbcGeom::kVariableOrder && !orders) {
    data.curve_type = CURVE_TYPE_POLY;
    data.knot_mode = NURBS_KNOT_MODE_NORMAL;
    data.do_cyclic = false;
  }

  if (data.curve_type == CURVE_TYPE_NURBS) {
    data.curves_orders.resize(curve_count);
  }

  /* Compute topological information. */

  int blender_offset = 0;
  int alembic_offset = 0;
  for (size_t i = 0; i < curve_count; i++) {
    const int vertices_count = (*per_curve_vertices_count)[i];

    const int curve_order = get_curve_order(smp.getType(), orders, i);

    data.offset_in_blender[i] = blender_offset;
    data.offset_in_alembic[i] = alembic_offset;
    data.curves_cyclic[i] = data.do_cyclic;

    if (data.curve_type == CURVE_TYPE_NURBS) {
      data.curves_orders[i] = curve_order;
    }

    /* Some software writes repeated vertices to indicate periodicity but Blender
     * should skip these if present. */
    const int overlap = data.do_cyclic ?
                            get_curve_overlap(
                                positions, alembic_offset, vertices_count, curve_order) :
                            0;

    if (data.curve_type == CURVE_TYPE_BEZIER) {
      blender_offset += bezier_point_count(vertices_count, data.do_cyclic);
    }
    else {
      blender_offset += (overlap >= vertices_count) ? vertices_count : (vertices_count - overlap);
    }

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

  const std::optional<SampleInterpolationSettings> interpolation_settings =
      get_sample_interpolation_settings(
          sample_sel, schema.getTimeSampling(), schema.getNumSamples());

  if (use_interpolation && interpolation_settings.has_value()) {
    Alembic::AbcGeom::ICurvesSchema::Sample ceil_smp;
    schema.get(ceil_smp, Alembic::Abc::ISampleSelector(interpolation_settings->ceil_index));
    if (samples_have_same_topology(smp, ceil_smp)) {
      data.ceil_positions = ceil_smp.getPositions();
      data.interpolation_settings = interpolation_settings;
    }
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
    const char **r_err_str) const
{
  if (!Alembic::AbcGeom::ICurves::matches(alembic_header)) {
    *r_err_str = RPT_(
        "Object type mismatch, Alembic object path pointed to Curves when importing, but not "
        "anymore.");
    return false;
  }

  if (ob->type != OB_CURVES) {
    *r_err_str = RPT_("Object type mismatch, Alembic object path points to Curves.");
    return false;
  }

  return true;
}

void AbcCurveReader::readObjectData(Main *bmain, const Alembic::Abc::ISampleSelector &sample_sel)
{
  Curves *curves = BKE_curves_add(bmain, m_data_name.c_str());

  m_object = BKE_object_add_only_object(bmain, OB_CURVES, m_object_name.c_str());
  m_object->data = curves;

  read_curves_sample(curves, false, m_curves_schema, sample_sel);

  if (m_settings->always_add_cache_reader || has_animations(m_curves_schema, m_settings)) {
    addCacheModifier();
  }
}

BLI_INLINE float3 interpolate_to_zup(const Span<Imath::V3f> &floor_positions,
                                     const Span<Imath::V3f> &ceil_positions,
                                     int i,
                                     float weight)
{
  float3 p;
  const Imath::V3f &floor_pos = floor_positions[i];
  const Imath::V3f &ceil_pos = ceil_positions[i];

  interp_v3_v3v3(p, floor_pos.getValue(), ceil_pos.getValue(), weight);
  copy_zup_from_yup(p, p);
  return p;
}

static void add_bezier_control_point(int cp,
                                     int offset,
                                     const Span<Imath::V3f> floor_positions,
                                     const Span<Imath::V3f> ceil_positions,
                                     MutableSpan<float3> positions,
                                     MutableSpan<float3> handles_left,
                                     MutableSpan<float3> handles_right,
                                     float weight)
{
  positions[cp] = interpolate_to_zup(floor_positions, ceil_positions, offset, weight);
  if (offset == 0) {
    handles_right[cp] = interpolate_to_zup(floor_positions, ceil_positions, offset + 1, weight);
    handles_left[cp] = 2.0f * positions[cp] - handles_right[cp];
  }
  else if (offset == floor_positions.size() - 1) {
    handles_left[cp] = interpolate_to_zup(floor_positions, ceil_positions, offset - 1, weight);
    handles_right[cp] = 2.0f * positions[cp] - handles_left[cp];
  }
  else {
    handles_left[cp] = interpolate_to_zup(floor_positions, ceil_positions, offset - 1, weight);
    handles_right[cp] = interpolate_to_zup(floor_positions, ceil_positions, offset + 1, weight);
  }
}

void AbcCurveReader::read_curves_sample(Curves *curves_id,
                                        bool use_interpolation,
                                        const ICurvesSchema &schema,
                                        const ISampleSelector &sample_sel)
{
  std::optional<PreprocessedSampleData> opt_preprocess = preprocess_sample(
      m_iobject.getFullName(), use_interpolation, schema, sample_sel);
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
    int16_t curve_resolution = get_curve_resolution(schema, sample_sel);
    if (curve_resolution > 0) {
      curves.resolution_for_write().fill(curve_resolution);
    }
  }

  MutableSpan<float3> curves_positions = curves.positions_for_write();

  Span<Imath::V3f> alembic_points{&(*data.positions)[0], int64_t((*data.positions).size())};
  Span<Imath::V3f> alembic_points_ceil;
  float interp_weight = 0.0f;
  if (data.interpolation_settings.has_value()) {
    alembic_points_ceil = {&(*data.ceil_positions)[0], int64_t((*data.ceil_positions).size())};
    interp_weight = data.interpolation_settings->weight;
  }
  else {
    alembic_points_ceil = alembic_points;
  }

  if (data.curve_type == CURVE_TYPE_BEZIER) {
    curves.handle_types_left_for_write().fill(BEZIER_HANDLE_ALIGN);
    curves.handle_types_right_for_write().fill(BEZIER_HANDLE_ALIGN);

    MutableSpan<float3> handles_right = curves.handle_positions_right_for_write();
    MutableSpan<float3> handles_left = curves.handle_positions_left_for_write();

    int point_offset = 0;
    for (const int i_curve : curves.curves_range()) {
      const int alembic_point_offset = data.offset_in_alembic[i_curve];
      const int alembic_point_count = data.offset_in_alembic[i_curve + 1] - alembic_point_offset;
      const int cp_count = data.offset_in_blender[i_curve + 1] - data.offset_in_blender[i_curve];

      int cp_offset = 0;
      for (const int cp : IndexRange(cp_count)) {
        add_bezier_control_point(
            cp,
            cp_offset,
            alembic_points.slice(alembic_point_offset, alembic_point_count),
            alembic_points_ceil.slice(alembic_point_offset, alembic_point_count),
            curves_positions.slice(point_offset, point_count),
            handles_left.slice(point_offset, point_count),
            handles_right.slice(point_offset, point_count),
            interp_weight);
        cp_offset += 3;
      }

      point_offset += cp_count;
    }
  }
  else {
    for (const int i_curve : curves.curves_range()) {
      int position_offset = data.offset_in_alembic[i_curve];
      for (const int i_point : curves.points_by_curve()[i_curve]) {
        if (data.interpolation_settings.has_value()) {
          curves_positions[i_point] = interpolate_to_zup(
              alembic_points, alembic_points_ceil, position_offset++, interp_weight);
        }
        else {
          curves_positions[i_point] = to_zup_float3(alembic_points[position_offset++]);
        }
      }
    }
  }

  if (data.do_cyclic) {
    curves.cyclic_for_write().copy_from(data.curves_cyclic);
  }

  if (data.radii) {
    MutableSpan<float> radii = curves.radius_for_write();

    Alembic::Abc::FloatArraySample alembic_widths = *data.radii;
    for (const int i_point : curves.points_range()) {
      radii[i_point] = alembic_widths[i_point] / 2.0f;
    }
  }

  if (data.curve_type == CURVE_TYPE_NURBS) {
    curves.nurbs_orders_for_write().copy_from(data.curves_orders);
    curves.nurbs_knots_modes_for_write().fill(data.knot_mode);

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
                                   int read_flag,
                                   const char * /*velocity_name*/,
                                   const float /*velocity_scale*/,
                                   const char ** /*r_err_str*/)
{
  Curves *curves = geometry_set.get_curves_for_write();

  bool use_interpolation = read_flag & MOD_MESHSEQ_INTERPOLATE_VERTICES;
  read_curves_sample(curves, use_interpolation, m_curves_schema, sample_sel);
}

}  // namespace blender::io::alembic
