/*
 * Copyright 2011-2016 Blender Foundation
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

#include "util/debug.h"

#include <stdlib.h>

#include "bvh/params.h"

#include "util/log.h"
#include "util/string.h"

CCL_NAMESPACE_BEGIN

DebugFlags::CPU::CPU()
    : avx2(true), avx(true), sse41(true), sse3(true), sse2(true), bvh_layout(BVH_LAYOUT_AUTO)
{
  reset();
}

void DebugFlags::CPU::reset()
{
#define STRINGIFY(x) #x
#define CHECK_CPU_FLAGS(flag, env) \
  do { \
    flag = (getenv(env) == NULL); \
    if (!flag) { \
      VLOG(1) << "Disabling " << STRINGIFY(flag) << " instruction set."; \
    } \
  } while (0)

  CHECK_CPU_FLAGS(avx2, "CYCLES_CPU_NO_AVX2");
  CHECK_CPU_FLAGS(avx, "CYCLES_CPU_NO_AVX");
  CHECK_CPU_FLAGS(sse41, "CYCLES_CPU_NO_SSE41");
  CHECK_CPU_FLAGS(sse3, "CYCLES_CPU_NO_SSE3");
  CHECK_CPU_FLAGS(sse2, "CYCLES_CPU_NO_SSE2");

#undef STRINGIFY
#undef CHECK_CPU_FLAGS

  bvh_layout = BVH_LAYOUT_AUTO;
}

DebugFlags::CUDA::CUDA() : adaptive_compile(false)
{
  reset();
}

DebugFlags::HIP::HIP() : adaptive_compile(false)
{
  reset();
}

void DebugFlags::CUDA::reset()
{
  if (getenv("CYCLES_CUDA_ADAPTIVE_COMPILE") != NULL)
    adaptive_compile = true;
}

void DebugFlags::HIP::reset()
{
  if (getenv("CYCLES_HIP_ADAPTIVE_COMPILE") != NULL)
    adaptive_compile = true;
}

DebugFlags::OptiX::OptiX()
{
  reset();
}

void DebugFlags::OptiX::reset()
{
  use_debug = false;
}

DebugFlags::DebugFlags() : viewport_static_bvh(false), running_inside_blender(false)
{
  /* Nothing for now. */
}

void DebugFlags::reset()
{
  viewport_static_bvh = false;
  cpu.reset();
  cuda.reset();
  optix.reset();
}

std::ostream &operator<<(std::ostream &os, DebugFlagsConstRef debug_flags)
{
  os << "CPU flags:\n"
     << "  AVX2       : " << string_from_bool(debug_flags.cpu.avx2) << "\n"
     << "  AVX        : " << string_from_bool(debug_flags.cpu.avx) << "\n"
     << "  SSE4.1     : " << string_from_bool(debug_flags.cpu.sse41) << "\n"
     << "  SSE3       : " << string_from_bool(debug_flags.cpu.sse3) << "\n"
     << "  SSE2       : " << string_from_bool(debug_flags.cpu.sse2) << "\n"
     << "  BVH layout : " << bvh_layout_name(debug_flags.cpu.bvh_layout) << "\n";

  os << "CUDA flags:\n"
     << "  Adaptive Compile : " << string_from_bool(debug_flags.cuda.adaptive_compile) << "\n";

  os << "OptiX flags:\n"
     << "  Debug : " << string_from_bool(debug_flags.optix.use_debug) << "\n";

  os << "HIP flags:\n"
     << "  HIP streams : " << string_from_bool(debug_flags.hip.adaptive_compile) << "\n";

  return os;
}

CCL_NAMESPACE_END
