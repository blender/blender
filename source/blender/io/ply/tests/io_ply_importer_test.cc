#include "testing/testing.h"
#include "tests/blendfile_loading_base_test.h"

#include "BKE_curve.h"
#include "BKE_customdata.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_scene.h"

#include "BLI_listbase.h"
#include "BLI_math_base.hh"
#include "BLI_math_vec_types.hh"

#include "BLO_readfile.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "DNA_curve_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "intern/ply_data.hh"
#include "ply_import.hh"

namespace blender::io::ply {

enum PLYFileType { ASCII, BINARY_LITTLE_ENDIAN, BINARY_BIG_ENDIAN };

struct Expectation {
  std::string name;
  PLYFileType type;
  PlyData *data;
  int totvert, totface, totedge;
  float3 vert_first, vert_last;
  // float3 normal_first;
  // float2 uv_first
  // float4 color_first = {-1, -1, -1, -1};

};

class PlyImportTest : public BlendfileLoadingBaseTest {
 public:
  PlyData cube;

  void SetUp() override
  {
    cube.vertices = {{1, 1, -1},
                     {1, -1, -1},
                     {-1, -1, -1},
                     {-1, 1, -1},
                     {1, 0.999999, 1},
                     {0.999999, -1, 1},
                     {-1, -1, 1},
                     {-1, 1, 1}};
  }

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

    // Import the test file
    std::string ply_path = blender::tests::flags_test_asset_dir() + "/io_tests/ply/" + path;
    strncpy(params.filepath, ply_path.c_str(), FILE_MAX - 1);
    importer_main(bfile->main, bfile->curscene, bfile->cur_view_layer, params);

    depsgraph_create(DAG_EVAL_VIEWPORT);

    DEGObjectIterSettings deg_iter_settings{};
    deg_iter_settings.depsgraph = depsgraph;
    deg_iter_settings.flags = DEG_ITER_OBJECT_FLAG_LINKED_DIRECTLY |
                              DEG_ITER_OBJECT_FLAG_LINKED_VIA_SET | DEG_ITER_OBJECT_FLAG_VISIBLE |
                              DEG_ITER_OBJECT_FLAG_DUPLI;
    size_t object_index = 0;

    // Iterate over the objects in the viewport
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

        // Test if mesh has expected amount of vertices, edges, and faces
        ASSERT_EQ(mesh->totvert, exp.totvert);
        ASSERT_EQ(mesh->totedge, exp.totedge);
        ASSERT_EQ(mesh->totface, exp.totface);

        // Test if first and last vertices match
        const Span<MVert> verts = mesh->verts();
        EXPECT_V3_NEAR(verts.first().co, exp.vert_first, 0.0001f);
        EXPECT_V3_NEAR(verts.last().co, exp.vert_last, 0.0001f);

        // Fetch normal data from mesh and test if it matches expectation
        // Currently we don't support normal data yet
        /* const float3 *lnors = (const float3 *)CustomData_get_layer(&mesh->ldata, CD_NORMAL);
        float3 normal_first = lnors != nullptr ? lnors[0] : float3(0, 0, 0);
        EXPECT_V3_NEAR(normal_first, exp.normal_first, 0.0001f); */

        // Fetch UV data from mesh and test if it matches expectation
        // Currently we don't support uv data yet
        /* const MLoopUV *mloopuv = static_cast<const MLoopUV *>(
            CustomData_get_layer(&mesh->ldata, CD_MLOOPUV));
        float2 uv_first = mloopuv ? float2(mloopuv->uv) : float2(0, 0);
        EXPECT_V2_NEAR(uv_first, exp.uv_first, 0.0001f); */

        // Check if expected mesh has vertex colours, and if it does => fetch vertex colour data and test if it matches
        // Currently we don't support vertex colours yet.
        /* if (exp.color_first.x >= 0) {
          const float4 *colors = (const float4 *)CustomData_get_layer(&mesh->vdata, CD_PROP_COLOR);
          EXPECT_TRUE(colors != nullptr);
          EXPECT_V4_NEAR(colors[0], exp.color_first, 0.0001f);
        }
        else {
          EXPECT_FALSE(CustomData_has_layer(&mesh->vdata, CD_PROP_COLOR));
        } */
      }
      ++object_index;
    }

    DEG_OBJECT_ITER_END;
    EXPECT_EQ(object_index, expect_count);
  }
};

