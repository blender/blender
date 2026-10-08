/* SPDX-FileCopyrightText: 2026 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <gtest/gtest.h>

#include "util/colorspace.h"

#include <OpenColorIO/OpenColorIO.h>
namespace OCIO = OCIO_NAMESPACE;

CCL_NAMESPACE_BEGIN

/* Detect scene linear interop ID, using the builtin OpenColorIO default config with the
 * scene_linear role changed to the given colorspace. */
static std::string scene_linear_interop_id(const char *scene_linear, const bool srgb_encoded)
{
  const OCIO::ConstConfigRcPtr prev_config = OCIO::GetCurrentConfig();

  const OCIO::ConfigRcPtr config =
      OCIO::Config::CreateFromFile("ocio://default")->createEditableCopy();
  config->setRole("scene_linear", scene_linear);
  OCIO::SetCurrentConfig(config);

  const std::string interop_id = ColorSpaceManager::get_scene_linear_interop_id(srgb_encoded);

  OCIO::SetCurrentConfig(prev_config);
  return interop_id;
}

TEST(util_colorspace, scene_linear_interop_id)
{
  EXPECT_EQ(scene_linear_interop_id("Linear Rec.709 (sRGB)", false), "lin_rec709_scene");
  EXPECT_EQ(scene_linear_interop_id("Linear Rec.709 (sRGB)", true), "srgb_rec709_scene");

  EXPECT_EQ(scene_linear_interop_id("Linear Rec.2020", false), "lin_rec2020_scene");
  EXPECT_EQ(scene_linear_interop_id("Linear Rec.2020", true), "srgb_rec2020_scene");

  EXPECT_EQ(scene_linear_interop_id("ACEScg", false), "lin_ap1_scene");
  EXPECT_EQ(scene_linear_interop_id("ACEScg", true), "srgb_ap1_scene");

  EXPECT_EQ(scene_linear_interop_id("Linear P3-D65", false), "unknown");
}

CCL_NAMESPACE_END
