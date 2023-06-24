# SPDX-FileCopyrightText: 2022 Blender Foundation
#
# SPDX-License-Identifier: Apache-2.0

#####################################################################
# Cycles Hydra render delegate
#####################################################################

set(INC
  ..
)
set(INC_SYS
  ${USD_INCLUDE_DIRS}
  ${Epoxy_INCLUDE_DIRS}
  ${PYTHON_INCLUDE_DIRS}
)

set(LIB
  cycles_scene
  cycles_session
  cycles_graph
  ${Epoxy_LIBRARIES}
  ${BOOST_PYTHON_LIBRARIES}
)
cycles_external_libraries_append(LIB)

set(SRC_HD_CYCLES_HEADERS
  attribute.h
  camera.h
  config.h
  curves.h
  field.h
  file_reader.h
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
  field.cpp
  file_reader.cpp
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

if(WITH_OPENVDB)
  add_definitions(-DWITH_OPENVDB ${OPENVDB_DEFINITIONS})
  list(APPEND INC_SYS
    ${OPENVDB_INCLUDE_DIRS}
  )
endif()

# Blender libraries do not include hgiGL, so build without display driver then.
if(EXISTS ${USD_INCLUDE_DIR}/pxr/imaging/hgiGL)
  add_definitions(-DWITH_HYDRA_DISPLAY_DRIVER)
  list(APPEND SRC_HD_CYCLES display_driver.cpp)
  list(APPEND SRC_HD_CYCLES_HEADERS display_driver.h)
endif()

# Silence warning from USD headers using deprecated TBB header.
add_definitions(
  -D__TBB_show_deprecation_message_atomic_H
  -D__TBB_show_deprecation_message_task_H
)

include_directories(${INC})
include_directories(SYSTEM ${INC_SYS})

add_library(cycles_hydra STATIC
  ${SRC_HD_CYCLES}
  ${SRC_HD_CYCLES_HEADERS}
)

target_compile_options(cycles_hydra
  PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/wd4003 /wd4244 /wd4506>
  $<$<CXX_COMPILER_ID:GNU>:-Wno-float-conversion -Wno-double-promotion -Wno-deprecated>
)

target_compile_definitions(cycles_hydra
  PRIVATE
  GLOG_NO_ABBREVIATED_SEVERITIES=1
  OSL_DEBUG=$<CONFIG:DEBUG>
  TBB_USE_DEBUG=$<CONFIG:DEBUG>
  $<$<CXX_COMPILER_ID:MSVC>:NOMINMAX=1>
)

target_link_libraries(cycles_hydra
  PUBLIC
  ${USD_LIBRARIES}
  PRIVATE
  ${LIB}
)

if(WITH_CYCLES_HYDRA_RENDER_DELEGATE)
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
    cycles_hydra
  )

  if(APPLE)
    set_property(
      TARGET
      ${HdCyclesPluginName}
      APPEND_STRING PROPERTY LINK_FLAGS
      " -Wl,-exported_symbols_list,'${CMAKE_CURRENT_SOURCE_DIR}/resources/apple_symbols.map'"
    )
  elseif(UNIX)
    set_property(
      TARGET
      ${HdCyclesPluginName}
      APPEND_STRING PROPERTY LINK_FLAGS
      " -Wl,--version-script='${CMAKE_CURRENT_SOURCE_DIR}/resources/linux_symbols.map'"
    )
  endif()


  if(WITH_CYCLES_BLENDER)
    # Install inside add-on
    set(CYCLES_HYDRA_INSTALL_PATH ${CYCLES_INSTALL_PATH}/hydra)
  else()
    # Install next to cycles executable
    set(CYCLES_HYDRA_INSTALL_PATH ${CMAKE_INSTALL_PREFIX}/hydra)
  endif()

  # Put the root plugInfo.json one level up
  delayed_install("${CMAKE_CURRENT_SOURCE_DIR}" "plugInfo.json" ${CYCLES_HYDRA_INSTALL_PATH})
  delayed_install("" $<TARGET_FILE:${HdCyclesPluginName}> ${CYCLES_HYDRA_INSTALL_PATH})

  set_target_properties(${HdCyclesPluginName}
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

  set(PLUG_INFO_ROOT "..")

  if(WITH_CYCLES_BLENDER)
    # Full path not yet know at this point and RELATIVE_PATH requires
    # absolute path as input. So just set manually.
    set(PLUG_INFO_LIBRARY_PATH "../${HdCyclesPluginName}${CMAKE_SHARED_LIBRARY_SUFFIX}")
    set(PLUG_INFO_RESOURCE_PATH "../..")
  else()
    file(RELATIVE_PATH
      PLUG_INFO_LIBRARY_PATH
      "${CYCLES_HYDRA_INSTALL_PATH}/${HdCyclesPluginName}"
      "${CYCLES_HYDRA_INSTALL_PATH}/${HdCyclesPluginName}${CMAKE_SHARED_LIBRARY_SUFFIX}")
    file(RELATIVE_PATH PLUG_INFO_RESOURCE_PATH
      "${CYCLES_HYDRA_INSTALL_PATH}/${HdCyclesPluginName}"
      "${CYCLES_INSTALL_PATH}")
  endif()

  configure_file(resources/plugInfo.json
    ${CMAKE_CURRENT_BINARY_DIR}/resources/plugInfo.json
    @ONLY
  )

  delayed_install("${CMAKE_CURRENT_BINARY_DIR}/resources" "plugInfo.json" "${CYCLES_HYDRA_INSTALL_PATH}/${HdCyclesPluginName}/resources")
endif()
