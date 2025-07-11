/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "util/debug.h"

#include <cstdlib>

#include "util/log.h"

CCL_NAMESPACE_BEGIN

DebugFlags::CPU::CPU()
{
  reset();
}

void DebugFlags::CPU::reset()
{
#define STRINGIFY(x) #x
#define CHECK_CPU_FLAGS(flag, env) \
  do { \
    flag = (getenv(env) == nullptr); \
    if (!flag) { \
      LOG_INFO << "Disabling " << STRINGIFY(flag) << " instruction set."; \
    } \
  } while (0)

  CHECK_CPU_FLAGS(avx2, "CYCLES_CPU_NO_AVX2");

#undef STRINGIFY
#undef CHECK_CPU_FLAGS

  bvh_layout = BVH_LAYOUT_AUTO;
}

DebugFlags::CUDA::CUDA()
{
  reset();
}

DebugFlags::HIP::HIP()
{
  reset();
}

DebugFlags::Metal::Metal()
{
  reset();
}

void DebugFlags::CUDA::reset()
{
  if (getenv("CYCLES_CUDA_ADAPTIVE_COMPILE") != nullptr) {
    adaptive_compile = true;
  }
}

void DebugFlags::HIP::reset()
{
  if (getenv("CYCLES_HIP_ADAPTIVE_COMPILE") != nullptr) {
    adaptive_compile = true;
  }
}

void DebugFlags::Metal::reset()
{
  if (getenv("CYCLES_METAL_ADAPTIVE_COMPILE") != nullptr) {
    adaptive_compile = true;
  }

  if (const char *str = getenv("CYCLES_METAL_LOCAL_ATOMIC_SORT")) {
    use_local_atomic_sort = (atoi(str) != 0);
  }

  if (const char *str = getenv("CYCLES_METAL_NANOVDB")) {
    use_nanovdb = (atoi(str) != 0);
  }

  if (const char *str = getenv("CYCLES_METAL_ASYNC_PSO_CREATION")) {
    use_async_pso_creation = (atoi(str) != 0);
  }

  if (const char *str = getenv("CYCLES_METALRT_PCMI")) {
    use_metalrt_pcmi = (atoi(str) != 0);
  }
}

DebugFlags::OptiX::OptiX()
{
  reset();
}

void DebugFlags::OptiX::reset()
{
  use_debug = false;
}

void DebugFlags::reset()
{
  cpu.reset();
  cuda.reset();
  optix.reset();
  metal.reset();
}

CCL_NAMESPACE_END
