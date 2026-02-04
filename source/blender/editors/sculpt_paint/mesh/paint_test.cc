/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_paint_bvh.hh"
#include "BKE_subdiv.hh"
#include "BKE_subdiv_ccg.hh"
#include "BKE_subdiv_eval.hh"

#include "DNA_mesh_types.h"

#include "GEO_mesh_primitive_cuboid.hh"

#include "CLG_log.h"

#include "intern/bmesh_mesh.hh"
#include "intern/bmesh_mesh_convert.hh"

#include "sculpt_dyntopo.hh"

#include "testing/testing.h"

namespace blender::bke::tests {
class MeshPaintBVHTest : public testing::Test {
 public:
  Mesh *cube_mesh;

  static void SetUpTestSuite()
  {
    CLG_init();
    BKE_idtype_init();
  }

  static void TearDownTestSuite()
  {
    CLG_exit();
  }

  void SetUp() override
  {
    cube_mesh = geometry::create_cuboid_mesh(float3(1.0, 1.0, 1.0), 10, 10, 10);
  }

  void TearDown() override
  {
    BKE_id_free(nullptr, cube_mesh);
  }
};

TEST_F(MeshPaintBVHTest, from_mesh)
{
  pbvh::Tree tree = pbvh::Tree::from_mesh(*cube_mesh);
  EXPECT_GT(tree.nodes<pbvh::MeshNode>().size(), 0)
      << "Paint BVH should have some non-zero amount of nodes";
}

class GridsBVHTest : public testing::Test {
 public:
  Mesh *cube_mesh;
  std::unique_ptr<SubdivCCG> subdiv_ccg;

  static void SetUpTestSuite()
  {
    CLG_init();
    BKE_idtype_init();
  }

  static void TearDownTestSuite()
  {
    CLG_exit();
  }

  void SetUp() override
  {
    cube_mesh = geometry::create_cuboid_mesh(float3(1.0, 1.0, 1.0), 10, 10, 10);
    subdiv::Settings settings;
    settings.is_adaptive = true;
    settings.level = 4;
    settings.use_creases = true;
    settings.vtx_boundary_interpolation = subdiv::SUBDIV_VTX_BOUNDARY_EDGE_AND_CORNER;
    settings.fvar_linear_interpolation = subdiv::SUBDIV_FVAR_LINEAR_INTERPOLATION_BOUNDARIES;

    subdiv::Subdiv *subdiv = bke::subdiv::new_from_mesh(&settings, cube_mesh);
    subdiv::eval_begin_from_mesh(subdiv, cube_mesh, subdiv::SUBDIV_EVALUATOR_TYPE_CPU);

    SubdivToCCGSettings subdiv_to_ccg_settings;
    subdiv_to_ccg_settings.resolution = 3;
    subdiv_to_ccg_settings.need_normal = true;
    subdiv_to_ccg_settings.need_mask = false;
    subdiv_ccg = BKE_subdiv_to_ccg(*subdiv, subdiv_to_ccg_settings, *cube_mesh, nullptr);
  }

  void TearDown() override
  {
    BKE_id_free(nullptr, cube_mesh);
  }
};

TEST_F(GridsBVHTest, from_grids)
{
  pbvh::Tree tree = pbvh::Tree::from_grids(*cube_mesh, *subdiv_ccg);
  EXPECT_GT(tree.nodes<pbvh::GridsNode>().size(), 0)
      << "Paint BVH should have some non-zero amount of nodes";
}

class BMeshPaintBVHTest : public testing::Test {
 public:
  Mesh *cube_mesh;
  BMesh *bm;

  static void SetUpTestSuite()
  {
    CLG_init();
    BKE_idtype_init();
  }

  static void TearDownTestSuite()
  {
    CLG_exit();
  }

  void SetUp() override
  {
    cube_mesh = geometry::create_cuboid_mesh(float3(1.0, 1.0, 1.0), 10, 10, 10);
    /* Create triangles-only BMesh. */
    const BMAllocTemplate allocsize = BMALLOC_TEMPLATE_FROM_ME(cube_mesh);

    BMeshCreateParams create_params{};
    create_params.use_toolflags = false;

    bm = BM_mesh_create(&allocsize, &create_params);

    BMeshFromMeshParams convert_params{};
    convert_params.calc_face_normal = true;
    convert_params.calc_vert_normal = true;
    BM_mesh_bm_from_me(bm, cube_mesh, &convert_params);

    ed::sculpt_paint::dyntopo::triangulate(bm);

    BM_data_layer_ensure_named(bm, &bm->vdata, CD_PROP_INT32, ".sculpt_dyntopo_node_id_vertex");
    BM_data_layer_ensure_named(bm, &bm->pdata, CD_PROP_INT32, ".sculpt_dyntopo_node_id_face");
  }

  void TearDown() override
  {
    BM_mesh_free(bm);
    BKE_id_free(nullptr, cube_mesh);
  }
};

TEST_F(BMeshPaintBVHTest, from_bmesh)
{
  pbvh::Tree tree = pbvh::Tree::from_bmesh(*bm);
  EXPECT_GT(tree.nodes<pbvh::BMeshNode>().size(), 0)
      << "Paint BVH should have some non-zero amount of nodes";
}
}  // namespace blender::bke::tests
