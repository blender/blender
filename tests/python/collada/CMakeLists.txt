# SPDX-FileCopyrightText: 2018-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# --env-system-scripts allows to run without the install target.

# Use '--write-blend=/tmp/test.blend' to view output

# Some tests are interesting but take too long to run
# and don't give deterministic results
set(USE_EXPERIMENTAL_TESTS FALSE)

set(TEST_SRC_DIR ${CMAKE_SOURCE_DIR}/../lib/tests)
set(TEST_OUT_DIR ${CMAKE_BINARY_DIR}/tests)

# ugh, any better way to do this on testing only?
execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory ${TEST_OUT_DIR})

# if(NOT IS_DIRECTORY ${TEST_SRC_DIR})
#   message(FATAL_ERROR "CMake test directory not found!")
# endif()

# all calls to blender use this
if(APPLE)
  if(${CMAKE_GENERATOR} MATCHES "Xcode")
    set(TEST_BLENDER_EXE_PARAMS --background --factory-startup --debug-memory --debug-exit-on-error)
  else()
    set(TEST_BLENDER_EXE_PARAMS --background --factory-startup --debug-memory --debug-exit-on-error --env-system-scripts ${CMAKE_SOURCE_DIR}/scripts)
  endif()
else()
  set(TEST_BLENDER_EXE_PARAMS --background --factory-startup --debug-memory --debug-exit-on-error --env-system-scripts ${CMAKE_SOURCE_DIR}/scripts)
endif()

# for testing with valgrind prefix: valgrind --track-origins=yes --error-limit=no
# set(TEST_BLENDER_EXE_BARE ${TEST_BLENDER_EXE})
# set(TEST_BLENDER_EXE ${TEST_BLENDER_EXE} ${TEST_BLENDER_EXE_PARAMS} )

# ------------------------------------------------------------------------------
# GENERAL PYTHON CORRECTNESS TESTS
macro(COLLADA_TEST module test_name blend_file)
  add_test(
    NAME bf_io_collada_${module}_${test_name}
    COMMAND "$<TARGET_FILE:blender>" ${TEST_BLENDER_EXE_PARAMS} ${TEST_SRC_DIR}/collada/${module}/${blend_file}
    --python ${CMAKE_CURRENT_LIST_DIR}/${module}/test_${module}_${test_name}.py --
    --testdir ${TEST_SRC_DIR}/collada/${module}
  )
endmacro()

# Tests are disabled because they only work on Windows
# Tests will be redone completely to work reliable
#
# collada_test(mesh simple mesh_simple.blend)
# collada_test(animation simple suzannes_parent_inverse.blend)
