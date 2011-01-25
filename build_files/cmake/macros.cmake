# -*- mode: cmake; indent-tabs-mode: t; -*-
# $Id$

# Nicer makefiles with -I/1/foo/ instead of -I/1/2/3/../../foo/
# use it instead of include_directories()
macro(blender_include_dirs
	includes)

	foreach(inc ${ARGV})
		get_filename_component(abs_inc ${inc} ABSOLUTE)
		list(APPEND all_incs ${abs_inc})
	endforeach()
	include_directories(${all_incs})
endmacro()

# only MSVC uses SOURCE_GROUP
macro(blender_add_lib_nolist
	name
	sources
	includes)

	# message(STATUS "Configuring library ${name}")

	blender_include_dirs("${includes}")
	add_library(${name} ${sources})

	# Group by location on disk
	source_group("Source Files" FILES CMakeLists.txt)
	foreach(SRC ${sources})
		get_filename_component(SRC_EXT ${SRC} EXT)
		if(${SRC_EXT} MATCHES ".h" OR ${SRC_EXT} MATCHES ".hpp")
			source_group("Header Files" FILES ${SRC})
		else()
			source_group("Source Files" FILES ${SRC})
		endif()
	endforeach()
endmacro()

#	# works fine but having the includes listed is helpful for IDE's (QtCreator/MSVC)
#	macro(blender_add_lib_nolist
#		name
#		sources
#		includes)
#
#		message(STATUS "Configuring library ${name}")
#		include_directories(${includes})
#		add_library(${name} ${sources})
#	endmacro()

macro(blender_add_lib
	name
	sources
	includes)

	blender_add_lib_nolist(${name} "${sources}" "${includes}")

	set_property(GLOBAL APPEND PROPERTY BLENDER_LINK_LIBS ${name})

endmacro()

macro(SETUP_LIBDIRS)
	# see "cmake --help-policy CMP0003"
	if(COMMAND cmake_policy)
		cmake_policy(SET CMP0003 NEW)
	endif()

	link_directories(${JPEG_LIBPATH} ${PNG_LIBPATH} ${ZLIB_LIBPATH} ${FREETYPE_LIBPATH})

	if(WITH_PYTHON)
		link_directories(${PYTHON_LIBPATH})
	endif()
	if(WITH_INTERNATIONAL)
		link_directories(${ICONV_LIBPATH})
		link_directories(${GETTEXT_LIBPATH})
	endif()
	if(WITH_SDL)
		link_directories(${SDL_LIBPATH})
	endif()
	if(WITH_CODEC_FFMPEG)
		link_directories(${FFMPEG_LIBPATH})
	endif()
	if(WITH_IMAGE_OPENEXR)
		link_directories(${OPENEXR_LIBPATH})
	endif()
	if(WITH_IMAGE_TIFF)
		link_directories(${TIFF_LIBPATH})
	endif()
	if(WITH_LCMS)
		link_directories(${LCMS_LIBPATH})
	endif()
	if(WITH_CODEC_QUICKTIME)
		link_directories(${QUICKTIME_LIBPATH})
	endif()
	if(WITH_OPENAL)
		link_directories(${OPENAL_LIBPATH})
	endif()
	if(WITH_JACK)
		link_directories(${JACK_LIBPATH})
	endif()
	if(WITH_CODEC_SNDFILE)
		link_directories(${SNDFILE_LIBPATH})
	endif()
	if(WITH_SAMPLERATE)
		link_directories(${LIBSAMPLERATE_LIBPATH})
	endif()
	if(WITH_FFTW3)
		link_directories(${FFTW3_LIBPATH})
	endif()
	if(WITH_OPENCOLLADA)
		link_directories(${OPENCOLLADA_LIBPATH})
		link_directories(${PCRE_LIBPATH})
		link_directories(${EXPAT_LIBPATH})
	endif()

	if(WIN32 AND NOT UNIX)
		link_directories(${PTHREADS_LIBPATH})
	endif()
