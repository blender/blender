/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BLI_math_solvers.h"

TEST(math_solvers, Tridiagonal1)
{
  float a[1] = {1};  // ignored
  float b[1] = {2};
  float c[1] = {1};  // ignored
  float d[1] = {4};
  float x[1];

  EXPECT_TRUE(BLI_tridiagonal_solve(a, b, c, d, x, 1));
  EXPECT_FLOAT_EQ(x[0], 2);
}

TEST(math_solvers, Tridiagonal3)
{
  float a[3] = {1, 2, 3};  // 1 ignored
  float b[3] = {4, 5, 6};
  float c[3] = {7, 8, 9};  // 9 ignored
  float d[3] = {18, 36, 24};
  float x[3];

  EXPECT_TRUE(BLI_tridiagonal_solve(a, b, c, d, x, 3));
  EXPECT_FLOAT_EQ(x[0], 1);
  EXPECT_FLOAT_EQ(x[1], 2);
  EXPECT_FLOAT_EQ(x[2], 3);
}

TEST(math_solvers, CyclicTridiagonal1)
{
  float a[1] = {1};
  float b[1] = {2};
  float c[1] = {1};
  float d[1] = {4};
  float x[1];

  EXPECT_TRUE(BLI_tridiagonal_solve_cyclic(a, b, c, d, x, 1));
  EXPECT_FLOAT_EQ(x[0], 1);
}

TEST(math_solvers, CyclicTridiagonal2)
{
  float a[2] = {1, 2};
  float b[2] = {3, 4};
  float c[2] = {5, 6};
  float d[2] = {15, 16};
  float x[2];

  EXPECT_TRUE(BLI_tridiagonal_solve_cyclic(a, b, c, d, x, 2));
  EXPECT_FLOAT_EQ(x[0], 1);
  EXPECT_FLOAT_EQ(x[1], 2);
}

TEST(math_solvers, CyclicTridiagonal3)
{
  float a[3] = {1, 2, 3};
  float b[3] = {4, 5, 6};
  float c[3] = {7, 8, 9};
  float d[3] = {21, 36, 33};
  float x[3];

  EXPECT_TRUE(BLI_tridiagonal_solve_cyclic(a, b, c, d, x, 3));
  EXPECT_FLOAT_EQ(x[0], 1);
  EXPECT_FLOAT_EQ(x[1], 2);
  EXPECT_FLOAT_EQ(x[2], 3);
}
