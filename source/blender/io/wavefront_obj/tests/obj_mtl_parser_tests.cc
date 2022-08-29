/* SPDX-License-Identifier: Apache-2.0 */

#include <gtest/gtest.h>

#include "BKE_appdir.h"

#include "testing/testing.h"

#include "obj_import_file_reader.hh"

namespace blender::io::obj {

class obj_mtl_parser_test : public testing::Test {
 public:
  void check_string(const char *text, const MTLMaterial *expect, size_t expect_count)
  {
    BKE_tempdir_init(nullptr);
    std::string tmp_dir = BKE_tempdir_base();
    std::string tmp_file_name = "mtl_test.mtl";
    std::string tmp_file_path = tmp_dir + "/" + tmp_file_name;
    FILE *tmp_file = BLI_fopen(tmp_file_path.c_str(), "wb");
    fputs(text, tmp_file);
    fclose(tmp_file);

    check_impl(tmp_file_name, tmp_dir, expect, expect_count);

    BLI_delete(tmp_file_path.c_str(), false, false);
  }
  void check(const char *file, const MTLMaterial *expect, size_t expect_count)
  {
    std::string obj_dir = blender::tests::flags_test_asset_dir() + "/io_tests/obj/";
    check_impl(file, obj_dir, expect, expect_count);
  }
  void check_impl(StringRefNull mtl_file_path,
                  StringRefNull file_dir,
                  const MTLMaterial *expect,
                  size_t expect_count)
  {
    MTLParser parser(mtl_file_path, file_dir + "dummy.obj");
    Map<std::string, std::unique_ptr<MTLMaterial>> materials;
    parser.parse_and_store(materials);

    for (int i = 0; i < expect_count; ++i) {
      const MTLMaterial &exp = expect[i];
      if (!materials.contains(exp.name)) {
        fprintf(stderr, "Material '%s' was expected in parsed result\n", exp.name.c_str());
        ADD_FAILURE();
        continue;
      }
      const MTLMaterial &got = *materials.lookup(exp.name);
      const float tol = 0.0001f;
      EXPECT_V3_NEAR(exp.Ka, got.Ka, tol);
      EXPECT_V3_NEAR(exp.Kd, got.Kd, tol);
      EXPECT_V3_NEAR(exp.Ks, got.Ks, tol);
      EXPECT_V3_NEAR(exp.Ke, got.Ke, tol);
      EXPECT_NEAR(exp.Ns, got.Ns, tol);
      EXPECT_NEAR(exp.Ni, got.Ni, tol);
      EXPECT_NEAR(exp.d, got.d, tol);
      EXPECT_NEAR(exp.map_Bump_strength, got.map_Bump_strength, tol);
      EXPECT_EQ(exp.illum, got.illum);
      for (int key = 0; key < (int)MTLTexMapType::Count; key++) {
        const MTLTexMap &exp_tex = exp.texture_maps[key];
        const MTLTexMap &got_tex = got.texture_maps[key];
        EXPECT_STREQ(exp_tex.image_path.c_str(), got_tex.image_path.c_str());
        EXPECT_V3_NEAR(exp_tex.translation, got_tex.translation, tol);
        EXPECT_V3_NEAR(exp_tex.scale, got_tex.scale, tol);
        EXPECT_EQ(exp_tex.projection_type, got_tex.projection_type);
      }
    }
    EXPECT_EQ(materials.size(), expect_count);
  }
};

TEST_F(obj_mtl_parser_test, string_newlines_whitespace)
{
  const char *text =
      "# a comment\n"
      "  # indented comment\n"
      "# comment with CRLF line ending\r\n"
      "\r\n"

      "newmtl simple\n"
      "Ka 0.1 0.2 0.3\n"
      "illum 4\n"

      "newmtl\ttab_indentation\n"
      "Kd\t \t0.2   0.3\t0.4    \t  \n"

      "newmtl space_after_name \t \n"
      "Ks 0.4 0.5 0.6\n"

      "newmtl    space_before_name\n"

      "newmtl indented_values\n"
      "  Ka 0.5 0.6 0.7\n"
      "\t\t\tKd 0.6 0.7 0.8\n"

      "newmtl crlf_ending\r\n"
      "Ns 5.0\r\n"
      "map_Kd    sometex_d.png\r\n"
      "map_Ks sometex_s_spaces_after_name.png   \t   \r\n";
  MTLMaterial mat[6];
  mat[0].name = "simple";
  mat[0].Ka = {0.1f, 0.2f, 0.3f};
  mat[0].illum = 4;
  mat[1].name = "tab_indentation";
  mat[1].Kd = {0.2f, 0.3f, 0.4f};
  mat[2].name = "space_after_name";
  mat[2].Ks = {0.4f, 0.5f, 0.6f};
  mat[3].name = "space_before_name";
  mat[4].name = "indented_values";
  mat[4].Ka = {0.5f, 0.6f, 0.7f};
  mat[4].Kd = {0.6f, 0.7f, 0.8f};
  mat[5].name = "crlf_ending";
  mat[5].Ns = 5.0f;
  mat[5].tex_map_of_type(MTLTexMapType::Kd).image_path = "sometex_d.png";
  mat[5].tex_map_of_type(MTLTexMapType::Ks).image_path = "sometex_s_spaces_after_name.png";
  check_string(text, mat, ARRAY_SIZE(mat));
}

TEST_F(obj_mtl_parser_test, cube)
{
  MTLMaterial mat;
  mat.name = "red";
  mat.Ka = {0.2f, 0.2f, 0.2f};
  mat.Kd = {1, 0, 0};
  check("cube.mtl", &mat, 1);
}

TEST_F(obj_mtl_parser_test, all_objects)
{
  MTLMaterial mat[7];
  for (auto &m : mat) {
    m.Ka = {1, 1, 1};
    m.Ks = {0.5f, 0.5f, 0.5f};
    m.Ke = {0, 0, 0};
    m.Ns = 250;
    m.Ni = 1;
    m.d = 1;
    m.illum = 2;
  }
  mat[0].name = "Blue";
  mat[0].Kd = {0, 0, 1};
  mat[1].name = "BlueDark";
  mat[1].Kd = {0, 0, 0.5f};
  mat[2].name = "Green";
  mat[2].Kd = {0, 1, 0};
  mat[3].name = "GreenDark";
  mat[3].Kd = {0, 0.5f, 0};
  mat[4].name = "Material";
  mat[4].Kd = {0.8f, 0.8f, 0.8f};
  mat[5].name = "Red";
  mat[5].Kd = {1, 0, 0};
  mat[6].name = "RedDark";
  mat[6].Kd = {0.5f, 0, 0};
  check("all_objects.mtl", mat, ARRAY_SIZE(mat));
}

TEST_F(obj_mtl_parser_test, materials)
{
  MTLMaterial mat[6];
  mat[0].name = "no_textures_red";
  mat[0].Ka = {0.3f, 0.3f, 0.3f};
  mat[0].Kd = {0.8f, 0.3f, 0.1f};
  mat[0].Ns = 5.624998f;

  mat[1].name = "four_maps";
  mat[1].Ka = {1, 1, 1};
  mat[1].Kd = {0.8f, 0.8f, 0.8f};
  mat[1].Ks = {0.5f, 0.5f, 0.5f};
  mat[1].Ke = {0, 0, 0};
  mat[1].Ns = 1000;
  mat[1].Ni = 1.45f;
  mat[1].d = 1;
  mat[1].illum = 2;
  mat[1].map_Bump_strength = 1;
  {
    MTLTexMap &kd = mat[1].tex_map_of_type(MTLTexMapType::Kd);
    kd.image_path = "texture.png";
    MTLTexMap &ns = mat[1].tex_map_of_type(MTLTexMapType::Ns);
    ns.image_path = "sometexture_Roughness.png";
    MTLTexMap &refl = mat[1].tex_map_of_type(MTLTexMapType::refl);
    refl.image_path = "sometexture_Metallic.png";
    MTLTexMap &bump = mat[1].tex_map_of_type(MTLTexMapType::bump);
    bump.image_path = "sometexture_Normal.png";
  }

  mat[2].name = "Clay";
  mat[2].Ka = {1, 1, 1};
  mat[2].Kd = {0.8f, 0.682657f, 0.536371f};
  mat[2].Ks = {0.5f, 0.5f, 0.5f};
  mat[2].Ke = {0, 0, 0};
  mat[2].Ns = 440.924042f;
  mat[2].Ni = 1.45f;
  mat[2].d = 1;
  mat[2].illum = 2;

  mat[3].name = "Hat";
  mat[3].Ka = {1, 1, 1};
  mat[3].Kd = {0.8f, 0.8f, 0.8f};
  mat[3].Ks = {0.5f, 0.5f, 0.5f};
  mat[3].Ns = 800;
  mat[3].map_Bump_strength = 0.5f;
  {
    MTLTexMap &kd = mat[3].tex_map_of_type(MTLTexMapType::Kd);
    kd.image_path = "someHatTexture_BaseColor.jpg";
    MTLTexMap &ns = mat[3].tex_map_of_type(MTLTexMapType::Ns);
    ns.image_path = "someHatTexture_Roughness.jpg";
    MTLTexMap &refl = mat[3].tex_map_of_type(MTLTexMapType::refl);
    refl.image_path = "someHatTexture_Metalness.jpg";
    MTLTexMap &bump = mat[3].tex_map_of_type(MTLTexMapType::bump);
    bump.image_path = "someHatTexture_Normal.jpg";
  }

  mat[4].name = "Parser_Test";
  mat[4].Ka = {0.1f, 0.2f, 0.3f};
  mat[4].Kd = {0.4f, 0.5f, 0.6f};
  mat[4].Ks = {0.7f, 0.8f, 0.9f};
  mat[4].illum = 6;
  mat[4].Ns = 15.5;
  mat[4].Ni = 1.5;
  mat[4].d = 0.5;
  mat[4].map_Bump_strength = 0.1f;
  {
    MTLTexMap &kd = mat[4].tex_map_of_type(MTLTexMapType::Kd);
    kd.image_path = "sometex_d.png";
    MTLTexMap &ns = mat[4].tex_map_of_type(MTLTexMapType::Ns);
    ns.image_path = "sometex_ns.psd";
    MTLTexMap &refl = mat[4].tex_map_of_type(MTLTexMapType::refl);
    refl.image_path = "clouds.tiff";
    refl.scale = {1.5f, 2.5f, 3.5f};
    refl.translation = {4.5f, 5.5f, 6.5f};
    refl.projection_type = SHD_PROJ_SPHERE;
    MTLTexMap &bump = mat[4].tex_map_of_type(MTLTexMapType::bump);
    bump.image_path = "somebump.tga";
    bump.scale = {3, 4, 5};
  }

  mat[5].name = "Parser_ScaleOffset_Test";
  {
    MTLTexMap &kd = mat[5].tex_map_of_type(MTLTexMapType::Kd);
    kd.translation = {2.5f, 0.0f, 0.0f};
    kd.image_path = "OffsetOneValue.png";
    MTLTexMap &ks = mat[5].tex_map_of_type(MTLTexMapType::Ks);
    ks.scale = {1.5f, 2.5f, 1.0f};
    ks.translation = {3.5f, 4.5f, 0.0f};
    ks.image_path = "ScaleOffsetBothTwovalues.png";
    MTLTexMap &ns = mat[5].tex_map_of_type(MTLTexMapType::Ns);
    ns.scale = {0.5f, 1.0f, 1.0f};
    ns.image_path = "1.Value.png";
  }

  check("materials.mtl", mat, ARRAY_SIZE(mat));
}

}  // namespace blender::io::obj
