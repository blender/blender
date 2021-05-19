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
#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_span.hh"
#include "BLI_string_ref.hh"

#include "DNA_curve_types.h"

#include "BKE_curve.h"
#include "BKE_spline.hh"

using blender::Array;
using blender::float3;
using blender::float4x4;
using blender::Map;
using blender::Span;
using blender::StringRefNull;

blender::Span<SplinePtr> CurveEval::splines() const
{
  return splines_;
}

blender::MutableSpan<SplinePtr> CurveEval::splines()
{
  return splines_;
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
 * Return the start indices for each of the curve spline's evaluated points, as if they were part
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

std::unique_ptr<CurveEval> curve_eval_from_dna_curve(const Curve &dna_curve)
{
  std::unique_ptr<CurveEval> curve = std::make_unique<CurveEval>();

  const ListBase *nurbs = BKE_curve_nurbs_get(&const_cast<Curve &>(dna_curve));

  /* TODO: Optimize by reserving the correct points size. */
  LISTBASE_FOREACH (const Nurb *, nurb, nurbs) {
    switch (nurb->type) {
      case CU_BEZIER: {
        std::unique_ptr<BezierSpline> spline = std::make_unique<BezierSpline>();
        spline->set_resolution(nurb->resolu);
        spline->set_cyclic(nurb->flagu & CU_NURB_CYCLIC);

        for (const BezTriple &bezt : Span(nurb->bezt, nurb->pntsu)) {
          spline->add_point(bezt.vec[1],
                            handle_type_from_dna_bezt((eBezTriple_Handle)bezt.h1),
                            bezt.vec[0],
                            handle_type_from_dna_bezt((eBezTriple_Handle)bezt.h2),
                            bezt.vec[2],
                            bezt.radius,
                            bezt.tilt);
        }
        spline->attributes.reallocate(spline->size());
        curve->add_spline(std::move(spline));
        break;
      }
      case CU_NURBS: {
        std::unique_ptr<NURBSpline> spline = std::make_unique<NURBSpline>();
        spline->set_resolution(nurb->resolu);
        spline->set_cyclic(nurb->flagu & CU_NURB_CYCLIC);
        spline->set_order(nurb->orderu);
        spline->knots_mode = knots_mode_from_dna_nurb(nurb->flagu);

        for (const BPoint &bp : Span(nurb->bp, nurb->pntsu)) {
          spline->add_point(bp.vec, bp.radius, bp.tilt, bp.vec[3]);
        }
        spline->attributes.reallocate(spline->size());
        curve->add_spline(std::move(spline));
        break;
      }
      case CU_POLY: {
        std::unique_ptr<PolySpline> spline = std::make_unique<PolySpline>();
        spline->set_cyclic(nurb->flagu & CU_NURB_CYCLIC);

        for (const BPoint &bp : Span(nurb->bp, nurb->pntsu)) {
          spline->add_point(bp.vec, bp.radius, bp.tilt);
        }
        spline->attributes.reallocate(spline->size());
        curve->add_spline(std::move(spline));
        break;
      }
      default: {
        BLI_assert_unreachable();
        break;
      }
    }
  }

  /* Though the curve has no attributes, this is necessary to properly set the custom data size. */
  curve->attributes.reallocate(curve->splines().size());

  /* Note: Normal mode is stored separately in each spline to facilitate combining splines
   * from multiple curve objects, where the value may be different. */
  const Spline::NormalCalculationMode normal_mode = normal_mode_from_dna_curve(
      dna_curve.twist_mode);
  for (SplinePtr &spline : curve->splines()) {
    spline->normal_mode = normal_mode;
  }

  return curve;
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
  Map<StringRefNull, AttributeMetaData> map;
  for (const SplinePtr &spline : splines_) {
    BLI_assert(spline->attributes.data.totlayer == layer_len);
    spline->attributes.foreach_attribute(
        [&](StringRefNull name, const AttributeMetaData &meta_data) {
          map.add_or_modify(
              name,
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