/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "testing/testing.h"

#include "MEM_guardedalloc.h"

#include "BLI_array.hh"
#include "BLI_array_utils.hh"
#include "BLI_bounds.hh"
#include "BLI_offset_indices.hh"

#include "BKE_attribute.hh"
#include "BKE_curves.hh"
#include "BKE_grease_pencil_fills.hh"
#include "BKE_gtest_base.hh"

#include "ED_grease_pencil.hh"

#include <fstream>
#include <iostream>
#include <sstream>
#include <type_traits>

/* Should tests draw their output to an HTML file? */
#define DO_DRAW 0

namespace blender::ed::greasepencil::tests {

using namespace blender::ed::greasepencil::boolean;

static void CSS_setup_style(std::ofstream &f)
{
  constexpr int border_width = 5;
  constexpr int stroke_width = 3;
  constexpr int stroke_dasharray = 15;

  f << ".ui-group {\n"
       "  border: "
    << border_width << "px solid black;\n"
    << "  text-align: center;\n"
       "}\n"
       "\n";

  f << ".ui-list {\n"
       "  align-items: center;\n"
       "  display: inline-flex;\n"
       "}\n"
       "\n";

  f << ".polygon-A {\n"
       "  fill: blue;\n"
       "  fill-opacity: 0.25;\n"
       "  stroke: blue;\n"
       "  stroke-width: "
    << stroke_width
    << "px;\n"
       "  stroke-dasharray: "
    << stroke_dasharray
    << "px;\n"
       "}\n";
  f << ".polygon-B {\n"
       "  fill: red;\n"
       "  fill-opacity: 0.25;\n"
       "  stroke: red;\n"
       "  stroke-width: "
    << stroke_width
    << "px;\n"
       "  stroke-dasharray: "
    << stroke_dasharray
    << "px;\n"
       "}\n";
  f << ".polygon-C {\n"
       "  fill: green;\n"
       "  stroke: black;\n"
       "  stroke-width: "
    << stroke_width + 1
    << "px;\n"
       "  fill-opacity: 0.75;\n"
       "}\n"
       "\n";

  f << ".cut-A {\n"
       "  fill: none;\n"
       "  stroke: blue;\n"
       "  stroke-width: "
    << stroke_width
    << "px;\n"
       "  stroke-dasharray: "
    << stroke_dasharray
    << "px;\n"
       "}\n";
  f << ".cut-B {\n"
       "  fill: red;\n"
       "  stroke: red;\n"
       "  fill-opacity: 0.25;\n"
       "  stroke-width: "
    << stroke_width
    << "px;\n"
       "  stroke-dasharray: "
    << stroke_dasharray
    << "px;\n"
       "}\n";
  f << ".cut-C {\n"
       "  fill: none;\n"
       "  stroke: black;\n"
       "  stroke-width: "
    << stroke_width + 1
    << "px;\n"
       "}\n";
}

class SVGMapping {
 public:
  float2 topleft;
  float scale;
  float view_width;
  float view_height;

  float SX(const float x) const
  {
    return ((x - topleft[0]) * scale);
  }

  float SY(const float y) const
  {
    return ((topleft[1] - y) * scale);
  }