endmacro()

macro(setup_liblinks
	target)
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${PLATFORM_LINKFLAGS} ")

	target_link_libraries(${target} ${OPENGL_gl_LIBRARY} ${OPENGL_glu_LIBRARY} ${JPEG_LIBRARY} ${PNG_LIBRARIES} ${ZLIB_LIBRARIES} ${LLIBS})

	# since we are using the local libs for python when compiling msvc projects, we need to add _d when compiling debug versions
	if(WITH_PYTHON)
		target_link_libraries(${target} ${PYTHON_LINKFLAGS})

		if(WIN32 AND NOT UNIX)
			target_link_libraries(${target} debug ${PYTHON_LIBRARY}_d)
			target_link_libraries(${target} optimized ${PYTHON_LIBRARY})
		else()
			target_link_libraries(${target} ${PYTHON_LIBRARY})
		endif()
	endif()

	target_link_libraries(${target} ${OPENGL_glu_LIBRARY} ${JPEG_LIB} ${PNG_LIB} ${ZLIB_LIB})
	target_link_libraries(${target} ${FREETYPE_LIBRARY})

	if(WITH_INTERNATIONAL)
		target_link_libraries(${target} ${GETTEXT_LIB})

		if(WIN32 AND NOT UNIX)
			target_link_libraries(${target} ${ICONV_LIB})
		endif()
	endif()

	if(WITH_OPENAL)
		target_link_libraries(${target} ${OPENAL_LIBRARY})
	endif()
	if(WITH_FFTW3)
		target_link_libraries(${target} ${FFTW3_LIB})
	endif()
	if(WITH_JACK)
		target_link_libraries(${target} ${JACK_LIB})
	endif()
	if(WITH_CODEC_SNDFILE)
		target_link_libraries(${target} ${SNDFILE_LIB})
	endif()
	if(WITH_SAMPLERATE)
		target_link_libraries(${target} ${LIBSAMPLERATE_LIB})
	endif()
	if(WITH_SDL)
		target_link_libraries(${target} ${SDL_LIBRARY})
	endif()
	if(WITH_CODEC_QUICKTIME)
		target_link_libraries(${target} ${QUICKTIME_LIB})
	endif()
	if(WITH_IMAGE_TIFF)
		target_link_libraries(${target} ${TIFF_LIBRARY})
	endif()
	if(WITH_IMAGE_OPENEXR)
		if(WIN32 AND NOT UNIX)
			foreach(loop_var ${OPENEXR_LIB})
				target_link_libraries(${target} debug ${loop_var}_d)
				target_link_libraries(${target} optimized ${loop_var})
			endforeach()
		else()
			target_link_libraries(${target} ${OPENEXR_LIB})
		endif()
	endif()
	if(WITH_LCMS)
		target_link_libraries(${target} ${LCMS_LIBRARY})
	endif()
	if(WITH_CODEC_FFMPEG)
		target_link_libraries(${target} ${FFMPEG_LIB})
	endif()
	if(WITH_OPENCOLLADA)
		if(WIN32 AND NOT UNIX)
			foreach(loop_var ${OPENCOLLADA_LIB})
				target_link_libraries(${target} debug ${loop_var}_d)
				target_link_libraries(${target} optimized ${loop_var})
			endforeach()
			target_link_libraries(${target} debug ${PCRE_LIB}_d)
			target_link_libraries(${target} optimized ${PCRE_LIB})
			if(EXPAT_LIB)
				target_link_libraries(${target} debug ${EXPAT_LIB}_d)
				target_link_libraries(${target} optimized ${EXPAT_LIB})
			endif()
		else()
			target_link_libraries(${target} ${OPENCOLLADA_LIB})
			target_link_libraries(${target} ${PCRE_LIB})
			target_link_libraries(${target} ${EXPAT_LIB})
		endif()
	endif()
	if(WITH_LCMS)
		if(WIN32 AND NOT UNIX)
			target_link_libraries(${target} debug ${LCMS_LIB}_d)
			target_link_libraries(${target} optimized ${LCMS_LIB})
		endif()
	endif()
	if(WIN32 AND NOT UNIX)
		target_link_libraries(${target} ${PTHREADS_LIB})
	endif()
