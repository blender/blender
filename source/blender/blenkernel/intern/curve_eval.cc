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
 */

#include "BLI_array.hh"
#include "BLI_index_range.hh"
#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_span.hh"
#include "BLI_string_ref.hh"
#include "BLI_task.hh"
#include "BLI_vector.hh"

#include "DNA_curve_types.h"

#include "BKE_anonymous_attribute.hh"
#include "BKE_curve.h"
#include "BKE_spline.hh"

using blender::Array;
using blender::float3;
using blender::float4x4;
using blender::IndexRange;
using blender::Map;
using blender::MutableSpan;
using blender::Span;
using blender::StringRefNull;
using blender::Vector;
using blender::bke::AttributeIDRef;

blender::Span<SplinePtr> CurveEval::splines() const
{
  return splines_;
}

blender::MutableSpan<SplinePtr> CurveEval::splines()
{
  return splines_;
}

/**
 * \return True if the curve contains a spline with the given type.
 *
 * \note If you are looping over all of the splines in the same scope anyway,
 * it's better to avoid calling this function, in case there are many splines.
 */
bool CurveEval::has_spline_with_type(const Spline::Type type) const
{
  for (const SplinePtr &spline : this->splines()) {
    if (spline->type() == type) {
      return true;
    }
  }
  return false;
}

void CurveEval::resize(const int size)
{
  splines_.resize(size);
  attributes.reallocate(size);
}

/**
 * \warning Call #reallocate on the spline's attributes after adding all splines.
 */
void CurveEval::add_spline(SplinePtr spline)
{
  splines_.append(std::move(spline));
}

void CurveEval::remove_splines(blender::IndexMask mask)
{
  for (int i = mask.size() - 1; i >= 0; i--) {
    splines_.remove_and_reorder(mask.indices()[i]);
  }
}

void CurveEval::translate(const float3 &translation)
{
  for (SplinePtr &spline : this->splines()) {
    spline->translate(translation);
    spline->mark_cache_invalid();
  }
}

void CurveEval::transform(const float4x4 &matrix)
{
  for (SplinePtr &spline : this->splines()) {
    spline->transform(matrix);
  }
}

void CurveEval::bounds_min_max(float3 &min, float3 &max, const bool use_evaluated) const
{
  for (const SplinePtr &spline : this->splines()) {
    spline->bounds_min_max(min, max, use_evaluated);
  }
}

/**
 * Return the start indices for each of the curve spline's control points, if they were part
 * of a flattened array. This can be used to facilitate parallelism by avoiding the need to
 * accumulate an offset while doing more complex calculations.
 *
 * \note The result array is one longer than the spline count; the last element is the total size.
 */
blender::Array<int> CurveEval::control_point_offsets() const
{
  Array<int> offsets(splines_.size() + 1);
  int offset = 0;
  for (const int i : splines_.index_range()) {
    offsets[i] = offset;
    offset += splines_[i]->size();
  }
  offsets.last() = offset;
  return offsets;
}

/**
 * Exactly like #control_point_offsets, but uses the number of evaluated points instead.
 */
blender::Array<int> CurveEval::evaluated_point_offsets() const
{
  Array<int> offsets(splines_.size() + 1);
  int offset = 0;
  for (const int i : splines_.index_range()) {
    offsets[i] = offset;
    offset += splines_[i]->evaluated_points_size();
  }
  offsets.last() = offset;
  return offsets;
}

/**
 * Return the accumulated length at the start of every spline in the curve.
 *
 * \note The result is one longer than the spline count; the last element is the total length.
 */
blender::Array<float> CurveEval::accumulated_spline_lengths() const
{
  Array<float> spline_lengths(splines_.size() + 1);
  float spline_length = 0.0f;
  for (const int i : splines_.index_range()) {
    spline_lengths[i] = spline_length;
    spline_length += splines_[i]->length();
  }
  spline_lengths.last() = spline_length;
  return spline_lengths;
}

void CurveEval::mark_cache_invalid()
{
  for (SplinePtr &spline : splines_) {
    spline->mark_cache_invalid();
  }
}

