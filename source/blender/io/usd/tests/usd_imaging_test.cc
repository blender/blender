/* SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#include "testing/testing.h"

#include "usd_tests_common.h"

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/capsule.h>
#include <pxr/usdImaging/usdImaging/capsuleAdapter.h>

namespace blender::io::usd {

class USDImagingTest : public testing::Test {
};

TEST_F(USDImagingTest, CapsuleAdapterTest)
{
  /* A simple test to exercise the UsdImagingGprimAdapter API to
   * ensure the code compiles, links and returns reasonable results.
   * We create a capsule shape on an in-memory stage and attempt
   * to access the shape's points and topology. */

  /* We must register USD plugin paths before creating the stage
   * to avoid a crash in the USD asset resolver initialization code. */
  if (register_usd_plugins_for_tests().empty()) {
    FAIL();
    return;
  }

  pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();

  if (!stage) {
    FAIL() << "Couldn't create in-memory stage.";
    return;
  }

  pxr::UsdGeomCapsule capsule = pxr::UsdGeomCapsule::Define(stage, pxr::SdfPath("/Capsule"));

  if (!capsule) {
    FAIL() << "Couldn't create UsdGeomCapsule.";
    return;
  }

  pxr::UsdImagingCapsuleAdapter capsule_adapter;
  pxr::VtValue points_value = capsule_adapter.GetPoints(capsule.GetPrim(),
                                                        pxr::UsdTimeCode::Default());
  if (!points_value.IsHolding<pxr::VtArray<pxr::GfVec3f>>()) {
    FAIL() << "Mesh points value holding unexpected type.";
    return;
  }

  pxr::VtArray<pxr::GfVec3f> points = points_value.Get<pxr::VtArray<pxr::GfVec3f>>();
  EXPECT_FALSE(points.empty());

  pxr::VtValue topology_value = capsule_adapter.GetTopology(
      capsule.GetPrim(), pxr::SdfPath(), pxr::UsdTimeCode::Default());

  if (!topology_value.IsHolding<pxr::HdMeshTopology>()) {
    FAIL() << "Mesh topology value holding unexpected type.";
    return;
  }

  pxr::HdMeshTopology topology = topology_value.Get<pxr::HdMeshTopology>();

  pxr::VtArray<int> vertex_counts = topology.GetFaceVertexCounts();
  EXPECT_FALSE(vertex_counts.empty());

  pxr::VtArray<int> vertex_indices = topology.GetFaceVertexIndices();
  EXPECT_FALSE(vertex_indices.empty());
}

}  // namespace blender::io::usd
