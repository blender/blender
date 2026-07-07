# SPDX-FileCopyrightText: 2011-2021 Blender Foundation
#
# SPDX-License-Identifier: Apache-2.0

set(INC
  ..
  ../../sky/include
  ../../mikktspace
)

set(SRC
  attribute.cpp
  background.cpp
  bake.cpp
  camera.cpp
  constant_fold.cpp
  devicescene.cpp
  film.cpp
  geometry.cpp
  geometry_attributes.cpp
  geometry_bvh.cpp
  geometry_mesh.cpp
  hair.cpp
  image.cpp
  image_oiio.cpp
  image_sky.cpp
  image_vdb.cpp
  integrator.cpp
  light.cpp
  light_tree.cpp
  light_tree_debug.cpp
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
  stats.cpp
  svm.cpp
  tables.cpp
  tabulated_sobol.cpp
  volume.cpp
)

set(SRC_HEADERS
  attribute.h
  bake.h
  background.h
  camera.h
  constant_fold.h
  devicescene.h
  film.h
  geometry.h
  hair.h
  image.h
  image_oiio.h
  image_sky.h
  image_vdb.h
  integrator.h
  light.h
  light_tree.h
  light_tree_debug.h
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
  stats.h
  svm.h
  tables.h
  tabulated_sobol.h
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
endif()

if(WITH_OPENVDB)
  list(APPEND LIB
    bf::dependencies::optional::openvdb
  )
endif()

if(WITH_NANOVDB)
  list(APPEND INC_SYS
    ${NANOVDB_INCLUDE_DIRS}
  )
endif()

include_directories(${INC})
include_directories(SYSTEM ${INC_SYS})

cycles_add_library(cycles_scene "${LIB}" ${SRC} ${SRC_HEADERS})
