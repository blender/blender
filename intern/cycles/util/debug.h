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

#ifndef __UTIL_DEBUG_H__
#define __UTIL_DEBUG_H__

#include <cassert>
#include <iostream>

#include "bvh/params.h"

CCL_NAMESPACE_BEGIN

/* Global storage for all sort of flags used to fine-tune behavior of particular
 * areas for the development purposes, without officially exposing settings to
 * the interface.
 */
class DebugFlags {
 public:
  /* Use static BVH in viewport, to match final render exactly. */
  bool viewport_static_bvh;

  bool running_inside_blender;

  /* Descriptor of CPU feature-set to be used. */
  struct CPU {
    CPU();

    /* Reset flags to their defaults. */
    void reset();

    /* Flags describing which instructions sets are allowed for use. */
    bool avx2;
    bool avx;
    bool sse41;
    bool sse3;
    bool sse2;

    /* Check functions to see whether instructions up to the given one
     * are allowed for use.
     */
    bool has_avx2()
    {
      return has_avx() && avx2;
    }
    bool has_avx()
    {
      return has_sse41() && avx;
    }
    bool has_sse41()
    {
      return has_sse3() && sse41;
    }
    bool has_sse3()
    {
      return has_sse2() && sse3;
    }
    bool has_sse2()
    {
      return sse2;
    }

    /* Requested BVH layout.
     *
     * By default the fastest will be used. For debugging the BVH used by other
     * CPUs and GPUs can be selected here instead.
     */
    BVHLayout bvh_layout;
  };

  /* Descriptor of CUDA feature-set to be used. */
  struct CUDA {
    CUDA();

    /* Reset flags to their defaults. */
    void reset();

    /* Whether adaptive feature based runtime compile is enabled or not.
     * Requires the CUDA Toolkit and only works on Linux at the moment. */
    bool adaptive_compile;
  };

  /* Descriptor of HIP feature-set to be used. */
  struct HIP {
    HIP();

    /* Reset flags to their defaults. */
    void reset();

    /* Whether adaptive feature based runtime compile is enabled or not.*/
    bool adaptive_compile;
  };

  /* Descriptor of OptiX feature-set to be used. */
  struct OptiX {
    OptiX();

    /* Reset flags to their defaults. */
    void reset();

    /* Load OptiX module with debug capabilities. Will lower logging verbosity level, enable
     * validations, and lower optimization level. */
    bool use_debug;
  };

  /* Get instance of debug flags registry. */
  static DebugFlags &get()
  {
    static DebugFlags instance;
    return instance;
  }

  /* Reset flags to their defaults. */
  void reset();

  /* Requested CPU flags. */
  CPU cpu;

  /* Requested CUDA flags. */
  CUDA cuda;

  /* Requested OptiX flags. */
  OptiX optix;

  /* Requested HIP flags. */
  HIP hip;

 private:
  DebugFlags();

#if (__cplusplus > 199711L)
 public:
  explicit DebugFlags(DebugFlags const & /*other*/) = delete;
  void operator=(DebugFlags const & /*other*/) = delete;
#else
 private:
  explicit DebugFlags(DebugFlags const & /*other*/);
  void operator=(DebugFlags const & /*other*/);
#endif
};

typedef DebugFlags &DebugFlagsRef;
typedef const DebugFlags &DebugFlagsConstRef;

inline DebugFlags &DebugFlags()
{
  return DebugFlags::get();
}

CCL_NAMESPACE_END

#endif /* __UTIL_DEBUG_H__ */
