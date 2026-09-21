/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <sstream>

#include "testing/testing.h"

#include "BLI_fileops.hh"
#include "BLI_index_range.hh"
#include "BLI_path_utils.hh"
#include "BLI_string.hh"
#include "BLI_vector.hh"

#include "DNA_color_types.h"
#include "DNA_image_types.h"

#include "BKE_appdir.hh"
#include "BKE_gtest_base.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"

#include "IMB_colormanagement.hh"
#include "IMB_imbuf.hh"

#include "OCIO_colorspace.hh"
#include "OCIO_config.hh"

namespace blender::imbuf::tests {

static std::string bundled_config_path()
{
  char configfile[FILE_MAX];
  BLI_path_join(configfile,
                sizeof(configfile),
                blender::tests::flags_test_release_dir().c_str(),
                "datafiles",
                "colormanagement",
                BCM_CONFIG_FILE);
  return configfile;
}

class ColorManagementConfigSwitchTest : public bke::BlenderGTestBase {
 public:
  static void SetUpTestSuite()
  {
    BLI_setenv("OCIO", bundled_config_path().c_str());
    bke::BlenderGTestBase::SetUpTestSuite();
    BKE_tempdir_init(nullptr);
  }

  static void TearDownTestSuite()
  {
    BKE_tempdir_session_purge();
    bke::BlenderGTestBase::TearDownTestSuite();
    BLI_setenv("OCIO", nullptr);
  }

 protected:
  void TearDown() override
  {
    IMB_colormanagement_switch_config(bundled_config_path().c_str());
  }
};

TEST_F(ColorManagementConfigSwitchTest, colorspace_pointers_survive_switch)
{
  const std::string config = bundled_config_path();

  struct TrackedBuffer {
    ImBuf *ibuf;
    const ColorSpace *byte_colorspace;
    const ColorSpace *float_colorspace;
    std::string byte_name;
    std::string float_name;
  };

  const char *byte_role = IMB_colormanagement_role_colorspace_name_get(COLOR_ROLE_DEFAULT_BYTE);

  Vector<TrackedBuffer> buffers;
  for (const int float_role : {COLOR_ROLE_SCENE_LINEAR, COLOR_ROLE_DATA}) {
    ImBuf *ibuf = IMB_allocImBuf(4, 4, ImBufFlags::ByteData | ImBufFlags::FloatData);
    IMB_colormanagement_assign_byte_colorspace(ibuf, byte_role);
    IMB_colormanagement_assign_float_colorspace(
        ibuf, IMB_colormanagement_role_colorspace_name_get(float_role));

    ASSERT_NE(ibuf->byte_buffer.colorspace, nullptr);
    ASSERT_NE(ibuf->float_buffer.colorspace, nullptr);
    buffers.append({ibuf,
                    ibuf->byte_buffer.colorspace,
                    ibuf->float_buffer.colorspace,
                    ibuf->byte_buffer.colorspace->name(),
                    ibuf->float_buffer.colorspace->name()});
  }

  /* Switch twice, the second time to verify repeated switching does not break anything either. */
  for (int i = 0; i < 2; i++) {
    SCOPED_TRACE("switch " + std::to_string(i));
    ASSERT_TRUE(IMB_colormanagement_switch_config(config.c_str()));

    for (const TrackedBuffer &tracked : buffers) {
      EXPECT_EQ(tracked.ibuf->byte_buffer.colorspace, tracked.byte_colorspace);
      EXPECT_EQ(tracked.ibuf->float_buffer.colorspace, tracked.float_colorspace);
      EXPECT_EQ(tracked.byte_colorspace->name(), tracked.byte_name);
      EXPECT_EQ(tracked.float_colorspace->name(), tracked.float_name);
      EXPECT_EQ(IMB_colormanagement_space_get_named(tracked.byte_name.c_str()),
                tracked.byte_colorspace);
      EXPECT_EQ(IMB_colormanagement_space_get_named(tracked.float_name.c_str()),
                tracked.float_colorspace);
    }
  }

  for (const TrackedBuffer &tracked : buffers) {
    IMB_freeImBuf(tracked.ibuf);
  }
}

static const char *extra_colorspace_name = "test_extra_space";

static std::string write_config_with_extra_colorspace(const std::string &config_path)
{
  /* Read config. */
  fstream config_stream(config_path, std::ios::in);
  if (!config_stream.is_open()) {
    return "";
  }

  std::stringstream config_buffer;
  config_buffer << config_stream.rdbuf();
  std::string config_text = config_buffer.str();

  /* Insert an extra colorspace at the start of the colorspaces: section.
   * Simple colorspace that multiples scene linear by 2. */
  const std::string marker = "\ncolorspaces:\n";
  const size_t marker_pos = config_text.find(marker);
  if (marker_pos == std::string::npos) {
    return "";
  }

  const std::string extra_colorspace =
      "  - !<ColorSpace>\n"
      "    name: " +
      std::string(extra_colorspace_name) +
      "\n"
      "    family: Test\n"
      "    bitdepth: 32f\n"
      "    isdata: false\n"
      "    from_scene_reference: !<MatrixTransform> {matrix: [2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 2, 0, "
      "0, 0, 0, 1]}\n";
  config_text.insert(marker_pos + marker.size(), extra_colorspace);

  /* Write modified config to temporary directory. */
  char extra_config_path[FILE_MAX];
  BLI_path_join(extra_config_path,
                sizeof(extra_config_path),
                BKE_tempdir_session(),
                "test_extra_config.ocio");

  fstream extra_config_stream(extra_config_path, std::ios::out | std::ios::trunc);
  if (!extra_config_stream.is_open()) {
    return "";
  }
  extra_config_stream << config_text;
  return extra_config_path;
}

TEST_F(ColorManagementConfigSwitchTest, removed_colorspace_is_retained)
{
  const std::string config = bundled_config_path();
  const std::string extra_config = write_config_with_extra_colorspace(config);

  /* Switch from bundled config to a new config with an extra colorspace, and:
   * - Create an imbuf with this color space
   * - Compute the result of converting to this color space */
  ASSERT_TRUE(IMB_colormanagement_switch_config(extra_config.c_str()));
  ASSERT_NE(IMB_colormanagement_space_get_named(extra_colorspace_name), nullptr);

  ImBuf *ibuf = IMB_allocImBuf(4, 4, ImBufFlags::FloatData);
  IMB_colormanagement_assign_float_colorspace(ibuf, extra_colorspace_name);
  const ColorSpace *colorspace = ibuf->float_buffer.colorspace;
  ASSERT_NE(colorspace, nullptr);

  float expected_pixel[3] = {0.5f, 0.25f, 0.125f};
  IMB_colormanagement_colorspace_to_scene_linear_v3(expected_pixel, colorspace);
  ASSERT_NE(expected_pixel[0], 0.5f);

  /* Switch back to the bundled config and verify conversion still has:
   * - A matching color space in the ImBuf
   * - Converting to this color space continues to work as before, even though it
   *   does not exist in the bundled config. */
  ASSERT_TRUE(IMB_colormanagement_switch_config(config.c_str()));

  EXPECT_EQ(ibuf->float_buffer.colorspace, colorspace);
  EXPECT_EQ(IMB_colormanagement_space_get_named(extra_colorspace_name), nullptr);

  float pixel[3] = {0.5f, 0.25f, 0.125f};
  IMB_colormanagement_colorspace_to_scene_linear_v3(pixel, colorspace);
  EXPECT_V3_NEAR(pixel, expected_pixel, 1e-6f);

  IMB_freeImBuf(ibuf);
}

TEST_F(ColorManagementConfigSwitchTest, active_inactive_color_spaces_by_index)
{
  /* Check inactive color spaces have an index after active color spaces. */
  const ColorManagedConfig &config = IMB_colormanagement_get_config();
  ASSERT_GT(config.get_num_all_color_spaces(), config.get_num_active_color_spaces());

  for (const int i : IndexRange(config.get_num_all_color_spaces())) {
    const ColorSpace *colorspace = config.get_color_space_by_index(i);
    ASSERT_NE(colorspace, nullptr);
    EXPECT_EQ(colorspace->index, i);
  }
  EXPECT_EQ(config.get_color_space_by_index(config.get_num_all_color_spaces()), nullptr);
}

class ColorManagementInteropIDTest : public ColorManagementConfigSwitchTest {
 protected:
  Main *bmain_ = nullptr;

