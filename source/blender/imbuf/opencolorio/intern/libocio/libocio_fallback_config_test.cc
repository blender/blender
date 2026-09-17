/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "testing/testing.h"

#include "CLG_log.h"

#include "OCIO_config.hh"

#include "libocio_config.hh"

namespace blender::ocio {

class ocio_fallback_config : public testing::Test {
 public:
  static void SetUpTestSuite()
  {
    CLG_init();
  }

  static void TearDownTestSuite()
  {
    CLG_exit();
  }
};

TEST_F(ocio_fallback_config, loads_and_validates)
{
  std::unique_ptr<Config> config = Config::create_fallback();
  ASSERT_NE(config, nullptr);

  /* Strict OpenColorIO validation, which loading alone does not do. */
  EXPECT_NO_THROW(static_cast<LibOCIOConfig &>(*config).get_ocio_config()->validate());
}

}  // namespace blender::ocio
