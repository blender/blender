/* SPDX-FileCopyrightText: 2011-2024 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "util/ies.h"

CCL_NAMESPACE_BEGIN

TEST(util_ies, invalid)
{
  IESFile ies_file;

  EXPECT_FALSE(ies_file.load("Hello, World!"));
}

CCL_NAMESPACE_END
