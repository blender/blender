# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2020 Blender Foundation. All rights reserved.

# Build the test runner. This runner takes care of running all GTests, i.e.
# the code that was built using the blender_add_test_lib() CMake macro (see
# macros.cmake).
set(SRC
  blender_test.cc
)

if(WITH_BUILDINFO)
  list(APPEND SRC
    "$<TARGET_OBJECTS:buildinfoobj>"
  )
endif()


# Test libraries need to be linked "whole archive", because they're not
# directly referenced from other code.
get_property(_test_libs GLOBAL PROPERTY BLENDER_TEST_LIBS)
if(WIN32 OR APPLE)
  # Windows and macOS set target_link_options after target creation.
elseif(UNIX)
  list(APPEND TEST_LIBS "-Wl,--whole-archive" ${_test_libs} "-Wl,--no-whole-archive")
else()
  message(FATAL_ERROR "Unknown how to link whole-archive with your compiler ${CMAKE_CXX_COMPILER_ID}")
endif()

# This builds `bin/tests/blender_test`, but does not add it as a single test.
blender_src_gtest_ex(
  NAME blender
  SRC "${SRC}"
  EXTRA_LIBS "${TEST_LIBS}"
  SKIP_ADD_TEST
)
setup_platform_linker_libs(blender_test)

if(WIN32)
  foreach(_lib ${_test_libs})
    # Both target_link_libraries and target_link_options are required here
    # target_link_libraries will add any dependent libraries, while just setting
    # the wholearchive flag in target link options will not.
    target_link_libraries(blender_test ${_lib})
    target_link_options(blender_test PRIVATE /wholearchive:$<TARGET_FILE:${_lib}>)
  endforeach()
  set_target_properties(blender_test PROPERTIES VS_DEBUGGER_ENVIRONMENT "${PLATFORM_ENV_INSTALL};$<TARGET_FILE_DIR:blender>")
elseif(APPLE)
  # force_load for `_test_libs` ensures that all symbols definitely make it into the test binary.
  # But linking against them again using `target_link_libraries` creates duplicate symbol
  # errors when linker cannot deduplicate a force loaded and linked library.
  # So force load test libraries separately, and link against their non-"test libraries"
  # dependencies separately.

  # Gather dependencies of all test libraries.
  set(_test_libs_dependencies)
  foreach(_lib ${_test_libs})
    get_target_property(_interface_libs ${_lib} INTERFACE_LINK_LIBRARIES)
    if(_interface_libs)
      list(APPEND _test_libs_dependencies ${_interface_libs})
    endif()
    unset(_interface_libs)
  endforeach()

  # Force load test libraries. Ensure that they are not linked twice in case they
  # are used as dependencies of other test libraries.
  foreach(_lib ${_test_libs})
    list(REMOVE_ITEM _test_libs_dependencies ${_lib})
    add_dependencies(blender_test ${_lib})
    target_link_options(blender_test PRIVATE "LINKER:-force_load,$<TARGET_FILE:${_lib}>")
  endforeach()

  target_link_libraries(blender_test "${_test_libs_dependencies}")
  unset(_test_libs_dependencies)
endif()

unset(_test_libs)
