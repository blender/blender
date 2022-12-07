#include "testing/testing.h"

namespace blender::io::ply {

// Use this as template for making your own tests

class GenericPlyTest : public testing::Test {
};

TEST_F(GenericPlyTest, TestingIfPlyTestingWorks)
{
  EXPECT_EQ(2, 2);
}

}  // namespace blender::io::ply
