# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2021 Blender Foundation. All rights reserved.

function(add_blender_as_python_module_test testname testscript)
  if(NOT TEST_PYTHON_EXE)
    message(FATAL_ERROR "No Python configured for running tests, set TEST_PYTHON_EXE.")
  endif()

  add_test(
    NAME ${testname}
    COMMAND ${TEST_PYTHON_EXE} ${testscript} ${ARGN}
  )
endfunction()

add_blender_as_python_module_test(import_bpy ${CMAKE_CURRENT_LIST_DIR}/import_bpy.py ${CMAKE_INSTALL_PREFIX_WITH_CONFIG})
