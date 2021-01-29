#=============================================================================
# Copyright 2014 Blender Foundation.
#
# Distributed under the OSI-approved BSD 3-Clause License,
# see accompanying file BSD-3-Clause-license.txt for details.
#
# Inspired on the Testing.cmake from Libmv
#
#=============================================================================

function(GET_BLENDER_TEST_INSTALL_DIR VARIABLE_NAME)
  get_property(GENERATOR_IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
  if(GENERATOR_IS_MULTI_CONFIG)
    string(REPLACE "\${BUILD_TYPE}" "$<CONFIG>" TEST_INSTALL_DIR ${CMAKE_INSTALL_PREFIX})
  else()
    string(REPLACE "\${BUILD_TYPE}" "" TEST_INSTALL_DIR ${CMAKE_INSTALL_PREFIX})
  endif()
  set(${VARIABLE_NAME} "${TEST_INSTALL_DIR}" PARENT_SCOPE)
endfunction()


macro(BLENDER_SRC_GTEST_EX)
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
    endif()

    add_definitions(-DBLENDER_GFLAGS_NAMESPACE=${GFLAGS_NAMESPACE})
    add_definitions(${GFLAGS_DEFINES})
    add_definitions(${GLOG_DEFINES})

    add_executable(${TARGET_NAME} ${ARG_SRC} ${MANIFEST})
    setup_platform_linker_flags(${TARGET_NAME})
    target_include_directories(${TARGET_NAME} PUBLIC "${TEST_INC}")
    target_include_directories(${TARGET_NAME} SYSTEM PUBLIC "${TEST_INC_SYS}")
    target_link_libraries(${TARGET_NAME} ${ARG_EXTRA_LIBS} ${PLATFORM_LINKLIBS})
    if(WITH_TBB)
      # Force TBB libraries to be in front of MKL (part of OpenImageDenoise), so
      # that it is initialized before MKL and static library initialization order
      # issues are avoided.
      target_link_libraries(${TARGET_NAME} ${TBB_LIBRARIES})
      if(WITH_OPENIMAGEDENOISE)
        target_link_libraries(${TARGET_NAME} ${OPENIMAGEDENOISE_LIBRARIES})
      endif()
    endif()
    target_link_libraries(${TARGET_NAME}
                          bf_testing_main
                          bf_intern_eigen
                          bf_intern_guardedalloc
                          extern_gtest
                          extern_gmock
                          # needed for glog
                          ${PTHREADS_LIBRARIES}
                          ${GLOG_LIBRARIES}
                          ${GFLAGS_LIBRARIES})
    if(WITH_OPENMP_STATIC)
      target_link_libraries(${TARGET_NAME} ${OpenMP_LIBRARIES})
    endif()
    if(UNIX AND NOT APPLE)
      target_link_libraries(${TARGET_NAME} bf_intern_libc_compat)
    endif()
    if(WITH_TBB)
      target_link_libraries(${TARGET_NAME} ${TBB_LIBRARIES})
    endif()
    if(WITH_GMP)
      target_link_libraries(${TARGET_NAME} ${GMP_LIBRARIES})
    endif()

    GET_BLENDER_TEST_INSTALL_DIR(TEST_INSTALL_DIR)
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
    endif()
    if(WIN32)
      set_target_properties(${TARGET_NAME} PROPERTIES VS_GLOBAL_VcpkgEnabled "false")
      unset(MANIFEST)
    endif()
    unset(TEST_INC)
    unset(TEST_INC_SYS)
    unset(TARGET_NAME)
  endif()
endmacro()

macro(BLENDER_SRC_GTEST NAME SRC EXTRA_LIBS)
  BLENDER_SRC_GTEST_EX(
    NAME "${NAME}"
    SRC "${SRC}"
    EXTRA_LIBS "${EXTRA_LIBS}")
endmacro()

macro(BLENDER_TEST NAME EXTRA_LIBS)
  BLENDER_SRC_GTEST_EX(
    NAME "${NAME}"
    SRC "${NAME}_test.cc"
    EXTRA_LIBS "${EXTRA_LIBS}")
endmacro()

macro(BLENDER_TEST_PERFORMANCE NAME EXTRA_LIBS)
  BLENDER_SRC_GTEST_EX(
    NAME "${NAME}"
    SRC "${NAME}_test.cc"
    EXTRA_LIBS "${EXTRA_LIBS}"
    SKIP_ADD_TEST)
endmacro()
