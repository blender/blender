# -*- mode: cmake; indent-tabs-mode: t; -*-


# foo_bar.spam --> foo_barMySuffix.spam
macro(file_suffix
	file_name_new file_name file_suffix
	)

	get_filename_component(_file_name_PATH ${file_name} PATH)
	get_filename_component(_file_name_NAME_WE ${file_name} NAME_WE)
	get_filename_component(_file_name_EXT ${file_name} EXT)
	set(${file_name_new} "${_file_name_PATH}/${_file_name_NAME_WE}${file_suffix}${_file_name_EXT}")

	unset(_file_name_PATH)
	unset(_file_name_NAME_WE)
	unset(_file_name_EXT)
endmacro()

# useful for adding debug suffix to library lists:
# /somepath/foo.lib --> /somepath/foo_d.lib
macro(file_list_suffix
	fp_list_new fp_list fn_suffix
	)

	# incase of empty list
	set(_fp)
	set(_fp_suffixed)

	set(fp_list_new)

	foreach(_fp ${fp_list})
		file_suffix(_fp_suffixed "${_fp}" "${fn_suffix}")
		list(APPEND "${fp_list_new}" "${_fp_suffixed}")
	endforeach()

	unset(_fp)
	unset(_fp_suffixed)

endmacro()


macro(target_link_libraries_optimized TARGET LIBS)
	foreach(_LIB ${LIBS})
		target_link_libraries(${TARGET} optimized "${_LIB}")
	endforeach()
	unset(_LIB)
endmacro()

macro(target_link_libraries_debug TARGET LIBS)
	foreach(_LIB ${LIBS})
		target_link_libraries(${TARGET} debug "${_LIB}")
	endforeach()
	unset(_LIB)
endmacro()

# Nicer makefiles with -I/1/foo/ instead of -I/1/2/3/../../foo/
# use it instead of include_directories()
macro(blender_include_dirs
	includes)
	set(_ALL_INCS "")
	foreach(_INC ${ARGV})
		get_filename_component(_ABS_INC ${_INC} ABSOLUTE)
		list(APPEND _ALL_INCS ${_ABS_INC})
		# for checking for invalid includes, disable for regular use
		##if(NOT EXISTS "${_ABS_INC}/")
		##	message(FATAL_ERROR "Include not found: ${_ABS_INC}/")
		##endif()
	endforeach()
	include_directories(${_ALL_INCS})
	unset(_INC)
	unset(_ABS_INC)
	unset(_ALL_INCS)
endmacro()

macro(blender_include_dirs_sys
	includes)
	set(_ALL_INCS "")
	foreach(_INC ${ARGV})
		get_filename_component(_ABS_INC ${_INC} ABSOLUTE)
		list(APPEND _ALL_INCS ${_ABS_INC})
		##if(NOT EXISTS "${_ABS_INC}/")
		##	message(FATAL_ERROR "Include not found: ${_ABS_INC}/")
		##endif()
	endforeach()
	include_directories(SYSTEM ${_ALL_INCS})
	unset(_INC)
	unset(_ABS_INC)
	unset(_ALL_INCS)
endmacro()

macro(blender_source_group
	sources)

	# Group by location on disk
	source_group("Source Files" FILES CMakeLists.txt)

	foreach(_SRC ${sources})
		get_filename_component(_SRC_EXT ${_SRC} EXT)
		if((${_SRC_EXT} MATCHES ".h") OR (${_SRC_EXT} MATCHES ".hpp"))
			source_group("Header Files" FILES ${_SRC})
		else()
			source_group("Source Files" FILES ${_SRC})
		endif()
	endforeach()

	unset(_SRC)
	unset(_SRC_EXT)
endmacro()


# only MSVC uses SOURCE_GROUP
macro(blender_add_lib_nolist
	name
	sources
	includes
	includes_sys)

	# message(STATUS "Configuring library ${name}")

	# include_directories(${includes})
	# include_directories(SYSTEM ${includes_sys})
	blender_include_dirs("${includes}")
	blender_include_dirs_sys("${includes_sys}")

	add_library(${name} ${sources})

	# works fine without having the includes
	# listed is helpful for IDE's (QtCreator/MSVC)
	blender_source_group("${sources}")

