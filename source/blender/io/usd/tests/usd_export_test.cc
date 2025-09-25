/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "testing/testing.h"
#include "tests/blendfile_loading_base_test.h"

#include <pxr/base/tf/stringUtils.h>
#include <pxr/base/vt/types.h>
#include <pxr/base/vt/value.h>
#include <pxr/usd/sdf/types.h>
#include <pxr/usd/usd/common.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/mesh.h>

#include "DNA_image_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_node_types.h"

#include "BKE_context.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"

#include "BLI_fileops.h"
#include "BLI_listbase.h"
#include "BLI_math_vector_types.hh"
#include "BLI_path_utils.hh"

#include "BLO_readfile.hh"

#include "BKE_node_runtime.hh"

#include "DEG_depsgraph.hh"

#include "usd.hh"
#include "usd_utils.hh"
#include "usd_writer_material.hh"

namespace blender::io::usd {

const StringRefNull simple_scene_filename = "usd/usd_simple_scene.blend";
const StringRefNull materials_filename = "usd/usd_materials_export.blend";
const StringRefNull output_filename = "output.usd";

static const bNode *find_node_for_type_in_graph(const bNodeTree *nodetree,
                                                const blender::StringRefNull type_idname);

class UsdExportTest : public BlendfileLoadingBaseTest {
 protected:
  bContext *context = nullptr;

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

  void SetUp() override
  {
    BlendfileLoadingBaseTest::SetUp();
  }

  void TearDown() override
  {
    BlendfileLoadingBaseTest::TearDown();
    CTX_free(context);
    context = nullptr;

    if (BLI_exists(output_filename.c_str())) {
      BLI_delete(output_filename.c_str(), false, false);
    }
  }

  pxr::UsdPrim get_first_child_mesh(const pxr::UsdPrim prim)
  {
    for (auto child : prim.GetChildren()) {
      if (child.IsA<pxr::UsdGeomMesh>()) {
        return child;
      }
    }
    return pxr::UsdPrim();
  }

  /**
   * Loop the sockets on the Blender `bNode`, and fail if any of their values do
   * not match the equivalent Attribute values on the `UsdPrim`.
   */
  void compare_blender_node_to_usd_prim(const bNode *bsdf_node, const pxr::UsdPrim &bsdf_prim)
  {
    ASSERT_NE(bsdf_node, nullptr);
    ASSERT_TRUE(bool(bsdf_prim));

    for (const auto *socket : bsdf_node->input_sockets()) {
      const pxr::TfToken attribute_token = blender::io::usd::token_for_input(socket->name);
      if (attribute_token.IsEmpty()) {
        /* This socket is not translated between Blender and USD. */
        continue;
      }

      const pxr::UsdAttribute bsdf_attribute = bsdf_prim.GetAttribute(attribute_token);
      pxr::SdfPathVector paths;
      bsdf_attribute.GetConnections(&paths);
      if (!paths.empty() || !bsdf_attribute.IsValid()) {
        /* Skip if the attribute is connected or has an error. */
        continue;
      }

      const float socket_value_f = *socket->default_value_typed<float>();
      const float3 socket_value_3f = *socket->default_value_typed<float3>();
      float attribute_value_f;
      pxr::GfVec3f attribute_value_3f;

      switch (socket->type) {
        case SOCK_FLOAT:
          bsdf_attribute.Get(&attribute_value_f, 0.0);
          EXPECT_FLOAT_EQ(socket_value_f, attribute_value_f);
          break;

        case SOCK_VECTOR:
          bsdf_attribute.Get(&attribute_value_3f, 0.0);
          EXPECT_FLOAT_EQ(socket_value_3f[0], attribute_value_3f[0]);
          EXPECT_FLOAT_EQ(socket_value_3f[1], attribute_value_3f[1]);
          EXPECT_FLOAT_EQ(socket_value_3f[2], attribute_value_3f[2]);
          break;

        case SOCK_RGBA:
          bsdf_attribute.Get(&attribute_value_3f, 0.0);
          EXPECT_FLOAT_EQ(socket_value_3f[0], attribute_value_3f[0]);
          EXPECT_FLOAT_EQ(socket_value_3f[1], attribute_value_3f[1]);
          EXPECT_FLOAT_EQ(socket_value_3f[2], attribute_value_3f[2]);
          break;

        default:
          FAIL() << "Socket " << socket->name << " has unsupported type " << socket->type;
          break;
      }
    }
  }

