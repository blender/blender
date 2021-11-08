/*
 * Copyright 2011-2020 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "testing/testing.h"

#include "util/transform.h"
#include "util/vector.h"

CCL_NAMESPACE_BEGIN

TEST(transform_motion_decompose, Degenerated)
{
  // Simple case: single degenerated matrix.
  {
    vector<Transform> motion = {transform_scale(0.0f, 0.0f, 0.0f)};
    vector<DecomposedTransform> decomp(motion.size());
    transform_motion_decompose(decomp.data(), motion.data(), motion.size());
    EXPECT_TRUE(transform_decomposed_isfinite_safe(&decomp[0]));
  }

  // Copy from previous to current.
  {
    vector<Transform> motion = {transform_rotate(M_PI_4_F, one_float3()),
                                transform_scale(0.0f, 0.0f, 0.0f)};
    vector<DecomposedTransform> decomp(motion.size());
    transform_motion_decompose(decomp.data(), motion.data(), motion.size());
    EXPECT_NEAR(len(decomp[1].x - decomp[0].x), 0.0f, 1e-6f);
  }

  // Copy from next to current.
  {
    vector<Transform> motion = {transform_scale(0.0f, 0.0f, 0.0f),
                                transform_rotate(M_PI_4_F, one_float3())};
    vector<DecomposedTransform> decomp(motion.size());
    transform_motion_decompose(decomp.data(), motion.data(), motion.size());
    EXPECT_NEAR(len(decomp[0].x - decomp[1].x), 0.0f, 1e-6f);
  }
}

CCL_NAMESPACE_END
