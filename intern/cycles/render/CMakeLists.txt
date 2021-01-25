# Copyright 2011-2020 Blender Foundation
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
  ../../glew-mx
  ../../sky/include
)

set(INC_SYS
  ${GLEW_INCLUDE_DIR}
)

set(SRC
  alembic.cpp
  attribute.cpp
  background.cpp
  bake.cpp
  buffers.cpp
  camera.cpp
  colorspace.cpp
  constant_fold.cpp
  coverage.cpp
  denoising.cpp
  film.cpp
  geometry.cpp
  graph.cpp
  hair.cpp
  image.cpp
  image_oiio.cpp
  image_sky.cpp
  image_vdb.cpp
  integrator.cpp
  jitter.cpp
  light.cpp
  merge.cpp
  mesh.cpp
  mesh_displace.cpp
  mesh_subdivision.cpp
  nodes.cpp
  procedural.cpp
  object.cpp
  osl.cpp
  particles.cpp
  curves.cpp
  scene.cpp
  session.cpp
  shader.cpp
  sobol.cpp
  stats.cpp
  svm.cpp
  tables.cpp
  tile.cpp
  volume.cpp
)

set(SRC_HEADERS
  alembic.h
  attribute.h
  bake.h
  background.h
  buffers.h
  camera.h
  colorspace.h
  constant_fold.h
  coverage.h
  denoising.h
  film.h
  geometry.h
  graph.h
  hair.h
  image.h
  image_oiio.h
  image_sky.h
  image_vdb.h
  integrator.h
  light.h
  jitter.h
  merge.h
  mesh.h
  nodes.h
  object.h
  osl.h
  particles.h
  procedural.h
  curves.h
  scene.h
  session.h
  shader.h
  sobol.h
  stats.h
  svm.h
  tables.h
  tile.h
  volume.h
)

set(LIB
  cycles_bvh
  cycles_device
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
  if(WIN32)
    add_definitions(-DOpenColorIO_STATIC)
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

cycles_add_library(cycles_render "${LIB}" ${SRC} ${SRC_HEADERS})
