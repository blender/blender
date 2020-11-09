/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup depsgraph
 */

#include "intern/builder/deg_builder_rna.h"

#include "testing/testing.h"

namespace blender::deg::tests {

class TestableRNANodeQuery : public RNANodeQuery {
 public:
  static bool contains(const char *prop_identifier, const char *rna_path_component)
  {
    return RNANodeQuery::contains(prop_identifier, rna_path_component);
  }
};

TEST(deg_builder_rna, contains)
{
  EXPECT_TRUE(TestableRNANodeQuery::contains("location", "location"));
  EXPECT_TRUE(TestableRNANodeQuery::contains("location.x", "location"));
  EXPECT_TRUE(TestableRNANodeQuery::contains("pose.bone[\"blork\"].location", "location"));
  EXPECT_TRUE(TestableRNANodeQuery::contains("pose.bone[\"blork\"].location.x", "location"));
  EXPECT_TRUE(TestableRNANodeQuery::contains("pose.bone[\"blork\"].location[0]", "location"));

  EXPECT_FALSE(TestableRNANodeQuery::contains("", "location"));
  EXPECT_FALSE(TestableRNANodeQuery::contains("locatio", "location"));
  EXPECT_FALSE(TestableRNANodeQuery::contains("locationnn", "location"));
  EXPECT_FALSE(TestableRNANodeQuery::contains("test_location", "location"));
  EXPECT_FALSE(TestableRNANodeQuery::contains("location_test", "location"));
  EXPECT_FALSE(TestableRNANodeQuery::contains("test_location_test", "location"));
  EXPECT_FALSE(TestableRNANodeQuery::contains("pose.bone[\"location\"].scale", "location"));
  EXPECT_FALSE(TestableRNANodeQuery::contains("pose.bone[\"location\"].scale[0]", "location"));
}

}  // namespace blender::deg::tests
