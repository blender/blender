/* SPDX-FileCopyrightText: 2023-2025 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */
#include "BLI_array_utils.hh"
#include "BLI_string.h"

#include "BKE_appdir.hh"
#include "BKE_curves.hh"
#include "BKE_geometry_compare.hh"
#include "BKE_idtype.hh"

#include "obj_export_file_writer.hh"
#include "obj_export_nurbs.hh"
#include "obj_exporter.hh"
#include "obj_importer.hh"

#include "CLG_log.h"
#include "testing/testing.h"

namespace blender::io::obj {

static OBJExportParams default_export_params(const std::string &filepath)
{
  OBJExportParams params;
  params.forward_axis = eIOAxis::IO_AXIS_Y;
  params.up_axis = eIOAxis::IO_AXIS_Z;
  STRNCPY(params.filepath, filepath.c_str());
  return params;
}

static OBJImportParams default_import_params(const std::string &filepath)
{
  OBJImportParams params;
  params.forward_axis = eIOAxis::IO_AXIS_Y;
  params.up_axis = eIOAxis::IO_AXIS_Z;
  STRNCPY(params.filepath, filepath.c_str());
  return params;
}

class OBJCurvesTest : public testing::Test {
 public:
  static void SetUpTestSuite()
  {
    /* BKE_id_free() hits a code path that uses CLOG, which crashes if not initialized properly. */
    CLG_init();

    /* Might not be necessary but... */
    BKE_idtype_init();
  }

  static void TearDownTestSuite()
  {
    CLG_exit();
  }

  void write_curves(const Span<std::unique_ptr<IOBJCurve>> curves, OBJExportParams params)
  {
    export_objects(params, Span<std::unique_ptr<OBJMesh>>(nullptr, 0), curves, params.filepath);
  }

  void write_curves(const std::unique_ptr<IOBJCurve> &curve, OBJExportParams params)
  {
    Span<std::unique_ptr<IOBJCurve>> span(&curve, 1);
    write_curves(span, params);
  }

  void write_curves(const bke::CurvesGeometry &curve, OBJExportParams params)
  {
    float4x4 identity = float4x4::identity();
    std::unique_ptr<IOBJCurve> curve_wrapper(new OBJCurves(curve, identity, "test"));
    write_curves(curve_wrapper, params);
  }

  Vector<bke::GeometrySet> read_curves(OBJImportParams params)
  {
    Vector<bke::GeometrySet> geoms;
    importer_geometry(params, geoms);
    return geoms;
  }

  static bke::CurvesGeometry create_curves(Span<float3> points, bool cyclic)
  {
    bke::CurvesGeometry curves(points.size(), 1);
    curves.offsets_for_write()[0] = 0;
    curves.offsets_for_write()[1] = points.size();
    curves.cyclic_for_write()[0] = cyclic;
    curves.positions_for_write().copy_from(points);
    return curves;
  }

  static bke::CurvesGeometry create_rational_nurbs(
      Span<float3> points, Span<float> weights, bool cyclic, int8_t order, KnotsMode mode)
  {
    bke::CurvesGeometry curves = create_curves(points, cyclic);
    curves.nurbs_orders_for_write()[0] = order;
    curves.nurbs_knots_modes_for_write()[0] = int8_t(mode);
    curves.nurbs_weights_for_write().copy_from(weights);

    return curves;
  }

  static bke::CurvesGeometry create_nurbs(Span<float3> points,
                                          bool cyclic,
                                          int8_t order,
                                          KnotsMode mode)
  {
    bke::CurvesGeometry curves = create_curves(points, cyclic);
    curves.nurbs_orders_for_write()[0] = order;
    curves.nurbs_knots_modes_for_write()[0] = int8_t(mode);
    curves.nurbs_weights_for_write().fill(1.0f);

    return curves;
  }

