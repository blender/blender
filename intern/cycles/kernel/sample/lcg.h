/*
 * Copyright 2011-2013 Blender Foundation
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
#pragma once

CCL_NAMESPACE_BEGIN

/* Linear Congruential Generator */

ccl_device uint lcg_step_uint(uint *rng)
{
  /* implicit mod 2^32 */
  *rng = (1103515245 * (*rng) + 12345);
  return *rng;
}

ccl_device float lcg_step_float(uint *rng)
{
  /* implicit mod 2^32 */
  *rng = (1103515245 * (*rng) + 12345);
  return (float)*rng * (1.0f / (float)0xFFFFFFFF);
}

ccl_device uint lcg_init(uint seed)
{
  uint rng = seed;
  lcg_step_uint(&rng);
  return rng;
}

ccl_device_inline uint lcg_state_init(const uint rng_hash,
                                      const uint rng_offset,
                                      const uint sample,
                                      const uint scramble)
{
  return lcg_init(rng_hash + rng_offset + sample * scramble);
}

CCL_NAMESPACE_END
