/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include "BLI_math_solvers.h"

TEST(math_solvers, Tridiagonal1)
{
  const float a[1] = {1};  // ignored
  const float b[1] = {2};
  const float c[1] = {1};  // ignored
  const float d[1] = {4};
  float x[1];

  EXPECT_TRUE(BLI_tridiagonal_solve(a, b, c, d, x, 1));
  EXPECT_FLOAT_EQ(x[0], 2);
}

TEST(math_solvers, Tridiagonal3)
{
  const float a[3] = {1, 2, 3};  // 1 ignored
  const float b[3] = {4, 5, 6};
  const float c[3] = {7, 8, 9};  // 9 ignored
  const float d[3] = {18, 36, 24};
  float x[3];

  EXPECT_TRUE(BLI_tridiagonal_solve(a, b, c, d, x, 3));
  EXPECT_FLOAT_EQ(x[0], 1);
  EXPECT_FLOAT_EQ(x[1], 2);
  EXPECT_FLOAT_EQ(x[2], 3);
}

TEST(math_solvers, CyclicTridiagonal1)
{
  const float a[1] = {1};
  const float b[1] = {2};
  const float c[1] = {1};
  const float d[1] = {4};
  float x[1];

  EXPECT_TRUE(BLI_tridiagonal_solve_cyclic(a, b, c, d, x, 1));
  EXPECT_FLOAT_EQ(x[0], 1);
}

TEST(math_solvers, CyclicTridiagonal2)
{
  const float a[2] = {1, 2};
  const float b[2] = {3, 4};
  const float c[2] = {5, 6};
  const float d[2] = {15, 16};
  float x[2];

  EXPECT_TRUE(BLI_tridiagonal_solve_cyclic(a, b, c, d, x, 2));
  EXPECT_FLOAT_EQ(x[0], 1);
  EXPECT_FLOAT_EQ(x[1], 2);
}

TEST(math_solvers, CyclicTridiagonal3)
{
  const float a[3] = {1, 2, 3};
  const float b[3] = {4, 5, 6};
  const float c[3] = {7, 8, 9};
  const float d[3] = {21, 36, 33};
  float x[3];

  EXPECT_TRUE(BLI_tridiagonal_solve_cyclic(a, b, c, d, x, 3));
  EXPECT_FLOAT_EQ(x[0], 1);
  EXPECT_FLOAT_EQ(x[1], 2);
  EXPECT_FLOAT_EQ(x[2], 3);
}
