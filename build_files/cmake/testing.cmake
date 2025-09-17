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

# Add the necessary LSAN/ASAN options to the given list of environment variables.
# Typically used after adding a test, before calling
#   `set_tests_properties(${testname} PROPERTIES ENVIRONMENT "${_envvar_list}")`,
# to ensure that it will run with the correct sanitizer settings.
#
# \param envvars_list: A list of extra environment variables to define for that test.
#                      Note that this does no check for (re-)definition of a same variable.
function(blender_test_set_envvars testname envvar_list)
  if(PLATFORM_ENV_INSTALL)
    list(APPEND envvar_list "${PLATFORM_ENV_INSTALL}")
  endif()

  if(NOT CMAKE_BUILD_TYPE MATCHES "Release")
    if(WITH_COMPILER_ASAN)
      set(_lsan_options "LSAN_OPTIONS=print_suppressions=false:suppressions=${CMAKE_SOURCE_DIR}/tools/config/analysis/lsan.supp")
      # FIXME: That `allocator_may_return_null=true` ASAN option is only needed for the
      # `guardedalloc` test, would be nice to allow tests definition to pass extra envvars better.
      set(_asan_options "ASAN_OPTIONS=allocator_may_return_null=true")
      if(DEFINED ENV{LSAN_OPTIONS})
        set(_lsan_options "${_lsan_options}:$ENV{LSAN_OPTIONS}")
      endif()
      if(DEFINED ENV{ASAN_OPTIONS})
        set(_asan_options "${_asan_options}:$ENV{ASAN_OPTIONS}")
      endif()
      list(APPEND envvar_list "${_lsan_options}" "${_asan_options}")
    endif()
  endif()
  if(WITH_COMPILER_CODE_COVERAGE AND CMAKE_C_COMPILER_ID MATCHES "Clang")
    list(APPEND envvar_list "LLVM_PROFILE_FILE=${COMPILER_CODE_COVERAGE_DATA_DIR}/raw/blender_%p.profraw")
  endif()
  # Can only be called once per test to define its custom environment variables.
  set_tests_properties(${testname} PROPERTIES ENVIRONMENT "${envvar_list}")
endfunction()

macro(blender_src_gtest_ex)
  if(WITH_GTESTS)
    set(options)
    set(oneValueArgs NAME)
    set(multiValueArgs SRC EXTRA_LIBS COMMAND_ARGS)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    unset(options)
    unset(oneValueArgs)
    unset(multiValueArgs)

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
    if(WIN32 AND NOT WITH_WINDOWS_EXTERNAL_MANIFEST)
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
    if(WIN32)
      set_target_properties(${TARGET_NAME} PROPERTIES VS_GLOBAL_VcpkgEnabled "false")

      if(WITH_WINDOWS_EXTERNAL_MANIFEST)
        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/tests.exe.manifest ${TESTS_OUTPUT_DIR}/${TARGET_NAME}.exe.manifest
        )
      endif()
    endif()
    unset(MANIFEST)
    unset(TEST_INC)
    unset(TEST_INC_SYS)
    unset(TARGET_NAME)
  endif()
endmacro()

function(blender_add_ctests)
  if(ARGC LESS 1)
    message(FATAL_ERROR "No arguments supplied to blender_add_ctests()")
  endif()
  if(NOT EXISTS "${CMAKE_SOURCE_DIR}/tests/files/render")
    return()
  endif()

  # Parse the arguments
  set(oneValueArgs DISCOVER_TESTS TARGET SUITE_NAME)
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
  if(${ARGS_DISCOVER_TESTS})
    include(GTest)
    gtest_add_tests(
      TARGET ${ARGS_TARGET}
      SOURCES "${ARGS_SOURCES}"
      TEST_PREFIX ${ARGS_SUITE_NAME}
      WORKING_DIRECTORY "${TEST_INSTALL_DIR}"
      EXTRA_ARGS
        --test-assets-dir "${CMAKE_SOURCE_DIR}/tests/files"
        --test-release-dir "${_test_release_dir}"
    )
  else()
    add_test(
      NAME ${ARGS_SUITE_NAME}
      COMMAND ${ARGS_TARGET}
        --test-assets-dir "${CMAKE_SOURCE_DIR}/tests/files"
        --test-release-dir "${_test_release_dir}"
      WORKING_DIRECTORY ${TEST_INSTALL_DIR}
    )
  endif()
  blender_test_set_envvars("${ARGS_SUITE_NAME}" "")

  unset(_test_release_dir)