endmacro()


macro(blender_add_lib
	name
	sources
	includes
	includes_sys)

	blender_add_lib_nolist(${name} "${sources}" "${includes}" "${includes_sys}")

	set_property(GLOBAL APPEND PROPERTY BLENDER_LINK_LIBS ${name})
endmacro()


macro(SETUP_LIBDIRS)
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
	if(WITH_BOOST)
		link_directories(${BOOST_LIBPATH})
	endif()
	if(WITH_OPENIMAGEIO)
		link_directories(${OPENIMAGEIO_LIBPATH})
	endif()
	if(WITH_IMAGE_OPENJPEG AND UNIX AND NOT APPLE)
		link_directories(${OPENJPEG_LIBPATH})
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
	if(WITH_FFTW3)
		link_directories(${FFTW3_LIBPATH})
	endif()
	if(WITH_OPENCOLLADA)
		link_directories(${OPENCOLLADA_LIBPATH})
		link_directories(${PCRE_LIBPATH})
		link_directories(${EXPAT_LIBPATH})
	endif()
	if(WITH_MEM_JEMALLOC)
		link_directories(${JEMALLOC_LIBPATH})
	endif()

	if(WIN32 AND NOT UNIX)
		link_directories(${PTHREADS_LIBPATH})
	endif()
endmacro()

