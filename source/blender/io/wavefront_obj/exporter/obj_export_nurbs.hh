/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup obj
 */

#pragma once

#include "BLI_math_vector_types.hh"

namespace blender {

struct Curve;
struct Nurb;
struct OBJExportParams;

namespace bke {
class CurvesGeometry;
}

namespace io::obj {

/**
 * Finds the range within the control points that represents the sequence of valid spans or
 * 'segments'.
 *
 * For example, if a NURBS curve of order 2 has following 5 knots:
 *      [0, 0, 0, 1, 1]
 * associated to three control points. Valid control point range would be
 * the interval [1, 2] and the knot sequence [0, 0, 1, 1] since the first
 * knot/point does not contribute to any span/segment.
 */
Span<float> valid_nurb_control_point_range(int8_t order,
                                           Span<float> knots,
                                           IndexRange &point_range);

/**
 * Curve object wrapper providing access to the a Curve Object's properties.
 * Curve objects can contain multiple individual splines.
 */
class IOBJCurve {
 public:
  virtual ~IOBJCurve() = default;

  virtual const float4x4 &object_transform() const = 0;

  virtual const char *get_curve_name() const = 0;

  /**
   *  Number of splines associated with the Curve object.assign_if_different
   */
  virtual int total_splines() const = 0;
  /**
   * \param spline_index: Zero-based index of spline of interest.
   * \return Total vertices in a spline.
   */
  virtual int total_spline_vertices(int spline_index) const = 0;
  /**
   * Get the number of control points on the U-dimension.
   */
  virtual int num_control_points_u(int spline_index) const = 0;
  /**
   * Get the number of control points on the V-dimension.
   */
  virtual int num_control_points_v(int spline_index) const = 0;
  /**
   * Get the degree of the NURBS spline for the U-dimension.
   */
  virtual int get_nurbs_degree_u(int spline_index) const = 0;
  /**
   * Get the degree of the NURBS spline for the V-dimension.
   */
  virtual int get_nurbs_degree_v(int spline_index) const = 0;
  /**
   * True if the indexed spline is cyclic along U dimension.
   */
  virtual bool get_cyclic_u(int spline_index) const = 0;
  /**
   * Get the knot vector for the U-dimension. Computes knots using the buffer if necessary.
   */
  virtual Span<float> get_knots_u(int spline_index, Vector<float> &buffer) const = 0;
  /**
   * Get coordinates for the (non-looped) spline control points.
   */
  virtual Span<float3> vertex_coordinates(int spline_index,
                                          Vector<float3> &dynamic_point_buffer) const = 0;
};

class OBJCurves : public IOBJCurve, NonCopyable {
 private:
  const bke::CurvesGeometry &curve_;
  const float4x4 transform_;
  const std::string name_;

 public:
  OBJCurves(const bke::CurvesGeometry &curve, const float4x4 &transform, const std::string &name);
  virtual ~OBJCurves() override = default;

  const float4x4 &object_transform() const override;

  const char *get_curve_name() const override;

  int total_splines() const override;
  int total_spline_vertices(int spline_index) const override;
  int num_control_points_u(int spline_index) const override;
  int num_control_points_v(int spline_index) const override;
  int get_nurbs_degree_u(int spline_index) const override;
  int get_nurbs_degree_v(int spline_index) const override;
  bool get_cyclic_u(int spline_index) const override;
  Span<float> get_knots_u(int spline_index, Vector<float> &buffer) const override;
  Span<float3> vertex_coordinates(int spline_index,
                                  Vector<float3> &dynamic_point_buffer) const override;
};

class OBJLegacyCurve : public IOBJCurve, NonCopyable {
 private:
  const Object *export_object_eval_;
  const Curve *export_curve_;

  const Nurb *get_spline(int spline_index) const;

 public:
  OBJLegacyCurve(const Depsgraph *depsgraph, Object *curve_object);
  virtual ~OBJLegacyCurve() override = default;

  const float4x4 &object_transform() const override;

  const char *get_curve_name() const override;

  int total_splines() const override;
  int total_spline_vertices(int spline_index) const override;
  int num_control_points_u(int spline_index) const override;
  int num_control_points_v(int spline_index) const override;
  int get_nurbs_degree_u(int spline_index) const override;
  int get_nurbs_degree_v(int spline_index) const override;
  bool get_cyclic_u(int spline_index) const override;
  Span<float> get_knots_u(int spline_index, Vector<float> &buffer) const override;
  Span<float3> vertex_coordinates(int spline_index,
                                  Vector<float3> &dynamic_point_buffer) const override;
};

}  // namespace io::obj
}  // namespace blender
