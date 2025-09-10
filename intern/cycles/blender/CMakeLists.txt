# SPDX-FileCopyrightText: 2011-2022 Blender Foundation
#
# SPDX-License-Identifier: Apache-2.0

set(INC
  ..
  ../../../source/blender/makesrna
  ${CMAKE_BINARY_DIR}/source/blender/makesrna/intern
)

set(INC_SYS
  ${Epoxy_INCLUDE_DIRS}
  ${PYTHON_INCLUDE_DIRS}
)

set(SRC
  camera.cpp
  device.cpp
  display_driver.cpp
  image.cpp
  geometry.cpp
  light.cpp
  light_linking.cpp
  mesh.cpp
  object.cpp
  object_cull.cpp
  output_driver.cpp
  particles.cpp
  pointcloud.cpp
  curves.cpp
  logging.cpp
  python.cpp
  session.cpp
  shader.cpp
  sync.cpp
  viewport.cpp
  volume.cpp

  attribute_convert.h
  CCL_api.h
  device.h
  display_driver.h
  id_map.h
  image.h
  light_linking.h
  object_cull.h
  output_driver.h
  sync.h
  session.h
  util.h
  viewport.h
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  PRIVATE bf::imbuf
  PRIVATE bf::gpu
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::intern::clog
  PRIVATE bf::nodes
  PRIVATE bf::render
  cycles_bvh
  cycles_device
  cycles_graph
  cycles_kernel
  cycles_scene
  cycles_session
  cycles_subd
  cycles_util

  ${Epoxy_LIBRARIES}
  ${PYTHON_LINKFLAGS}
  ${PYTHON_LIBRARIES}
)

set(ADDON_FILES
  addon/__init__.py
  addon/camera.py
  addon/engine.py
  addon/operators.py
  addon/osl.py
  addon/presets.py
  addon/properties.py
  addon/ui.py
  addon/version_update.py
)

if(WITH_CYCLES_DEVICE_HIP)
  add_definitions(-DWITH_HIP)
endif()

if(WITH_CYCLES_DEVICE_METAL)
  add_definitions(-DWITH_METAL)
endif()

if(WITH_MOD_FLUID)
  add_definitions(-DWITH_FLUID)
endif()

if(WITH_TBB)
  add_definitions(-DWITH_TBB)
endif()

if(WITH_OPENVDB)
  list(APPEND INC
    ../../openvdb
  )
  list(APPEND LIB
    ${OPENVDB_LIBRARIES}
  )
endif()

if(WITH_OPENIMAGEDENOISE)
  add_definitions(-DWITH_OPENIMAGEDENOISE)
  list(APPEND INC_SYS
    ${OPENIMAGEDENOISE_INCLUDE_DIRS}
  )
endif()

if(WITH_CYCLES_OSL)
  list(APPEND LIB
    ${OSL_LIBRARIES}
  )
endif()

blender_add_lib(bf_intern_cycles "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

add_dependencies(bf_intern_cycles bf_rna)

delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${ADDON_FILES}" ${CYCLES_INSTALL_PATH})
