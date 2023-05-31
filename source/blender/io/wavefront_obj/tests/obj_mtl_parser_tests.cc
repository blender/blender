/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <gtest/gtest.h>

#include "BKE_appdir.h"

#include "testing/testing.h"

#include "obj_export_mtl.hh"
#include "obj_import_file_reader.hh"

namespace blender::io::obj {

class obj_mtl_parser_test : public testing::Test {
 public:
  void check_string(const char *text, const MTLMaterial *expect, size_t expect_count)
  {
    BKE_tempdir_init(nullptr);
    std::string tmp_dir = BKE_tempdir_base();
    std::string tmp_file_name = "mtl_test.mtl";
    std::string tmp_file_path = tmp_dir + SEP_STR + tmp_file_name;
    FILE *tmp_file = BLI_fopen(tmp_file_path.c_str(), "wb");
    fputs(text, tmp_file);
    fclose(tmp_file);

    check_impl(tmp_file_name, tmp_dir, expect, expect_count);

    BLI_delete(tmp_file_path.c_str(), false, false);
  }
  void check(const char *file, const MTLMaterial *expect, size_t expect_count)
  {
    std::string obj_dir = blender::tests::flags_test_asset_dir() +
                          (SEP_STR "io_tests" SEP_STR "obj" SEP_STR);
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
      EXPECT_V3_NEAR(exp.ambient_color, got.ambient_color, tol);
      EXPECT_V3_NEAR(exp.color, got.color, tol);
      EXPECT_V3_NEAR(exp.spec_color, got.spec_color, tol);
      EXPECT_V3_NEAR(exp.emission_color, got.emission_color, tol);
      EXPECT_V3_NEAR(exp.transmit_color, got.transmit_color, tol);
      EXPECT_NEAR(exp.spec_exponent, got.spec_exponent, tol);
      EXPECT_NEAR(exp.ior, got.ior, tol);
      EXPECT_NEAR(exp.alpha, got.alpha, tol);
      EXPECT_NEAR(exp.normal_strength, got.normal_strength, tol);
      EXPECT_EQ(exp.illum_mode, got.illum_mode);
      EXPECT_NEAR(exp.roughness, got.roughness, tol);
      EXPECT_NEAR(exp.metallic, got.metallic, tol);
      EXPECT_NEAR(exp.sheen, got.sheen, tol);
      EXPECT_NEAR(exp.cc_thickness, got.cc_thickness, tol);
      EXPECT_NEAR(exp.cc_roughness, got.cc_roughness, tol);
      EXPECT_NEAR(exp.aniso, got.aniso, tol);
      EXPECT_NEAR(exp.aniso_rot, got.aniso_rot, tol);
      for (int key = 0; key < int(MTLTexMapType::Count); key++) {
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
  mat[0].ambient_color = {0.1f, 0.2f, 0.3f};
  mat[0].illum_mode = 4;
  mat[1].name = "tab_indentation";
  mat[1].color = {0.2f, 0.3f, 0.4f};
  mat[2].name = "space_after_name";
  mat[2].spec_color = {0.4f, 0.5f, 0.6f};
  mat[3].name = "space_before_name";
  mat[4].name = "indented_values";
  mat[4].ambient_color = {0.5f, 0.6f, 0.7f};
  mat[4].color = {0.6f, 0.7f, 0.8f};
  mat[5].name = "crlf_ending";
  mat[5].spec_exponent = 5.0f;
  mat[5].tex_map_of_type(MTLTexMapType::Color).image_path = "sometex_d.png";
  mat[5].tex_map_of_type(MTLTexMapType::Specular).image_path = "sometex_s_spaces_after_name.png";
  check_string(text, mat, ARRAY_SIZE(mat));
}

TEST_F(obj_mtl_parser_test, cube)
{
  MTLMaterial mat;
  mat.name = "red";
  mat.ambient_color = {0.2f, 0.2f, 0.2f};
  mat.color = {1, 0, 0};
  check("cube.mtl", &mat, 1);
}

TEST_F(obj_mtl_parser_test, all_objects)
{
  MTLMaterial mat[7];
  for (auto &m : mat) {
    m.ambient_color = {1, 1, 1};
    m.spec_color = {0.5f, 0.5f, 0.5f};
    m.emission_color = {0, 0, 0};
    m.spec_exponent = 250;
    m.ior = 1;
    m.alpha = 1;
    m.illum_mode = 2;
  }
  mat[0].name = "Blue";
  mat[0].color = {0, 0, 1};
  mat[1].name = "BlueDark";
  mat[1].color = {0, 0, 0.5f};
  mat[2].name = "Green";
  mat[2].color = {0, 1, 0};
  mat[3].name = "GreenDark";
  mat[3].color = {0, 0.5f, 0};
  mat[4].name = "Material";
  mat[4].color = {0.8f, 0.8f, 0.8f};
  mat[5].name = "Red";
  mat[5].color = {1, 0, 0};
  mat[6].name = "RedDark";
  mat[6].color = {0.5f, 0, 0};
  check("all_objects.mtl", mat, ARRAY_SIZE(mat));
}

TEST_F(obj_mtl_parser_test, materials)
{
  MTLMaterial mat[6];
  mat[0].name = "no_textures_red";
  mat[0].ambient_color = {0.3f, 0.3f, 0.3f};
  mat[0].color = {0.8f, 0.3f, 0.1f};
  mat[0].spec_exponent = 5.624998f;

  mat[1].name = "four_maps";
  mat[1].ambient_color = {1, 1, 1};
  mat[1].color = {0.8f, 0.8f, 0.8f};
  mat[1].spec_color = {0.5f, 0.5f, 0.5f};
  mat[1].emission_color = {0, 0, 0};
  mat[1].spec_exponent = 1000;
  mat[1].ior = 1.45f;
  mat[1].alpha = 1;
  mat[1].illum_mode = 2;
  mat[1].normal_strength = 1;
  {
    MTLTexMap &kd = mat[1].tex_map_of_type(MTLTexMapType::Color);
    kd.image_path = "texture.png";
    MTLTexMap &ns = mat[1].tex_map_of_type(MTLTexMapType::SpecularExponent);
    ns.image_path = "sometexture_Roughness.png";
    MTLTexMap &refl = mat[1].tex_map_of_type(MTLTexMapType::Reflection);
    refl.image_path = "sometexture_Metallic.png";
    MTLTexMap &bump = mat[1].tex_map_of_type(MTLTexMapType::Normal);
    bump.image_path = "sometexture_Normal.png";
  }

  mat[2].name = "Clay";
  mat[2].ambient_color = {1, 1, 1};
  mat[2].color = {0.8f, 0.682657f, 0.536371f};
  mat[2].spec_color = {0.5f, 0.5f, 0.5f};
  mat[2].emission_color = {0, 0, 0};
  mat[2].spec_exponent = 440.924042f;
  mat[2].ior = 1.45f;
  mat[2].alpha = 1;
  mat[2].illum_mode = 2;

  mat[3].name = "Hat";
  mat[3].ambient_color = {1, 1, 1};
  mat[3].color = {0.8f, 0.8f, 0.8f};
  mat[3].spec_color = {0.5f, 0.5f, 0.5f};
  mat[3].spec_exponent = 800;
  mat[3].normal_strength = 0.5f;
  {
    MTLTexMap &kd = mat[3].tex_map_of_type(MTLTexMapType::Color);
    kd.image_path = "someHatTexture_BaseColor.jpg";
    MTLTexMap &ns = mat[3].tex_map_of_type(MTLTexMapType::SpecularExponent);
    ns.image_path = "someHatTexture_Roughness.jpg";
    MTLTexMap &refl = mat[3].tex_map_of_type(MTLTexMapType::Reflection);
    refl.image_path = "someHatTexture_Metalness.jpg";
    MTLTexMap &bump = mat[3].tex_map_of_type(MTLTexMapType::Normal);
    bump.image_path = "someHatTexture_Normal.jpg";
  }

  mat[4].name = "Parser_Test";
  mat[4].ambient_color = {0.1f, 0.2f, 0.3f};
  mat[4].color = {0.4f, 0.5f, 0.6f};
  mat[4].spec_color = {0.7f, 0.8f, 0.9f};
  mat[4].illum_mode = 6;
  mat[4].spec_exponent = 15.5;
  mat[4].ior = 1.5;
  mat[4].alpha = 0.5;
  mat[4].normal_strength = 0.1f;
  mat[4].transmit_color = {0.1f, 0.3f, 0.5f};
  mat[4].normal_strength = 0.1f;
  mat[4].roughness = 0.2f;
  mat[4].metallic = 0.3f;
  mat[4].sheen = 0.4f;
  mat[4].cc_thickness = 0.5f;
  mat[4].cc_roughness = 0.6f;
  mat[4].aniso = 0.7f;
  mat[4].aniso_rot = 0.8f;
  {
    MTLTexMap &kd = mat[4].tex_map_of_type(MTLTexMapType::Color);
    kd.image_path = "sometex_d.png";
    MTLTexMap &ns = mat[4].tex_map_of_type(MTLTexMapType::SpecularExponent);
    ns.image_path = "sometex_ns.psd";
    MTLTexMap &refl = mat[4].tex_map_of_type(MTLTexMapType::Reflection);
    refl.image_path = "clouds.tiff";
    refl.scale = {1.5f, 2.5f, 3.5f};
    refl.translation = {4.5f, 5.5f, 6.5f};
    refl.projection_type = SHD_PROJ_SPHERE;
    MTLTexMap &bump = mat[4].tex_map_of_type(MTLTexMapType::Normal);
    bump.image_path = "somebump.tga";
    bump.scale = {3, 4, 5};
  }

  mat[5].name = "Parser_ScaleOffset_Test";
  {
    MTLTexMap &kd = mat[5].tex_map_of_type(MTLTexMapType::Color);
    kd.translation = {2.5f, 0.0f, 0.0f};
    kd.image_path = "OffsetOneValue.png";
    MTLTexMap &ks = mat[5].tex_map_of_type(MTLTexMapType::Specular);
    ks.scale = {1.5f, 2.5f, 1.0f};
    ks.translation = {3.5f, 4.5f, 0.0f};
    ks.image_path = "ScaleOffsetBothTwovalues.png";
    MTLTexMap &ns = mat[5].tex_map_of_type(MTLTexMapType::SpecularExponent);
    ns.scale = {0.5f, 1.0f, 1.0f};
    ns.image_path = "1.Value.png";
  }

  check("materials.mtl", mat, ARRAY_SIZE(mat));
}

TEST_F(obj_mtl_parser_test, materials_without_pbr)
{
  MTLMaterial mat[2];
  mat[0].name = "Mat1";
  mat[0].spec_exponent = 360.0f;
  mat[0].ambient_color = {0.9f, 0.9f, 0.9f};
  mat[0].color = {0.8f, 0.276449f, 0.101911f};
  mat[0].spec_color = {0.25f, 0.25f, 0.25f};
  mat[0].emission_color = {0, 0, 0};
  mat[0].ior = 1.45f;
  mat[0].alpha = 1;
  mat[0].illum_mode = 3;

  mat[1].name = "Mat2";
  mat[1].ambient_color = {1, 1, 1};
  mat[1].color = {0.8f, 0.8f, 0.8f};
  mat[1].spec_color = {0.5f, 0.5f, 0.5f};
  mat[1].ior = 1.45f;
  mat[1].alpha = 1;
  mat[1].illum_mode = 2;
  {
    MTLTexMap &ns = mat[1].tex_map_of_type(MTLTexMapType::SpecularExponent);
    ns.image_path = "../blend_geometry/texture_roughness.png";
    MTLTexMap &ke = mat[1].tex_map_of_type(MTLTexMapType::Emission);
    ke.image_path = "../blend_geometry/texture_illum.png";
  }

  check("materials_without_pbr.mtl", mat, ARRAY_SIZE(mat));
}

TEST_F(obj_mtl_parser_test, materials_pbr)
{
  MTLMaterial mat[2];
  mat[0].name = "Mat1";
  mat[0].color = {0.8f, 0.276449f, 0.101911f};
  mat[0].spec_color = {0.25f, 0.25f, 0.25f};
  mat[0].emission_color = {0, 0, 0};
  mat[0].ior = 1.45f;
  mat[0].alpha = 1;
  mat[0].illum_mode = 3;
  mat[0].roughness = 0.4f;
  mat[0].metallic = 0.9f;
  mat[0].sheen = 0.3f;
  mat[0].cc_thickness = 0.393182f;
  mat[0].cc_roughness = 0.05f;
  mat[0].aniso = 0.2f;
  mat[0].aniso_rot = 0.0f;

  mat[1].name = "Mat2";
  mat[1].color = {0.8f, 0.8f, 0.8f};
  mat[1].spec_color = {0.5f, 0.5f, 0.5f};
  mat[1].ior = 1.45f;
  mat[1].alpha = 1;
  mat[1].illum_mode = 2;
  mat[1].metallic = 0.0f;
  mat[1].cc_thickness = 0.3f;
  mat[1].cc_roughness = 0.4f;
  mat[1].aniso = 0.8f;
  mat[1].aniso_rot = 0.7f;
  {
    MTLTexMap &pr = mat[1].tex_map_of_type(MTLTexMapType::Roughness);
    pr.image_path = "../blend_geometry/texture_roughness.png";
    MTLTexMap &ps = mat[1].tex_map_of_type(MTLTexMapType::Sheen);
    ps.image_path = "../blend_geometry/texture_checker.png";
    MTLTexMap &ke = mat[1].tex_map_of_type(MTLTexMapType::Emission);
    ke.image_path = "../blend_geometry/texture_illum.png";
  }

  check("materials_pbr.mtl", mat, ARRAY_SIZE(mat));
}

}  // namespace blender::io::obj