endfunction()

# Add tests for a Blender library, to be called in tandem with blender_add_lib().
#
# If WITH_TESTS_SINGLE_BINARY is enabled, tests will be put into the blender_test
# executable, and a separate ctest will be generated for every gtest contained in it.
#
# If WITH_TESTS_SINGLE_BINARY is disabled, this works identically to
# blender_add_test_suite_executable.
#
# The function accepts an optional argument which denotes list of sources which
# is to be compiled-in with the suite sources for each of the suites when the
# WITH_TESTS_SINGLE_BINARY configuration is set to OFF.
function(blender_add_test_suite_lib
  name
  sources
  includes
  includes_sys
  library_deps
  )

  # Sources which are common for all suits and do not need to yield their own
  # test suite binaries when WITH_TESTS_SINGLE_BINARY is OFF.
  set(common_sources ${ARGN})

  if(WITH_TESTS_SINGLE_BINARY)
    add_cc_flags_custom_test(${name}_tests PARENT_SCOPE)

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

    blender_add_lib__impl(${name}_tests
      "${sources};${common_sources}" "${includes}" "${includes_sys}" "${library_deps}")

    target_compile_definitions(${name}_tests PRIVATE ${GFLAGS_DEFINES})
    target_compile_definitions(${name}_tests PRIVATE ${GLOG_DEFINES})

    set_property(GLOBAL APPEND PROPERTY BLENDER_TEST_LIBS ${name}_tests)

    blender_add_ctests(
      TARGET blender_test
      SUITE_NAME ${name}
      SOURCES "${sources};${common_sources}"
      DISCOVER_TESTS TRUE
    )
  else()
    blender_add_test_suite_executable(
      "${name}"
      "${sources}"
      "${includes}"
      "${includes_sys}"
      "${library_deps}"
      "${common_sources}"
    )
  endif()
endfunction()


