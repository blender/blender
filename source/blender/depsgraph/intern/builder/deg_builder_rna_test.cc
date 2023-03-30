/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2020 Blender Foundation */

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
