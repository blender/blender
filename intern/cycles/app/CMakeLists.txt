# SPDX-FileCopyrightText: 2011-2022 Blender Foundation
#
# SPDX-License-Identifier: Apache-2.0

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

if(WITH_CYCLES_OSL)
  list(APPEND LIB cycles_kernel_osl)
endif()

if(CYCLES_STANDALONE_REPOSITORY)
  list(APPEND LIB extern_sky)
else()
  list(APPEND LIB bf_intern_sky)
endif()

if(WITH_CYCLES_STANDALONE AND WITH_CYCLES_STANDALONE_GUI)
  list(APPEND INC_SYS
    ${Epoxy_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
  )
  list(APPEND LIB ${Epoxy_LIBRARIES} ${SDL2_LIBRARIES})
endif()

if(WITH_USD)
  # Silence warning from USD headers using deprecated TBB header.
  add_definitions(
    -D__TBB_show_deprecation_message_atomic_H
    -D__TBB_show_deprecation_message_task_H
  )

  list(APPEND INC_SYS
    ${USD_INCLUDE_DIRS}
  )
  list(APPEND LIB
    cycles_hydra
    ${USD_LIBRARIES}
  )
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
      # Frameworks used by SDL.
      string(CONCAT _cycles_sdl_frameworks
        " -framework AudioToolbox"
        " -framework AudioUnit"
        " -framework Cocoa"
        " -framework CoreAudio"
        " -framework CoreHaptics"
        " -framework CoreVideo"
        " -framework ForceFeedback"
        " -framework GameController"
      )
      set_property(
        TARGET cycles
        APPEND PROPERTY LINK_FLAGS
        "${_cycles_sdl_frameworks}"
      )
    endif()
  endif()

  if(CYCLES_STANDALONE_REPOSITORY)
    cycles_install_libraries(cycles)

    if(WITH_USD AND USD_LIBRARY_DIR)
      install(DIRECTORY
        ${USD_LIBRARY_DIR}/usd
        DESTINATION ${CMAKE_INSTALL_PREFIX}
      )

      install(DIRECTORY
        ${USD_LIBRARY_DIR}/../plugin/usd
        DESTINATION ${CMAKE_INSTALL_PREFIX}
      )
    endif()
  endif()

  install(
    TARGETS cycles
    DESTINATION ${CMAKE_INSTALL_PREFIX})

  add_test(
    NAME cycles_version
    COMMAND ${CMAKE_INSTALL_PREFIX}/$<TARGET_FILE_NAME:cycles> --version)
endif()

if(WITH_CYCLES_PRECOMPUTE)
  set(SRC
    cycles_precompute.cpp
  )

  add_executable(cycles_precompute ${SRC} ${INC} ${INC_SYS})
  unset(SRC)

  target_link_libraries(cycles_precompute PRIVATE ${LIB})

  install(
    TARGETS cycles_precompute
    DESTINATION ${CMAKE_INSTALL_PREFIX})
endif()
