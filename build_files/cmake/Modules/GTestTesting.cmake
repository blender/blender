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
		get_property(_current_include_directories
		             DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		             PROPERTY INCLUDE_DIRECTORIES)
		set(TEST_INC
			${_current_include_directories}
			${CMAKE_SOURCE_DIR}/tests/gtests
			${CMAKE_SOURCE_DIR}/extern/glog/src
			${CMAKE_SOURCE_DIR}/extern/gflags/src
			${CMAKE_SOURCE_DIR}/extern/gtest/include
			${CMAKE_SOURCE_DIR}/extern/gmock/include
		)
		unset(_current_include_directories)

		add_executable(${NAME}_test ${SRC})
		target_link_libraries(${NAME}_test
		                      ${EXTRA_LIBS}
		                      ${PLATFORM_LINKLIBS}
		                      bf_testing_main
		                      bf_intern_guardedalloc
		                      extern_gtest
		                      extern_gmock
		                      # needed for glog
		                      ${PTHREADS_LIBRARIES}
		                      extern_glog
		                      extern_gflags)
		set_target_properties(${NAME}_test PROPERTIES
		                      RUNTIME_OUTPUT_DIRECTORY         "${TESTS_OUTPUT_DIR}"
		                      RUNTIME_OUTPUT_DIRECTORY_RELEASE "${TESTS_OUTPUT_DIR}"
		                      RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${TESTS_OUTPUT_DIR}"
		                      INCLUDE_DIRECTORIES              "${TEST_INC}")
		if(${DO_ADD_TEST})
			add_test(${NAME}_test ${TESTS_OUTPUT_DIR}/${NAME}_test)
		endif()
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
