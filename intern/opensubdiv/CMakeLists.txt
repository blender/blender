# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# The Original Code is Copyright (C) 2013, Blender Foundation
# All rights reserved.
# ***** END GPL LICENSE BLOCK *****

set(INC
  .
  ../guardedalloc
)

set(INC_SYS
)

set(SRC
  opensubdiv_capi.h
  opensubdiv_capi_type.h
  opensubdiv_converter_capi.h
  opensubdiv_evaluator_capi.h
  opensubdiv_topology_refiner_capi.h
)

set(LIB
)

if(WITH_OPENSUBDIV)
  macro(OPENSUBDIV_DEFINE_COMPONENT component)
    if(${${component}})
      add_definitions(-D${component})
    endif()
  endmacro()

  list(APPEND INC_SYS
    ${OPENSUBDIV_INCLUDE_DIRS}
    ${GLEW_INCLUDE_PATH}
  )

  list(APPEND SRC
    # Base.
    internal/base/memory.h
    internal/base/opensubdiv_capi.cc
    internal/base/type.h
    internal/base/type_convert.cc
    internal/base/type_convert.h
    internal/base/util.cc
    internal/base/util.h

    # Device.
    internal/device/device_context_cuda.cc
    internal/device/device_context_cuda.h
    internal/device/device_context_glsl_compute.cc
    internal/device/device_context_glsl_compute.h
    internal/device/device_context_glsl_transform_feedback.cc
    internal/device/device_context_glsl_transform_feedback.h
    internal/device/device_context_opencl.cc
    internal/device/device_context_opencl.h
    internal/device/device_context_openmp.cc
    internal/device/device_context_openmp.h

    # Evaluator.
    internal/evaluator/eval_output.cc
    internal/evaluator/eval_output.h
    internal/evaluator/eval_output_cpu.cc
    internal/evaluator/eval_output_cpu.h
    internal/evaluator/eval_output_gpu.cc
    internal/evaluator/eval_output_gpu.h
    internal/evaluator/evaluator_cache_impl.cc
    internal/evaluator/evaluator_cache_impl.h
    internal/evaluator/evaluator_capi.cc
    internal/evaluator/evaluator_impl.cc
    internal/evaluator/evaluator_impl.h
    internal/evaluator/gl_compute_evaluator.cc
    internal/evaluator/gl_compute_evaluator.h
    internal/evaluator/patch_map.cc
    internal/evaluator/patch_map.h


    # Topology.
    internal/topology/mesh_topology.cc
    internal/topology/mesh_topology_compare.cc
    internal/topology/mesh_topology.h
    internal/topology/topology_refiner_capi.cc
    internal/topology/topology_refiner_factory.cc
    internal/topology/topology_refiner_impl.cc
    internal/topology/topology_refiner_impl_compare.cc
    internal/topology/topology_refiner_impl.h
  )

  list(APPEND LIB
    ${OPENSUBDIV_LIBRARIES}
  )

  if(WITH_OPENMP_STATIC)
    list(APPEND LIB
      ${OpenMP_LIBRARIES}
    )
  endif()

  OPENSUBDIV_DEFINE_COMPONENT(OPENSUBDIV_HAS_OPENMP)
  OPENSUBDIV_DEFINE_COMPONENT(OPENSUBDIV_HAS_OPENCL)
  OPENSUBDIV_DEFINE_COMPONENT(OPENSUBDIV_HAS_CUDA)
  OPENSUBDIV_DEFINE_COMPONENT(OPENSUBDIV_HAS_GLSL_TRANSFORM_FEEDBACK)
  OPENSUBDIV_DEFINE_COMPONENT(OPENSUBDIV_HAS_GLSL_COMPUTE)

  add_definitions(${GL_DEFINITIONS})
  add_definitions(-DOSD_USES_GLEW)

  if(WIN32)
    add_definitions(-DNOMINMAX)
    add_definitions(-D_USE_MATH_DEFINES)
  endif()

  data_to_c_simple(internal/evaluator/shaders/glsl_compute_kernel.glsl SRC)
else()
  list(APPEND SRC
    stub/opensubdiv_stub.cc
    stub/opensubdiv_evaluator_stub.cc
    stub/opensubdiv_topology_refiner_stub.cc
  )
endif()

blender_add_lib(bf_intern_opensubdiv "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# Tests.

if(WITH_GTESTS AND WITH_OPENSUBDIV)
  include(GTestTesting)

  add_definitions(${GFLAGS_DEFINES})
  add_definitions(${GLOG_DEFINES})
  add_definitions(-DBLENDER_GFLAGS_NAMESPACE=${GFLAGS_NAMESPACE})

  blender_add_test_executable(opensubdiv_mesh_topology_test "internal/topology/mesh_topology_test.cc" "${INC}" "${INC_SYS}" "${LIB};bf_intern_opensubdiv")
endif()