static BezierSpline::HandleType handle_type_from_dna_bezt(const eBezTriple_Handle dna_handle_type)
{
  switch (dna_handle_type) {
    case HD_FREE:
      return BezierSpline::HandleType::Free;
    case HD_AUTO:
      return BezierSpline::HandleType::Auto;
    case HD_VECT:
      return BezierSpline::HandleType::Vector;
    case HD_ALIGN:
      return BezierSpline::HandleType::Align;
    case HD_AUTO_ANIM:
      return BezierSpline::HandleType::Auto;
    case HD_ALIGN_DOUBLESIDE:
      return BezierSpline::HandleType::Align;
  }
  BLI_assert_unreachable();
  return BezierSpline::HandleType::Auto;
}

static Spline::NormalCalculationMode normal_mode_from_dna_curve(const int twist_mode)
{
  switch (twist_mode) {
    case CU_TWIST_Z_UP:
      return Spline::NormalCalculationMode::ZUp;
    case CU_TWIST_MINIMUM:
      return Spline::NormalCalculationMode::Minimum;
    case CU_TWIST_TANGENT:
      return Spline::NormalCalculationMode::Tangent;
  }
  BLI_assert_unreachable();
  return Spline::NormalCalculationMode::Minimum;
}

static NURBSpline::KnotsMode knots_mode_from_dna_nurb(const short flag)
{
  switch (flag & (CU_NURB_ENDPOINT | CU_NURB_BEZIER)) {
    case CU_NURB_ENDPOINT:
      return NURBSpline::KnotsMode::EndPoint;
    case CU_NURB_BEZIER:
      return NURBSpline::KnotsMode::Bezier;
    default:
      return NURBSpline::KnotsMode::Normal;
  }

  BLI_assert_unreachable();
  return NURBSpline::KnotsMode::Normal;
}

static SplinePtr spline_from_dna_bezier(const Nurb &nurb)
{
  std::unique_ptr<BezierSpline> spline = std::make_unique<BezierSpline>();
  spline->set_resolution(nurb.resolu);
  spline->set_cyclic(nurb.flagu & CU_NURB_CYCLIC);

  Span<const BezTriple> src_points{nurb.bezt, nurb.pntsu};
  spline->resize(src_points.size());
  MutableSpan<float3> positions = spline->positions();
  MutableSpan<float3> handle_positions_left = spline->handle_positions_left();
  MutableSpan<float3> handle_positions_right = spline->handle_positions_right();
  MutableSpan<BezierSpline::HandleType> handle_types_left = spline->handle_types_left();
  MutableSpan<BezierSpline::HandleType> handle_types_right = spline->handle_types_right();
  MutableSpan<float> radii = spline->radii();
  MutableSpan<float> tilts = spline->tilts();

  blender::threading::parallel_for(src_points.index_range(), 2048, [&](IndexRange range) {
    for (const int i : range) {
      const BezTriple &bezt = src_points[i];
      positions[i] = bezt.vec[1];
      handle_positions_left[i] = bezt.vec[0];
      handle_types_left[i] = handle_type_from_dna_bezt((eBezTriple_Handle)bezt.h1);
      handle_positions_right[i] = bezt.vec[2];
      handle_types_right[i] = handle_type_from_dna_bezt((eBezTriple_Handle)bezt.h2);
      radii[i] = bezt.radius;
      tilts[i] = bezt.tilt;
    }
  });

  return spline;
}

static SplinePtr spline_from_dna_nurbs(const Nurb &nurb)
{
  std::unique_ptr<NURBSpline> spline = std::make_unique<NURBSpline>();
  spline->set_resolution(nurb.resolu);
  spline->set_cyclic(nurb.flagu & CU_NURB_CYCLIC);
  spline->set_order(nurb.orderu);
  spline->knots_mode = knots_mode_from_dna_nurb(nurb.flagu);

  Span<const BPoint> src_points{nurb.bp, nurb.pntsu};
  spline->resize(src_points.size());
  MutableSpan<float3> positions = spline->positions();
  MutableSpan<float> weights = spline->weights();
  MutableSpan<float> radii = spline->radii();
  MutableSpan<float> tilts = spline->tilts();

  blender::threading::parallel_for(src_points.index_range(), 2048, [&](IndexRange range) {
    for (const int i : range) {
      const BPoint &bp = src_points[i];
      positions[i] = bp.vec;
      weights[i] = bp.vec[3];
      radii[i] = bp.radius;
      tilts[i] = bp.tilt;
    }
  });

  return spline;
}