  void SetUp() override
  {
    bmain_ = BKE_main_new();
  }

  void TearDown() override
  {
    BKE_main_free(bmain_);
    ColorManagementConfigSwitchTest::TearDown();
  }
};

TEST_F(ColorManagementInteropIDTest, stored_color_spaces_resolve_by_interop_id)
{
  Image *image = BKE_id_new<Image>(bmain_, "image");
  ColorManagedColorspaceSettings &settings = image->colorspace_settings;

  /* Resolve as if reading a file storing name and interop_id. */
  auto resolve = [&](const char *name, const char *interop_id) -> std::string {
    STRNCPY(settings.name, name);
    STRNCPY(settings.interop_id, interop_id);
    IMB_colormanagement_check_file_config(bmain_);
    return settings.name;
  };

  /* Set along with the name, except for roles. */
  IMB_colormanagement_colorspace_settings_set(&settings, "Non-Color");
  EXPECT_STREQ(settings.interop_id, "data");
  IMB_colormanagement_colorspace_settings_set(&settings, "scene_linear");
  EXPECT_STREQ(settings.interop_id, "");
  /* Color spaces with non-primary interop IDs don't store it. */
  IMB_colormanagement_colorspace_settings_set(&settings, "Filmic sRGB");
  EXPECT_STREQ(settings.interop_id, "");

  /* The interop ID takes precedence name. */
  EXPECT_EQ(resolve("sRGB", "lin_rec709_scene"), "Linear Rec.709");
  /* Unknown interop IDs fall back to the name. */
  EXPECT_EQ(resolve("sRGB", "unknown_scene"), "sRGB");
  EXPECT_EQ(resolve("Unknown", "unknown_scene"), "");

  /* Names of another config found by interop ID. */
  EXPECT_EQ(resolve("Raw", "data"), "Non-Color");
  EXPECT_STREQ(settings.interop_id, "data");
  EXPECT_EQ(resolve("Linear Rec.709 (sRGB)", "lin_rec709_scene"), "Linear Rec.709");

  /* Names set in the same config resolve to themselves. */
  for (const char *name : {"Filmic sRGB", "AgX Base sRGB", "sRGB", "Non-Color"}) {
    IMB_colormanagement_colorspace_settings_set(&settings, "Linear Rec.709");
    IMB_colormanagement_colorspace_settings_set(&settings, name);
    const std::string interop_id = settings.interop_id;
    EXPECT_EQ(resolve(name, interop_id.c_str()), name);
  }

  /* Test the other way around. */
  ASSERT_TRUE(IMB_colormanagement_switch_config("ocio://default"));
  EXPECT_EQ(resolve("Linear Rec.2020", "lin_rec2020_scene"), "Linear Rec.2020");
  EXPECT_EQ(resolve("ACEScg", "lin_ap1_scene"), "ACEScg");
  EXPECT_EQ(resolve("Some ACEScct", "acescct_ap1"), "ACEScct");
}

}  // namespace blender::imbuf::tests
