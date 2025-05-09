/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "view_specific_look.hh"

#include "testing/testing.h"

namespace blender::ocio {

TEST(ocio_view_look, split_view_specific_look)
{
  {
    StringRef view, ui_name;
    EXPECT_FALSE(split_view_specific_look("", view, ui_name));
    EXPECT_EQ(view, "");
    EXPECT_EQ(ui_name, "");
  }

  {
    StringRef view, ui_name;
    EXPECT_FALSE(split_view_specific_look("Very Low Contrast", view, ui_name));
    EXPECT_EQ(view, "");
    EXPECT_EQ(ui_name, "Very Low Contrast");
  }

  {
    StringRef view, ui_name;
    EXPECT_TRUE(split_view_specific_look("AgX - Punchy", view, ui_name));
    EXPECT_EQ(view, "AgX");
    EXPECT_EQ(ui_name, "Punchy");
  }

  {
    StringRef view, ui_name;
    EXPECT_TRUE(split_view_specific_look("AgX - Punchy - New", view, ui_name));
    EXPECT_EQ(view, "AgX");
    EXPECT_EQ(ui_name, "Punchy - New");
  }
}

}  // namespace blender::ocio