TEST_F(PlyImportTest, PLYImportCube)
{
  PlyData plyData;
  plyData.vertices = {{1, 1, -1},
                      {1, -1, -1},
                      {-1, -1, -1},
                      {-1, 1, -1},
                      {1, 0.999999, 1},
                      {-1, 1, 1},
                      {-1, -1, 1},
                      {0.999999, -1.000001, 1},
                      {1, 1, -1},
                      {1, 0.999999, 1},
                      {0.999999, -1.000001, 1},
                      {1, -1, -1},
                      {1, -1, -1},
                      {0.999999, -1.000001, 1},
                      {-1, -1, 1},
                      {-1, -1, -1},
                      {-1, -1, -1},
                      {-1, -1, 1},
                      {-1, 1, 1},
                      {-1, 1, -1},
                      {1, 0.999999, 1},
                      {1, 1, -1},
                      {-1, 1, -1},
                      {-1, 1, 1}};

  plyData.vertex_normals = {{0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, 1},  {0, 0, 1},
                            {0, 0, 1},  {0, 0, 1},  {1, 0, 0},  {1, 0, 0},  {1, 0, 0},  {1, 0, 0},
                            {0, -1, 0}, {0, -1, 0}, {0, -1, 0}, {0, -1, 0}, {-1, 0, 0}, {-1, 0, 0},
                            {-1, 0, 0}, {-1, 0, 0}, {0, 1, 0},  {0, 1, 0},  {0, 1, 0},  {0, 1, 0}};

  plyData.vertex_colors = {{1, 0.8470588235294118, 0},
                           {0, 0.011764705882352941, 1},
                           {0, 0.011764705882352941, 1},
                           {1, 0.8470588235294118, 0},
                           {1, 0.8509803921568627, 0.08627450980392157},
                           {1, 0.8470588235294118, 0},
                           {0, 0.00392156862745098, 1},
                           {0.00392156862745098, 0.00392156862745098, 1},
                           {1, 0.8470588235294118, 0.01568627450980392},
                           {1, 0.8509803921568627, 0.08627450980392157},
                           {0.00392156862745098, 0.00392156862745098, 1},
                           {0, 0.00392156862745098, 1},
                           {0, 0.00392156862745098, 1},
                           {0.00392156862745098, 0.00392156862745098, 1},
                           {0, 0.00392156862745098, 1},
                           {0, 0.00392156862745098, 1},
                           {0, 0.011764705882352941, 1},
                           {0, 0.00392156862745098, 1},
                           {1, 0.8470588235294118, 0},
                           {1, 0.8470588235294118, 0},
                           {1, 0.8509803921568627, 0.08627450980392157},
                           {1, 0.8470588235294118, 0},
                           {1, 0.8470588235294118, 0},
                           {1, 0.8470588235294118, 0}};

  EXPECT_EQ(24, plyData.vertices.size());
  EXPECT_EQ(24, plyData.vertex_normals.size());
  EXPECT_EQ(24, plyData.vertex_colors.size());

  Expectation expect[] = {
      {"OBCube", PLYFileType::ASCII, &cube, 8, 6, 12, float3(1, 1, -1), float3(-1, 1, 1)},
      {"cube_ascii", PLYFileType::ASCII, &plyData, 24, 6, 24, float3(1, 1, -1), float3(-1, 1, 1)}};

  import_and_check("cube_ascii.ply", expect, 2);
}

TEST_F(PlyImportTest, PLYImportBunny) {
  Expectation expect[] = {
    {"OBCube", PLYFileType::ASCII, &cube, 8, 6, 12, float3(1, 1, -1), float3(-1, 1, 1)},
    {"bunny2", PLYFileType::BINARY_LITTLE_ENDIAN, nullptr, 1623, 1000, 1513}
  };
  import_and_check("bunny2.ply", expect, 2);
}

}  // namespace blender::io::ply
