/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "util/md5.h"

CCL_NAMESPACE_BEGIN

TEST(util, util_md5_string)
{
  /* The hash is calculated using `echo -n "Hello, World\!" | md5 | tr '[:lower:]' '[:upper:]'`. */
  EXPECT_EQ(util_md5_string("Hello, World!"), "65A8E27D8879283831B664BD8B7F0AD4");
}

CCL_NAMESPACE_END
