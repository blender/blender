# SPDX-FileCopyrightText: 2011-2022 Blender Foundation
#
# SPDX-License-Identifier: Apache-2.0

set(INC
  ..
)

set(INC_SYS
)

set(SRC
  adaptive_sampling.cpp
  denoiser.cpp
  denoiser_gpu.cpp
  denoiser_oidn.cpp
  denoiser_oidn_base.cpp
  denoiser_oidn_gpu.cpp
  denoiser_optix.cpp
  path_trace.cpp
  tile.cpp
  pass_accessor.cpp
  pass_accessor_cpu.cpp
  pass_accessor_gpu.cpp
  path_trace_display.cpp
  path_trace_tile.cpp
  path_trace_work.cpp
  path_trace_work_cpu.cpp
  path_trace_work_gpu.cpp
  render_scheduler.cpp
  shader_eval.cpp
  work_balancer.cpp
  work_tile_scheduler.cpp
)

set(SRC_HEADERS
  adaptive_sampling.h
  denoiser.h
  denoiser_gpu.h
  denoiser_oidn.h
  denoiser_oidn_base.h
  denoiser_oidn_gpu.h
  denoiser_optix.h
  guiding.h
  path_trace.h
  tile.h
  pass_accessor.h
  pass_accessor_cpu.h
  pass_accessor_gpu.h
  path_trace_display.h
  path_trace_tile.h
  path_trace_work.h
  path_trace_work_cpu.h
  path_trace_work_gpu.h
  render_scheduler.h
  shader_eval.h
  work_balancer.h
  work_tile_scheduler.h
)

set(LIB
  PUBLIC cycles_device

  # NOTE: Is required for RenderBuffers access. Might consider moving files around a bit to
  # avoid such cyclic dependency.
  PUBLIC cycles_session

  PUBLIC cycles_util
  PRIVATE bf::dependencies::optional::openimagedenoise
  PRIVATE bf::dependencies::optional::openpgl
)

include_directories(${INC})
include_directories(SYSTEM ${INC_SYS})

cycles_add_library(cycles_integrator "${LIB}" ${SRC} ${SRC_HEADERS})
