#include "BLI_span.hh"

#include "BKE_curves.hh"
#include "BKE_curves_utils.hh"
#include "BKE_geometry_set.hh"

namespace blender::geometry {

/*
 * Create a new Curves instance by trimming the input curves. Copying the selected splines
 * between the start and end points.
 */
bke::CurvesGeometry trim_curves(const bke::CurvesGeometry &src_curves,
                                IndexMask selection,
                                Span<bke::curves::CurvePoint> start_points,
                                Span<bke::curves::CurvePoint> end_points);

/**
 * Find the point(s) and piecewise segment corresponding to the given distance along the length of
 * the curve. Returns points on the evaluated curve for Catmull-Rom and NURBS splines.
 *
 * \param curves: Curve geometry to sample.
 * \param lengths: Distance along the curve on form [0.0, length] to determine the point for.
 * \param curve_indices: Curve index to lookup for each 'length', negative index are set to 0.
 * \param is_normalized: If true, 'lengths' are normalized to the interval [0.0, 1.0].
 */
Array<bke::curves::CurvePoint, 12> lookup_curve_points(const bke::CurvesGeometry &curves,
                                                       Span<float> lengths,
                                                       Span<int64_t> curve_indices,
                                                       bool is_normalized);

}  // namespace blender::geometry