macro(setup_liblinks
	target)

	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${PLATFORM_LINKFLAGS}")
	set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} ${PLATFORM_LINKFLAGS_DEBUG}")

	target_link_libraries(${target}
			${OPENGL_gl_LIBRARY}
			${OPENGL_glu_LIBRARY}
			${PNG_LIBRARIES}
			${ZLIB_LIBRARIES}
			${FREETYPE_LIBRARY}
			${LAPACK_LIBRARIES})

	# since we are using the local libs for python when compiling msvc projects, we need to add _d when compiling debug versions
	if(WITH_PYTHON)  # AND NOT WITH_PYTHON_MODULE  # WIN32 needs
		target_link_libraries(${target} ${PYTHON_LINKFLAGS})

		if(WIN32 AND NOT UNIX)
			file_list_suffix(PYTHON_LIBRARIES_DEBUG "${PYTHON_LIBRARIES}" "_d")
			target_link_libraries_debug(${target} "${PYTHON_LIBRARIES_DEBUG}")
			target_link_libraries_optimized(${target} "${PYTHON_LIBRARIES}")
			unset(PYTHON_LIBRARIES_DEBUG)
		else()
			target_link_libraries(${target} ${PYTHON_LIBRARIES})
		endif()
	endif()

	if(NOT WITH_BUILTIN_GLEW)
		target_link_libraries(${target} ${GLEW_LIBRARY})
	endif()

	if(WITH_INTERNATIONAL)
		target_link_libraries(${target} ${GETTEXT_LIBRARIES})

		if(WIN32 AND NOT UNIX)
			target_link_libraries(${target} ${ICONV_LIBRARIES})
		endif()
	endif()

	if(WITH_OPENAL)
		target_link_libraries(${target} ${OPENAL_LIBRARY})
	endif()
	if(WITH_FFTW3)
		target_link_libraries(${target} ${FFTW3_LIBRARIES})
	endif()
	if(WITH_JACK)
		target_link_libraries(${target} ${JACK_LIBRARIES})
	endif()
	if(WITH_CODEC_SNDFILE)
		target_link_libraries(${target} ${SNDFILE_LIBRARIES})
	endif()
	if(WITH_SDL)
		target_link_libraries(${target} ${SDL_LIBRARY})
	endif()
	if(WITH_CODEC_QUICKTIME)
		target_link_libraries(${target} ${QUICKTIME_LIBRARIES})
	endif()
	if(WITH_IMAGE_TIFF)
		target_link_libraries(${target} ${TIFF_LIBRARY})
	endif()
	if(WITH_OPENIMAGEIO)
		target_link_libraries(${target} ${OPENIMAGEIO_LIBRARIES})
	endif()
	if(WITH_BOOST)
		target_link_libraries(${target} ${BOOST_LIBRARIES})
	endif()
	target_link_libraries(${target} ${JPEG_LIBRARIES})
	if(WITH_IMAGE_OPENEXR)
		if(WIN32 AND NOT UNIX AND NOT CMAKE_COMPILER_IS_GNUCC)
			file_list_suffix(OPENEXR_LIBRARIES_DEBUG "${OPENEXR_LIBRARIES}" "_d")
			target_link_libraries_debug(${target} "${OPENEXR_LIBRARIES_DEBUG}")
			target_link_libraries_optimized(${target} "${OPENEXR_LIBRARIES}")
			unset(OPENEXR_LIBRARIES_DEBUG)
		else()
			target_link_libraries(${target} ${OPENEXR_LIBRARIES})
		endif()
	endif()
	if(WITH_IMAGE_OPENJPEG AND UNIX AND NOT APPLE)
		target_link_libraries(${target} ${OPENJPEG_LIBRARIES})
	endif()
	if(WITH_CODEC_FFMPEG)

		# Strange!, without this ffmpeg gives linking errors (on linux)
		# even though its linked above
		target_link_libraries(${target} ${OPENGL_glu_LIBRARY})

		target_link_libraries(${target} ${FFMPEG_LIBRARIES})
	endif()
	if(WITH_OPENCOLLADA)
		if(WIN32 AND NOT UNIX)
			file_list_suffix(OPENCOLLADA_LIBRARIES_DEBUG "${OPENCOLLADA_LIBRARIES}" "_d")
			target_link_libraries_debug(${target} "${OPENCOLLADA_LIBRARIES_DEBUG}")
			target_link_libraries_optimized(${target} "${OPENCOLLADA_LIBRARIES}")
			unset(OPENCOLLADA_LIBRARIES_DEBUG)

			file_list_suffix(PCRE_LIBRARIES_DEBUG "${PCRE_LIBRARIES}" "_d")
			target_link_libraries_debug(${target} "${PCRE_LIBRARIES_DEBUG}")
			target_link_libraries_optimized(${target} "${PCRE_LIBRARIES}")
			unset(PCRE_LIBRARIES_DEBUG)

			if(EXPAT_LIB)
				file_list_suffix(EXPAT_LIB_DEBUG "${EXPAT_LIB}" "_d")
				target_link_libraries_debug(${target} "${EXPAT_LIB_DEBUG}")
				target_link_libraries_optimized(${target} "${EXPAT_LIB}")
				unset(EXPAT_LIB_DEBUG)
			endif()
		else()
			target_link_libraries(${target}
					${OPENCOLLADA_LIBRARIES}
					${PCRE_LIBRARIES}
					${XML2_LIBRARIES}
					${EXPAT_LIB})
		endif()
	endif()
	if(WITH_MEM_JEMALLOC)
		target_link_libraries(${target} ${JEMALLOC_LIBRARIES})
	endif()
	if(WITH_INPUT_NDOF)
		target_link_libraries(${target} ${NDOF_LIBRARIES})
	endif()

	if(WIN32 AND NOT UNIX)
		target_link_libraries(${target} ${PTHREADS_LIBRARIES})
	endif()

	target_link_libraries(${target} ${PLATFORM_LINKLIBS})
endmacro()

