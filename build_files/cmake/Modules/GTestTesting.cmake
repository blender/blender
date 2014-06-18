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

macro(BLENDER_SRC_GTEST NAME SRC EXTRA_LIBS)
	if(WITH_GTESTS)
		get_property(_current_include_directories
		             DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		             PROPERTY INCLUDE_DIRECTORIES)
		set(TEST_INC
			${_current_include_directories}
			${CMAKE_SOURCE_DIR}/tests/gtests
			${CMAKE_SOURCE_DIR}/extern/libmv/third_party/glog/src
			${CMAKE_SOURCE_DIR}/extern/libmv/third_party/gflags
			${CMAKE_SOURCE_DIR}/extern/gtest/include
		)
		unset(_current_include_directories)

		add_executable(${NAME}_test ${SRC})
		target_link_libraries(${NAME}_test
		                      ${EXTRA_LIBS}
		                      bf_testing_main
		                      bf_intern_guardedalloc
		                      extern_gtest
		                      extern_glog)
		set_target_properties(${NAME}_test PROPERTIES
		                      RUNTIME_OUTPUT_DIRECTORY         "${TESTS_OUTPUT_DIR}"
		                      RUNTIME_OUTPUT_DIRECTORY_RELEASE "${TESTS_OUTPUT_DIR}"
		                      RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${TESTS_OUTPUT_DIR}"
		                      INCLUDE_DIRECTORIES              "${TEST_INC}")
		add_test(${NAME}_test ${TESTS_OUTPUT_DIR}/${NAME}_test)
	endif()
endmacro()

macro(BLENDER_TEST NAME EXTRA_LIBS)
	BLENDER_SRC_GTEST("${NAME}" "${NAME}_test.cc" "${EXTRA_LIBS}")
endmacro()
