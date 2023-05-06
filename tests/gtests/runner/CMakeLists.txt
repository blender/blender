# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2020 Blender Foundation

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
  foreach(_lib ${_test_libs})
    # We need -force_load for every test library and target_link_libraries will
    # deduplicate it. So explicitly set as linker option for every test lib.
    target_link_libraries(blender_test ${_lib} "-Wl,-force_load,$<TARGET_PROPERTY:${_lib},IMPORTED_LOCATION>")
  endforeach()
endif()

unset(_test_libs)
