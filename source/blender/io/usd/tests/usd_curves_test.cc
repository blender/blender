/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "testing/testing.h"
#include "tests/blendfile_loading_base_test.h"

#include <pxr/base/plug/registry.h>
#include <pxr/base/tf/stringUtils.h>
#include <pxr/base/vt/types.h>
#include <pxr/base/vt/value.h>
#include <pxr/usd/sdf/types.h>
#include <pxr/usd/usd/object.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/basisCurves.h>
#include <pxr/usd/usdGeom/curves.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/nurbsCurves.h>
#include <pxr/usd/usdGeom/subset.h>
#include <pxr/usd/usdGeom/tokens.h>

#include "DNA_material_types.h"
#include "DNA_node_types.h"

#include "BKE_context.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_mesh.hh"
#include "BKE_node.h"
#include "BLI_fileops.h"
#include "BLI_math.h"
#include "BLI_math_vector_types.hh"
#include "BLI_path_util.h"
#include "BLO_readfile.h"

#include "BKE_node_runtime.hh"

#include "DEG_depsgraph.h"

#include "WM_api.hh"

#include "usd.h"

namespace blender::io::usd {

const StringRefNull usd_curves_test_filename = "usd/usd_curves_test.blend";
const StringRefNull output_filename = "usd/output.usda";

static void check_catmullRom_curve(const pxr::UsdPrim prim,
                                   const bool is_periodic,
                                   const int vertex_count);
static void check_bezier_curve(const pxr::UsdPrim bezier_prim,
                               const bool is_periodic,
                               const int vertex_count);
static void check_nurbs_curve(const pxr::UsdPrim nurbs_prim,
                              const int vertex_count,
                              const int knots_count,
                              const int order);
static void check_nurbs_circle(const pxr::UsdPrim nurbs_prim,
                               const int vertex_count,
                               const int knots_count,
                               const int order);

class UsdCurvesTest : public BlendfileLoadingBaseTest {
 protected:
  struct bContext *context = nullptr;

 public:
  bool load_file_and_depsgraph(const StringRefNull &filepath,
                               const eEvaluationMode eval_mode = DAG_EVAL_VIEWPORT)
  {
    if (!blendfile_load(filepath.c_str())) {
      return false;
    }
    depsgraph_create(eval_mode);

    context = CTX_create();
    CTX_data_main_set(context, bfile->main);
    CTX_data_scene_set(context, bfile->curscene);

    return true;
  }

  virtual void SetUp() override
  {
    BlendfileLoadingBaseTest::SetUp();
  }

