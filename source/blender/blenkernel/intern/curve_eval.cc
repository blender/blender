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
#include "BLI_span.hh"

#include "DNA_curve_types.h"

#include "BKE_curve.h"
#include "BKE_spline.hh"

using blender::float3;
using blender::float4x4;
using blender::Span;

CurveEval *CurveEval::copy()
{
  CurveEval *new_curve = new CurveEval();

  for (SplinePtr &spline : this->splines) {
    new_curve->splines.append(spline->copy());
  }

  return new_curve;
}

void CurveEval::translate(const float3 &translation)
{
  for (SplinePtr &spline : this->splines) {
    spline->translate(translation);
    spline->mark_cache_invalid();
  }
}

void CurveEval::transform(const float4x4 &matrix)
{
  for (SplinePtr &spline : this->splines) {
    spline->transform(matrix);
  }
}

void CurveEval::bounds_min_max(float3 &min, float3 &max, const bool use_evaluated) const
{
  for (const SplinePtr &spline : this->splines) {
    spline->bounds_min_max(min, max, use_evaluated);
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

std::unique_ptr<CurveEval> curve_eval_from_dna_curve(const Curve &dna_curve)
{
  std::unique_ptr<CurveEval> curve = std::make_unique<CurveEval>();

  const ListBase *nurbs = BKE_curve_nurbs_get(&const_cast<Curve &>(dna_curve));

  curve->splines.reserve(BLI_listbase_count(nurbs));

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

        curve->splines.append(std::move(spline));
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

        curve->splines.append(std::move(spline));
        break;
      }
      case CU_POLY: {
        std::unique_ptr<PolySpline> spline = std::make_unique<PolySpline>();
        spline->set_cyclic(nurb->flagu & CU_NURB_CYCLIC);

        for (const BPoint &bp : Span(nurb->bp, nurb->pntsu)) {
          spline->add_point(bp.vec, bp.radius, bp.tilt);
        }

        curve->splines.append(std::move(spline));
        break;
      }
      default: {
        BLI_assert_unreachable();
        break;
      }
    }
  }

  /* Note: Normal mode is stored separately in each spline to facilitate combining splines
   * from multiple curve objects, where the value may be different. */
  const Spline::NormalCalculationMode normal_mode = normal_mode_from_dna_curve(
      dna_curve.twist_mode);
  for (SplinePtr &spline : curve->splines) {
    spline->normal_mode = normal_mode;
  }

  return curve;
}

/** \} */
