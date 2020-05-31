
set(INC
  ..
  ../../glew-mx
  ../../guardedalloc
  ../../mikktspace
  ../../../source/blender/makesdna
  ../../../source/blender/makesrna
  ../../../source/blender/blenlib
  ${CMAKE_BINARY_DIR}/source/blender/makesrna/intern
)

set(INC_SYS
  ${PYTHON_INCLUDE_DIRS}
  ${GLEW_INCLUDE_DIR}
)

set(SRC
  blender_camera.cpp
  blender_device.cpp
  blender_image.cpp
  blender_geometry.cpp
  blender_light.cpp
  blender_mesh.cpp
  blender_object.cpp
  blender_object_cull.cpp
  blender_particles.cpp
  blender_curves.cpp
  blender_logging.cpp
  blender_python.cpp
  blender_session.cpp
  blender_shader.cpp
  blender_sync.cpp
  blender_texture.cpp
  blender_viewport.cpp
  blender_volume.cpp

  CCL_api.h
  blender_device.h
  blender_id_map.h
  blender_image.h
  blender_object_cull.h
  blender_sync.h
  blender_session.h
  blender_texture.h
  blender_util.h
  blender_viewport.h
)

set(LIB
  cycles_bvh
  cycles_device
  cycles_graph
  cycles_kernel
  cycles_render
  cycles_subd
  cycles_util

  ${PYTHON_LINKFLAGS}
  ${PYTHON_LIBRARIES}
)

if(WITH_CYCLES_LOGGING)
  list(APPEND LIB
    ${GLOG_LIBRARIES}
    ${GFLAGS_LIBRARIES}
  )
endif()

set(ADDON_FILES
  addon/__init__.py
  addon/engine.py
  addon/operators.py
  addon/osl.py
  addon/presets.py
  addon/properties.py
  addon/ui.py
  addon/version_update.py
)

add_definitions(${GL_DEFINITIONS})

if(WITH_CYCLES_DEVICE_OPENCL)
  add_definitions(-DWITH_OPENCL)
endif()

if(WITH_CYCLES_NETWORK)
  add_definitions(-DWITH_NETWORK)
endif()

if(WITH_MOD_FLUID)
  add_definitions(-DWITH_FLUID)
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

if(WITH_OPENIMAGEDENOISE)
  add_definitions(-DWITH_OPENIMAGEDENOISE)
  list(APPEND INC_SYS
    ${OPENIMAGEDENOISE_INCLUDE_DIRS}
  )
endif()

blender_add_lib(bf_intern_cycles "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# avoid link failure with clang 3.4 debug
if(CMAKE_C_COMPILER_ID MATCHES "Clang" AND NOT ${CMAKE_C_COMPILER_VERSION} VERSION_LESS '3.4')
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -gline-tables-only")
endif()

add_dependencies(bf_intern_cycles bf_rna)

delayed_install(${CMAKE_CURRENT_SOURCE_DIR} "${ADDON_FILES}" ${CYCLES_INSTALL_PATH})
