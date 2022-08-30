# SPDX-License-Identifier: Apache-2.0
# Copyright 2011-2022 Blender Foundation

#####################################################################
# Cycles standalone executable
#####################################################################

set(INC
  ..
)
set(INC_SYS
)

set(LIB
  cycles_device
  cycles_kernel
  cycles_scene
  cycles_session
  cycles_bvh
  cycles_subd
  cycles_graph
  cycles_util
)

if(WITH_ALEMBIC)
  add_definitions(-DWITH_ALEMBIC)
  list(APPEND INC_SYS
    ${ALEMBIC_INCLUDE_DIRS}
  )
  list(APPEND LIB
    ${ALEMBIC_LIBRARIES}
  )
endif()

if(WITH_CYCLES_OSL)
  list(APPEND LIB cycles_kernel_osl)
endif()

if(CYCLES_STANDALONE_REPOSITORY)
  list(APPEND LIB extern_sky)
else()
  list(APPEND LIB bf_intern_sky)
endif()

if(WITH_CYCLES_STANDALONE AND WITH_CYCLES_STANDALONE_GUI)
  list(APPEND INC_SYS ${Epoxy_INCLUDE_DIRS} ${SDL2_INCLUDE_DIRS})
  list(APPEND LIB ${Epoxy_LIBRARIES} ${SDL2_LIBRARIES})
endif()

cycles_external_libraries_append(LIB)

# Common configuration.

include_directories(${INC})
include_directories(SYSTEM ${INC_SYS})

# Application build targets

if(WITH_CYCLES_STANDALONE)
  set(SRC
    cycles_standalone.cpp
    cycles_xml.cpp
    cycles_xml.h
    oiio_output_driver.cpp
    oiio_output_driver.h
  )

  if(WITH_CYCLES_STANDALONE_GUI)
    list(APPEND SRC
      opengl/display_driver.cpp
      opengl/display_driver.h
      opengl/shader.cpp
      opengl/shader.h
      opengl/window.cpp
      opengl/window.h
    )
  endif()

  add_executable(cycles ${SRC} ${INC} ${INC_SYS})
  unset(SRC)

  target_link_libraries(cycles PRIVATE ${LIB})

  if(APPLE)
    if(WITH_CYCLES_STANDALONE_GUI)
      set_property(TARGET cycles APPEND_STRING PROPERTY LINK_FLAGS
        " -framework Cocoa -framework CoreAudio -framework AudioUnit -framework AudioToolbox -framework ForceFeedback -framework CoreVideo")
    endif()
  endif()

  if(UNIX AND NOT APPLE)
    set_target_properties(cycles PROPERTIES INSTALL_RPATH $ORIGIN/lib)
  endif()

  if(CYCLES_STANDALONE_REPOSITORY)
    cycles_install_libraries(cycles)
  endif()

  install(PROGRAMS
    $<TARGET_FILE:cycles>
    DESTINATION ${CMAKE_INSTALL_PREFIX})
endif()

#####################################################################
# Cycles cubin compiler executable
#####################################################################

if(WITH_CYCLES_CUBIN_COMPILER)
  # 32 bit windows is special, nvrtc is not supported on x86, so even
  # though we are building 32 bit blender a 64 bit cubin_cc will have
  # to be build to compile the cubins.
  if(MSVC AND NOT CMAKE_CL_64)
    message("Building with CUDA not supported on 32 bit, skipped")
    set(WITH_CYCLES_CUDA_BINARIES OFF CACHE BOOL "" FORCE)
  else()
    set(SRC
      cycles_cubin_cc.cpp
    )
    set(INC
      ../../../extern/cuew/include
    )
    set(LIB
    )
    cycles_external_libraries_append(LIB)
    add_executable(cycles_cubin_cc ${SRC})
    include_directories(${INC})
    target_link_libraries(cycles_cubin_cc PRIVATE ${LIB})
    unset(SRC)
    unset(INC)
  endif()
endif()
