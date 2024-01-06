/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "tests/blendfile_loading_base_test.h"

#include "BKE_mesh.hh"
#include "BKE_object.hh"

#include "BLI_math_base.h"
#include "BLI_math_vector_types.hh"
#include "BLI_string.h"

#include "BLO_readfile.h"

#include "DEG_depsgraph_query.hh"

#include "stl_import.hh"

namespace blender::io::stl {

struct Expectation {
  int verts_num, edges_num, faces_num, corners_num;
  float3 vert_first, vert_last;
};

class stl_importer_test : public BlendfileLoadingBaseTest {
 public:
  stl_importer_test()
  {
    params.forward_axis = IO_AXIS_NEGATIVE_Z;
    params.up_axis = IO_AXIS_Y;
    params.use_facet_normal = false;
    params.use_scene_unit = false;
    params.global_scale = 1.0f;
    params.use_mesh_validate = true;
  }

  void import_and_check(const char *path, const Expectation &expect)
  {
    if (!blendfile_load("io_tests" SEP_STR "blend_geometry" SEP_STR "all_quads.blend")) {
      ADD_FAILURE();
      return;
    }

    std::string stl_path = blender::tests::flags_test_asset_dir() +
                           SEP_STR "io_tests" SEP_STR "stl" SEP_STR + path;
    STRNCPY(params.filepath, stl_path.c_str());
    importer_main(bfile->main, bfile->curscene, bfile->cur_view_layer, params);

    depsgraph_create(DAG_EVAL_VIEWPORT);

    DEGObjectIterSettings deg_iter_settings{};
    deg_iter_settings.depsgraph = depsgraph;
    deg_iter_settings.flags = DEG_ITER_OBJECT_FLAG_LINKED_DIRECTLY |
                              DEG_ITER_OBJECT_FLAG_LINKED_VIA_SET | DEG_ITER_OBJECT_FLAG_VISIBLE |
                              DEG_ITER_OBJECT_FLAG_DUPLI;

    constexpr bool print_result_scene = false;
    if (print_result_scene) {
      printf("Result was:\n");
      DEG_OBJECT_ITER_BEGIN (&deg_iter_settings, object) {
        printf("  {");
        if (object->type == OB_MESH) {
          Mesh *mesh = BKE_object_get_evaluated_mesh(object);
          const Span<float3> positions = mesh->vert_positions();
          printf("%i, %i, %i, %i, float3(%g, %g, %g), float3(%g, %g, %g)",
                 mesh->verts_num,
                 mesh->edges_num,
                 mesh->faces_num,
                 mesh->corners_num,
                 positions.first().x,
                 positions.first().y,
                 positions.first().z,
                 positions.last().x,
                 positions.last().y,
                 positions.last().z);
        }
        printf("},\n");
      }
      DEG_OBJECT_ITER_END;
    }

    size_t object_index = 0;
    DEG_OBJECT_ITER_BEGIN (&deg_iter_settings, object) {
      ++object_index;
      /* First object is from loaded scene. */
      if (object_index == 1) {
        continue;
      }
      EXPECT_V3_NEAR(object->loc, float3(0, 0, 0), 0.0001f);
      EXPECT_V3_NEAR(object->rot, float3(M_PI_2, 0, 0), 0.0001f);
      EXPECT_V3_NEAR(object->scale, float3(1, 1, 1), 0.0001f);
      Mesh *mesh = BKE_object_get_evaluated_mesh(object);
      EXPECT_EQ(mesh->verts_num, expect.verts_num);
      EXPECT_EQ(mesh->edges_num, expect.edges_num);
      EXPECT_EQ(mesh->faces_num, expect.faces_num);
      EXPECT_EQ(mesh->corners_num, expect.corners_num);
      const Span<float3> positions = mesh->vert_positions();
      EXPECT_V3_NEAR(positions.first(), expect.vert_first, 0.0001f);
      EXPECT_V3_NEAR(positions.last(), expect.vert_last, 0.0001f);
      break;
    }
    DEG_OBJECT_ITER_END;
  }

  STLImportParams params;
};

TEST_F(stl_importer_test, all_quads)
{
  Expectation expect = {8, 18, 12, 36, float3(1, 1, 1), float3(1, -1, 1)};
  import_and_check("all_quads.stl", expect);
}

TEST_F(stl_importer_test, cubes_positioned)
{
  Expectation expect = {24, 54, 36, 108, float3(1, 1, 1), float3(5.49635f, 0.228398f, -1.11237f)};
  import_and_check("cubes_positioned.stl", expect);
}

TEST_F(stl_importer_test, non_uniform_scale)
{
  Expectation expect = {140, 378, 252, 756, float3(0, 0, -0.3f), float3(-0.866025f, -1.5f, 0)};
  import_and_check("non_uniform_scale.stl", expect);
}

}  // namespace blender::io::stl
