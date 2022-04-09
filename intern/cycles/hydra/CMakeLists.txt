# SPDX-License-Identifier: Apache-2.0
# Copyright 2022 Blender Foundation

#####################################################################
# Cycles Hydra render delegate
#####################################################################

set(INC
  ..
)
set(INC_SYS
  ${USD_INCLUDE_DIRS}
  ${GLEW_INCLUDE_DIR}
)

set(LIB
  cycles_scene
  cycles_session
  cycles_graph
  ${CYCLES_GLEW_LIBRARIES}
)
cycles_external_libraries_append(LIB)

set(INC_HD_CYCLES
  attribute.h
  camera.h
  config.h
  curves.h
  display_driver.h
  field.h
  geometry.h
  geometry.inl
  instancer.h
  light.h
  material.h
  mesh.h
  node_util.h
  output_driver.h
  pointcloud.h
  render_buffer.h
  render_delegate.h
  render_pass.h
  session.h
  volume.h
)

set(SRC_HD_CYCLES
  attribute.cpp
  curves.cpp
  camera.cpp
  display_driver.cpp
  field.cpp
  instancer.cpp
  light.cpp
  material.cpp
  mesh.cpp
  node_util.cpp
  output_driver.cpp
  pointcloud.cpp
  render_buffer.cpp
  render_delegate.cpp
  render_pass.cpp
  session.cpp
  volume.cpp
)

add_definitions(${GL_DEFINITIONS})

if(WITH_OPENVDB)
  add_definitions(-DWITH_OPENVDB ${OPENVDB_DEFINITIONS})
  list(APPEND INC_SYS
    ${OPENVDB_INCLUDE_DIRS}
  )
endif()

include_directories(${INC})
include_directories(SYSTEM ${INC_SYS})

add_library(hdCyclesStatic STATIC
  ${SRC_HD_CYCLES}
  ${INC_HD_CYCLES}
)

target_compile_options(hdCyclesStatic
  PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/wd4003 /wd4244 /wd4506>
  $<$<CXX_COMPILER_ID:GNU>:-Wno-float-conversion -Wno-double-promotion -Wno-deprecated>
)

target_compile_definitions(hdCyclesStatic
  PRIVATE
  GLOG_NO_ABBREVIATED_SEVERITIES=1
  OSL_DEBUG=$<CONFIG:DEBUG>
  TBB_USE_DEBUG=$<CONFIG:DEBUG>
  $<$<CXX_COMPILER_ID:MSVC>:NOMINMAX=1>
)

target_link_libraries(hdCyclesStatic
  PUBLIC
  hd
  hgi
  hgiGL
  PRIVATE
  ${LIB}
)

set(HdCyclesPluginName hdCycles)
add_library(${HdCyclesPluginName} SHARED
  plugin.h
  plugin.cpp
)

set_target_properties(${HdCyclesPluginName}
  PROPERTIES PREFIX ""
)

target_compile_definitions(${HdCyclesPluginName}
  PRIVATE
  MFB_PACKAGE_NAME=${HdCyclesPluginName}
  MFB_ALT_PACKAGE_NAME=${HdCyclesPluginName}
  GLOG_NO_ABBREVIATED_SEVERITIES=1
  OSL_DEBUG=$<CONFIG:DEBUG>
  TBB_USE_DEBUG=$<CONFIG:DEBUG>
  $<$<CXX_COMPILER_ID:MSVC>:NOMINMAX=1>
)

target_link_libraries(${HdCyclesPluginName}
  hdCyclesStatic
)

if(WITH_CYCLES_BLENDER)
  set(CYCLES_HYDRA_INSTALL_PATH "../")
else()
  set(CYCLES_HYDRA_INSTALL_PATH ${CMAKE_INSTALL_PREFIX})
  # Put the root plugInfo.json one level up
  delayed_install("${CMAKE_CURRENT_SOURCE_DIR}" "plugInfo.json" ${CMAKE_INSTALL_PREFIX})
endif()

delayed_install("" $<TARGET_FILE:${HdCyclesPluginName}> ${CYCLES_HYDRA_INSTALL_PATH})

set(PLUG_INFO_ROOT "..")

file(RELATIVE_PATH PLUG_INFO_LIBRARY_PATH "${CYCLES_HYDRA_INSTALL_PATH}/${HdCyclesPluginName}" "${CYCLES_HYDRA_INSTALL_PATH}/${HdCyclesPluginName}${CMAKE_SHARED_LIBRARY_SUFFIX}")
file(RELATIVE_PATH PLUG_INFO_RESOURCE_PATH "${CYCLES_HYDRA_INSTALL_PATH}/${HdCyclesPluginName}" "${CYCLES_INSTALL_PATH}")

configure_file(resources/plugInfo.json
  ${CMAKE_CURRENT_BINARY_DIR}/resources/plugInfo.json
  @ONLY
)

delayed_install("${CMAKE_CURRENT_BINARY_DIR}/resources" "plugInfo.json" "${CYCLES_HYDRA_INSTALL_PATH}/${HdCyclesPluginName}/resources")
