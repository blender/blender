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

	if(WITH_PYTHON)  #  AND NOT WITH_PYTHON_MODULE  # WIN32 needs
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
	if(WITH_IMAGE_OPENJPEG AND UNIX AND NOT APPLE)
		link_directories(${OPENJPEG_LIBPATH})
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

	target_link_libraries(${target} ${OPENGL_gl_LIBRARY} ${OPENGL_glu_LIBRARY} ${JPEG_LIBRARIES} ${PNG_LIBRARIES} ${ZLIB_LIBRARIES} ${LLIBS})

	# since we are using the local libs for python when compiling msvc projects, we need to add _d when compiling debug versions
	if(WITH_PYTHON)  # AND NOT WITH_PYTHON_MODULE  # WIN32 needs
		target_link_libraries(${target} ${PYTHON_LINKFLAGS})

		if(WIN32 AND NOT UNIX)
			target_link_libraries(${target} debug ${PYTHON_LIBRARY}_d)
			target_link_libraries(${target} optimized ${PYTHON_LIBRARY})
		else()
			target_link_libraries(${target} ${PYTHON_LIBRARY})
		endif()
	endif()

	target_link_libraries(${target} ${OPENGL_glu_LIBRARY} ${JPEG_LIBRARIES} ${PNG_LIBRARIES} ${ZLIB_LIBRARIES})
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
	if(WITH_IMAGE_OPENJPEG AND UNIX AND NOT APPLE)
		target_link_libraries(${target} ${OPENJPEG_LIB})
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
macro(remove_flag
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
		remove_flag("-Wstrict-prototypes")
		remove_flag("-Wunused-parameter")
		remove_flag("-Wwrite-strings")
		remove_flag("-Wshadow")
		remove_flag("-Werror=[^ ]+")
		remove_flag("-Werror")
	endif()

	if(MSVC)
		# TODO
	endif()

endmacro()

macro(ADD_CHECK_C_COMPILER_FLAG
	_CFLAGS
	_CACHE_VAR
	_FLAG)

	include(CheckCCompilerFlag)

	CHECK_C_COMPILER_FLAG("${_FLAG}" "${_CACHE_VAR}")
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
	# So cmake depends on BKE_blender.h, beware of inf-loops!
	CONFIGURE_FILE(${CMAKE_SOURCE_DIR}/source/blender/blenkernel/BKE_blender.h ${CMAKE_BINARY_DIR}/source/blender/blenkernel/BKE_blender.h.done)

	file(STRINGS ${CMAKE_SOURCE_DIR}/source/blender/blenkernel/BKE_blender.h _contents REGEX "^#define[ \t]+BLENDER_.*$")

	string(REGEX REPLACE ".*#define[ \t]+BLENDER_VERSION[ \t]+([0-9]+).*" "\\1" _out_version "${_contents}")
	string(REGEX REPLACE ".*#define[ \t]+BLENDER_SUBVERSION[ \t]+([0-9]+).*" "\\1" _out_subversion "${_contents}")
	string(REGEX REPLACE ".*#define[ \t]+BLENDER_VERSION_CHAR[ \t]+([a-z]+).*" "\\1" _out_version_char "${_contents}")
	string(REGEX REPLACE ".*#define[ \t]+BLENDER_VERSION_CYCLE[ \t]+([a-z]+).*" "\\1" _out_version_cycle "${_contents}")

	if(NOT ${_out_version} MATCHES "[0-9]+")
		message(FATAL_ERROR "Version parsing failed for BLENDER_VERSION")
	endif()

	if(NOT ${_out_subversion} MATCHES "[0-9]+")
		message(FATAL_ERROR "Version parsing failed for BLENDER_SUBVERSION")
	endif()

	if(NOT ${_out_version_char} MATCHES "[a-z]+")
		message(FATAL_ERROR "Version parsing failed for BLENDER_VERSION_CHAR")
	endif()

	if(NOT ${_out_version_cycle} MATCHES "[a-z]+")
		message(FATAL_ERROR "Version parsing failed for BLENDER_VERSION_CYCLE")
	endif()

	math(EXPR BLENDER_VERSION_MAJOR "${_out_version} / 100")
	math(EXPR BLENDER_VERSION_MINOR "${_out_version} % 100")
	set(BLENDER_VERSION "${BLENDER_VERSION_MAJOR}.${BLENDER_VERSION_MINOR}")

	set(BLENDER_SUBVERSION ${_out_subversion})
	set(BLENDER_VERSION_CHAR ${_out_version_char})
	set(BLENDER_VERSION_CYCLE ${_out_version_cycle})

	# for packaging, alpha to numbers
	if(${BLENDER_VERSION_CHAR})
		set(BLENDER_VERSION_CHAR_INDEX "0")
	else()
		set(_char_ls a b c d e f g h i j k l m n o p q r s t u v w q y z)
		list(FIND _char_ls ${BLENDER_VERSION_CHAR} _out_version_char_index)
		math(EXPR BLENDER_VERSION_CHAR_INDEX "${_out_version_char_index} + 1")
		unset(_char_ls)
		unset(_out_version_char_index)
	endif()

	unset(_out_subversion)
	unset(_out_version_char)
	unset(_out_version_cycle)

	# message(STATUS "Version (Internal): ${BLENDER_VERSION}.${BLENDER_SUBVERSION}, Version (external): ${BLENDER_VERSION}${BLENDER_VERSION_CHAR}-${BLENDER_VERSION_CYCLE}")
endmacro()
