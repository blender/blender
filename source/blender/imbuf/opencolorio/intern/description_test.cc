/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "description.hh"

#include "testing/testing.h"

namespace blender::ocio {

TEST(ocio_description, cleanup_description)
{
  EXPECT_EQ(cleanup_description(""), "");
  EXPECT_EQ(cleanup_description("\n\rfoo\r\n"), "foo");
  EXPECT_EQ(cleanup_description("\n\rfoo\r\nbar\r\n"), "foo  bar");
}

}  // namespace blender::ocio