  virtual void TearDown() override
  {
    BlendfileLoadingBaseTest::TearDown();
    CTX_free(context);
    context = nullptr;

    if (BLI_exists(output_filename.c_str())) {
      BLI_delete(output_filename.c_str(), false, false);
    }
  }
};

TEST_F(UsdCurvesTest, usd_export_curves)
{
  if (!load_file_and_depsgraph(usd_curves_test_filename)) {
    ADD_FAILURE();
    return;
  }

  /* File sanity check. */
  EXPECT_EQ(BLI_listbase_count(&bfile->main->objects), 6);

  USDExportParams params;

  const bool result = USD_export(context, output_filename.c_str(), &params, false);
  EXPECT_TRUE(result) << "USD export should succed.";

  pxr::UsdStageRefPtr stage = pxr::UsdStage::Open(output_filename);
  ASSERT_NE(stage, nullptr) << "Stage should not be null after opening usd file.";

  {
    std::string prim_name = pxr::TfMakeValidIdentifier("BezierCurve");
    pxr::UsdPrim test_prim = stage->GetPrimAtPath(pxr::SdfPath("/BezierCurve/" + prim_name));
    EXPECT_TRUE(test_prim.IsValid());
    check_bezier_curve(test_prim, false, 7);
  }

  {
    std::string prim_name = pxr::TfMakeValidIdentifier("BezierCircle");
    pxr::UsdPrim test_prim = stage->GetPrimAtPath(pxr::SdfPath("/BezierCircle/" + prim_name));
    EXPECT_TRUE(test_prim.IsValid());
    check_bezier_curve(test_prim, true, 13);
  }

  {
    std::string prim_name = pxr::TfMakeValidIdentifier("NurbsCurve");
    pxr::UsdPrim test_prim = stage->GetPrimAtPath(pxr::SdfPath("/NurbsCurve/" + prim_name));
    EXPECT_TRUE(test_prim.IsValid());
    check_nurbs_curve(test_prim, 6, 20, 4);
  }

  {
    std::string prim_name = pxr::TfMakeValidIdentifier("NurbsCircle");
    pxr::UsdPrim test_prim = stage->GetPrimAtPath(pxr::SdfPath("/NurbsCircle/" + prim_name));
    EXPECT_TRUE(test_prim.IsValid());
    check_nurbs_circle(test_prim, 8, 13, 3);
  }

  {
    std::string prim_name = pxr::TfMakeValidIdentifier("Curves");
    pxr::UsdPrim test_prim = stage->GetPrimAtPath(pxr::SdfPath("/Cube/Curves/" + prim_name));
    EXPECT_TRUE(test_prim.IsValid());
    check_catmullRom_curve(test_prim, false, 8);
  }
}

/**
 * Test that the provided prim is a valid catmullRom curve. We also check it matches the expected
 * wrap type, and has the expected number of vertices.
 */
static void check_catmullRom_curve(const pxr::UsdPrim prim,
                                   const bool is_periodic,
                                   const int vertex_count)
{
  auto curve = pxr::UsdGeomBasisCurves(prim);

  pxr::VtValue basis;
  pxr::UsdAttribute basis_attr = curve.GetBasisAttr();
  basis_attr.Get(&basis);
  auto basis_token = basis.Get<pxr::TfToken>();

  EXPECT_EQ(basis_token, pxr::UsdGeomTokens->catmullRom)
      << "Basis token should be catmullRom for catmullRom curve";

  pxr::VtValue type;
  pxr::UsdAttribute type_attr = curve.GetTypeAttr();
  type_attr.Get(&type);
  auto type_token = type.Get<pxr::TfToken>();

  EXPECT_EQ(type_token, pxr::UsdGeomTokens->cubic)
      << "Type token should be cubic for catmullRom curve";

  pxr::VtValue wrap;
  pxr::UsdAttribute wrap_attr = curve.GetWrapAttr();
  wrap_attr.Get(&wrap);
  auto wrap_token = wrap.Get<pxr::TfToken>();

  if (is_periodic) {
    EXPECT_EQ(wrap_token, pxr::UsdGeomTokens->periodic)
        << "Wrap token should be periodic for periodic curve";
  }
  else {
    EXPECT_EQ(wrap_token, pxr::UsdGeomTokens->pinned)
        << "Wrap token should be pinned for nonperiodic catmullRom curve";
  }

  pxr::UsdAttribute vert_count_attr = curve.GetCurveVertexCountsAttr();
  pxr::VtArray<int> vert_counts;
  vert_count_attr.Get(&vert_counts);

  EXPECT_EQ(vert_counts.size(), 3) << "Prim should contain verts for three curves";
  EXPECT_EQ(vert_counts[0], vertex_count) << "Curve 0 should have " << vertex_count << " verts.";
  EXPECT_EQ(vert_counts[1], vertex_count) << "Curve 1 should have " << vertex_count << " verts.";
  EXPECT_EQ(vert_counts[2], vertex_count) << "Curve 2 should have " << vertex_count << " verts.";
}

/**
 * Test that the provided prim is a valid bezier curve. We also check it matches the expected
 * wrap type, and has the expected number of vertices.
 */
static void check_bezier_curve(const pxr::UsdPrim bezier_prim,
                               const bool is_periodic,
                               const int vertex_count)
{
  auto curve = pxr::UsdGeomBasisCurves(bezier_prim);

  pxr::VtValue basis;
  pxr::UsdAttribute basis_attr = curve.GetBasisAttr();
  basis_attr.Get(&basis);
  auto basis_token = basis.Get<pxr::TfToken>();

  EXPECT_EQ(basis_token, pxr::UsdGeomTokens->bezier)
      << "Basis token should be bezier for bezier curve";

  pxr::VtValue type;
  pxr::UsdAttribute type_attr = curve.GetTypeAttr();
  type_attr.Get(&type);
  auto type_token = type.Get<pxr::TfToken>();

  EXPECT_EQ(type_token, pxr::UsdGeomTokens->cubic)
      << "Type token should be cubic for bezier curve";

  pxr::VtValue wrap;
  pxr::UsdAttribute wrap_attr = curve.GetWrapAttr();
  wrap_attr.Get(&wrap);
  auto wrap_token = wrap.Get<pxr::TfToken>();

  if (is_periodic) {
    EXPECT_EQ(wrap_token, pxr::UsdGeomTokens->periodic)
        << "Wrap token should be periodic for periodic curve";
  }
  else {
    EXPECT_EQ(wrap_token, pxr::UsdGeomTokens->nonperiodic)
        << "Wrap token should be nonperiodic for nonperiodic curve";
  }

  auto widths_interp_token = curve.GetWidthsInterpolation();
  EXPECT_EQ(widths_interp_token, pxr::UsdGeomTokens->varying)
      << "Widths interpolation token should be varying for bezier curve";

  pxr::UsdAttribute vert_count_attr = curve.GetCurveVertexCountsAttr();
  pxr::VtArray<int> vert_counts;
  vert_count_attr.Get(&vert_counts);

  EXPECT_EQ(vert_counts.size(), 1) << "Prim should only contains verts for a single curve";
  EXPECT_EQ(vert_counts[0], vertex_count) << "Curve should have " << vertex_count << " verts.";
}

/**
 * Test that the provided prim is a valid NURBS curve. We also check it matches the expected
 * wrap type, and has the expected number of vertices. For NURBS, we also validate that the knots
 * layout matches the expected layout for periodic/non-periodic curves according to the USD spec.
 */
static void check_nurbs_curve(const pxr::UsdPrim nurbs_prim,
                              const int vertex_count,
                              const int knots_count,
                              const int order)
{
  auto curve = pxr::UsdGeomNurbsCurves(nurbs_prim);

  pxr::UsdAttribute order_attr = curve.GetOrderAttr();
  pxr::VtArray<int> orders;
  order_attr.Get(&orders);

  EXPECT_EQ(orders.size(), 2) << "Prim should contain orders for two curves";
  EXPECT_EQ(orders[0], order) << "Curves should have order " << order;
  EXPECT_EQ(orders[1], order) << "Curves should have order " << order;

  pxr::UsdAttribute knots_attr = curve.GetKnotsAttr();
  pxr::VtArray<double> knots;
  knots_attr.Get(&knots);

  EXPECT_EQ(knots.size(), knots_count) << "Curve should have " << knots_count << " knots.";
  for (int i = 0; i < 2; i++) {
    int zeroth_knot_index = i * (knots_count / 2);

    EXPECT_EQ(knots[zeroth_knot_index], knots[zeroth_knot_index + 1])
        << "NURBS curve should satisfy this knots rule for a nonperiodic curve";
    EXPECT_EQ(knots[knots.size() - 1], knots[knots.size() - 2])
        << "NURBS curve should satisfy this knots rule for a nonperiodic curve";
  }

  auto widths_interp_token = curve.GetWidthsInterpolation();
  EXPECT_EQ(widths_interp_token, pxr::UsdGeomTokens->vertex)
      << "Widths interpolation token should be vertex for NURBS curve";

  pxr::UsdAttribute vert_count_attr = curve.GetCurveVertexCountsAttr();
  pxr::VtArray<int> vert_counts;
  vert_count_attr.Get(&vert_counts);

  EXPECT_EQ(vert_counts.size(), 2) << "Prim should contain verts for two curves";
  EXPECT_EQ(vert_counts[0], vertex_count) << "Curve should have " << vertex_count << " verts.";
  EXPECT_EQ(vert_counts[1], vertex_count) << "Curve should have " << vertex_count << " verts.";
}

/**
 * Test that the provided prim is a valid NURBS curve. We also check it matches the expected
 * wrap type, and has the expected number of vertices. For NURBS, we also validate that the knots
 * layout matches the expected layout for periodic/non-periodic curves according to the USD spec.
 */
static void check_nurbs_circle(const pxr::UsdPrim nurbs_prim,
                               const int vertex_count,
                               const int knots_count,
                               const int order)
{
  auto curve = pxr::UsdGeomNurbsCurves(nurbs_prim);

  pxr::UsdAttribute order_attr = curve.GetOrderAttr();
  pxr::VtArray<int> orders;
  order_attr.Get(&orders);

  EXPECT_EQ(orders.size(), 1) << "Prim should contain orders for one curves";
  EXPECT_EQ(orders[0], order) << "Curve should have order " << order;

  pxr::UsdAttribute knots_attr = curve.GetKnotsAttr();
  pxr::VtArray<double> knots;
  knots_attr.Get(&knots);

  EXPECT_EQ(knots.size(), knots_count) << "Curve should have " << knots_count << " knots.";

  EXPECT_EQ(knots[0], knots[1] - (knots[knots.size() - 2] - knots[knots.size() - 3]))
      << "NURBS curve should satisfy this knots rule for a periodic curve";
  EXPECT_EQ(knots[knots.size() - 1], knots[knots.size() - 2] + (knots[2] - knots[1]))
      << "NURBS curve should satisfy this knots rule for a periodic curve";

  auto widths_interp_token = curve.GetWidthsInterpolation();
  EXPECT_EQ(widths_interp_token, pxr::UsdGeomTokens->vertex)
      << "Widths interpolation token should be vertex for NURBS curve";

  pxr::UsdAttribute vert_count_attr = curve.GetCurveVertexCountsAttr();
  pxr::VtArray<int> vert_counts;
  vert_count_attr.Get(&vert_counts);

  EXPECT_EQ(vert_counts.size(), 1) << "Prim should contain verts for one curve";
  EXPECT_EQ(vert_counts[0], vertex_count) << "Curve should have " << vertex_count << " verts.";
}

}  // namespace blender::io::usd
