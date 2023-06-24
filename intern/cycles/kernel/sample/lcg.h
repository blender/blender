/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

CCL_NAMESPACE_BEGIN

/* Linear Congruential Generator */

/* This is templated to handle multiple address spaces on Metal. */
template<class T> ccl_device uint lcg_step_uint(T rng)
{
  /* implicit mod 2^32 */
  *rng = (1103515245 * (*rng) + 12345);
  return *rng;
}

/* This is templated to handle multiple address spaces on Metal. */
template<class T> ccl_device float lcg_step_float(T rng)
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
