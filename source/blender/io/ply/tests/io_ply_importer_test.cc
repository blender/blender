/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "tests/blendfile_loading_base_test.h"

#include "BKE_attribute.hh"
#include "BKE_mesh.hh"
#include "BKE_object.h"

#include "BLO_readfile.h"

#include "DEG_depsgraph_query.h"

#include "IO_ply.h"
#include "ply_data.hh"
#include "ply_import.hh"
#include "ply_import_binary.hh"

namespace blender::io::ply {

struct Expectation {
  std::string name;
  PlyFormatType type;
  int totvert, totpoly, totedge;
  float3 vert_first, vert_last;
  float3 normal_first = {0, 0, 0};
  float2 uv_first;
  float4 color_first = {-1, -1, -1, -1};
};

class PlyImportTest : public BlendfileLoadingBaseTest {
 public:
  void import_and_check(const char *path, const Expectation *expect, size_t expect_count)
  {
    if (!blendfile_load("io_tests/blend_geometry/all_quads.blend")) {
      ADD_FAILURE();
      return;
    }

    PLYImportParams params;
    params.global_scale = 1.0f;
    params.forward_axis = IO_AXIS_NEGATIVE_Z;
    params.up_axis = IO_AXIS_Y;
    params.merge_verts = false;
    params.vertex_colors = PLY_VERTEX_COLOR_NONE;

    /* Import the test file. */
    std::string ply_path = blender::tests::flags_test_asset_dir() + "/io_tests/ply/" + path;
    strncpy(params.filepath, ply_path.c_str(), FILE_MAX - 1);
    importer_main(bfile->main, bfile->curscene, bfile->cur_view_layer, params, nullptr);

    depsgraph_create(DAG_EVAL_VIEWPORT);

    DEGObjectIterSettings deg_iter_settings{};
    deg_iter_settings.depsgraph = depsgraph;
    deg_iter_settings.flags = DEG_ITER_OBJECT_FLAG_LINKED_DIRECTLY |
                              DEG_ITER_OBJECT_FLAG_LINKED_VIA_SET | DEG_ITER_OBJECT_FLAG_VISIBLE |
                              DEG_ITER_OBJECT_FLAG_DUPLI;
    size_t object_index = 0;

    /* Iterate over the objects in the viewport */
    DEG_OBJECT_ITER_BEGIN (&deg_iter_settings, object) {
      if (object_index >= expect_count) {
        ADD_FAILURE();
        break;
      }

      const Expectation &exp = expect[object_index];

      ASSERT_STREQ(object->id.name, exp.name.c_str());
      EXPECT_V3_NEAR(object->loc, float3(0, 0, 0), 0.0001f);

      EXPECT_V3_NEAR(object->scale, float3(1, 1, 1), 0.0001f);
      if (object->type == OB_MESH) {
        Mesh *mesh = BKE_object_get_evaluated_mesh(object);

        /* Test if mesh has expected amount of vertices, edges, and faces. */
        ASSERT_EQ(mesh->totvert, exp.totvert);
        ASSERT_EQ(mesh->totedge, exp.totedge);
        ASSERT_EQ(mesh->totpoly, exp.totpoly);

        /* Test if first and last vertices match. */
        const Span<float3> verts = mesh->vert_positions();
        EXPECT_V3_NEAR(verts.first(), exp.vert_first, 0.0001f);
        EXPECT_V3_NEAR(verts.last(), exp.vert_last, 0.0001f);

        /* Fetch normal data from mesh and test if it matches expectation. */
        if (BKE_mesh_has_custom_loop_normals(mesh)) {
          const Span<float3> vertex_normals = mesh->vert_normals();
          ASSERT_FALSE(vertex_normals.is_empty());
          EXPECT_V3_NEAR(vertex_normals[0], exp.normal_first, 0.0001f);
        }

        /* Fetch UV data from mesh and test if it matches expectation. */
        blender::bke::AttributeAccessor attributes = mesh->attributes();
        VArray<float2> uvs = attributes.lookup<float2>("UVMap");
        float2 uv_first = !uvs.is_empty() ? uvs[0] : float2(0, 0);
        EXPECT_V2_NEAR(uv_first, exp.uv_first, 0.0001f);

        /* Check if expected mesh has vertex colors, and tests if it matches. */
        if (CustomData_has_layer(&mesh->vdata, CD_PROP_COLOR)) {
          const float4 *colors = (const float4 *)CustomData_get_layer(&mesh->vdata, CD_PROP_COLOR);
          ASSERT_TRUE(colors != nullptr);
          EXPECT_V4_NEAR(colors[0], exp.color_first, 0.0001f);
        }
      }
      ++object_index;
    }

    DEG_OBJECT_ITER_END;
    EXPECT_EQ(object_index, expect_count);
  }
};

TEST_F(PlyImportTest, PLYImportCube)
{
  Expectation expect[] = {{"OBCube",
                           ASCII,
                           8,
                           6,
                           12,
                           float3(1, 1, -1),
                           float3(-1, 1, 1),
                           float3(0.5773, 0.5773, -0.5773),
                           float2(0, 0)},
                          {"OBcube_ascii",
                           ASCII,
                           24,
                           6,
                           24,
                           float3(1, 1, -1),
                           float3(-1, 1, 1),
                           float3(0, 0, -1),
                           float2(0.979336, 0.844958),
                           float4(1, 0.8470, 0, 1)}};
  import_and_check("cube_ascii.ply", expect, 2);
}

TEST_F(PlyImportTest, PLYImportASCIIEdgeTest)
{
  Expectation expect[] = {{"OBCube",
                           ASCII,
                           8,
                           6,
                           12,
                           float3(1, 1, -1),
                           float3(-1, 1, 1),
                           float3(0.5773, 0.5773, -0.5773)},
                          {"OBASCII_wireframe_cube",
                           ASCII,
                           8,
                           0,
                           12,
                           float3(-1, -1, -1),
                           float3(1, 1, 1),
                           float3(-2, 0, -1)}};

  import_and_check("ASCII_wireframe_cube.ply", expect, 2);
}

TEST_F(PlyImportTest, PLYImportBunny)
{
  Expectation expect[] = {{"OBCube",
                           ASCII,
                           8,
                           6,
                           12,
                           float3(1, 1, -1),
                           float3(-1, 1, 1),
                           float3(0.5773, 0.5773, -0.5773)},
                          {"OBbunny2",
                           BINARY_LE,
                           1623,
                           1000,
                           1513,
                           float3(0.0380425, 0.109755, 0.0161689),
                           float3(-0.0722821, 0.143895, -0.0129091),
                           float3(-2, -2, -2)}};
  import_and_check("bunny2.ply", expect, 2);
}

TEST_F(PlyImportTest, PlyImportManySmallHoles)
{
  Expectation expect[] = {{"OBCube",
                           ASCII,
                           8,
                           6,
                           12,
                           float3(1, 1, -1),
                           float3(-1, 1, 1),
                           float3(0.5773, 0.5773, -0.5773)},
                          {"OBmany_small_holes",
                           BINARY_LE,
                           2004,
                           3524,
                           5564,
                           float3(-0.0131592, -0.0598382, 1.58958),
                           float3(-0.0177622, 0.0105153, 1.61977),
                           float3(-2, -2, -2),
                           float2(0, 0),
                           float4(0.7215, 0.6784, 0.6627, 1)}};
  import_and_check("many_small_holes.ply", expect, 2);
}

TEST_F(PlyImportTest, PlyImportWireframeCube)
{
  Expectation expect[] = {{"OBCube",
                           ASCII,
                           8,
                           6,
                           12,
                           float3(1, 1, -1),
                           float3(-1, 1, 1),
                           float3(0.5773, 0.5773, -0.5773)},
                          {"OBwireframe_cube",
                           BINARY_LE,
                           8,
                           0,
                           12,
                           float3(-1, -1, -1),
                           float3(1, 1, 1),
                           float3(-2, -2, -2)}};
  import_and_check("wireframe_cube.ply", expect, 2);
}

TEST(PlyImportFunctionsTest, PlySwapBytes)
{
  /* Individual bits shouldn't swap with each other. */
  uint8_t val8 = 0xA8;
  uint8_t exp8 = 0xA8;
  uint8_t actual8 = swap_bytes<uint8_t>(val8);
  ASSERT_EQ(exp8, actual8);

  uint16_t val16 = 0xFEB0;
  uint16_t exp16 = 0xB0FE;
  uint16_t actual16 = swap_bytes<uint16_t>(val16);
  ASSERT_EQ(exp16, actual16);

  uint32_t val32 = 0x80A37B0A;
  uint32_t exp32 = 0x0A7BA380;
  uint32_t actual32 = swap_bytes<uint32_t>(val32);
  ASSERT_EQ(exp32, actual32);

  uint64_t val64 = 0x0102030405060708;
  uint64_t exp64 = 0x0807060504030201;
  uint64_t actual64 = swap_bytes<uint64_t>(val64);
  ASSERT_EQ(exp64, actual64);
}

}  // namespace blender::io::ply
