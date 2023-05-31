/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "testing/testing.h"

#include "BLI_fileops.hh"
#include "BLI_hash_mm2a.h"

#include "ply_import.hh"
#include "ply_import_buffer.hh"
#include "ply_import_data.hh"

namespace blender::io::ply {

struct Expectation {
  int totvert, totpoly, totindex, totedge;
  uint16_t polyhash = 0, edgehash = 0;
  float3 vert_first, vert_last;
  float3 normal_first = {0, 0, 0};
  float2 uv_first = {0, 0};
  float4 color_first = {-1, -1, -1, -1};
};

class ply_import_test : public testing::Test {
 public:
  void import_and_check(const char *path, const Expectation &exp)
  {
    std::string ply_path = blender::tests::flags_test_asset_dir() +
                           SEP_STR "io_tests" SEP_STR "ply" SEP_STR + path;

    /* Use a small read buffer size for better coverage of buffer refilling behavior. */
    PlyReadBuffer infile(ply_path.c_str(), 128);
    PlyHeader header;
    const char *header_err = read_header(infile, header);
    if (header_err != nullptr) {
      ADD_FAILURE();
      return;
    }
    std::unique_ptr<PlyData> data = import_ply_data(infile, header);
    if (!data->error.empty()) {
      fprintf(stderr, "%s\n", data->error.c_str());
      ASSERT_EQ(0, exp.totvert);
      ASSERT_EQ(0, exp.totpoly);
      return;
    }

    /* Test expected amount of vertices, edges, and faces. */
    ASSERT_EQ(data->vertices.size(), exp.totvert);
    ASSERT_EQ(data->edges.size(), exp.totedge);
    ASSERT_EQ(data->face_sizes.size(), exp.totpoly);
    ASSERT_EQ(data->face_vertices.size(), exp.totindex);

    /* Test hash of face and edge index data. */
    BLI_HashMurmur2A hash;
    BLI_hash_mm2a_init(&hash, 0);
    uint32_t offset = 0;
    for (uint32_t face_size : data->face_sizes) {
      BLI_hash_mm2a_add(&hash, (const uchar *)&data->face_vertices[offset], face_size * 4);
      offset += face_size;
    }
    uint16_t face_hash = BLI_hash_mm2a_end(&hash);
    if (!data->face_vertices.is_empty()) {
      ASSERT_EQ(face_hash, exp.polyhash);
    }

    if (!data->edges.is_empty()) {
      uint16_t edge_hash = BLI_hash_mm2(
          (const uchar *)data->edges.data(), data->edges.size() * sizeof(data->edges[0]), 0);
      ASSERT_EQ(edge_hash, exp.edgehash);
    }

    /* Test if first and last vertices match. */
    EXPECT_V3_NEAR(data->vertices.first(), exp.vert_first, 0.0001f);
    EXPECT_V3_NEAR(data->vertices.last(), exp.vert_last, 0.0001f);

    /* Check if first normal matches. */
    float3 got_normal = data->vertex_normals.is_empty() ? float3(0, 0, 0) :
                                                          data->vertex_normals.first();
    EXPECT_V3_NEAR(got_normal, exp.normal_first, 0.0001f);

    /* Check if first UV matches. */
    float2 got_uv = data->uv_coordinates.is_empty() ? float2(0, 0) : data->uv_coordinates.first();
    EXPECT_V2_NEAR(got_uv, exp.uv_first, 0.0001f);

    /* Check if first color matches. */
    float4 got_color = data->vertex_colors.is_empty() ? float4(-1, -1, -1, -1) :
                                                        data->vertex_colors.first();
    EXPECT_V4_NEAR(got_color, exp.color_first, 0.0001f);
  }
};

TEST_F(ply_import_test, PLYImportCube)
{
  Expectation expect = {24,
                        6,
                        24,
                        0,
                        26429,
                        0,
                        float3(1, 1, -1),
                        float3(-1, 1, 1),
                        float3(0, 0, -1),
                        float2(0.979336, 0.844958),
                        float4(1, 0.8470, 0, 1)};
  import_and_check("cube_ascii.ply", expect);
}

TEST_F(ply_import_test, PLYImportWireframeCube)
{
  Expectation expect = {8, 0, 0, 12, 0, 31435, float3(-1, -1, -1), float3(1, 1, 1)};
  import_and_check("ASCII_wireframe_cube.ply", expect);
  import_and_check("wireframe_cube.ply", expect);
}

TEST_F(ply_import_test, PLYImportBunny)
{
  Expectation expect = {1623,
                        1000,
                        3000,
                        0,
                        62556,
                        0,
                        float3(0.0380425, 0.109755, 0.0161689),
                        float3(-0.0722821, 0.143895, -0.0129091)};
  import_and_check("bunny2.ply", expect);
}

TEST_F(ply_import_test, PlyImportManySmallHoles)
{
  Expectation expect = {2004,
                        3524,
                        10572,
                        0,
                        15143,
                        0,
                        float3(-0.0131592, -0.0598382, 1.58958),
                        float3(-0.0177622, 0.0105153, 1.61977),
                        float3(0, 0, 0),
                        float2(0, 0),
                        float4(0.7215, 0.6784, 0.6627, 1)};
  import_and_check("many_small_holes.ply", expect);
}

TEST_F(ply_import_test, PlyImportColorNotFull)
{
  Expectation expect = {4, 1, 4, 0, 37235, 0, float3(1, 0, 1), float3(-1, 0, 1)};
  import_and_check("color_not_full_a.ply", expect);
  import_and_check("color_not_full_b.ply", expect);
}

TEST_F(ply_import_test, PlyImportCustomDataElements)
{
  Expectation expect = {600,
                        0,
                        0,
                        0,
                        0,
                        0,
                        float3(-0.78193f, 0.40659f, -1),
                        float3(-0.75537f, 1, -0.24777f),
                        float3(0, 0, 0),
                        float2(0, 0),
                        float4(0.31373f, 0, 0, 1)};
  import_and_check("custom_data_elements.ply", expect);
}

TEST_F(ply_import_test, PlyImportDoubleXYZ)
{
  Expectation expect = {4,
                        1,
                        4,
                        0,
                        37235,
                        0,
                        float3(1, 0, 1),
                        float3(-1, 0, 1),
                        float3(0, 0, 0),
                        float2(0, 0),
                        float4(1, 0, 0, 1)};
  import_and_check("double_xyz_a.ply", expect);
  import_and_check("double_xyz_b.ply", expect);
}

TEST_F(ply_import_test, PlyImportFaceIndicesNotFirstProp)
{
  Expectation expect = {4, 2, 6, 0, 4136, 0, float3(1, 0, 1), float3(-1, 0, 1)};
  import_and_check("face_indices_not_first_prop_a.ply", expect);
  import_and_check("face_indices_not_first_prop_b.ply", expect);
}

TEST_F(ply_import_test, PlyImportFaceIndicesPrecededByList)
{
  Expectation expect = {4, 2, 6, 0, 4136, 0, float3(1, 0, 1), float3(-1, 0, 1)};
  import_and_check("face_indices_preceded_by_list_a.ply", expect);
  import_and_check("face_indices_preceded_by_list_b.ply", expect);
}

TEST_F(ply_import_test, PlyImportFaceUVsColors)
{
  Expectation expect = {4, 1, 4, 0, 37235, 0, float3(1, 0, 1), float3(-1, 0, 1)};
  import_and_check("face_uvs_colors_a.ply", expect);
  import_and_check("face_uvs_colors_b.ply", expect);
}

TEST_F(ply_import_test, PlyImportFacesFirst)
{
  Expectation expect = {4,
                        1,
                        4,
                        0,
                        37235,
                        0,
                        float3(1, 0, 1),
                        float3(-1, 0, 1),
                        float3(0, 0, 0),
                        float2(0, 0),
                        float4(1, 0, 0, 1)};
  import_and_check("faces_first_a.ply", expect);
  import_and_check("faces_first_b.ply", expect);
}

TEST_F(ply_import_test, PlyImportFloatFormats)
{
  Expectation expect = {4,
                        1,
                        4,
                        0,
                        37235,
                        0,
                        float3(1, 0, 1),
                        float3(-1, 0, 1),
                        float3(0, 0, 0),
                        float2(0, 0),
                        float4(0.5f, 0, 0.25f, 1)};
  import_and_check("float_formats_a.ply", expect);
  import_and_check("float_formats_b.ply", expect);
}

TEST_F(ply_import_test, PlyImportPositionNotFull)
{
  Expectation expect = {0, 0, 0, 0};
  import_and_check("position_not_full_a.ply", expect);
  import_and_check("position_not_full_b.ply", expect);
}

TEST_F(ply_import_test, PlyImportTristrips)
{
  Expectation expect = {6, 4, 12, 0, 3404, 0, float3(1, 0, 1), float3(-3, 0, 1)};
  import_and_check("tristrips_a.ply", expect);
  import_and_check("tristrips_b.ply", expect);
}

TEST_F(ply_import_test, PlyImportTypeAliases)
{
  Expectation expect = {4,
                        1,
                        4,
                        0,
                        37235,
                        0,
                        float3(1, 0, 1),
                        float3(-1, 0, 1),
                        float3(0, 0, 0),
                        float2(0, 0),
                        float4(220 / 255.0f, 20 / 255.0f, 20 / 255.0f, 1)};
  import_and_check("type_aliases_a.ply", expect);
  import_and_check("type_aliases_b.ply", expect);
  import_and_check("type_aliases_be_b.ply", expect);
}

TEST_F(ply_import_test, PlyImportVertexCompOrder)
{
  Expectation expect = {4,
                        1,
                        4,
                        0,
                        37235,
                        0,
                        float3(1, 0, 1),
                        float3(-1, 0, 1),
                        float3(0, 0, 0),
                        float2(0, 0),
                        float4(0.8f, 0.2f, 0, 1)};
  import_and_check("vertex_comp_order_a.ply", expect);
  import_and_check("vertex_comp_order_b.ply", expect);
}

//@TODO: test with vertex element having list properties
//@TODO: test with edges starting with non-vertex index properties
//@TODO: test various malformed headers
//@TODO: UVs with: s,t; u,v; texture_u,texture_v; texture_s,texture_t (from miniply)
//@TODO: colors with: r,g,b in addition to red,green,blue (from miniply)
//@TODO: importing bunny2 with old importer results in smooth shading; flat shading with new one

}  // namespace blender::io::ply