function(blender_add_test_executable_impl
  name
  sources
  includes
  includes_sys
  library_deps
  )

  set(oneValueArgs ADD_CTESTS DISCOVER_TESTS)
  set(multiValueArgs)
  cmake_parse_arguments(ARGS "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
  unset(oneValueArgs)
  unset(multiValueArgs)

  add_cc_flags_custom_test(${name} PARENT_SCOPE)

  ## Otherwise external projects will produce warnings that we cannot fix.
  remove_strict_flags()

  blender_src_gtest_ex(
    NAME ${name}
    SRC "${sources}"
    EXTRA_LIBS "${library_deps}"
  )

  if(ARGS_ADD_CTESTS)
    blender_add_ctests(
      TARGET ${name}_test
      SUITE_NAME ${name}
      SOURCES "${sources}"
      DISCOVER_TESTS ${ARGS_DISCOVER_TESTS}
    )
  endif()

  blender_target_include_dirs(${name}_test ${includes})
  blender_target_include_dirs_sys(${name}_test ${includes_sys})
  blender_source_group("${name}_test" "${sources}")
endfunction()

# Add tests for a Blender library, to be called in tandem with blender_add_lib().
#
# If WITH_TESTS_SINGLE_BINARY is enabled, this will generate a single executable
# named ${name}_test, and generate a separate ctest for every gtest contained in it.
#
# If WITH_TESTS_SINGLE_BINARY is disabled, this will generate an executable
# named ${name}_${source}_test for every source file (with redundant prefixes and
# postfixes stripped).
#
# To be used for smaller isolated libraries, that do not have many dependencies.
# For libraries that do drag in many other Blender libraries and would create a
# very large executable, blender_add_test_suite_lib() should be used instead.
#
# The function accepts an optional argument which denotes list of sources which
# is to be compiled-in with the suit sources for each of the suites when the
# WITH_TESTS_SINGLE_BINARY configuration is set to OFF.
function(blender_add_test_suite_executable
  name
  sources
  includes
  includes_sys
  library_deps
  )

  # Sources which are common for all suits and do not need to yield their own
  # test suit binaries when WITH_TESTS_SINGLE_BINARY is OFF.
  set(common_sources ${ARGN})

  if(WITH_TESTS_SINGLE_BINARY)
    blender_add_test_executable_impl(
      "${name}"
      "${sources};${common_sources}"
      "${includes}"
      "${includes_sys}"
      "${library_deps}"
      ADD_CTESTS TRUE
      DISCOVER_TESTS TRUE
    )
  else()
    foreach(source ${sources})
      get_filename_component(_source_ext ${source} LAST_EXT)
      if(NOT ${_source_ext} MATCHES "^\.h")
        # Generate test name without redundant prefixes and postfixes.
        get_filename_component(_test_name ${source} NAME_WE)
        if(NOT ${_test_name} MATCHES "^${name}_")
          set(_test_name "${name}_${_test_name}")
        endif()
        string(REGEX REPLACE "_test$" "" _test_name ${_test_name})
        string(REGEX REPLACE "_tests$" "" _test_name ${_test_name})

        blender_add_test_executable_impl(
          "${_test_name}"
          "${source};${common_sources}"
          "${includes}"
          "${includes_sys}"
          "${library_deps}"
          ADD_CTESTS TRUE
          DISCOVER_TESTS FALSE
        )

        # Work-around run-time dynamic loader error
        #   symbol not found in flat namespace '_PyBaseObject_Type'
        #
        # Some tests are testing modules which are linked against Python, while some of unit
        # tests might not use code path which uses Python functionality. In this case linker
        # will optimize out all symbols from Python since it decides they are not used. This
        # somehow conflicts with other libraries which are linked against the test binary and
        # perform search of _PyBaseObject_Type on startup.
        #
        # Work-around by telling the linker that the python libraries should not be stripped.
        if(APPLE)
          target_link_libraries("${_test_name}_test" PRIVATE "-Wl,-force_load,${PYTHON_LIBRARIES}")
        endif()

        if(WITH_BUILDINFO)
          target_link_libraries("${_test_name}_test" PRIVATE buildinfoobj)
        endif()
      endif()
    endforeach()
  endif()
endfunction()

# Add test for a Blender library, to be called in tandem with blender_add_lib().
# Source files will be compiled into a single ${name}_test executable.
#
# To be used for smaller isolated libraries, that do not have many dependencies.
# For libraries that do drag in many other Blender libraries and would create a
# very large executable, blender_add_test_lib() should be used instead.
function(blender_add_test_executable
  name
  sources
  includes
  includes_sys
  library_deps
  )
  blender_add_test_executable_impl(
    "${name}"
    "${sources}"
    "${includes}"
    "${includes_sys}"
    "${library_deps}"
    ADD_CTESTS TRUE
    DISCOVER_TESTS FALSE
  )
endfunction()

# Add performance test. This is like blender_add_test_executable, but no ctest
# is generated and the binary should be run manually.
function(blender_add_test_performance_executable
  name
  sources
  includes
  includes_sys
  library_deps
  )
  blender_add_test_executable_impl(
    "${name}"
    "${sources}"
    "${includes}"
    "${includes_sys}"
    "${library_deps}"
    ADD_CTESTS FALSE
    DISCOVER_TESTS FALSE
  )
endfunction()