  SVGMapping(const Bounds<float2> &bounds)
  {
    constexpr int max_draw_width = 600;
    constexpr int max_draw_height = 400;

    const float draw_margin = (bounds.size().x + bounds.size().y) * 0.05;

    Bounds<float2> bounds_padded = bounds;
    bounds_padded.pad(draw_margin);

    topleft = float2(bounds_padded.min.x, bounds_padded.max.y);
    const float width = bounds_padded.size().x;
    const float height = bounds_padded.size().y;
    const float aspect = height / width;
    view_width = max_draw_width;
    view_height = int(view_width * aspect);
    if (view_height > max_draw_height) {
      view_height = max_draw_height;
      view_width = int(view_height / aspect);
    }
    scale = view_width / width;
  }
};

static void SVG_add_path(std::ofstream &f,
                         const std::string &class_name,
                         const Span<float2> &positions,
                         const GroupedSpan<int> shapes,
                         const IndexMask &shape_mask,
                         const OffsetIndices<int> points_by_curve,
                         const VArraySpan<bool> &cyclic,
                         const SVGMapping &mapping)
{
  shape_mask.foreach_index([&](const int64_t shape_i) {
    f << "<path class = \"" << class_name << "\" d = \"";

    const Span<int> shape = shapes[shape_i];

    for (const int pos : shape.index_range()) {
      const int curve_i = shape[pos];

      if (pos != 0) {
        f << " ";
      }

      f << "M ";
      const IndexRange points = points_by_curve[curve_i];
      for (const int i : points.index_range()) {
        const float2 &pos = positions[points[i]];

        if (i == 1) {
          f << " L ";
        }
        else if (i != 0) {
          f << ", ";
        }
        f << mapping.SX(pos[0]) << "," << mapping.SY(pos[1]);
      }
      if (cyclic[curve_i]) {
        f << " Z";
      }
    }

    f << "\"";
    f << " fill-rule=\"evenodd\"";
    f << "/>\n";
  });
}

static bool draw_append = false; /* Will be set to true after first call. */

static std::ofstream get_file_stream()
{
  constexpr const char *drawfile = "./boolean_curves_test_draw.html";

  std::ofstream f;
  if (draw_append) {
    f.open(drawfile, std::ios_base::app);
  }
  else {
    f.open(drawfile);
  }
  if (!f) {
    std::cout << "Could not open file " << drawfile << "\n";
    return f;
  }

  if (!draw_append) {
    f << "<!DOCTYPE html>\n";

    f << "<style>\n";

    CSS_setup_style(f);

    f << "</style>\n";
  }

  draw_append = true;

  return f;
}

static void draw_divider_start(const std::string &label)
{
  if (!DO_DRAW) {
    return;
  }

  std::ofstream f = get_file_stream();
  if (!f) {
    return;
  }

  f << "<div class=\"ui-group\">\n";
  f << "<h1>" << label << "</h1>\n";
  f << "<div class=\"ui-list\">\n";
}

static void draw_divider_end()
{
  if (!DO_DRAW) {
    return;
  }

  std::ofstream f = get_file_stream();
  if (!f) {
    return;
  }

  /* Exit `ui-list` */
  f << "</div>\n";
  /* Exit `ui-group` */
  f << "</div>\n";
}

static float2 project_fn(const float3 position)
{
  return float2(position.x, position.y);
};

static void draw_results(const std::string &label,
                         const std::string &type,
                         const bke::CurvesGeometry &src_curves,
                         const bke::CurvesGeometry &dst_curves,
                         const IndexMask &clipping_shapes)
{
  using namespace bke::greasepencil;

  if (!DO_DRAW) {
    return;
  }

  std::ofstream f = get_file_stream();
  if (!f) {
    return;
  }

  const OffsetIndices<int> src_points_by_curve = src_curves.points_by_curve();
  const OffsetIndices<int> dst_points_by_curve = dst_curves.points_by_curve();
  const VArraySpan<bool> src_cyclic = src_curves.cyclic();
  const VArraySpan<bool> dst_cyclic = dst_curves.cyclic();

  const bke::AttributeAccessor src_attributes = src_curves.attributes();
  const bke::AttributeAccessor dst_attributes = dst_curves.attributes();

  Array<float2> src_positions_2d(src_curves.points_num());
  const Span<float3> src_positions = src_curves.positions();
  for (const int i : src_curves.points_range()) {
    src_positions_2d[i] = project_fn(src_positions[i]);
  }

  Array<float2> dst_positions_2d(dst_curves.points_num());
  const Span<float3> dst_positions = dst_curves.positions();
  for (const int i : dst_curves.points_range()) {
    dst_positions_2d[i] = project_fn(dst_positions[i]);
  }

  const VArray<int> src_fill_ids = *src_attributes.lookup<int>("fill_id", bke::AttrDomain::Curve);
  const VArray<int> dst_fill_ids = *dst_attributes.lookup<int>("fill_id", bke::AttrDomain::Curve);

  const ShapeData src_shape_data = shapes_from_fill_ids(src_fill_ids, src_curves.curves_num());
  const ShapeData dst_shape_data = shapes_from_fill_ids(dst_fill_ids, dst_curves.curves_num());

  const GroupedSpan<int> src_shapes = src_shape_data.shapes();
  const GroupedSpan<int> dst_shapes = dst_shape_data.shapes();

  IndexMaskMemory memory;
  const IndexMask subject_shapes = clipping_shapes.complement(src_shapes.index_range(), memory);

  const SVGMapping mapping = SVGMapping(*bounds::min_max(src_positions_2d.as_span()));

  f << "<div>\n";
  f << "<svg width=\"" << mapping.view_width << "\" height=\"" << mapping.view_height << "\">\n";

  SVG_add_path(f,
               type + "-A",
               src_positions_2d,
               src_shapes,
               subject_shapes,
               src_points_by_curve,
               src_cyclic,
               mapping);
  SVG_add_path(f,
               type + "-B",
               src_positions_2d,
               src_shapes,
               clipping_shapes,
               src_points_by_curve,
               src_cyclic,
               mapping);

  SVG_add_path(f,
               type + "-C",
               dst_positions_2d,
               dst_shapes,
               dst_shapes.index_range(),
               dst_points_by_curve,
               dst_cyclic,
               mapping);

  f << "</svg>\n";
  f << "<h2>" << label << "</h2>\n";
  f << "</div>\n";
}

static bke::CurvesGeometry create_test_curves(const Span<int> offsets,
                                              const Span<float2> points,
                                              const Span<int> fill_ids,
                                              const Span<bool> cyclic)
{
  BLI_assert(!offsets.is_empty());
  const int curves_num = offsets.size() - 1;
  const int points_num = offsets.last();
  BLI_assert(cyclic.size() == curves_num);
  BLI_assert(fill_ids.size() == curves_num);

  bke::CurvesGeometry curves(points_num, curves_num);
  curves.offsets_for_write().copy_from(offsets);
  curves.cyclic_for_write().copy_from(cyclic);

  bke::MutableAttributeAccessor attributes = curves.attributes_for_write();

  bke::SpanAttributeWriter<int> fill_id_writer = attributes.lookup_or_add_for_write_span<int>(
      "fill_id", bke::AttrDomain::Curve);
  fill_id_writer.span.copy_from(fill_ids);
  fill_id_writer.finish();

  MutableSpan<float3> positions = curves.positions_for_write();
  for (const int i : curves.points_range()) {
    positions[i] = float3(points[i], 0.0f);
  }

  return curves;
}

static void expect_boolean_result_coord(const bke::CurvesGeometry &dst_curves,
                                        const Array<Vector<float2>> &expected_points)
{
  Array<float2> dst_positions_2d(dst_curves.points_num());
  const Span<float3> dst_positions = dst_curves.positions();
  for (const int i : dst_curves.points_range()) {
    dst_positions_2d[i] = project_fn(dst_positions[i]);
  }

  const OffsetIndices<int> points_by_curve = dst_curves.points_by_curve();

  EXPECT_EQ(dst_curves.curves_num(), expected_points.size());
  if (dst_curves.curves_num() != expected_points.size()) {
    return;
  }

  int total_size = 0;
  for (const int i : expected_points.index_range()) {
    total_size += expected_points[i].size();
  }

  EXPECT_EQ(dst_positions_2d.size(), total_size);
  if (dst_positions_2d.size() != total_size) {
    return;
  }

  for (const int curve_i : points_by_curve.index_range()) {
    const IndexRange points = points_by_curve[curve_i];

    const Span<float2> expected_sub_points = expected_points[curve_i];

    EXPECT_EQ(expected_sub_points.size(), points.size());
    if (expected_sub_points.size() != points.size()) {
      return;
    }

    for (const int i : points.index_range()) {
      const float2 &point = dst_positions_2d[points[i]];
      const float2 &expected_point = expected_sub_points[i];

      EXPECT_NEAR(point.x, expected_point.x, 1e-4);
      EXPECT_NEAR(point.y, expected_point.y, 1e-4);
    }
  }
}

static bke::CurvesGeometry test_curve_boolean(const Operation opt,
                                              const bke::CurvesGeometry &src_curves,
                                              const Span<int> fill_ids,
                                              const IndexMask &clipping_shapes)
{
  using namespace bke::greasepencil;
  CurveBooleanOpParameters op_params;
  op_params.boolean_mode = opt;
  op_params.keep_caps = true;
  op_params.skip_clipping_attributes = false;
  op_params.separate_islands = false;

  const ShapeData shapes_data = shapes_from_fill_ids(VArray<int>::from_span(fill_ids),
                                                     src_curves.curves_num());
  const GroupedSpan<int> shapes = shapes_data.shapes();

  return curve_boolean(op_params, src_curves, project_fn, shapes, clipping_shapes);
}

class GreasePencilBooleanTest : public bke::BlenderGTestBase {};

TEST_F(GreasePencilBooleanTest, Squares)
{
  draw_divider_start("Squares");

  const Array<float2> points = {{0, 0}, {2, 0}, {2, 2}, {0, 2}, {1, 1}, {3, 1}, {3, 3}, {1, 3}};
  const Array<int> points_by_curve = {0, 4, 8};
  const Array<int> fill_ids = {1, 2};
  const Array<bool> is_cyclic = {true, true};
  const IndexRange clipping_shapes = IndexRange(1, 1);

  const bke::CurvesGeometry src_curves = create_test_curves(
      points_by_curve, points, fill_ids, is_cyclic);

  {
    const bke::CurvesGeometry dst_curves = test_curve_boolean(
        Operation::Intersect, src_curves, fill_ids, clipping_shapes);

    const Array<Vector<float2>> expected_points = {{{2, 1}, {2, 2}, {1, 2}, {1, 1}}};
    expect_boolean_result_coord(dst_curves, expected_points);

    draw_results("Intersection", "polygon", src_curves, dst_curves, clipping_shapes);
  }
  {
    const bke::CurvesGeometry dst_curves = test_curve_boolean(
        Operation::Union, src_curves, fill_ids, clipping_shapes);

    const Array<Vector<float2>> expected_points = {
        {{1, 2}, {0, 2}, {0, 0}, {2, 0}, {2, 1}, {3, 1}, {3, 3}, {1, 3}}};
    expect_boolean_result_coord(dst_curves, expected_points);

    draw_results("Union", "polygon", src_curves, dst_curves, clipping_shapes);
  }
  {
    const bke::CurvesGeometry dst_curves = test_curve_boolean(
        Operation::Difference, src_curves, fill_ids, clipping_shapes);

    const Array<Vector<float2>> expected_points = {
        {{1, 2}, {0, 2}, {0, 0}, {2, 0}, {2, 1}, {1, 1}}};
    expect_boolean_result_coord(dst_curves, expected_points);

    draw_results("Difference", "polygon", src_curves, dst_curves, clipping_shapes);
  }

  draw_divider_end();
}

TEST_F(GreasePencilBooleanTest, Simple)
{
  draw_divider_start("Simple");

  /**
   * This is a replica of Fig. 10 from
   * Greiner, Günther; Kai Hormann (1998). "Efficient clipping of arbitrary polygons". ACM
   * Transactions on Graphics. 17 (2): 71-83.
   */
  const Array<float2> points = {
      {0, 6}, {8, 6}, {8, 3}, {0, 3}, {6, 0}, {6, 4}, {4, 2}, {2, 4}, {2, 0}};
  const Array<int> points_by_curve = {0, 4, 9};
  const Array<bool> is_cyclic = {true, true};
  const Array<int> fill_ids = {1, 2};
  const IndexRange clipping_shapes = IndexRange(1, 1);

  const bke::CurvesGeometry src_curves = create_test_curves(
      points_by_curve, points, fill_ids, is_cyclic);

  {
    const bke::CurvesGeometry dst_curves = test_curve_boolean(
        Operation::Intersect, src_curves, fill_ids, clipping_shapes);

    const Array<Vector<float2>> expected_points = {{{6, 3}, {5, 3}, {6, 4}},
                                                   {{3, 3}, {2, 3}, {2, 4}}};
    expect_boolean_result_coord(dst_curves, expected_points);

    draw_results("Intersection", "polygon", src_curves, dst_curves, clipping_shapes);
  }
  {
    const bke::CurvesGeometry dst_curves = test_curve_boolean(
        Operation::Union, src_curves, fill_ids, clipping_shapes);

    const Array<Vector<float2>> expected_points = {
        {{2, 3}, {0, 3}, {0, 6}, {8, 6}, {8, 3}, {6, 3}, {6, 0}, {2, 0}},
        {{5, 3}, {3, 3}, {4, 2}}};
    expect_boolean_result_coord(dst_curves, expected_points);

    draw_results("Union", "polygon", src_curves, dst_curves, clipping_shapes);
  }
  {
    const bke::CurvesGeometry dst_curves = test_curve_boolean(
        Operation::Difference, src_curves, fill_ids, clipping_shapes);

    const Array<Vector<float2>> expected_points = {
        {{2, 3}, {0, 3}, {0, 6}, {8, 6}, {8, 3}, {6, 3}, {6, 4}, {5, 3}, {3, 3}, {2, 4}}};
    expect_boolean_result_coord(dst_curves, expected_points);

    draw_results("Difference", "polygon", src_curves, dst_curves, clipping_shapes);
  }

  draw_divider_end();
}

TEST_F(GreasePencilBooleanTest, Complex)
{
  draw_divider_start("Complex");

  /**
   * This is a replica of Fig. 16 from
   * Greiner, Günther; Kai Hormann (1998). "Efficient clipping of arbitrary polygons". ACM
   * Transactions on Graphics. 17 (2): 71-83.
   */
  const Array<float2> points = {
      {14, 1}, {0, 5}, {14, 10}, {5, 6}, {14, 6}, {5, 5}, {9, 13}, {13, 0}, {9, 9}, {6, 0}};
  const Array<int> points_by_curve = {0, 6, 10};
  const Array<bool> is_cyclic = {true, true};
  const Array<int> fill_ids = {1, 2};
  const IndexRange clipping_shapes = IndexRange(1, 1);

  const bke::CurvesGeometry src_curves = create_test_curves(
      points_by_curve, points, fill_ids, is_cyclic);

  {
    const bke::CurvesGeometry dst_curves = test_curve_boolean(
        Operation::Intersect, src_curves, fill_ids, clipping_shapes);

    const Array<Vector<float2>> expected_points = {
        {{12.5662, 1.4096}, {12.3454, 1.4727}, {12.2, 1.8}, {12.4851, 1.6732}},
        {{7, 3}, {6.7113, 3.0824}, {6.9534, 4.1317}, {7.3225, 3.9677}},
        {{7.7964, 7.7844}, {8.7027, 8.1081}, {8.5217, 7.5652}, {7.6571, 7.1809}},
        {{9.3013, 8.3219}, {10.3267, 8.6881}, {10.4135, 8.4060}, {9.4536, 7.9793}},
        {{7.3846, 6}, {8, 6}, {7.7692, 5.3076}, {7.2105, 5.2456}},
        {{10.3333, 6}, {11.1538, 6}, {11.2479, 5.6942}, {10.5058, 5.6117}}};
    expect_boolean_result_coord(dst_curves, expected_points);

    draw_results("Intersection", "polygon", src_curves, dst_curves, clipping_shapes);
  }
  {
    const bke::CurvesGeometry dst_curves = test_curve_boolean(
        Operation::Union, src_curves, fill_ids, clipping_shapes);

    const Array<Vector<float2>> expected_points = {
        {{12.4851, 1.6732},
         {14, 1},
         {12.5662, 1.4096},
         {13, 0},
         {12.3454, 1.4727},
         {7, 3},
         {6, 0},
         {6.7113, 3.0824},
         {0, 5},
         {7.7964, 7.7844},
         {9, 13},
         {10.3267, 8.6881},
         {14, 10},
         {10.4135, 8.4060},
         {11.1538, 6},
         {14, 6},
         {11.2479, 5.6942}},
        {{8.7027, 8.1081}, {9.3013, 8.3219}, {9, 9}},
        {{9.4536, 7.9793}, {8.5217, 7.5652}, {8, 6}, {10.3333, 6}},
        {{7.6571, 7.1809}, {5, 6}, {7.3846, 6}},
        {{10.5058, 5.6117}, {7.7692, 5.3076}, {7.3225, 3.9677}, {12.2, 1.8}},
        {{7.2105, 5.2456}, {5, 5}, {6.9534, 4.1317}}};
    expect_boolean_result_coord(dst_curves, expected_points);

    draw_results("Union", "polygon", src_curves, dst_curves, clipping_shapes);
  }
  {
    const bke::CurvesGeometry dst_curves = test_curve_boolean(
        Operation::Difference, src_curves, fill_ids, clipping_shapes);

    const Array<Vector<float2>> expected_points = {
        {{12.4851, 1.6732}, {14, 1}, {12.5662, 1.4096}},
        {{12.3454, 1.4727}, {7, 3}, {7.3225, 3.9677}, {12.2, 1.8}},
        {{6.7113, 3.0824},
         {0, 5},
         {7.7964, 7.7844},
         {7.6571, 7.1809},
         {5, 6},
         {7.3846, 6},
         {7.2105, 5.2456},
         {5, 5},
         {6.9534, 4.1317}},
        {{8.7027, 8.1081}, {9.3013, 8.3219}, {9.4536, 7.9793}, {8.5217, 7.5652}},
        {{10.3267, 8.6881}, {14, 10}, {10.4135, 8.4060}},
        {{8, 6}, {10.3333, 6}, {10.5058, 5.6117}, {7.7692, 5.3076}},
        {{11.1538, 6}, {14, 6}, {11.2479, 5.6942}}};
    expect_boolean_result_coord(dst_curves, expected_points);

    draw_results("Difference", "polygon", src_curves, dst_curves, clipping_shapes);
  }

  draw_divider_end();
}

TEST_F(GreasePencilBooleanTest, Last_Edge_Loop)
{
  draw_divider_start("Last Edge Loop");

  /**
   * These shapes are designed to test the following:
   *   1: Intersection with the last edge.
   *   2: Segment connected through a full loop around.
   *   3: Multiple intersection on one edge not in order.
   *   4: Having a self intersection.
   */
  const Array<float2> points = {
      {0, 5}, {0, 0}, {7, 0}, {7, 5}, {2, 3}, {0, 7}, {3, 7}, {6, 3}, {7, 6}, {3, 3}, {2, 6}};
  const Array<int> points_by_curve = {0, 4, 11};
  const Array<bool> is_cyclic = {true, true};
  const Array<int> fill_ids = {1, 2};
  const IndexRange clipping_shapes = IndexRange(1, 1);

  const bke::CurvesGeometry src_curves = create_test_curves(
      points_by_curve, points, fill_ids, is_cyclic);

  {
    const bke::CurvesGeometry dst_curves = test_curve_boolean(
        Operation::Intersect, src_curves, fill_ids, clipping_shapes);

    const Array<Vector<float2>> expected_points = {
        {{6.6666, 5}, {5.6666, 5}, {3, 3}, {2.3333, 5}, {4.5000, 5}, {6, 3}},
        {{2, 5}, {1, 5}, {2, 3}}};
    expect_boolean_result_coord(dst_curves, expected_points);

    draw_results("Intersection", "polygon", src_curves, dst_curves, clipping_shapes);
  }
  {
    const bke::CurvesGeometry dst_curves = test_curve_boolean(
        Operation::Union, src_curves, fill_ids, clipping_shapes);

    const Array<Vector<float2>> expected_points = {{{1, 5},
                                                    {0, 5},
                                                    {0, 0},
                                                    {7, 0},
                                                    {7, 5},
                                                    {6.6666, 5},
                                                    {7, 6},
                                                    {5.6666, 5},
                                                    {4.5, 5},
                                                    {3, 7},
                                                    {0, 7}},
                                                   {{2.3333, 5}, {2, 5}, {2, 6}}};
    expect_boolean_result_coord(dst_curves, expected_points);

    draw_results("Union", "polygon", src_curves, dst_curves, clipping_shapes);
  }
  {
    const bke::CurvesGeometry dst_curves = test_curve_boolean(
        Operation::Difference, src_curves, fill_ids, clipping_shapes);

    const Array<Vector<float2>> expected_points = {{{1, 5},
                                                    {0, 5},
                                                    {0, 0},
                                                    {7, 0},
                                                    {7, 5},
                                                    {6.6666, 5},
                                                    {6, 3},
                                                    {4.5, 5},
                                                    {5.6666, 5},
                                                    {3, 3},
                                                    {2.3333, 5},
                                                    {2, 5},
                                                    {2, 3}}};
    expect_boolean_result_coord(dst_curves, expected_points);

    draw_results("Difference", "polygon", src_curves, dst_curves, clipping_shapes);
  }

  draw_divider_end();
}

TEST_F(GreasePencilBooleanTest, Simple_Cuts)
{
  draw_divider_start("Cuts");

  {
    const Array<float2> points = {
        {5, 7}, {3, 6}, {0, 2}, {0, 0}, {1, 6}, {3, 4}, {3, 1}, {0, 4}, {2, 3}};
    const Array<int> points_by_curve = {0, 4, 9};
    const Array<bool> is_cyclic = {false, true};
    const Array<int> fill_ids = {0, 1};
    const IndexRange clipping_shapes = IndexRange(1, 1);

    const bke::CurvesGeometry src_curves = create_test_curves(
        points_by_curve, points, fill_ids, is_cyclic);

    const bke::CurvesGeometry dst_curves = test_curve_boolean(
        Operation::Difference, src_curves, fill_ids, clipping_shapes);

    const Array<Vector<float2>> expected_points = {{{5, 7}, {3, 6}, {2.14286, 4.85714}},
                                                   {{1.61538, 4.15385}, {1.09091, 3.45455}},
                                                   {{0.857143, 3.14286}, {0, 2}, {0, 0}}};
    expect_boolean_result_coord(dst_curves, expected_points);

    draw_results("Simple Cut 1", "cut", src_curves, dst_curves, clipping_shapes);
  }
  {
    const Array<float2> points = {{5, 5}, {3, 5}, {1, 3}, {1, 1}, {5, 6}, {6, 5}, {1, 0}, {0, 1}};
    const Array<int> points_by_curve = {0, 4, 8};
    const Array<bool> is_cyclic = {false, true};
    const Array<int> fill_ids = {0, 1};
    const IndexRange clipping_shapes = IndexRange(1, 1);

    const bke::CurvesGeometry src_curves = create_test_curves(
        points_by_curve, points, fill_ids, is_cyclic);

    const bke::CurvesGeometry dst_curves = test_curve_boolean(
        Operation::Difference, src_curves, fill_ids, clipping_shapes);

    const Array<Vector<float2>> expected_points = {{{4, 5}, {3, 5}, {1, 3}, {1, 2}}};
    expect_boolean_result_coord(dst_curves, expected_points);

    draw_results("Simple Cut 2", "cut", src_curves, dst_curves, clipping_shapes);
  }
  {
    const Array<float2> points = {{6, 8},
                                  {4, 7},
                                  {1, 3},
                                  {1, 1},
                                  {3, 7},
                                  {5, 5},
                                  {1, 0},
                                  {0, 4},
                                  {2, 3},
                                  {1, 5},
                                  {3, 4},
                                  {2, 6},
                                  {4, 5}};
    const Array<int> points_by_curve = {0, 4, 13};
    const Array<bool> is_cyclic = {false, true};
    const Array<int> fill_ids = {0, 1};
    const IndexRange clipping_shapes = IndexRange(1, 1);

    const bke::CurvesGeometry src_curves = create_test_curves(
        points_by_curve, points, fill_ids, is_cyclic);

    const bke::CurvesGeometry dst_curves = test_curve_boolean(
        Operation::Difference, src_curves, fill_ids, clipping_shapes);

    const Array<Vector<float2>> expected_points = {{{6, 8}, {4, 7}, {3.57143, 6.42857}},
                                                   {{3.4, 6.2}, {2.90909, 5.54545}},
                                                   {{2.5, 5}, {2.09091, 4.45455}},
                                                   {{1.6, 3.8}, {1.27273, 3.36364}}};
    expect_boolean_result_coord(dst_curves, expected_points);

    draw_results("Simple Cut 3", "cut", src_curves, dst_curves, clipping_shapes);
  }
  {
    const Array<float2> points = {{6, 7},
                                  {4, 6},
                                  {1, 2},
                                  {1, 0},
                                  {0, 4},
                                  {2, 2},
                                  {7, 8},
                                  {3, 7},
                                  {4, 5},
                                  {2, 6},
                                  {3, 4},
                                  {1, 5},
                                  {2, 3}};
    const Array<int> points_by_curve = {0, 4, 13};
    const Array<bool> is_cyclic = {false, true};
    const Array<int> fill_ids = {0, 1};
    const IndexRange clipping_shapes = IndexRange(1, 1);

    const bke::CurvesGeometry src_curves = create_test_curves(
        points_by_curve, points, fill_ids, is_cyclic);

    const bke::CurvesGeometry dst_curves = test_curve_boolean(
        Operation::Difference, src_curves, fill_ids, clipping_shapes);

    const Array<Vector<float2>> expected_points = {{{3.7, 5.6}, {3.45455, 5.27273}},
                                                   {{2.8, 4.4}, {2.63636, 4.18182}},
                                                   {{1.9, 3.2}, {1.81818, 3.09091}},
                                                   {{1.42857, 2.57143}, {1, 2}, {1, 0}}};
    expect_boolean_result_coord(dst_curves, expected_points);

    draw_results("Simple Cut 4", "cut", src_curves, dst_curves, clipping_shapes);
  }
  {
    const Array<float2> points = {
        {6, 5}, {4, 5}, {1, 2}, {1, 0}, {1, 4}, {3, 1}, {5, 3}, {2, 5}, {3, 3}};
    const Array<int> points_by_curve = {0, 4, 9};
    const Array<bool> is_cyclic = {true, true};
    const Array<int> fill_ids = {0, 1};
    const IndexRange clipping_shapes = IndexRange(1, 1);

    const bke::CurvesGeometry src_curves = create_test_curves(
        points_by_curve, points, fill_ids, is_cyclic);

    const bke::CurvesGeometry dst_curves = test_curve_boolean(
        Operation::Difference, src_curves, fill_ids, clipping_shapes);

    const Array<Vector<float2>> expected_points = {{{4.4, 3.4}, {6, 5}, {4, 5}, {3.2, 4.2}},
                                                   {{2.66667, 3.66667}, {2.33333, 3.33333}},
                                                   {{1.8, 2.8}, {1, 2}, {1, 0}, {2.6, 1.6}}};
    expect_boolean_result_coord(dst_curves, expected_points);

    draw_results("Cyclical Cut", "cut", src_curves, dst_curves, clipping_shapes);
  }
  draw_divider_end();
}

TEST_F(GreasePencilBooleanTest, Square_With_Hole)
{
  draw_divider_start("Square With Hole");

  const Array<float2> points = {{0, 0},
                                {0, 5},
                                {5, 5},
                                {5, 0},

                                {1, 1},
                                {1, 4},
                                {4, 4},
                                {4, 1},

                                {2, 2},
                                {2, 7},
                                {7, 7},
                                {7, 2}};
  const Array<int> points_by_curve = {0, 4, 8, 12};
  const Array<bool> is_cyclic = {true, true, true};
  const Array<int> fill_ids = {1, 1, 2};
  const IndexRange clipping_shapes = IndexRange::from_begin_end(1, 2);

  const bke::CurvesGeometry src_curves = create_test_curves(
      points_by_curve, points, fill_ids, is_cyclic);

  {
    const bke::CurvesGeometry dst_curves = test_curve_boolean(
        Operation::Difference, src_curves, fill_ids, clipping_shapes);

    const Array<Vector<float2>> expected_points = {
        {{5, 2}, {5, 0}, {0, 0}, {0, 5}, {2, 5}, {2, 4}, {1, 4}, {1, 1}, {4, 1}, {4, 2}}};
    expect_boolean_result_coord(dst_curves, expected_points);

    draw_results("Difference", "polygon", src_curves, dst_curves, clipping_shapes);
  }
  draw_divider_end();
}

TEST_F(GreasePencilBooleanTest, Squares_With_Holes)
{
  draw_divider_start("Squares With Holes");

  const Array<float2> points = {{0, 0},
                                {0, 5},
                                {5, 5},
                                {5, 0},

                                {1, 1},
                                {1, 4},
                                {4, 4},
                                {4, 1},

                                {2, 2},
                                {2, 7},
                                {7, 7},
                                {7, 2},

                                {3, 3},
                                {3, 6},
                                {6, 6},
                                {6, 3}};
  const Array<int> points_by_curve = {0, 4, 8, 12, 16};
  const Array<bool> is_cyclic = {true, true, true, true};
  const Array<int> fill_ids = {1, 1, 2, 2};
  const IndexRange clipping_shapes = IndexRange::from_begin_end(1, 2);

  const bke::CurvesGeometry src_curves = create_test_curves(
      points_by_curve, points, fill_ids, is_cyclic);

  {
    const bke::CurvesGeometry dst_curves = test_curve_boolean(
        Operation::Difference, src_curves, fill_ids, clipping_shapes);

    const Array<Vector<float2>> expected_points = {
        {{5, 2}, {5, 0}, {0, 0}, {0, 5}, {2, 5}, {2, 4}, {1, 4}, {1, 1}, {4, 1}, {4, 2}},
        {{3, 5}, {5, 5}, {5, 3}, {4, 3}, {4, 4}, {3, 4}}};
    expect_boolean_result_coord(dst_curves, expected_points);

    draw_results("Difference", "polygon", src_curves, dst_curves, clipping_shapes);
  }
  draw_divider_end();
}

TEST_F(GreasePencilBooleanTest, Multiple_Shapes)
{
  draw_divider_start("Multiple Shapes");

  const Array<float2> points = {{0, 2},
                                {0, 7},
                                {5, 7},
                                {5, 2},

                                {2, 0},
                                {2, 5},
                                {7, 5},
                                {7, 0},

                                {3, 3},
                                {3, 8},
                                {8, 8},
                                {8, 3}};
  const Array<int> points_by_curve = {0, 4, 8, 12};
  const Array<bool> is_cyclic = {true, true, true};
  const Array<int> fill_ids = {1, 2, 3};

  const bke::CurvesGeometry src_curves = create_test_curves(
      points_by_curve, points, fill_ids, is_cyclic);

  /**
   * Multiple separate but intersecting subject shapes.
   * The two subject shapes should be affected by the clipping shape, but not join into one.
   */
  {
    const IndexRange clipping_shapes = IndexRange::from_begin_end(2, 3);
    const bke::CurvesGeometry dst_curves = test_curve_boolean(
        Operation::Intersect, src_curves, fill_ids, clipping_shapes);

    const Array<Vector<float2>> expected_points = {{{3, 7}, {5, 7}, {5, 3}, {3, 3}},
                                                   {{3, 5}, {7, 5}, {7, 3}, {3, 3}}};
    expect_boolean_result_coord(dst_curves, expected_points);

    draw_results("2 Subjects Intersection", "polygon", src_curves, dst_curves, clipping_shapes);
  }
  {
    const IndexRange clipping_shapes = IndexRange::from_begin_end(2, 3);
    const bke::CurvesGeometry dst_curves = test_curve_boolean(
        Operation::Difference, src_curves, fill_ids, clipping_shapes);

    const Array<Vector<float2>> expected_points = {
        {{5, 3}, {5, 2}, {0, 2}, {0, 7}, {3, 7}, {3, 3}},
        {{7, 3}, {7, 0}, {2, 0}, {2, 5}, {3, 5}, {3, 3}}};
    expect_boolean_result_coord(dst_curves, expected_points);

    draw_results("2 Subjects Difference", "polygon", src_curves, dst_curves, clipping_shapes);
  }
  draw_divider_end();
}

}  // namespace blender::ed::greasepencil::tests