  void run_nurbs_test(const Span<float3> points,
                      const int8_t order,
                      const KnotsMode mode,
                      const bool cyclic,
                      bke::CurvesGeometry &src_curve,
                      const bke::CurvesGeometry *&result_curve,
                      Span<float3> expected_points = Span<float3>(),
                      const KnotsMode *expected_mode = nullptr,
                      const bool *expected_cyclic = nullptr)
  {
    BKE_tempdir_init(nullptr);
    std::string tempdir = std::string(BKE_tempdir_base());
    std::string out_file_path = tempdir + BLI_path_basename("io_obj/tmp_6f5273f4.obj");

    /* Write/Read */
    src_curve = OBJCurvesTest::create_nurbs(points, cyclic, order, mode);
    ASSERT_TRUE(src_curve.cyclic()[0] == cyclic); /* Validate test function */

    write_curves(src_curve, default_export_params(out_file_path));

    Vector<bke::GeometrySet> result = read_curves(default_import_params(out_file_path));

    ASSERT_TRUE(result.size() == 1);
    result_curve = &result[0].get_curves()->geometry.wrap();

    /* Validate properties */
    EXPECT_EQ(result_curve->nurbs_orders()[0], order);
    EXPECT_EQ(result_curve->cyclic()[0], expected_cyclic ? *expected_cyclic : cyclic);
    EXPECT_EQ(result_curve->nurbs_knots_modes()[0], int8_t(expected_mode ? *expected_mode : mode));

    const Span<float3> result_points = result_curve->positions();
    expected_points = expected_points.size() ? expected_points : points;
    ASSERT_EQ(expected_points.size(), result_points.size());
    EXPECT_NEAR_ARRAY_ND(
        expected_points.data(), result_points.data(), expected_points.size(), 3, 1e-4);

    if (result_curve->nurbs_knots_modes()[0] != KnotsMode::NURBS_KNOT_MODE_CUSTOM) {
      ASSERT_TRUE(result_curve->custom_knots == NULL);
    }
  }
};

const std::array<float3, 13> position_array{float3{1.0f, -1.0f, 2.0f},
                                            float3{2.0f, -2.0f, 4.0f},
                                            float3{3.0f, -3.0f, 6.0f},
                                            float3{4.0f, -4.0f, 8.0f},
                                            float3{5.0f, -5.0f, 10.0f},
                                            float3{6.0f, -6.0f, 12.0f},
                                            float3{7.0f, -7.0f, 14.0f},
                                            float3{1.0f / 4.0f, -2.0f, 3.0f / 6.0f},
                                            float3{1.0f / 6.0f, -3.0f, 3.0f / 9.0f},
                                            float3{1.0f / 8.0f, -4.0f, 3.0f / 12.0f},
                                            float3{1.0f / 5.0f, -5.0f, 3.0f / 11.0f},
                                            float3{1.0f / 3.0f, -6.0f, 3.0f / 10.0f},
                                            float3{1.0f / 2.0f, -7.0f, 3.0f / 9.0f}};
const Span<float3> position_data = Span<float3>(position_array);

/* -------------------------------------------------------------------- */
/** \name Knot vector: KnotMode::NURBS_KNOT_MODE_NORMAL
 * \{ */

TEST_F(OBJCurvesTest, nurbs_io_uniform_polyline)
{
  const int8_t order = 2;
  const KnotsMode mode = KnotsMode::NURBS_KNOT_MODE_NORMAL;
  const bool cyclic = false;
  const Span<float3> positions = position_data.slice(0, 5);

  const KnotsMode expected_mode = KnotsMode::NURBS_KNOT_MODE_ENDPOINT;

  bke::CurvesGeometry src;
  const bke::CurvesGeometry *result;
  run_nurbs_test(positions, order, mode, cyclic, src, result, positions, &expected_mode);

  /* Validate uniform knots, don't do this in general as it only verifies the knot generator
   * `bke::curves::nurbs::calculate_knots`. */
  Vector<float> knot_buffer(bke::curves::nurbs::knots_num(positions.size(), order, cyclic));
  bke::curves::nurbs::calculate_knots(positions.size(), mode, order, cyclic, knot_buffer);
  const Vector<int> multiplicity = bke::curves::nurbs::calculate_multiplicity_sequence(
      knot_buffer);

  std::array<int, 7> expected_mult;
  std::fill(expected_mult.begin(), expected_mult.end(), 1);
  EXPECT_EQ_SPAN<int>(multiplicity, expected_mult);
}

TEST_F(OBJCurvesTest, nurbs_io_uniform_deg5)
{
  const int8_t order = 6;
  const KnotsMode mode = KnotsMode::NURBS_KNOT_MODE_NORMAL;
  const Span<float3> positions = position_data.slice(0, 8);

  bke::CurvesGeometry src;
  const bke::CurvesGeometry *result;
  run_nurbs_test(positions, order, mode, false, src, result);
}

TEST_F(OBJCurvesTest, nurbs_io_uniform_clamped_polyline)
{
  const int8_t order = 2;
  const KnotsMode mode = KnotsMode::NURBS_KNOT_MODE_ENDPOINT;
  const Span<float3> positions = position_data.slice(0, 5);

  bke::CurvesGeometry src;
  const bke::CurvesGeometry *result;
  run_nurbs_test(positions, order, mode, false, src, result);
}

TEST_F(OBJCurvesTest, nurbs_io_uniform_endpoint_clamped_deg3)
{
  const int8_t order = 3;
  const KnotsMode mode = KnotsMode::NURBS_KNOT_MODE_ENDPOINT;
  const Span<float3> positions = position_data.slice(0, 5);

  bke::CurvesGeometry src;
  const bke::CurvesGeometry *result;
  run_nurbs_test(positions, order, mode, false, src, result);
}

TEST_F(OBJCurvesTest, nurbs_io_uniform_endpoint_clamped_deg5)
{
  const int8_t order = 6;
  const KnotsMode mode = KnotsMode::NURBS_KNOT_MODE_NORMAL;
  const Span<float3> positions = position_data.slice(0, 8);

  bke::CurvesGeometry src;
  const bke::CurvesGeometry *result;
  run_nurbs_test(positions, order, mode, false, src, result);
}

TEST_F(OBJCurvesTest, nurbs_io_uniform_cyclic_polyline)
{
  const int8_t order = 2;
  const KnotsMode mode = KnotsMode::NURBS_KNOT_MODE_NORMAL;
  const Span<float3> positions = position_data.slice(0, 5);

  const KnotsMode expected_mode = KnotsMode::NURBS_KNOT_MODE_ENDPOINT;

  bke::CurvesGeometry src;
  const bke::CurvesGeometry *result;
  run_nurbs_test(positions, order, mode, true, src, result, positions, &expected_mode);
}

TEST_F(OBJCurvesTest, nurbs_io_uniform_cyclic_deg4)
{
  const int8_t order = 5;
  const KnotsMode mode = KnotsMode::NURBS_KNOT_MODE_NORMAL;
  const Span<float3> positions = position_data.slice(0, 8);

  bke::CurvesGeometry src;
  const bke::CurvesGeometry *result;
  run_nurbs_test(positions, order, mode, true, src, result);
}

TEST_F(OBJCurvesTest, nurbs_io_uniform_cyclic_clamped_deg4)
{
  const int8_t order = 5;
  const KnotsMode mode = KnotsMode::NURBS_KNOT_MODE_ENDPOINT;
  const Span<float3> positions = position_data.slice(0, 12);

  bke::CurvesGeometry src;
  const bke::CurvesGeometry *result;
  run_nurbs_test(positions, order, mode, true, src, result);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Knot vector: KnotMode::NURBS_KNOT_MODE_ENDPOINT_BEZIER
 * \{ */

TEST_F(OBJCurvesTest, nurbs_io_bezier_clamped_single_segment_deg2)
{
  const int8_t order = 3;
  const KnotsMode mode = KnotsMode::NURBS_KNOT_MODE_ENDPOINT_BEZIER;
  const Span<float3> positions = position_data.slice(0, 3);

  bke::CurvesGeometry src;
  const bke::CurvesGeometry *result;
  run_nurbs_test(positions, order, mode, false, src, result);
}

TEST_F(OBJCurvesTest, nurbs_io_bezier_clamped_single_segment_deg4)
{
  const int8_t order = 5;
  const KnotsMode mode = KnotsMode::NURBS_KNOT_MODE_ENDPOINT_BEZIER;
  const Span<float3> positions = position_data.slice(0, 5);

  bke::CurvesGeometry src;
  const bke::CurvesGeometry *result;
  run_nurbs_test(positions, order, mode, false, src, result);
}

TEST_F(OBJCurvesTest, nurbs_io_bezier_clamped_deg2)
{
  const int8_t order = 3;
  const KnotsMode mode = KnotsMode::NURBS_KNOT_MODE_ENDPOINT_BEZIER;
  const Span<float3> positions = position_data.slice(0, 7);

  bke::CurvesGeometry src;
  const bke::CurvesGeometry *result;
  run_nurbs_test(positions, order, mode, false, src, result);
}

TEST_F(OBJCurvesTest, nurbs_io_bezier_clamped_uneven_deg2)
{
  const int8_t order = 3;
  const KnotsMode mode = KnotsMode::NURBS_KNOT_MODE_ENDPOINT_BEZIER;
  const Span<float3> positions = position_data.slice(0, 8);

  bke::CurvesGeometry src;
  const bke::CurvesGeometry *result;
  run_nurbs_test(positions, order, mode, false, src, result, positions.slice(0, 7));
}

TEST_F(OBJCurvesTest, nurbs_io_bezier_clamped_deg4)
{
  const int8_t order = 5;
  const KnotsMode mode = KnotsMode::NURBS_KNOT_MODE_ENDPOINT_BEZIER;
  const Span<float3> positions = position_data.slice(0, 13);

  /* Even (whole Bezier segments). */
  {
    bke::CurvesGeometry src;
    const bke::CurvesGeometry *result;
    run_nurbs_test(positions, order, mode, false, src, result);
  }

  {
    bke::CurvesGeometry src;
    const bke::CurvesGeometry *result;
    run_nurbs_test(positions.slice(0, 9), order, mode, false, src, result);
  }

  /* Uneven (incomplete segment). */

  {
    bke::CurvesGeometry src;
    const bke::CurvesGeometry *result;
    run_nurbs_test(positions.slice(0, 12), order, mode, false, src, result, positions.slice(0, 9));
  }

  {
    bke::CurvesGeometry src;
    const bke::CurvesGeometry *result;
    run_nurbs_test(positions.slice(0, 11), order, mode, false, src, result, positions.slice(0, 9));
  }

  {
    bke::CurvesGeometry src;
    const bke::CurvesGeometry *result;
    run_nurbs_test(positions.slice(0, 10), order, mode, false, src, result, positions.slice(0, 9));
  }
}

TEST_F(OBJCurvesTest, nurbs_io_bezier_clamped_cyclic_deg4_looped_12)
{
  const int8_t order = 5;
  const KnotsMode mode = KnotsMode::NURBS_KNOT_MODE_ENDPOINT_BEZIER;
  const Span<float3> positions = position_data.slice(0, 12);

  bke::CurvesGeometry src;
  const bke::CurvesGeometry *result;
  run_nurbs_test(positions.slice(0, 12), order, mode, true, src, result);
}

TEST_F(OBJCurvesTest, nurbs_io_bezier_clamped_cyclic_deg4_looped_8)
{
  const int8_t order = 5;
  const KnotsMode mode = KnotsMode::NURBS_KNOT_MODE_ENDPOINT_BEZIER;
  const Span<float3> positions = position_data.slice(0, 8);

  bke::CurvesGeometry src;
  const bke::CurvesGeometry *result;
  run_nurbs_test(positions, order, mode, true, src, result);
}

TEST_F(OBJCurvesTest, nurbs_io_bezier_clamped_cyclic_deg4_discontinous_13)
{
  const int8_t order = 5;
  const KnotsMode mode = KnotsMode::NURBS_KNOT_MODE_ENDPOINT_BEZIER;
  const Span<float3> positions = position_data;

  Vector<float3> expected(positions);
  expected.append(positions[0]);
  const bool expect_cyclic = false;
  const KnotsMode expect_mode = KnotsMode::NURBS_KNOT_MODE_CUSTOM;

  bke::CurvesGeometry src;
  const bke::CurvesGeometry *result;

  run_nurbs_test(
      positions, order, mode, true, src, result, expected, &expect_mode, &expect_cyclic);
}

TEST_F(OBJCurvesTest, nurbs_io_bezier_clamped_cyclic_deg4_discontinous_11)
{
  const int8_t order = 5;
  const KnotsMode mode = KnotsMode::NURBS_KNOT_MODE_ENDPOINT_BEZIER;
  const Span<float3> positions = position_data.slice(0, 11);

  Vector<float3> expected(positions);
  expected.append(positions[0]);
  const bool expect_cyclic = false;
  const KnotsMode expect_mode = KnotsMode::NURBS_KNOT_MODE_CUSTOM;

  bke::CurvesGeometry src;
  const bke::CurvesGeometry *result;

  run_nurbs_test(
      positions, order, mode, true, src, result, expected, &expect_mode, &expect_cyclic);
}

TEST_F(OBJCurvesTest, nurbs_io_bezier_clamped_cyclic_deg4_discontinous_10)
{
  const int8_t order = 5;
  const KnotsMode mode = KnotsMode::NURBS_KNOT_MODE_ENDPOINT_BEZIER;
  const Span<float3> positions = position_data.slice(0, 10);

  Vector<float3> expected(positions);
  expected.append(positions[0]);
  const bool expect_cyclic = false;
  const KnotsMode expect_mode = KnotsMode::NURBS_KNOT_MODE_CUSTOM;

  bke::CurvesGeometry src;
  const bke::CurvesGeometry *result;

  run_nurbs_test(
      positions, order, mode, true, src, result, expected, &expect_mode, &expect_cyclic);
}

TEST_F(OBJCurvesTest, nurbs_io_bezier_clamped_cyclic_deg4_discontinous_9)
{
  const int8_t order = 5;
  const KnotsMode mode = KnotsMode::NURBS_KNOT_MODE_ENDPOINT_BEZIER;
  const Span<float3> positions = position_data.slice(0, 9);

  Vector<float3> expected(positions);
  expected.append(positions[0]);
  const bool expect_cyclic = false;
  const KnotsMode expect_mode = KnotsMode::NURBS_KNOT_MODE_CUSTOM;

  bke::CurvesGeometry src;
  const bke::CurvesGeometry *result;

  run_nurbs_test(
      positions, order, mode, true, src, result, expected, &expect_mode, &expect_cyclic);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Knot vector: KnotMode::NURBS_KNOT_MODE_BEZIER
 * \{ */

TEST_F(OBJCurvesTest, nurbs_io_bezier_cyclic_deg4_looped_12)
{
  const int8_t order = 5;
  const KnotsMode mode = KnotsMode::NURBS_KNOT_MODE_BEZIER;
  const Span<float3> positions = position_data.slice(0, 12);

  Vector<float3> expected(positions.size());
  array_utils::copy(positions.slice(1, 11), expected.as_mutable_span().slice(0, 11));
  expected.last() = positions.first();

  const KnotsMode expect_mode = KnotsMode::NURBS_KNOT_MODE_ENDPOINT_BEZIER;

  bke::CurvesGeometry src;
  const bke::CurvesGeometry *result;
  run_nurbs_test(positions, order, mode, true, src, result, expected, &expect_mode);
}

TEST_F(OBJCurvesTest, nurbs_io_bezier_cyclic_deg4_looped_discontinous_13)
{
  const int8_t order = 5;
  const KnotsMode mode = KnotsMode::NURBS_KNOT_MODE_BEZIER;
  const Span<float3> positions = position_data.slice(0, 13);

  Vector<float3> expected(positions.size() + 1);
  array_utils::copy(positions.slice(1, 12), expected.as_mutable_span().slice(0, 12));
  expected.last(1) = positions.first();
  expected.last() = positions[1];

  const bool expect_cyclic = false;
  const KnotsMode expect_mode = KnotsMode::NURBS_KNOT_MODE_CUSTOM;

  bke::CurvesGeometry src;
  const bke::CurvesGeometry *result;
  run_nurbs_test(
      positions, order, mode, true, src, result, expected, &expect_mode, &expect_cyclic);
}

TEST_F(OBJCurvesTest, nurbs_io_bezier_cyclic_deg4_looped_discontinous_11)
{
  const int8_t order = 5;
  const KnotsMode mode = KnotsMode::NURBS_KNOT_MODE_BEZIER;
  const Span<float3> positions = position_data.slice(0, 11);

  Vector<float3> expected(positions.size() + 1);
  array_utils::copy(positions.slice(1, 10), expected.as_mutable_span().slice(0, 10));
  expected.last(1) = positions.first();
  expected.last() = positions[1];

  const bool expect_cyclic = false;
  const KnotsMode expect_mode = KnotsMode::NURBS_KNOT_MODE_CUSTOM;

  bke::CurvesGeometry src;
  const bke::CurvesGeometry *result;
  run_nurbs_test(
      positions, order, mode, true, src, result, expected, &expect_mode, &expect_cyclic);
}

TEST_F(OBJCurvesTest, nurbs_io_bezier_cyclic_deg4_looped_discontinous_10)
{
  const int8_t order = 5;
  const KnotsMode mode = KnotsMode::NURBS_KNOT_MODE_BEZIER;
  const Span<float3> positions = position_data.slice(0, 10);

  Vector<float3> expected(positions.size() + 1);
  array_utils::copy(positions.slice(1, 9), expected.as_mutable_span().slice(0, 9));
  expected.last(1) = positions.first();
  expected.last() = positions[1];

  const bool expect_cyclic = false;
  const KnotsMode expect_mode = KnotsMode::NURBS_KNOT_MODE_CUSTOM;

  bke::CurvesGeometry src;
  const bke::CurvesGeometry *result;
  run_nurbs_test(
      positions, order, mode, true, src, result, expected, &expect_mode, &expect_cyclic);
}

TEST_F(OBJCurvesTest, nurbs_io_bezier_cyclic_deg4_looped_discontinous_9)
{
  const int8_t order = 5;
  const KnotsMode mode = KnotsMode::NURBS_KNOT_MODE_BEZIER;
  const Span<float3> positions = position_data.slice(0, 9);

  Vector<float3> expected(positions.size() + 1);
  array_utils::copy(positions.slice(1, 8), expected.as_mutable_span().slice(0, 8));
  expected.last(1) = positions.first();
  expected.last() = positions[1];

  const bool expect_cyclic = false;
  const KnotsMode expect_mode = KnotsMode::NURBS_KNOT_MODE_CUSTOM;

  bke::CurvesGeometry src;
  const bke::CurvesGeometry *result;
  run_nurbs_test(
      positions, order, mode, true, src, result, expected, &expect_mode, &expect_cyclic);
}

TEST_F(OBJCurvesTest, nurbs_io_bezier_cyclic_deg4_looped_8)
{
  const int8_t order = 5;
  const KnotsMode mode = KnotsMode::NURBS_KNOT_MODE_BEZIER;
  const Span<float3> positions = position_data.slice(0, 8);

  Vector<float3> expected(positions.size());
  array_utils::copy(positions.slice(1, 7), expected.as_mutable_span().slice(0, 7));
  expected.last() = positions.first();

  const KnotsMode expect_mode = KnotsMode::NURBS_KNOT_MODE_ENDPOINT_BEZIER;

  bke::CurvesGeometry src;
  const bke::CurvesGeometry *result;
  run_nurbs_test(positions, order, mode, true, src, result, expected, &expect_mode);
}

/** \} */

}  // namespace blender::io::obj