endmacro()

macro(TEST_SSE_SUPPORT)
	include(CheckCSourceRuns)

	# message(STATUS "Detecting SSE support")
	if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
		set(CMAKE_REQUIRED_FLAGS "-msse -msse2")
	elseif(MSVC)
		set(CMAKE_REQUIRED_FLAGS "/arch:SSE2") # TODO, SSE 1 ?
	endif()

	if(NOT DEFINED ${SUPPORT_SSE_BUILD})
		check_c_source_runs("
			#include <xmmintrin.h>
			int main() { __m128 v = _mm_setzero_ps(); return 0; }"
		SUPPORT_SSE_BUILD)
		
		if(SUPPORT_SSE_BUILD)
			message(STATUS "SSE Support: detected.")
		else()
			message(STATUS "SSE Support: missing.")
		endif()
		set(${SUPPORT_SSE_BUILD} ${SUPPORT_SSE_BUILD} CACHE INTERNAL "SSE Test")
	endif()	

	if(NOT DEFINED ${SUPPORT_SSE2_BUILD})
		check_c_source_runs("
			#include <emmintrin.h>
			int main() { __m128d v = _mm_setzero_pd(); return 0; }"
		SUPPORT_SSE2_BUILD)

		if(SUPPORT_SSE2_BUILD)
			message(STATUS "SSE2 Support: detected.")
		else()
			message(STATUS "SSE2 Support: missing.")
		endif()	
		set(${SUPPORT_SSE2_BUILD} ${SUPPORT_SSE2_BUILD} CACHE INTERNAL "SSE2 Test")
	endif()

endmacro()

# when we have warnings as errors applied globally this
# needs to be removed for some external libs which we dont maintain.

