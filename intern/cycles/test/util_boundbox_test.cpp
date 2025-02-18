/* SPDX-FileCopyrightText: 2011-2025 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "util/boundbox.h"

#include "testing/testing.h"

#include "util/transform.h"

CCL_NAMESPACE_BEGIN

TEST(BoundBox, transformed)
{
  {
    const Transform tfm = transform_translate(make_float3(1, 2, 3));
    const BoundBox orig_bounds(make_float3(-2, -3, -4), make_float3(3, 4, 5));
    const BoundBox transformed_bounds = orig_bounds.transformed(&tfm);
    EXPECT_LE(len(transformed_bounds.min - make_float3(-1, -1, -1)), 1e-6f);
    EXPECT_LE(len(transformed_bounds.max - make_float3(4, 6, 8)), 1e-6f);
  }

  /* Non-valid boundbox should result in non-valid after transform. */
  {
    const Transform tfm = transform_scale(make_float3(1, 1, 1));
    EXPECT_FALSE(BoundBox(BoundBox::empty).transformed(&tfm).valid());
  }
}

CCL_NAMESPACE_END
