
cmake_policy(SET CMP0003 NEW)
cmake_policy(SET CMP0005 NEW)

cmake_minimum_required(VERSION 2.8)

# -----------------------------------------------------------------------------
# Macros


# stub macro, does nothing
macro(blender_add_lib
	name
	sources
	includes
	includes_sys
	)

endmacro()

# suffix relative paths so we can use external cmake files
macro(suffix_relpaths
	new_files files prefix)

	set(${new_files})
	foreach(_file ${files})
		if(IS_ABSOLUTE _file)
			list(APPEND ${new_files} ${_file})
		else()
			list(APPEND ${new_files} "${prefix}${_file}")
		endif()
	endforeach()
	unset(_file)
endmacro()


# -----------------------------------------------------------------------------
# Defines

# set the endian define
if(MSVC)
	# for some reason this fails on msvc
	add_definitions(-D__LITTLE_ENDIAN__)
else()
	include(TestBigEndian)
	test_big_endian(_SYSTEM_BIG_ENDIAN)
	if(_SYSTEM_BIG_ENDIAN)
		add_definitions(-D__BIG_ENDIAN__)
	else()
		add_definitions(-D__LITTLE_ENDIAN__)
	endif()
	unset(_SYSTEM_BIG_ENDIAN)
endif()


# -----------------------------------------------------------------------------
# Libraries

# ghost
include(${CMAKE_SOURCE_DIR}/../CMakeLists.txt)
suffix_relpaths(INC_NEW "${INC}" "../")
suffix_relpaths(SRC_NEW "${SRC}" "../")
include_directories(${INC_NEW})
add_library(ghost_lib ${SRC_NEW})

# string
include(${CMAKE_SOURCE_DIR}/../../string/CMakeLists.txt)
suffix_relpaths(INC_NEW "${INC}" "../../string/")
suffix_relpaths(SRC_NEW "${SRC}" "../../string/")
include_directories(${INC_NEW})
add_library(string_lib ${SRC_NEW})

# guardedalloc
include(${CMAKE_SOURCE_DIR}/../../guardedalloc/CMakeLists.txt)
suffix_relpaths(INC_NEW "${INC}" "../../guardedalloc/")
suffix_relpaths(SRC_NEW "${SRC}" "../../guardedalloc/")
include_directories(${INC_NEW})
add_library(guardedalloc_lib ${SRC_NEW})

# blenfont 
include(${CMAKE_SOURCE_DIR}/../../../source/blender/blenfont/CMakeLists.txt)
suffix_relpaths(INC_NEW "${INC}" "../../../source/blender/blenfont/")
suffix_relpaths(SRC_NEW "${SRC}" "../../../source/blender/blenfont/")
include_directories(${INC_NEW})
add_library(blenfont_lib ${SRC_NEW})

# grr, blenfont needs BLI
include_directories(
		"../../../source/blender/blenlib"
		"../../../source/blender/blenloader"
		)
add_library(bli_lib 
		"../../../source/blender/blenlib/intern/fileops.c"
		"../../../source/blender/blenlib/intern/rct.c"
		"../../../source/blender/blenlib/intern/string.c"
		"../../../source/blender/blenlib/intern/string_utf8.c"
		"../../../source/blender/blenlib/intern/listbase.c"
		"../../../source/blender/blenlib/intern/storage.c"
		"../../../source/blender/blenlib/intern/path_util.c"
		"../../../source/blender/blenlib/intern/BLI_dynstr.c"
		"../../../source/blender/blenlib/intern/BLI_linklist.c"
		"../../../source/blender/blenlib/intern/BLI_memarena.c"
		)


find_package(OpenGL REQUIRED)

find_package(Freetype REQUIRED)

find_package(ZLIB REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}/../)
include_directories(${OPENGL_INCLUDE_DIR})
include_directories(${FREETYPE_INCLUDE_DIRS})
include_directories(${CMAKE_SOURCE_DIR}/../../../source/blender/blenfont)

if(UNIX AND NOT APPLE)
	find_package(X11 REQUIRED)

	set(PLATFORM_LINKLIBS
		${X11_X11_LIB}
		${X11_Xinput_LIB}
	)
endif()

# -----------------------------------------------------------------------------
# Executables


# Gears (C)
add_executable(gears_c
		${CMAKE_SOURCE_DIR}/gears/GHOST_C-Test.c)

target_link_libraries(gears_c
		ghost_lib
		string_lib
		${OPENGL_gl_LIBRARY}
		${OPENGL_glu_LIBRARY}
		${PLATFORM_LINKLIBS}
		)


# Gears (C++)
add_executable(gears_cpp
		${CMAKE_SOURCE_DIR}/gears/GHOST_Test.cpp)

target_link_libraries(gears_cpp
		ghost_lib
		string_lib
		${OPENGL_gl_LIBRARY}
		${OPENGL_glu_LIBRARY}
		${PLATFORM_LINKLIBS}
		)


# MultiTest (C)
add_executable(multitest_c
	${CMAKE_SOURCE_DIR}/../../../source/blender/editors/datafiles/bfont.ttf.c
	${CMAKE_SOURCE_DIR}/multitest/Basic.c
	${CMAKE_SOURCE_DIR}/multitest/EventToBuf.c
	${CMAKE_SOURCE_DIR}/multitest/MultiTest.c
	${CMAKE_SOURCE_DIR}/multitest/ScrollBar.c
	${CMAKE_SOURCE_DIR}/multitest/Util.c
	${CMAKE_SOURCE_DIR}/multitest/WindowData.c
)

target_link_libraries(multitest_c
		blenfont_lib
		bli_lib
		ghost_lib
		string_lib
		guardedalloc_lib
		${OPENGL_gl_LIBRARY}
		${OPENGL_glu_LIBRARY}
		${FREETYPE_LIBRARY}
		${ZLIB_LIBRARIES}
		${PLATFORM_LINKLIBS}
		)
