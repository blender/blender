#=============================================================================
# Copyright 2014 Blender Foundation.
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#
# Inspired on the Testing.cmake from Libmv
#
#=============================================================================

macro(BLENDER_SRC_GTEST_EX NAME SRC EXTRA_LIBS DO_ADD_TEST)
  if(WITH_GTESTS)
    set(TARGET_NAME ${NAME}_test)
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

    add_executable(${TARGET_NAME} ${SRC})
    target_include_directories(${TARGET_NAME} PUBLIC "${TEST_INC}")
    target_include_directories(${TARGET_NAME} SYSTEM PUBLIC "${TEST_INC_SYS}")
    target_link_libraries(${TARGET_NAME}
                          ${EXTRA_LIBS}
                          ${PLATFORM_LINKLIBS}
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

    get_property(GENERATOR_IS_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
    if(GENERATOR_IS_MULTI_CONFIG)
      string(REPLACE "\${BUILD_TYPE}" "$<CONFIG>" TEST_INSTALL_DIR ${CMAKE_INSTALL_PREFIX})
    else()
      string(REPLACE "\${BUILD_TYPE}" "" TEST_INSTALL_DIR ${CMAKE_INSTALL_PREFIX})
    endif()

    set_target_properties(${TARGET_NAME} PROPERTIES
                          RUNTIME_OUTPUT_DIRECTORY         "${TESTS_OUTPUT_DIR}"
                          RUNTIME_OUTPUT_DIRECTORY_RELEASE "${TESTS_OUTPUT_DIR}"
                          RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${TESTS_OUTPUT_DIR}")
    if(${DO_ADD_TEST})
      add_test(NAME ${TARGET_NAME} COMMAND ${TESTS_OUTPUT_DIR}/${TARGET_NAME} WORKING_DIRECTORY ${TEST_INSTALL_DIR})

      # Don't fail tests on leaks since these often happen in external libraries
      # that we can't fix.
      set_tests_properties(${TARGET_NAME} PROPERTIES ENVIRONMENT LSAN_OPTIONS=exitcode=0)
    endif()
    unset(TEST_INC)
    unset(TEST_INC_SYS)
    unset(TARGET_NAME)
  endif()
endmacro()

macro(BLENDER_SRC_GTEST NAME SRC EXTRA_LIBS)
  BLENDER_SRC_GTEST_EX("${NAME}" "${SRC}" "${EXTRA_LIBS}" "TRUE")
endmacro()

macro(BLENDER_TEST NAME EXTRA_LIBS)
  BLENDER_SRC_GTEST_EX("${NAME}" "${NAME}_test.cc" "${EXTRA_LIBS}" "TRUE")
endmacro()

macro(BLENDER_TEST_PERFORMANCE NAME EXTRA_LIBS)
  BLENDER_SRC_GTEST_EX("${NAME}" "${NAME}_test.cc" "${EXTRA_LIBS}" "FALSE")
endmacro()