macro(TEST_SSE_SUPPORT
	_sse_flags
	_sse2_flags)

	include(CheckCSourceRuns)

	# message(STATUS "Detecting SSE support")
	if(CMAKE_COMPILER_IS_GNUCC OR (CMAKE_C_COMPILER_ID MATCHES "Clang"))
		set(${_sse_flags} "-msse")
		set(${_sse2_flags} "-msse2")
	elseif(MSVC)
		set(${_sse_flags} "/arch:SSE")
		set(${_sse2_flags} "/arch:SSE2")
	elseif(CMAKE_C_COMPILER_ID MATCHES "Intel")
		set(${_sse_flags} "")  # icc defaults to -msse
		set(${_sse2_flags} "-msse2")
	else()
		message(WARNING "SSE flags for this compiler: '${CMAKE_C_COMPILER_ID}' not known")
		set(${_sse_flags})
		set(${_sse2_flags})
	endif()

	set(CMAKE_REQUIRED_FLAGS "${${_sse_flags}} ${${_sse2_flags}}")

	if(NOT DEFINED SUPPORT_SSE_BUILD)
		# result cached
		check_c_source_runs("
			#include <xmmintrin.h>
			int main(void) { __m128 v = _mm_setzero_ps(); return 0; }"
		SUPPORT_SSE_BUILD)

		if(SUPPORT_SSE_BUILD)
			message(STATUS "SSE Support: detected.")
		else()
			message(STATUS "SSE Support: missing.")
		endif()
	endif()

	if(NOT DEFINED SUPPORT_SSE2_BUILD)
		# result cached
		check_c_source_runs("
			#include <emmintrin.h>
			int main(void) { __m128d v = _mm_setzero_pd(); return 0; }"
		SUPPORT_SSE2_BUILD)

		if(SUPPORT_SSE2_BUILD)
			message(STATUS "SSE2 Support: detected.")
		else()
			message(STATUS "SSE2 Support: missing.")
		endif()
	endif()

	unset(CMAKE_REQUIRED_FLAGS)
endmacro()

# when we have warnings as errors applied globally this
# needs to be removed for some external libs which we dont maintain.

# utility macro
macro(remove_cc_flag
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

macro(add_cc_flag
	flag)

	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${flag}")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}")
endmacro()

macro(remove_strict_flags)

	if(CMAKE_COMPILER_IS_GNUCC)
		remove_cc_flag("-Wstrict-prototypes")
		remove_cc_flag("-Wunused-parameter")
		remove_cc_flag("-Wwrite-strings")
		remove_cc_flag("-Wundef")
		remove_cc_flag("-Wshadow")
		remove_cc_flag("-Werror=[^ ]+")
		remove_cc_flag("-Werror")

		# negate flags implied by '-Wall'
		add_cc_flag("${CC_REMOVE_STRICT_FLAGS}")
	endif()

	if(MSVC)
		# TODO
	endif()

endmacro()

# note, we can only append flags on a single file so we need to negate the options.
# at the moment we cant shut up ffmpeg deprecations, so use this, but will
# probably add more removals here.
macro(remove_strict_flags_file
	filenames)

	foreach(_SOURCE ${ARGV})

		if(CMAKE_COMPILER_IS_GNUCC)
			set_source_files_properties(${_SOURCE}
				PROPERTIES
					COMPILE_FLAGS "${CC_REMOVE_STRICT_FLAGS}"
			)
		endif()

		if(MSVC)
			# TODO
		endif()

	endforeach()	

	unset(_SOURCE)

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

	# clumsy regex, only single char are ok but it could be unset

	string(LENGTH "${_out_version_char}" _out_version_char_len)
	if(NOT _out_version_char_len EQUAL 1)
		set(_out_version_char "")
	elseif(NOT ${_out_version_char} MATCHES "[a-z]+")
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
	string(COMPARE EQUAL "${BLENDER_VERSION_CHAR}" "" _out_version_char_empty)
	if(${_out_version_char_empty})
		set(BLENDER_VERSION_CHAR_INDEX "0")
	else()
		set(_char_ls a b c d e f g h i j k l m n o p q r s t u v w x y z)
		list(FIND _char_ls ${BLENDER_VERSION_CHAR} _out_version_char_index)
		math(EXPR BLENDER_VERSION_CHAR_INDEX "${_out_version_char_index} + 1")
		unset(_char_ls)
		unset(_out_version_char_index)
	endif()

	unset(_out_subversion)
	unset(_out_version_char)
	unset(_out_version_char_empty)
	unset(_out_version_cycle)

	# message(STATUS "Version (Internal): ${BLENDER_VERSION}.${BLENDER_SUBVERSION}, Version (external): ${BLENDER_VERSION}${BLENDER_VERSION_CHAR}-${BLENDER_VERSION_CYCLE}")
endmacro()


