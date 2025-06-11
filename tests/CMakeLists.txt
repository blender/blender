# SPDX-FileCopyrightText: 2014-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Always run tests from install path, so all required scripts and libraries
# are available and we are testing the actual installation layout.
#
# Getting the install path of the executable is somewhat involved, as there are
# no direct CMake generator expressions to get the install paths of executables.
set(TEST_INSTALL_DIR ${CMAKE_INSTALL_PREFIX_WITH_CONFIG})

# Path to Blender and Python executables for all platforms.
if(MSVC)
  set(TEST_BLENDER_EXE ${TEST_INSTALL_DIR}/blender.exe)
elseif(APPLE)
  set(TEST_BLENDER_EXE ${TEST_INSTALL_DIR}/Blender.app/Contents/MacOS/Blender)
else()
  if(WITH_INSTALL_PORTABLE)
    set(TEST_BLENDER_EXE ${TEST_INSTALL_DIR}/blender)
  else()
    set(TEST_BLENDER_EXE ${TEST_INSTALL_DIR}/bin/blender)
  endif()
endif()

# The installation directory's Python is the best one to use. However, it can only be there
# after the install step, # which means that Python will never be there on a fresh system.
# To suit different needs, the user can pass `-DTEST_PYTHON_EXE=/path/to/python` to CMake.
if(NOT TEST_PYTHON_EXE)
  set(TEST_PYTHON_EXE ${PYTHON_EXECUTABLE})
  if(FIRST_RUN)
    message(STATUS "Tests: Using Python executable: ${TEST_PYTHON_EXE}")
  endif()
elseif(NOT EXISTS ${TEST_PYTHON_EXE})
  message(FATAL_ERROR "Tests: TEST_PYTHON_EXE ${TEST_PYTHON_EXE} does not exist")
endif()


# Include these arguments before all others, they must not interfere with Python execution.
set(TEST_PYTHON_EXE_EXTRA_ARGS "")

# Check if this a Blender managed Python installation, if so, don't add `*.pyc` files.
if(DEFINED LIBDIR)
  path_is_prefix(LIBDIR TEST_PYTHON_EXE _is_prefix)
  if(_is_prefix)
    # Keep the Python in Blender's SVN LIBDIR pristine, to avoid conflicts on updating.
    set(TEST_PYTHON_EXE_EXTRA_ARGS "-B")
  endif()
  unset(_is_prefix)
endif()

# For testing with Valgrind
# set(TEST_BLENDER_EXE valgrind --track-origins=yes --error-limit=no ${TEST_BLENDER_EXE})

# Standard Blender arguments for running tests.
# Specify exit code so that if a Python script error happens, the test fails.
set(TEST_BLENDER_EXE_PARAMS
  --background --factory-startup --debug-memory --debug-exit-on-error --python-exit-code 1
)

# Python CTests
if(WITH_BLENDER AND WITH_PYTHON AND NOT WITH_PYTHON_MODULE)
  add_subdirectory(python)
endif()

# Blender as python module tests.
if(WITH_PYTHON_MODULE)
  add_subdirectory(blender_as_python_module)
endif()

# GTest
add_subdirectory(gtests)

if(WITH_COMPILER_CODE_COVERAGE)
  if(CMAKE_COMPILER_IS_GNUCC)
    set(COVERAGE_SCRIPT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/coverage/coverage.py)

    add_custom_target(coverage-report
      ${PYTHON_EXECUTABLE} ${COVERAGE_SCRIPT_PATH} report --build-directory ${CMAKE_BINARY_DIR} --no-browser
      USES_TERMINAL
    )

    add_custom_target(coverage-show
      ${PYTHON_EXECUTABLE} ${COVERAGE_SCRIPT_PATH} report --build-directory ${CMAKE_BINARY_DIR}
      USES_TERMINAL
    )

    add_custom_target(coverage-reset
      ${PYTHON_EXECUTABLE} ${COVERAGE_SCRIPT_PATH} reset --build-directory ${CMAKE_BINARY_DIR}
      USES_TERMINAL
    )
  elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
    add_custom_target(coverage-reset
      COMMAND ${CMAKE_COMMAND} -E
        remove_directory ${COMPILER_CODE_COVERAGE_DATA_DIR}
      USES_TERMINAL
    )
    add_custom_target(coverage-merge
      COMMENT "Merging code-coverage data."
      BYPRODUCTS "${COMPILER_CODE_COVERAGE_DATA_DIR}/blender.profdata"
      COMMAND "${LLVM_PROFDATA}"
        merge
        -sparse
        ${COMPILER_CODE_COVERAGE_DATA_DIR}/raw/*.profraw
        -o ${COMPILER_CODE_COVERAGE_DATA_DIR}/blender.profdata
    )
    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 18.1 OR
       (CMAKE_C_COMPILER_ID MATCHES "AppleClang" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 17.0)
      )
      # Available clang on 18.1+ only , Apple's compiler doesn't map 1:1 to clang
      # versions so on apple we check for version 17+ which maps to clang
      # version 19.1.4, given this is somewhat unintuitive there is a handy table on
      # https://en.wikipedia.org/wiki/Xcode#Xcode_15.0_-_(since_visionOS_support)_2
      set(_cov_extra_args -show-directory-coverage)
    endif()
    add_custom_target(coverage-report
      BYPRODUCTS "${COMPILER_CODE_COVERAGE_DATA_DIR}/report/index.html"
      SOURCES "${COMPILER_CODE_COVERAGE_DATA_DIR}/blender.profdata"
      COMMENT "Generating code-coverage report."
      COMMAND "${LLVM_COV}" show
        -format=html
        -show-instantiations=true
        -show-line-counts-or-regions
        ${_cov_extra_args}
        -instr-profile=${COMPILER_CODE_COVERAGE_DATA_DIR}/blender.profdata
        --output-dir=${COMPILER_CODE_COVERAGE_DATA_DIR}/report
        $<TARGET_FILE:blender>
      USES_TERMINAL
    )
    if(NOT WIN32)
      set(_open_cmd "open")
    endif()
    add_custom_target(coverage-show
      COMMENT "Starting browser for ${COMPILER_CODE_COVERAGE_DATA_DIR}/report/index.html"
      COMMAND ${_open_cmd} "${COMPILER_CODE_COVERAGE_DATA_DIR}/report/index.html"
      SOURCES "${COMPILER_CODE_COVERAGE_DATA_DIR}/report/index.html"
    )
    unset(_cov_extra_args)
    unset(_open_cmd)
  endif()
endif()
