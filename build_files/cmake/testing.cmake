# SPDX-FileCopyrightText: 2006-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

function(get_blender_test_install_dir VARIABLE_NAME)
  get_property(GENERATOR_IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
  if(GENERATOR_IS_MULTI_CONFIG)
    string(REPLACE "\${BUILD_TYPE}" "$<CONFIG>" TEST_INSTALL_DIR ${CMAKE_INSTALL_PREFIX})
  else()
    string(REPLACE "\${BUILD_TYPE}" "" TEST_INSTALL_DIR ${CMAKE_INSTALL_PREFIX})
  endif()
  set(${VARIABLE_NAME} "${TEST_INSTALL_DIR}" PARENT_SCOPE)
endfunction()

macro(blender_src_gtest_ex)
  if(WITH_GTESTS)
    set(options SKIP_ADD_TEST)
    set(oneValueArgs NAME)
    set(multiValueArgs SRC EXTRA_LIBS COMMAND_ARGS)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN} )

    set(TARGET_NAME ${ARG_NAME}_test)
    get_property(_current_include_directories
                 DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                 PROPERTY INCLUDE_DIRECTORIES)
    set(TEST_INC
      ${_current_include_directories}
      ${CMAKE_SOURCE_DIR}/tests/gtests
    )
    set(TEST_INC_SYS
      ${GLOG_INCLUDE_DIRS}
      ${GFLAGS_INCLUDE_DIRS}
      ${CMAKE_SOURCE_DIR}/extern/gtest/include
      ${CMAKE_SOURCE_DIR}/extern/gmock/include
    )
    unset(_current_include_directories)
    if(WIN32)
      set(MANIFEST "${CMAKE_BINARY_DIR}/tests.exe.manifest")
    else()
      set(MANIFEST "")
    endif()

    add_executable(${TARGET_NAME} ${ARG_SRC} ${MANIFEST})
    setup_platform_linker_flags(${TARGET_NAME})
    target_compile_definitions(${TARGET_NAME} PRIVATE ${GFLAGS_DEFINES})
    target_compile_definitions(${TARGET_NAME} PRIVATE ${GLOG_DEFINES})
    target_include_directories(${TARGET_NAME} PUBLIC "${TEST_INC}")
    target_include_directories(${TARGET_NAME} SYSTEM PUBLIC "${TEST_INC_SYS}")
    blender_link_libraries(${TARGET_NAME} "${ARG_EXTRA_LIBS};${PLATFORM_LINKLIBS}")
    if(WITH_TBB)
      # Force TBB libraries to be in front of MKL (part of OpenImageDenoise), so
      # that it is initialized before MKL and static library initialization order
      # issues are avoided.
      target_link_libraries(${TARGET_NAME} PRIVATE ${TBB_LIBRARIES})
      if(WITH_OPENIMAGEDENOISE)
        target_link_libraries(${TARGET_NAME} PRIVATE ${OPENIMAGEDENOISE_LIBRARIES})
      endif()
    endif()
    target_link_libraries(${TARGET_NAME} PRIVATE
                          bf_testing_main
                          bf_intern_eigen
                          bf_intern_guardedalloc
                          extern_gtest
                          extern_gmock
                          # Needed for GLOG.
                          ${GLOG_LIBRARIES}
                          ${GFLAGS_LIBRARIES})

    if(DEFINED PTHREADS_LIBRARIES) # Needed for GLOG.
      target_link_libraries(${TARGET_NAME} PRIVATE ${PTHREADS_LIBRARIES})
    endif()
    if(WITH_OPENMP AND WITH_OPENMP_STATIC)
      target_link_libraries(${TARGET_NAME} PRIVATE ${OpenMP_LIBRARIES})
    endif()
    if(UNIX AND NOT APPLE)
      target_link_libraries(${TARGET_NAME} PRIVATE bf_intern_libc_compat)
    endif()
    if(WITH_TBB)
      target_link_libraries(${TARGET_NAME} PRIVATE ${TBB_LIBRARIES})
    endif()
    if(WITH_GMP)
      target_link_libraries(${TARGET_NAME} PRIVATE ${GMP_LIBRARIES})
    endif()

    get_blender_test_install_dir(TEST_INSTALL_DIR)
    set_target_properties(${TARGET_NAME} PROPERTIES
                          RUNTIME_OUTPUT_DIRECTORY         "${TESTS_OUTPUT_DIR}"
                          RUNTIME_OUTPUT_DIRECTORY_RELEASE "${TESTS_OUTPUT_DIR}"
                          RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${TESTS_OUTPUT_DIR}")
    if(NOT ARG_SKIP_ADD_TEST)
      add_test(
        NAME ${TARGET_NAME}
        COMMAND ${TESTS_OUTPUT_DIR}/${TARGET_NAME} ${ARG_COMMAND_ARGS}
        WORKING_DIRECTORY ${TEST_INSTALL_DIR})

      # Don't fail tests on leaks since these often happen in external libraries
      # that we can't fix.
      set_tests_properties(${TARGET_NAME} PROPERTIES
        ENVIRONMENT LSAN_OPTIONS=exitcode=0:$ENV{LSAN_OPTIONS}
      )
      if(WIN32)
        set_tests_properties(${TARGET_NAME} PROPERTIES ENVIRONMENT "PATH=${CMAKE_INSTALL_PREFIX_WITH_CONFIG}/blender.shared/;$ENV{PATH}")
      endif()
    endif()
    if(WIN32)
      set_target_properties(${TARGET_NAME} PROPERTIES VS_GLOBAL_VcpkgEnabled "false")
    endif()
    unset(MANIFEST)
    unset(TEST_INC)
    unset(TEST_INC_SYS)
    unset(TARGET_NAME)
  endif()
endmacro()

function(blender_add_test_suite)
  if(ARGC LESS 1)
    message(FATAL_ERROR "No arguments supplied to blender_add_test_suite()")
  endif()

  # Parse the arguments
  set(oneValueArgs TARGET SUITE_NAME)
  set(multiValueArgs SOURCES)
  cmake_parse_arguments(ARGS "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  # Figure out the release dir, as some tests need files from there.
  get_blender_test_install_dir(TEST_INSTALL_DIR)
  if(APPLE)
    set(_test_release_dir ${TEST_INSTALL_DIR}/Blender.app/Contents/Resources/${BLENDER_VERSION})
  else()
    if(WIN32 OR WITH_INSTALL_PORTABLE)
      set(_test_release_dir ${TEST_INSTALL_DIR}/${BLENDER_VERSION})
    else()
      set(_test_release_dir ${TEST_INSTALL_DIR}/share/blender/${BLENDER_VERSION})
    endif()
  endif()

  # Define a test case with our custom gtest_add_tests() command.
  include(GTest)
  gtest_add_tests(
    TARGET ${ARGS_TARGET}
    SOURCES "${ARGS_SOURCES}"
    TEST_PREFIX ${ARGS_SUITE_NAME}
    WORKING_DIRECTORY "${TEST_INSTALL_DIR}"
    EXTRA_ARGS
      --test-assets-dir "${CMAKE_SOURCE_DIR}/../lib/tests"
      --test-release-dir "${_test_release_dir}"
  )
  if(WIN32)
    set_tests_properties(
      ${ARGS_SUITE_NAME} PROPERTIES
      ENVIRONMENT "PATH=${CMAKE_INSTALL_PREFIX_WITH_CONFIG}/blender.shared/;$ENV{PATH}"
    )
  endif()
  unset(_test_release_dir)
endfunction()

# Add tests for a Blender library, to be called in tandem with blender_add_lib().
# The tests will be part of the blender_test executable (see tests/gtests/runner).
function(blender_add_test_lib
  name
  sources
  includes
  includes_sys
  library_deps
  )

  add_cc_flags_custom_test(${name} PARENT_SCOPE)

  # Otherwise external projects will produce warnings that we cannot fix.
  remove_strict_flags()

  # This duplicates logic that's also in blender_src_gtest_ex.
  # TODO(Sybren): deduplicate after the general approach in D7649 has been approved.
  list(APPEND includes
    ${CMAKE_SOURCE_DIR}/tests/gtests
  )
  list(APPEND includes_sys
    ${GLOG_INCLUDE_DIRS}
    ${GFLAGS_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/extern/gtest/include
    ${CMAKE_SOURCE_DIR}/extern/gmock/include
  )

  blender_add_lib__impl(${name} "${sources}" "${includes}" "${includes_sys}" "${library_deps}")

  target_compile_definitions(${name} PRIVATE ${GFLAGS_DEFINES})
  target_compile_definitions(${name} PRIVATE ${GLOG_DEFINES})

  set_property(GLOBAL APPEND PROPERTY BLENDER_TEST_LIBS ${name})

  blender_add_test_suite(
    TARGET blender_test
    SUITE_NAME ${name}
    SOURCES "${sources}"
  )
endfunction()


# Add tests for a Blender library, to be called in tandem with blender_add_lib().
# Test will be compiled into a ${name}_test executable.
#
# To be used for smaller isolated libraries, that do not have many dependencies.
# For libraries that do drag in many other Blender libraries and would create a
# very large executable, blender_add_test_lib() should be used instead.
function(blender_add_test_executable_impl
  name
  add_test_suite
  sources
  includes
  includes_sys
  library_deps
  )

  add_cc_flags_custom_test(${name} PARENT_SCOPE)

  ## Otherwise external projects will produce warnings that we cannot fix.
  remove_strict_flags()

  blender_src_gtest_ex(
    NAME ${name}
    SRC "${sources}"
    EXTRA_LIBS "${library_deps}"
    SKIP_ADD_TEST
  )
  if(add_test_suite)
    blender_add_test_suite(
      TARGET ${name}_test
      SUITE_NAME ${name}
      SOURCES "${sources}"
    )
  endif()
  blender_target_include_dirs(${name}_test ${includes})
  blender_target_include_dirs_sys(${name}_test ${includes_sys})
endfunction()

function(blender_add_test_executable
  name
  sources
  includes
  includes_sys
  library_deps
  )
  blender_add_test_executable_impl(
    "${name}"
    TRUE
    "${sources}"
    "${includes}"
    "${includes_sys}"
    "${library_deps}"
   )
endfunction()

function(blender_add_performancetest_executable
  name
  sources
  includes
  includes_sys
  library_deps
  )
  blender_add_test_executable_impl(
    "${name}"
    FALSE
    "${sources}"
    "${includes}"
    "${includes_sys}"
    "${library_deps}"
  )
endfunction()