# utility macro
macro(_remove_strict_flags
	flag)

	string(REGEX REPLACE ${flag} "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
	string(REGEX REPLACE ${flag} "" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
	string(REGEX REPLACE ${flag} "" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
	string(REGEX REPLACE ${flag} "" CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL}")
	string(REGEX REPLACE ${flag} "" CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}")

	string(REGEX REPLACE ${flag} "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
	string(REGEX REPLACE ${flag} "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
	string(REGEX REPLACE ${flag} "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
	string(REGEX REPLACE ${flag} "" CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL}")
	string(REGEX REPLACE ${flag} "" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")

endmacro()

macro(remove_strict_flags)

	if(CMAKE_COMPILER_IS_GNUCC)
		_remove_strict_flags("-Wstrict-prototypes")
		_remove_strict_flags("-Wunused-parameter")
		_remove_strict_flags("-Wwrite-strings")
		_remove_strict_flags("-Wshadow")
		_remove_strict_flags("-Werror=[^ ]+")
		_remove_strict_flags("-Werror")
	endif()

	if(MSVC)
		# TODO
	endif()

endmacro()


# XXX, until cmake 2.8.4 is released.
INCLUDE(CheckCSourceCompiles)
MACRO (CHECK_C_COMPILER_FLAG__INTERNAL _FLAG _RESULT)
   SET(SAFE_CMAKE_REQUIRED_DEFINITIONS "${CMAKE_REQUIRED_DEFINITIONS}")
   SET(CMAKE_REQUIRED_DEFINITIONS "${_FLAG}")
   CHECK_C_SOURCE_COMPILES("int main(void) { return 0;}" ${_RESULT}
     # Some compilers do not fail with a bad flag
     FAIL_REGEX "unrecognized .*option"                     # GNU
     FAIL_REGEX "ignoring unknown option"                   # MSVC
     FAIL_REGEX "[Uu]nknown option"                         # HP
     FAIL_REGEX "[Ww]arning: [Oo]ption"                     # SunPro
     FAIL_REGEX "command option .* is not recognized"       # XL
     )
   SET (CMAKE_REQUIRED_DEFINITIONS "${SAFE_CMAKE_REQUIRED_DEFINITIONS}")
ENDMACRO (CHECK_C_COMPILER_FLAG__INTERNAL)
# XXX, end duplicate code.

macro(ADD_CHECK_C_COMPILER_FLAG
	_CFLAGS
	_CACHE_VAR
	_FLAG)

	# include(CheckCCompilerFlag)

	CHECK_C_COMPILER_FLAG__INTERNAL("${_FLAG}" "${_CACHE_VAR}")
	if(${_CACHE_VAR})
		# message(STATUS "Using CFLAG: ${_FLAG}")
		set(${_CFLAGS} "${${_CFLAGS}} ${_FLAG}")
	else()
		message(STATUS "Unsupported CFLAG: ${_FLAG}")
	endif()
endmacro()

macro(ADD_CHECK_CXX_COMPILER_FLAG
	_CXXFLAGS
	_CACHE_VAR
	_FLAG)

	include(CheckCXXCompilerFlag)

	CHECK_CXX_COMPILER_FLAG("${_FLAG}" "${_CACHE_VAR}")
	if(${_CACHE_VAR})
		# message(STATUS "Using CXXFLAG: ${_FLAG}")
		set(${_CXXFLAGS} "${${_CXXFLAGS}} ${_FLAG}")
	else()
		message(STATUS "Unsupported CXXFLAG: ${_FLAG}")
	endif()
endmacro()

macro(get_blender_version)
	file(READ ${CMAKE_SOURCE_DIR}/source/blender/blenkernel/BKE_blender.h CONTENT)
	string(REGEX REPLACE "\n" ";" CONTENT "${CONTENT}")
	string(REGEX REPLACE "\t" ";" CONTENT "${CONTENT}")
	string(REGEX REPLACE " " ";" CONTENT "${CONTENT}")

	foreach(ITEM ${CONTENT})
		if(LASTITEM MATCHES "BLENDER_VERSION")
			MATH(EXPR BLENDER_VERSION_MAJOR "${ITEM} / 100")
			MATH(EXPR BLENDER_VERSION_MINOR "${ITEM} % 100")
			set(BLENDER_VERSION "${BLENDER_VERSION_MAJOR}.${BLENDER_VERSION_MINOR}")
		endif()

		if(LASTITEM MATCHES "BLENDER_SUBVERSION")
			set(BLENDER_SUBVERSION ${ITEM})
		endif()

		if(LASTITEM MATCHES "BLENDER_MINVERSION")
			MATH(EXPR BLENDER_MINVERSION_MAJOR "${ITEM} / 100")
			MATH(EXPR BLENDER_MINVERSION_MINOR "${ITEM} % 100")
			set(BLENDER_MINVERSION "${BLENDER_MINVERSION_MAJOR}.${BLENDER_MINVERSION_MINOR}")
		endif()

		if(LASTITEM MATCHES "BLENDER_MINSUBVERSION")
			set(BLENDER_MINSUBVERSION ${ITEM})
		endif()

		set(LASTITEM ${ITEM})
	endforeach()

	# message(STATUS "Version major: ${BLENDER_VERSION_MAJOR}, Version minor: ${BLENDER_VERSION_MINOR}, Subversion: ${BLENDER_SUBVERSION}, Version: ${BLENDER_VERSION}")
	# message(STATUS "Minversion major: ${BLENDER_MINVERSION_MAJOR}, Minversion minor: ${BLENDER_MINVERSION_MINOR}, MinSubversion: ${BLENDER_MINSUBVERSION}, Minversion: ${BLENDER_MINVERSION}")
endmacro()
