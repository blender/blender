# Copyright 2011-2021 Blender Foundation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set(INC
  ..
  ../../sky/include
)

set(SRC
  alembic.cpp
  alembic_read.cpp
  attribute.cpp
  background.cpp
  bake.cpp
  camera.cpp
  colorspace.cpp
  constant_fold.cpp
  film.cpp
  geometry.cpp
  hair.cpp
  image.cpp
  image_oiio.cpp
  image_sky.cpp
  image_vdb.cpp
  integrator.cpp
  jitter.cpp
  light.cpp
  mesh.cpp
  mesh_displace.cpp
  mesh_subdivision.cpp
  procedural.cpp
  pointcloud.cpp
  object.cpp
  osl.cpp
  particles.cpp
  pass.cpp
  curves.cpp
  scene.cpp
  shader.cpp
  shader_graph.cpp
  shader_nodes.cpp
  sobol.cpp
  stats.cpp
  svm.cpp
  tables.cpp
  volume.cpp
)

set(SRC_HEADERS
  alembic.h
  alembic_read.h
  attribute.h
  bake.h
  background.h
  camera.h
  colorspace.h
  constant_fold.h
  film.h
  geometry.h
  hair.h
  image.h
  image_oiio.h
  image_sky.h
  image_vdb.h
  integrator.h
  light.h
  jitter.h
  mesh.h
  object.h
  osl.h
  particles.h
  pass.h
  procedural.h
  pointcloud.h
  curves.h
  scene.h
  shader.h
  shader_graph.h
  shader_nodes.h
  sobol.h
  stats.h
  svm.h
  tables.h
  volume.h
)

set(LIB
  cycles_bvh
  cycles_device
  cycles_integrator
  cycles_subd
  cycles_util
)

if(CYCLES_STANDALONE_REPOSITORY)
  list(APPEND LIB extern_sky)
else()
  list(APPEND LIB bf_intern_sky)
endif()

if(WITH_CYCLES_OSL)
  list(APPEND LIB
    cycles_kernel_osl
  )

  SET_PROPERTY(SOURCE osl.cpp PROPERTY COMPILE_FLAGS ${RTTI_DISABLE_FLAGS})
endif()

if(WITH_OPENCOLORIO)
  add_definitions(-DWITH_OCIO)
  include_directories(
    SYSTEM
    ${OPENCOLORIO_INCLUDE_DIRS}
  )
  list(APPEND LIB
    ${OPENCOLORIO_LIBRARIES}
  )
  if(WIN32)
    add_definitions(-DOpenColorIO_SKIP_IMPORTS)
  endif()
endif()

if(WITH_OPENVDB)
  add_definitions(-DWITH_OPENVDB ${OPENVDB_DEFINITIONS})
  list(APPEND INC_SYS
    ${OPENVDB_INCLUDE_DIRS}
  )
  list(APPEND LIB
    ${OPENVDB_LIBRARIES}
  )
endif()

if(WITH_ALEMBIC)
  add_definitions(-DWITH_ALEMBIC)
  list(APPEND INC_SYS
    ${ALEMBIC_INCLUDE_DIRS}
  )
  list(APPEND LIB
    ${ALEMBIC_LIBRARIES}
  )
endif()

if(WITH_NANOVDB)
  list(APPEND INC_SYS
    ${NANOVDB_INCLUDE_DIRS}
  )
endif()

include_directories(${INC})
include_directories(SYSTEM ${INC_SYS})

add_definitions(${GL_DEFINITIONS})

cycles_add_library(cycles_scene "${LIB}" ${SRC} ${SRC_HEADERS})
