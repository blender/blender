# SPDX-FileCopyrightText: 2021 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

function(add_blender_as_python_module_test testname testscript)
  if(NOT TEST_PYTHON_EXE)
    message(FATAL_ERROR "No Python configured for running tests, set TEST_PYTHON_EXE.")
  endif()

  add_test(
    NAME ${testname}
    COMMAND ${TEST_PYTHON_EXE} ${TEST_PYTHON_EXE_EXTRA_ARGS} ${CMAKE_CURRENT_LIST_DIR}/${testscript} ${ARGN}
  )

  # On macOS, asan library must be loaded early.
  if(APPLE AND WITH_COMPILER_ASAN)
    set_tests_properties(
      ${testname}
      PROPERTIES ENVIRONMENT DYLD_INSERT_LIBRARIES=${COMPILER_ASAN_LIBRARY}
    )
  endif()
endfunction()

add_blender_as_python_module_test(import_bpy import_bpy.py ${CMAKE_INSTALL_PREFIX_WITH_CONFIG})
add_blender_as_python_module_test(cleanup_bpy cleanup_bpy.py ${CMAKE_INSTALL_PREFIX_WITH_CONFIG})