static SplinePtr spline_from_dna_poly(const Nurb &nurb)
{
  std::unique_ptr<PolySpline> spline = std::make_unique<PolySpline>();
  spline->set_cyclic(nurb.flagu & CU_NURB_CYCLIC);

  Span<const BPoint> src_points{nurb.bp, nurb.pntsu};
  spline->resize(src_points.size());
  MutableSpan<float3> positions = spline->positions();
  MutableSpan<float> radii = spline->radii();
  MutableSpan<float> tilts = spline->tilts();

  blender::threading::parallel_for(src_points.index_range(), 2048, [&](IndexRange range) {
    for (const int i : range) {
      const BPoint &bp = src_points[i];
      positions[i] = bp.vec;
      radii[i] = bp.radius;
      tilts[i] = bp.tilt;
    }
  });

  return spline;
}

std::unique_ptr<CurveEval> curve_eval_from_dna_curve(const Curve &dna_curve,
                                                     const ListBase &nurbs_list)
{
  Vector<const Nurb *> nurbs(nurbs_list);

  std::unique_ptr<CurveEval> curve = std::make_unique<CurveEval>();
  curve->resize(nurbs.size());
  MutableSpan<SplinePtr> splines = curve->splines();

  blender::threading::parallel_for(nurbs.index_range(), 256, [&](IndexRange range) {
    for (const int i : range) {
      switch (nurbs[i]->type) {
        case CU_BEZIER:
          splines[i] = spline_from_dna_bezier(*nurbs[i]);
          break;
        case CU_NURBS:
          splines[i] = spline_from_dna_nurbs(*nurbs[i]);
          break;
        case CU_POLY:
          splines[i] = spline_from_dna_poly(*nurbs[i]);
          break;
        default:
          BLI_assert_unreachable();
          break;
      }
    }
  });

  /* Normal mode is stored separately in each spline to facilitate combining
   * splines from multiple curve objects, where the value may be different. */
  const Spline::NormalCalculationMode normal_mode = normal_mode_from_dna_curve(
      dna_curve.twist_mode);
  for (SplinePtr &spline : curve->splines()) {
    spline->normal_mode = normal_mode;
  }

  return curve;
}

std::unique_ptr<CurveEval> curve_eval_from_dna_curve(const Curve &dna_curve)
{
  return curve_eval_from_dna_curve(dna_curve, *BKE_curve_nurbs_get_for_read(&dna_curve));
}

/**
 * Check the invariants that curve control point attributes should always uphold, necessary
 * because attributes are stored on splines rather than in a flat array on the curve:
 *  - The same set of attributes exists on every spline.
 *  - Attributes with the same name have the same type on every spline.
 */
void CurveEval::assert_valid_point_attributes() const
{
#ifdef DEBUG
  if (splines_.size() == 0) {
    return;
  }
  const int layer_len = splines_.first()->attributes.data.totlayer;
  Map<AttributeIDRef, AttributeMetaData> map;
  for (const SplinePtr &spline : splines_) {
    BLI_assert(spline->attributes.data.totlayer == layer_len);
    spline->attributes.foreach_attribute(
        [&](const AttributeIDRef &attribute_id, const AttributeMetaData &meta_data) {
          map.add_or_modify(
              attribute_id,
              [&](AttributeMetaData *map_data) {
                /* All unique attribute names should be added on the first spline. */
                BLI_assert(spline == splines_.first());
                *map_data = meta_data;
              },
              [&](AttributeMetaData *map_data) {
                /* Attributes on different splines should all have the same type. */
                BLI_assert(meta_data == *map_data);
              });
          return true;
        },
        ATTR_DOMAIN_POINT);
  }
#endif
}