# hacks to override initial project settings
# these macros must be called directly before/after project(Blender)
macro(blender_project_hack_pre)
	# ----------------
	# MINGW HACK START
	# ignore system set flag, use our own
	# must be before project(...)
	# if the user wants to add their own its ok after first run.
	if(DEFINED CMAKE_C_STANDARD_LIBRARIES)
		set(_reset_standard_libraries OFF)
	else()
		set(_reset_standard_libraries ON)
	endif()

	# ------------------
	# GCC -O3 HACK START
	# needed because O3 can cause problems but
	# allow the builder to set O3 manually after.
	if(DEFINED CMAKE_C_FLAGS_RELEASE)
		set(_reset_standard_cflags_rel OFF)
	else()
		set(_reset_standard_cflags_rel ON)
	endif()
	if(DEFINED CMAKE_CXX_FLAGS_RELEASE)
		set(_reset_standard_cxxflags_rel OFF)
	else()
		set(_reset_standard_cxxflags_rel ON)
	endif()
endmacro()


macro(blender_project_hack_post)
	# --------------
	# MINGW HACK END
	if (_reset_standard_libraries)
		# Must come after project(...)
		#
		# MINGW workaround for -ladvapi32 being included which surprisingly causes
		# string formatting of floats, eg: printf("%.*f", 3, value). to crash blender
		# with a meaningless stack trace. by overriding this flag we ensure we only
		# have libs we define and that cmake & scons builds match.
		set(CMAKE_C_STANDARD_LIBRARIES "" CACHE STRING "" FORCE)
		set(CMAKE_CXX_STANDARD_LIBRARIES "" CACHE STRING "" FORCE)
		mark_as_advanced(
			CMAKE_C_STANDARD_LIBRARIES
			CMAKE_CXX_STANDARD_LIBRARIES
		)
	endif()
	unset(_reset_standard_libraries)


	# ----------------
	# GCC -O3 HACK END
	if(_reset_standard_cflags_rel)
		string(REGEX REPLACE "-O3" "-O2" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
		set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}" CACHE STRING "" FORCE)
		mark_as_advanced(CMAKE_C_FLAGS_RELEASE)
	endif()

	if(_reset_standard_cxxflags_rel)
		string(REGEX REPLACE "-O3" "-O2" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
		set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}" CACHE STRING "" FORCE)
		mark_as_advanced(CMAKE_CXX_FLAGS_RELEASE)
	endif()

	unset(_reset_standard_cflags_rel)
	unset(_reset_standard_cxxflags_rel)

	# ------------------------------------------------------------------
	# workaround for omission in cmake 2.8.4's GNU.cmake, fixed in 2.8.5
	if(CMAKE_COMPILER_IS_GNUCC)
		if(NOT DARWIN)
			set(CMAKE_INCLUDE_SYSTEM_FLAG_C "-isystem ")
		endif()
	endif()

endmacro()

# pair of macros to allow libraries to be specify files to install, but to
# only install them at the end so the directories don't get cleared with
# the files in them. used by cycles to install addon.
macro(delayed_install
	base
	files
	destination)

	foreach(f ${files})
		set_property(GLOBAL APPEND PROPERTY DELAYED_INSTALL_FILES ${base}/${f})
		set_property(GLOBAL APPEND PROPERTY DELAYED_INSTALL_DESTINATIONS ${destination})
	endforeach()
endmacro()

# note this is a function instead of a macro so that ${BUILD_TYPE} in targetdir
# does not get expanded in calling but is preserved
function(delayed_do_install
	targetdir)

	get_property(files GLOBAL PROPERTY DELAYED_INSTALL_FILES)
	get_property(destinations GLOBAL PROPERTY DELAYED_INSTALL_DESTINATIONS)

	if(files)
		list(LENGTH files n)
		math(EXPR n "${n}-1")

		foreach(i RANGE ${n})
			list(GET files ${i} f)
			list(GET destinations ${i} d)
			install(FILES ${f} DESTINATION ${targetdir}/${d})
		endforeach()
	endif()
endfunction()

macro(set_lib_path
		lvar
		lproj)

	
	if(MSVC10 AND EXISTS ${LIBDIR}/vc2010/${lproj})
		set(${lvar} ${LIBDIR}/vc2010/${lproj})
	else()
		set(${lvar} ${LIBDIR}/${lproj})
	endif()


endmacro()
