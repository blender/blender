/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_gtest_setup.hh"

#include "testing/testing.h"

namespace blender::bke {

class BlenderGTestBase : public testing::Test {
 public:
  static void SetUpTestSuite()
  {
    gtest_setup();
  }

  static void TearDownTestSuite()
  {
    gtest_teardown();
  }
};

}  // namespace blender::bke