  void compare_blender_image_to_usd_image_shader(const bNode *image_node,
                                                 const pxr::UsdPrim &image_prim)
  {
    const Image *image = reinterpret_cast<Image *>(image_node->id);

    const pxr::UsdShadeShader image_shader(image_prim);
    const pxr::UsdShadeInput file_input = image_shader.GetInput(pxr::TfToken("file"));
    EXPECT_TRUE(bool(file_input));

    pxr::VtValue file_val;
    EXPECT_TRUE(file_input.Get(&file_val));
    EXPECT_TRUE(file_val.IsHolding<pxr::SdfAssetPath>());

    pxr::SdfAssetPath image_prim_asset = file_val.Get<pxr::SdfAssetPath>();

    /* The path is expected to be relative, but that means in Blender the
     * path will start with //.
     */
    EXPECT_EQ(
        BLI_path_cmp_normalized(image->filepath + 2, image_prim_asset.GetAssetPath().c_str()), 0);
  }

  /*
   * Determine if a Blender Mesh matches a UsdGeomMesh prim by checking counts
   * on vertices, faces, face indices, and normals.
   */
  void compare_blender_mesh_to_usd_prim(const Mesh *mesh, const pxr::UsdGeomMesh &mesh_prim)
  {
    pxr::VtIntArray face_indices;
    pxr::VtIntArray face_counts;
    pxr::VtVec3fArray positions;
    pxr::VtVec3fArray normals;

    /* Our export doesn't use `primvars:normals` so we're not
     * looking for that to be written here. */
    mesh_prim.GetFaceVertexIndicesAttr().Get(&face_indices, 0.0);
    mesh_prim.GetFaceVertexCountsAttr().Get(&face_counts, 0.0);
    mesh_prim.GetPointsAttr().Get(&positions, 0.0);
    mesh_prim.GetNormalsAttr().Get(&normals, 0.0);

    EXPECT_EQ(mesh->verts_num, positions.size());
    EXPECT_EQ(mesh->faces_num, face_counts.size());
    EXPECT_EQ(mesh->corners_num, face_indices.size());
    EXPECT_EQ(mesh->corners_num, normals.size());
  }
};

TEST_F(UsdExportTest, usd_export_rain_mesh)
{
  if (!load_file_and_depsgraph(simple_scene_filename)) {
    FAIL() << "Unable to load file: " << simple_scene_filename;
    return;
  }

  /* File sanity check. */
  EXPECT_EQ(BLI_listbase_count(&bfile->main->objects), 3);

  USDExportParams params;
  params.export_materials = false;
  params.export_normals = true;
  params.export_uvmaps = false;

  bool result = USD_export(context, output_filename.c_str(), &params, false, nullptr);
  ASSERT_TRUE(result) << "Writing to " << output_filename << " failed!";

  pxr::UsdStageRefPtr stage = pxr::UsdStage::Open(output_filename);
  ASSERT_TRUE(bool(stage)) << "Unable to load Stage from " << output_filename;

  /*
   * Run the mesh comparison for all Meshes in the original scene.
   */
  LISTBASE_FOREACH (Object *, object, &bfile->main->objects) {
    const Mesh *mesh = static_cast<Mesh *>(object->data);
    const StringRefNull object_name(object->id.name + 2);

    const pxr::SdfPath sdf_path("/" + pxr::TfMakeValidIdentifier(object_name.c_str()));
    pxr::UsdPrim prim = stage->GetPrimAtPath(sdf_path);
    EXPECT_TRUE(bool(prim));

    const pxr::UsdGeomMesh mesh_prim(get_first_child_mesh(prim));
    EXPECT_TRUE(bool(mesh_prim));

    compare_blender_mesh_to_usd_prim(mesh, mesh_prim);
  }
}

static const bNode *find_node_for_type_in_graph(const bNodeTree *nodetree,
                                                const blender::StringRefNull type_idname)
{
  auto found_nodes = nodetree->nodes_by_type(type_idname);
  if (found_nodes.size() == 1) {
    return found_nodes[0];
  }

  return nullptr;
}

/*
 * Export Material test-- export a scene with a material, then read it back
 * in and check that the BSDF and Image Texture nodes translated correctly
 * by comparing values between the exported USD stage and the objects in
 * memory.
 */
TEST_F(UsdExportTest, usd_export_material)
{
  if (!load_file_and_depsgraph(materials_filename)) {
    FAIL() << "Unable to load file: " << materials_filename;
    return;
  }

  /* File sanity checks. */
  EXPECT_EQ(BLI_listbase_count(&bfile->main->objects), 6);
  /* There is 1 additional material because of the "Dots Stroke". */
  EXPECT_EQ(BLI_listbase_count(&bfile->main->materials), 7);

  Material *material = reinterpret_cast<Material *>(
      BKE_libblock_find_name(bfile->main, ID_MA, "Material"));

  EXPECT_TRUE(bool(material));

  USDExportParams params;
  params.export_materials = true;
  params.export_normals = true;
  params.export_textures = false;
  params.export_uvmaps = true;
  params.generate_preview_surface = true;
  params.generate_materialx_network = false;
  params.convert_world_material = false;
  params.relative_paths = false;

  const bool result = USD_export(context, output_filename.c_str(), &params, false, nullptr);
  ASSERT_TRUE(result) << "Unable to export stage to " << output_filename;

  pxr::UsdStageRefPtr stage = pxr::UsdStage::Open(output_filename);
  ASSERT_NE(stage, nullptr) << "Unable to open exported stage: " << output_filename;

  material->nodetree->ensure_topology_cache();
  const bNode *bsdf_node = find_node_for_type_in_graph(material->nodetree,
                                                       "ShaderNodeBsdfPrincipled");

  const std::string prim_name = pxr::TfMakeValidIdentifier(bsdf_node->name);
  const pxr::UsdPrim bsdf_prim = stage->GetPrimAtPath(
      pxr::SdfPath("/_materials/Material/" + prim_name));

  compare_blender_node_to_usd_prim(bsdf_node, bsdf_prim);

  const bNode *image_node = find_node_for_type_in_graph(material->nodetree, "ShaderNodeTexImage");
  ASSERT_NE(image_node, nullptr);
  ASSERT_NE(image_node->storage, nullptr);

  const std::string image_prim_name = pxr::TfMakeValidIdentifier(image_node->name);

  const pxr::UsdPrim image_prim = stage->GetPrimAtPath(
      pxr::SdfPath("/_materials/Material/" + image_prim_name));

  ASSERT_TRUE(bool(image_prim)) << "Unable to find Material prim from exported stage "
                                << output_filename;

  compare_blender_image_to_usd_image_shader(image_node, image_prim);
}

TEST(utilities, make_safe_name)
{
  /* ASCII variations. */
  ASSERT_EQ(make_safe_name("", false), std::string("_"));
  ASSERT_EQ(make_safe_name("|", false), std::string("_"));
  ASSERT_EQ(make_safe_name("1", false), std::string("_1"));
  ASSERT_EQ(make_safe_name("1Test", false), std::string("_1Test"));

  ASSERT_EQ(make_safe_name("Test", false), std::string("Test"));
  ASSERT_EQ(make_safe_name("Test|$bézier @ world", false), std::string("Test__b__zier___world"));
  ASSERT_EQ(make_safe_name("Test|ハローワールド", false),
            std::string("Test______________________"));
  ASSERT_EQ(make_safe_name("Test|Γεια σου κόσμε", false),
            std::string("Test___________________________"));
  ASSERT_EQ(make_safe_name("Test|∧hello ○ wórld", false), std::string("Test____hello_____w__rld"));

  /* Unicode variations. */
  ASSERT_EQ(make_safe_name("", true), std::string("_"));
  ASSERT_EQ(make_safe_name("|", true), std::string("_"));
  ASSERT_EQ(make_safe_name("1", true), std::string("_1"));
  ASSERT_EQ(make_safe_name("1Test", true), std::string("_1Test"));

  ASSERT_EQ(make_safe_name("Test", true), std::string("Test"));
  ASSERT_EQ(make_safe_name("Test|$bézier @ world", true), std::string("Test__bézier___world"));
  ASSERT_EQ(make_safe_name("Test|ハローワールド", true), std::string("Test_ハローワールド"));
  ASSERT_EQ(make_safe_name("Test|Γεια σου κόσμε", true), std::string("Test_Γεια_σου_κόσμε"));
  ASSERT_EQ(make_safe_name("Test|∧hello ○ wórld", true), std::string("Test__hello___wórld"));
}

}  // namespace blender::io::usd
