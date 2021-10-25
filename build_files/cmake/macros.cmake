# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# The Original Code is Copyright (C) 2006, Blender Foundation
# All rights reserved.
#
# The Original Code is: all of this file.
#
# Contributor(s): Jacques Beaurain.
#
# ***** END GPL LICENSE BLOCK *****

macro(list_insert_after
	list_id item_check item_add
	)
	set(_index)
	list(FIND "${list_id}" "${item_check}" _index)
	if("${_index}" MATCHES "-1")
		message(FATAL_ERROR "'${list_id}' doesn't contain '${item_check}'")
	endif()
	math(EXPR _index "${_index} + 1")
	list(INSERT ${list_id} "${_index}" ${item_add})
	unset(_index)
endmacro()

macro(list_insert_before
	list_id item_check item_add
	)
	set(_index)
	list(FIND "${list_id}" "${item_check}" _index)
	if("${_index}" MATCHES "-1")
		message(FATAL_ERROR "'${list_id}' doesn't contain '${item_check}'")
	endif()
	list(INSERT ${list_id} "${_index}" ${item_add})
	unset(_index)
endmacro()

function(list_assert_duplicates
	list_id
	)
	
	# message(STATUS "list data: ${list_id}")

	list(LENGTH list_id _len_before)
	list(REMOVE_DUPLICATES list_id)
	list(LENGTH list_id _len_after)
	# message(STATUS "list size ${_len_before} -> ${_len_after}")
	if(NOT _len_before EQUAL _len_after)
		message(FATAL_ERROR "duplicate found in list which should not contain duplicates: ${list_id}")
	endif()
	unset(_len_before)
	unset(_len_after)
endfunction()


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

if(UNIX AND NOT APPLE)
	macro(find_package_static)
		set(_cmake_find_library_suffixes_back ${CMAKE_FIND_LIBRARY_SUFFIXES})
		set(CMAKE_FIND_LIBRARY_SUFFIXES .a ${CMAKE_FIND_LIBRARY_SUFFIXES})
		find_package(${ARGV})
		set(CMAKE_FIND_LIBRARY_SUFFIXES ${_cmake_find_library_suffixes_back})
		unset(_cmake_find_library_suffixes_back)
	endmacro()

	macro(find_library_static)
		set(_cmake_find_library_suffixes_back ${CMAKE_FIND_LIBRARY_SUFFIXES})
		set(CMAKE_FIND_LIBRARY_SUFFIXES .a ${CMAKE_FIND_LIBRARY_SUFFIXES})
		find_library(${ARGV})
		set(CMAKE_FIND_LIBRARY_SUFFIXES ${_cmake_find_library_suffixes_back})
		unset(_cmake_find_library_suffixes_back)
	endmacro()
endif()

function(target_link_libraries_optimized
	TARGET
	LIBS
	)

	foreach(_LIB ${LIBS})
		target_link_libraries(${TARGET} optimized "${_LIB}")
	endforeach()
endfunction()

function(target_link_libraries_debug
	TARGET
	LIBS
	)

	foreach(_LIB ${LIBS})
		target_link_libraries(${TARGET} debug "${_LIB}")
	endforeach()
endfunction()

# Nicer makefiles with -I/1/foo/ instead of -I/1/2/3/../../foo/
# use it instead of include_directories()
function(blender_include_dirs
	includes
	)

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
endfunction()

function(blender_include_dirs_sys
	includes
	)

	set(_ALL_INCS "")
	foreach(_INC ${ARGV})
		get_filename_component(_ABS_INC ${_INC} ABSOLUTE)
		list(APPEND _ALL_INCS ${_ABS_INC})
		##if(NOT EXISTS "${_ABS_INC}/")
		##	message(FATAL_ERROR "Include not found: ${_ABS_INC}/")
		##endif()
	endforeach()
	include_directories(SYSTEM ${_ALL_INCS})
endfunction()

function(blender_source_group
	sources
	)

	# Group by location on disk
	source_group("Source Files" FILES CMakeLists.txt)

	foreach(_SRC ${sources})
		get_filename_component(_SRC_EXT ${_SRC} EXT)
		if((${_SRC_EXT} MATCHES ".h") OR
		   (${_SRC_EXT} MATCHES ".hpp") OR
		   (${_SRC_EXT} MATCHES ".hh"))

			set(GROUP_ID "Header Files")
		else()
			set(GROUP_ID "Source Files")
		endif()
		source_group("${GROUP_ID}" FILES ${_SRC})
	endforeach()
endfunction()


# Support per-target CMake flags
# Read from: CMAKE_C_FLAGS_**** (made upper case) when set.
#
# 'name' should alway match the target name,
# use this macro before add_library or add_executable.
#
# Optionally takes an arg passed to set(), eg PARENT_SCOPE.
macro(add_cc_flags_custom_test
	name
	)

	string(TOUPPER ${name} _name_upper)
	if(DEFINED CMAKE_C_FLAGS_${_name_upper})
		message(STATUS "Using custom CFLAGS: CMAKE_C_FLAGS_${_name_upper} in \"${CMAKE_CURRENT_SOURCE_DIR}\"")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_${_name_upper}}" ${ARGV1})
	endif()
	if(DEFINED CMAKE_CXX_FLAGS_${_name_upper})
		message(STATUS "Using custom CXXFLAGS: CMAKE_CXX_FLAGS_${_name_upper} in \"${CMAKE_CURRENT_SOURCE_DIR}\"")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${_name_upper}}" ${ARGV1})
	endif()
	unset(_name_upper)

endmacro()


# only MSVC uses SOURCE_GROUP
function(blender_add_lib__impl
	name
	sources
	includes
	includes_sys
	)

	# message(STATUS "Configuring library ${name}")

	# include_directories(${includes})
	# include_directories(SYSTEM ${includes_sys})
	blender_include_dirs("${includes}")
	blender_include_dirs_sys("${includes_sys}")

	add_library(${name} ${sources})

	# works fine without having the includes
	# listed is helpful for IDE's (QtCreator/MSVC)
	blender_source_group("${sources}")

	list_assert_duplicates("${sources}")
	list_assert_duplicates("${includes}")
	# Not for system includes because they can resolve to the same path
	# list_assert_duplicates("${includes_sys}")

endfunction()


function(blender_add_lib_nolist
	name
	sources
	includes
	includes_sys
	)

	add_cc_flags_custom_test(${name} PARENT_SCOPE)

	blender_add_lib__impl(${name} "${sources}" "${includes}" "${includes_sys}")
endfunction()

function(blender_add_lib
	name
	sources
	includes
	includes_sys
	)

	add_cc_flags_custom_test(${name} PARENT_SCOPE)

	blender_add_lib__impl(${name} "${sources}" "${includes}" "${includes_sys}")

	set_property(GLOBAL APPEND PROPERTY BLENDER_LINK_LIBS ${name})
endfunction()


function(SETUP_LIBDIRS)

	# NOTE: For all new libraries, use absolute library paths.
	# This should eventually be phased out.

	link_directories(${JPEG_LIBPATH} ${PNG_LIBPATH} ${ZLIB_LIBPATH} ${FREETYPE_LIBPATH})

	if(WITH_PYTHON)  #  AND NOT WITH_PYTHON_MODULE  # WIN32 needs
		link_directories(${PYTHON_LIBPATH})
	endif()
	if(WITH_SDL AND NOT WITH_SDL_DYNLOAD)
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
	if(WITH_OPENCOLORIO)
		link_directories(${OPENCOLORIO_LIBPATH})
	endif()
	if(WITH_OPENVDB)
		link_directories(${OPENVDB_LIBPATH})
	endif()
	if(WITH_OPENAL)
		link_directories(${OPENAL_LIBPATH})
	endif()
	if(WITH_JACK AND NOT WITH_JACK_DYNLOAD)
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
		## Never set
		# link_directories(${PCRE_LIBPATH})
		# link_directories(${EXPAT_LIBPATH})
	endif()
	if(WITH_LLVM)
		link_directories(${LLVM_LIBPATH})
	endif()

	if(WITH_ALEMBIC)
		link_directories(${ALEMBIC_LIBPATH})
		link_directories(${HDF5_LIBPATH})
	endif()

	if(WIN32 AND NOT UNIX)
		link_directories(${PTHREADS_LIBPATH})
	endif()
endfunction()

function(setup_liblinks
	target
	)

	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${PLATFORM_LINKFLAGS}" PARENT_SCOPE)
	set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} ${PLATFORM_LINKFLAGS_DEBUG}" PARENT_SCOPE)

	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${PLATFORM_LINKFLAGS}" PARENT_SCOPE)
	set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} ${PLATFORM_LINKFLAGS_DEBUG}" PARENT_SCOPE)

	set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} ${PLATFORM_LINKFLAGS}" PARENT_SCOPE)
	set(CMAKE_MODULE_LINKER_FLAGS_DEBUG "${CMAKE_MODULE_LINKER_FLAGS_DEBUG} ${PLATFORM_LINKFLAGS_DEBUG}" PARENT_SCOPE)

	target_link_libraries(
		${target}
		${PNG_LIBRARIES}
		${FREETYPE_LIBRARY}
	)

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

	if(WITH_LZO AND WITH_SYSTEM_LZO)
		target_link_libraries(${target} ${LZO_LIBRARIES})
	endif()
	if(WITH_SYSTEM_GLEW)
		target_link_libraries(${target} ${BLENDER_GLEW_LIBRARIES})
	endif()
	if(WITH_BULLET AND WITH_SYSTEM_BULLET)
		target_link_libraries(${target} ${BULLET_LIBRARIES})
	endif()
	if(WITH_AUDASPACE AND WITH_SYSTEM_AUDASPACE)
		target_link_libraries(${target} ${AUDASPACE_C_LIBRARIES} ${AUDASPACE_PY_LIBRARIES})
	endif()
	if(WITH_OPENAL)
		target_link_libraries(${target} ${OPENAL_LIBRARY})
	endif()
	if(WITH_FFTW3)
		target_link_libraries(${target} ${FFTW3_LIBRARIES})
	endif()
	if(WITH_JACK AND NOT WITH_JACK_DYNLOAD)
		target_link_libraries(${target} ${JACK_LIBRARIES})
	endif()
	if(WITH_CODEC_SNDFILE)
		target_link_libraries(${target} ${SNDFILE_LIBRARIES})
	endif()
	if(WITH_SDL AND NOT WITH_SDL_DYNLOAD)
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
	if(WITH_OPENCOLORIO)
		target_link_libraries(${target} ${OPENCOLORIO_LIBRARIES})
	endif()
	if(WITH_OPENSUBDIV OR WITH_CYCLES_OPENSUBDIV)
			target_link_libraries(${target} ${OPENSUBDIV_LIBRARIES})
	endif()
	if(WITH_OPENVDB)
		target_link_libraries(${target} ${OPENVDB_LIBRARIES} ${TBB_LIBRARIES})
	endif()
	if(WITH_CYCLES_OSL)
		target_link_libraries(${target} ${OSL_LIBRARIES})
	endif()
	if(WITH_BOOST)
		target_link_libraries(${target} ${BOOST_LIBRARIES})
		if(Boost_USE_STATIC_LIBS AND Boost_USE_ICU)
			target_link_libraries(${target} ${ICU_LIBRARIES})
		endif()
	endif()
	target_link_libraries(${target} ${JPEG_LIBRARIES})
	if(WITH_ALEMBIC)
		target_link_libraries(${target} ${ALEMBIC_LIBRARIES} ${HDF5_LIBRARIES})
	endif()
	if(WITH_IMAGE_OPENEXR)
		target_link_libraries(${target} ${OPENEXR_LIBRARIES})
	endif()
	if(WITH_IMAGE_OPENJPEG AND WITH_SYSTEM_OPENJPEG)
		target_link_libraries(${target} ${OPENJPEG_LIBRARIES})
	endif()
	if(WITH_CODEC_FFMPEG)
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
			target_link_libraries(
				${target}
				${OPENCOLLADA_LIBRARIES}
				${PCRE_LIBRARIES}
				${XML2_LIBRARIES}
				${EXPAT_LIB}
			)
		endif()
	endif()
	if(WITH_MEM_JEMALLOC)
		target_link_libraries(${target} ${JEMALLOC_LIBRARIES})
	endif()
	if(WITH_MOD_CLOTH_ELTOPO)
		target_link_libraries(${target} ${LAPACK_LIBRARIES})
	endif()
	if(WITH_LLVM)
		target_link_libraries(${target} ${LLVM_LIBRARY})
	endif()
	if(WIN32 AND NOT UNIX)
		target_link_libraries(${target} ${PTHREADS_LIBRARIES})
	endif()
	if(UNIX AND NOT APPLE)
		if(WITH_OPENMP_STATIC)
			target_link_libraries(${target} ${OpenMP_LIBRARIES})
		endif()
		if(WITH_INPUT_NDOF)
			target_link_libraries(${target} ${NDOF_LIBRARIES})
		endif()
	endif()
	if(WITH_SYSTEM_GLOG)
		target_link_libraries(${target} ${GLOG_LIBRARIES})
	endif()
	if(WITH_SYSTEM_GFLAGS)
		target_link_libraries(${target} ${GFLAGS_LIBRARIES})
	endif()

	# We put CLEW and CUEW here because OPENSUBDIV_LIBRARIES dpeends on them..
	if(WITH_CYCLES OR WITH_COMPOSITOR OR WITH_OPENSUBDIV)
		target_link_libraries(${target} "extern_clew")
		if(WITH_CUDA_DYNLOAD)
			target_link_libraries(${target} "extern_cuew")
		else()
			target_link_libraries(${target} ${CUDA_CUDA_LIBRARY})
		endif()
	endif()

	target_link_libraries(
		${target}
		${ZLIB_LIBRARIES}
	)

	#system libraries with no dependencies such as platform link libs or opengl should go last
	target_link_libraries(${target}
			${BLENDER_GL_LIBRARIES})

	#target_link_libraries(${target} ${PLATFORM_LINKLIBS} ${CMAKE_DL_LIBS})
	target_link_libraries(${target} ${PLATFORM_LINKLIBS})
endfunction()


function(SETUP_BLENDER_SORTED_LIBS)

	get_property(BLENDER_LINK_LIBS GLOBAL PROPERTY BLENDER_LINK_LIBS)

	list(APPEND BLENDER_LINK_LIBS
		bf_windowmanager
		bf_render
	)

	if(WITH_MOD_FLUID)
		list(APPEND BLENDER_LINK_LIBS bf_intern_elbeem)
	endif()

	if(WITH_CYCLES)
		list(APPEND BLENDER_LINK_LIBS
			cycles_render
			cycles_graph
			cycles_bvh
			cycles_device
			cycles_kernel
			cycles_util
			cycles_subd)
		if(WITH_CYCLES_OSL)
			list(APPEND BLENDER_LINK_LIBS cycles_kernel_osl)
		endif()
	endif()

	# Sort libraries
	set(BLENDER_SORTED_LIBS
		bf_windowmanager

		bf_editor_space_api
		bf_editor_space_action
		bf_editor_space_buttons
		bf_editor_space_console
		bf_editor_space_file
		bf_editor_space_graph
		bf_editor_space_image
		bf_editor_space_info
		bf_editor_space_logic
		bf_editor_space_nla
		bf_editor_space_node
		bf_editor_space_outliner
		bf_editor_space_script
		bf_editor_space_sequencer
		bf_editor_space_text
		bf_editor_space_time
		bf_editor_space_userpref
		bf_editor_space_view3d
		bf_editor_space_clip

		bf_editor_transform
		bf_editor_util
		bf_editor_uvedit
		bf_editor_curve
		bf_editor_gpencil
		bf_editor_interface
		bf_editor_mesh
		bf_editor_metaball
		bf_editor_object
		bf_editor_armature
		bf_editor_physics
		bf_editor_render
		bf_editor_screen
		bf_editor_sculpt_paint
		bf_editor_sound
		bf_editor_animation
		bf_editor_datafiles
		bf_editor_mask
		bf_editor_io

		bf_render
		bf_python
		bf_python_ext
		bf_python_mathutils
		bf_python_bmesh
		bf_freestyle
		bf_ikplugin
		bf_modifiers
		bf_alembic
		bf_bmesh
		bf_gpu
		bf_blenloader
		bf_blenkernel
		bf_physics
		bf_nodes
		bf_rna
		bf_imbuf
		bf_blenlib
		bf_depsgraph
		bf_intern_ghost
		bf_intern_string
		bf_avi
		bf_imbuf_cineon
		bf_imbuf_openexr
		bf_imbuf_openimageio
		bf_imbuf_dds
		bf_collada
		bf_intern_elbeem
		bf_intern_memutil
		bf_intern_guardedalloc
		bf_intern_ctr
		bf_intern_utfconv
		ge_blen_routines
		ge_converter
		ge_phys_dummy
		ge_phys_bullet
		bf_intern_smoke
		extern_lzma
		extern_curve_fit_nd
		ge_logic_ketsji
		extern_recastnavigation
		ge_logic
		ge_rasterizer
		ge_oglrasterizer
		ge_logic_expressions
		ge_scenegraph
		ge_logic_network
		ge_logic_ngnetwork
		ge_logic_loopbacknetwork
		bf_intern_moto
		extern_openjpeg
		ge_videotex
		bf_dna
		bf_blenfont
		bf_blentranslation
		bf_intern_audaspace
		bf_intern_mikktspace
		bf_intern_dualcon
		bf_intern_cycles
		cycles_render
		cycles_graph
		cycles_bvh
		cycles_device
		cycles_kernel
		cycles_util
		cycles_subd
		bf_intern_opencolorio
		bf_intern_eigen
		extern_rangetree
		extern_wcwidth
		bf_intern_libmv
		extern_sdlew

		bf_intern_glew_mx
	)

	if(NOT WITH_SYSTEM_GLOG)
		list(APPEND BLENDER_SORTED_LIBS extern_glog)
	endif()

	if(NOT WITH_SYSTEM_GFLAGS)
		list(APPEND BLENDER_SORTED_LIBS extern_gflags)
	endif()

	if(WITH_COMPOSITOR)
		# added for opencl compositor
		list_insert_before(BLENDER_SORTED_LIBS "bf_blenkernel" "bf_compositor")
		list_insert_after(BLENDER_SORTED_LIBS "bf_compositor" "bf_intern_opencl")
	endif()

	if(WITH_LIBMV)
		list(APPEND BLENDER_SORTED_LIBS extern_ceres)
	endif()

	if(WITH_MOD_CLOTH_ELTOPO)
		list(APPEND BLENDER_SORTED_LIBS extern_eltopo)
	endif()

	if(NOT WITH_SYSTEM_LZO)
		list(APPEND BLENDER_SORTED_LIBS extern_minilzo)
	endif()

	if(NOT WITH_SYSTEM_GLEW)
		list(APPEND BLENDER_SORTED_LIBS ${BLENDER_GLEW_LIBRARIES})
	endif()

	if(WITH_BINRELOC)
		list(APPEND BLENDER_SORTED_LIBS extern_binreloc)
	endif()

	if(WITH_CXX_GUARDEDALLOC)
		list(APPEND BLENDER_SORTED_LIBS bf_intern_guardedalloc_cpp)
	endif()

	if(WITH_IK_SOLVER)
		list_insert_after(BLENDER_SORTED_LIBS "bf_intern_elbeem" "bf_intern_iksolver")
	endif()

	if(WITH_IK_ITASC)
		list(APPEND BLENDER_SORTED_LIBS bf_intern_itasc)
	endif()

	if(WITH_CODEC_QUICKTIME)
		list(APPEND BLENDER_SORTED_LIBS bf_quicktime)
	endif()

	if(WITH_MOD_BOOLEAN)
		list(APPEND BLENDER_SORTED_LIBS extern_carve)
	endif()

	if(WITH_GHOST_XDND)
		list(APPEND BLENDER_SORTED_LIBS extern_xdnd)
	endif()

	if(WITH_CYCLES_OSL)
		list_insert_after(BLENDER_SORTED_LIBS "cycles_kernel" "cycles_kernel_osl")
	endif()

	if(WITH_INTERNATIONAL)
		list(APPEND BLENDER_SORTED_LIBS bf_intern_locale)
	endif()

	if(WITH_BULLET)
		list_insert_after(BLENDER_SORTED_LIBS "bf_blenkernel" "bf_intern_rigidbody")
	endif()

	if(WITH_BULLET AND NOT WITH_SYSTEM_BULLET)
		list_insert_after(BLENDER_SORTED_LIBS "ge_logic_ngnetwork" "extern_bullet")
	endif()

	if(WITH_GAMEENGINE_DECKLINK)
		list(APPEND BLENDER_SORTED_LIBS bf_intern_decklink)
	endif()

	if(WIN32)
		list(APPEND BLENDER_SORTED_LIBS bf_intern_gpudirect)
	endif()

	if(WITH_OPENSUBDIV OR WITH_CYCLES_OPENSUBDIV)
		list(APPEND BLENDER_SORTED_LIBS bf_intern_opensubdiv)
	endif()

	if(WITH_OPENVDB)
		list(APPEND BLENDER_SORTED_LIBS bf_intern_openvdb)
	endif()

	foreach(SORTLIB ${BLENDER_SORTED_LIBS})
		set(REMLIB ${SORTLIB})
		foreach(SEARCHLIB ${BLENDER_LINK_LIBS})
			if(${SEARCHLIB} STREQUAL ${SORTLIB})
				set(REMLIB "")
			endif()
		endforeach()
		if(REMLIB)
			# message(STATUS "Removing library ${REMLIB} from blender linking because: not configured")
			list(APPEND REM_MSG ${REMLIB})
			list(REMOVE_ITEM BLENDER_SORTED_LIBS ${REMLIB})
		endif()
	endforeach()
	if(REM_MSG)
		list(SORT REM_MSG)
		message(STATUS "Blender Skipping: (${REM_MSG})")
	endif()


	set(BLENDER_SORTED_LIBS ${BLENDER_SORTED_LIBS} PARENT_SCOPE)

	# for top-level tests
	set_property(GLOBAL PROPERTY BLENDER_SORTED_LIBS_PROP ${BLENDER_SORTED_LIBS})
endfunction()

macro(TEST_SSE_SUPPORT
	_sse_flags
	_sse2_flags)

	include(CheckCSourceRuns)

	# message(STATUS "Detecting SSE support")
	if(CMAKE_COMPILER_IS_GNUCC OR (CMAKE_C_COMPILER_ID MATCHES "Clang"))
		set(${_sse_flags} "-msse")
		set(${_sse2_flags} "-msse2")
	elseif(MSVC)
		# x86_64 has this auto enabled
		if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
			set(${_sse_flags} "")
			set(${_sse2_flags} "")
		else()
			set(${_sse_flags} "/arch:SSE")
			set(${_sse2_flags} "/arch:SSE2")
		endif()
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

# Only print message if running CMake first time
macro(message_first_run)
	if(FIRST_RUN)
		message(${ARGV})
	endif()
endmacro()

macro(TEST_UNORDERED_MAP_SUPPORT)
	# - Detect unordered_map availability
	# Test if a valid implementation of unordered_map exists
	# and define the include path
	# This module defines
	#  HAVE_UNORDERED_MAP, whether unordered_map implementation was found
	#
	#  HAVE_STD_UNORDERED_MAP_HEADER, <unordered_map.h> was found
	#  HAVE_UNORDERED_MAP_IN_STD_NAMESPACE, unordered_map is in namespace std
	#  HAVE_UNORDERED_MAP_IN_TR1_NAMESPACE, unordered_map is in namespace std::tr1
	#
	#  UNORDERED_MAP_INCLUDE_PREFIX, include path prefix for unordered_map, if found
	#  UNORDERED_MAP_NAMESPACE, namespace for unordered_map, if found

	include(CheckIncludeFileCXX)

	# Workaround for newer GCC (6.x+) where C++11 was enabled by default, which lead us
	# to a situation when there is <unordered_map> include but which can't be used uless
	# C++11 is enabled.
	if(CMAKE_COMPILER_IS_GNUCC AND (NOT "${CMAKE_C_COMPILER_VERSION}" VERSION_LESS "6.0") AND (NOT WITH_CXX11))
		set(HAVE_STD_UNORDERED_MAP_HEADER False)
	else()
		CHECK_INCLUDE_FILE_CXX("unordered_map" HAVE_STD_UNORDERED_MAP_HEADER)
	endif()
	if(HAVE_STD_UNORDERED_MAP_HEADER)
		# Even so we've found unordered_map header file it doesn't
		# mean unordered_map and unordered_set will be declared in
		# std namespace.
		#
		# Namely, MSVC 2008 have unordered_map header which declares
		# unordered_map class in std::tr1 namespace. In order to support
		# this, we do extra check to see which exactly namespace is
		# to be used.

		include(CheckCXXSourceCompiles)
		CHECK_CXX_SOURCE_COMPILES("#include <unordered_map>
		                          int main() {
		                            std::unordered_map<int, int> map;
		                            return 0;
		                          }"
		                          HAVE_UNORDERED_MAP_IN_STD_NAMESPACE)
		if(HAVE_UNORDERED_MAP_IN_STD_NAMESPACE)
			message_first_run(STATUS "Found unordered_map/set in std namespace.")

			set(HAVE_UNORDERED_MAP "TRUE")
			set(UNORDERED_MAP_INCLUDE_PREFIX "")
			set(UNORDERED_MAP_NAMESPACE "std")
		else()
			CHECK_CXX_SOURCE_COMPILES("#include <unordered_map>
			                          int main() {
			                            std::tr1::unordered_map<int, int> map;
			                            return 0;
			                          }"
			                          HAVE_UNORDERED_MAP_IN_TR1_NAMESPACE)
			if(HAVE_UNORDERED_MAP_IN_TR1_NAMESPACE)
				message_first_run(STATUS "Found unordered_map/set in std::tr1 namespace.")

				set(HAVE_UNORDERED_MAP "TRUE")
				set(UNORDERED_MAP_INCLUDE_PREFIX "")
				set(UNORDERED_MAP_NAMESPACE "std::tr1")
			else()
				message_first_run(STATUS "Found <unordered_map> but cannot find either std::unordered_map "
				                  "or std::tr1::unordered_map.")
			endif()
		endif()
	else()
		CHECK_INCLUDE_FILE_CXX("tr1/unordered_map" HAVE_UNORDERED_MAP_IN_TR1_NAMESPACE)
		if(HAVE_UNORDERED_MAP_IN_TR1_NAMESPACE)
			message_first_run(STATUS "Found unordered_map/set in std::tr1 namespace.")

			set(HAVE_UNORDERED_MAP "TRUE")
			set(UNORDERED_MAP_INCLUDE_PREFIX "tr1")
			set(UNORDERED_MAP_NAMESPACE "std::tr1")
		else()
			message_first_run(STATUS "Unable to find <unordered_map> or <tr1/unordered_map>. ")
		endif()
	endif()
endmacro()

macro(TEST_SHARED_PTR_SUPPORT)
	# This check are coming from Ceres library.
	#
	# Find shared pointer header and namespace.
	#
	# This module defines the following variables:
	#
	# SHARED_PTR_FOUND: TRUE if shared_ptr found.
	# SHARED_PTR_TR1_MEMORY_HEADER: True if <tr1/memory> header is to be used
	# for the shared_ptr object, otherwise use <memory>.
	# SHARED_PTR_TR1_NAMESPACE: TRUE if shared_ptr is defined in std::tr1 namespace,
	# otherwise it's assumed to be defined in std namespace.

	include(CheckIncludeFileCXX)
	include(CheckCXXSourceCompiles)
	set(SHARED_PTR_FOUND FALSE)
	# Workaround for newer GCC (6.x+) where C++11 was enabled by default, which lead us
	# to a situation when there is <unordered_map> include but which can't be used uless
	# C++11 is enabled.
	if(CMAKE_COMPILER_IS_GNUCC AND (NOT "${CMAKE_C_COMPILER_VERSION}" VERSION_LESS "6.0") AND (NOT WITH_CXX11))
		set(HAVE_STD_MEMORY_HEADER False)
	else()
		CHECK_INCLUDE_FILE_CXX(memory HAVE_STD_MEMORY_HEADER)
	endif()
	if(HAVE_STD_MEMORY_HEADER)
		# Finding the memory header doesn't mean that shared_ptr is in std
		# namespace.
		#
		# In particular, MSVC 2008 has shared_ptr declared in std::tr1.  In
		# order to support this, we do an extra check to see which namespace
		# should be used.
		CHECK_CXX_SOURCE_COMPILES("#include <memory>
		                           int main() {
		                             std::shared_ptr<int> int_ptr;
		                             return 0;
		                           }"
		                          HAVE_SHARED_PTR_IN_STD_NAMESPACE)

		if(HAVE_SHARED_PTR_IN_STD_NAMESPACE)
			message_first_run("-- Found shared_ptr in std namespace using <memory> header.")
			set(SHARED_PTR_FOUND TRUE)
		else()
			CHECK_CXX_SOURCE_COMPILES("#include <memory>
			                           int main() {
			                           std::tr1::shared_ptr<int> int_ptr;
			                           return 0;
			                           }"
			                          HAVE_SHARED_PTR_IN_TR1_NAMESPACE)
			if(HAVE_SHARED_PTR_IN_TR1_NAMESPACE)
				message_first_run("-- Found shared_ptr in std::tr1 namespace using <memory> header.")
				set(SHARED_PTR_TR1_NAMESPACE TRUE)
				set(SHARED_PTR_FOUND TRUE)
			endif()
		endif()
	endif()

	if(NOT SHARED_PTR_FOUND)
		# Further, gcc defines shared_ptr in std::tr1 namespace and
		# <tr1/memory> is to be included for this. And what makes things
		# even more tricky is that gcc does have <memory> header, so
		# all the checks above wouldn't find shared_ptr.
		CHECK_INCLUDE_FILE_CXX("tr1/memory" HAVE_TR1_MEMORY_HEADER)
		if(HAVE_TR1_MEMORY_HEADER)
			CHECK_CXX_SOURCE_COMPILES("#include <tr1/memory>
			                           int main() {
			                           std::tr1::shared_ptr<int> int_ptr;
			                           return 0;
			                           }"
			                           HAVE_SHARED_PTR_IN_TR1_NAMESPACE_FROM_TR1_MEMORY_HEADER)
			if(HAVE_SHARED_PTR_IN_TR1_NAMESPACE_FROM_TR1_MEMORY_HEADER)
				message_first_run("-- Found shared_ptr in std::tr1 namespace using <tr1/memory> header.")
				set(SHARED_PTR_TR1_MEMORY_HEADER TRUE)
				set(SHARED_PTR_TR1_NAMESPACE TRUE)
				set(SHARED_PTR_FOUND TRUE)
			endif()
		endif()
	endif()
endmacro()

# when we have warnings as errors applied globally this
# needs to be removed for some external libs which we dont maintain.

# utility macro
macro(remove_cc_flag
	_flag)

	foreach(flag ${ARGV})
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
	endforeach()
	unset(flag)

endmacro()

macro(add_cc_flag
	flag)

	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${flag}")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}")
endmacro()

macro(remove_strict_flags)

	if(CMAKE_COMPILER_IS_GNUCC)
		remove_cc_flag(
			"-Wstrict-prototypes"
			"-Wmissing-prototypes"
			"-Wmissing-declarations"
			"-Wmissing-format-attribute"
			"-Wunused-local-typedefs"
			"-Wunused-macros"
			"-Wunused-parameter"
			"-Wwrite-strings"
			"-Wredundant-decls"
			"-Wundef"
			"-Wshadow"
			"-Wdouble-promotion"
			"-Wold-style-definition"
			"-Werror=[^ ]+"
			"-Werror"
		)

		# negate flags implied by '-Wall'
		add_cc_flag("${CC_REMOVE_STRICT_FLAGS}")
	endif()

	if(CMAKE_C_COMPILER_ID MATCHES "Clang")
		remove_cc_flag(
			"-Wunused-parameter"
			"-Wunused-variable"
			"-Werror=[^ ]+"
			"-Werror"
		)

		# negate flags implied by '-Wall'
		add_cc_flag("${CC_REMOVE_STRICT_FLAGS}")
	endif()

	if(MSVC)
		# TODO
	endif()

endmacro()

macro(remove_extra_strict_flags)
	if(CMAKE_COMPILER_IS_GNUCC)
		remove_cc_flag(
			"-Wunused-parameter"
		)
	endif()

	if(CMAKE_C_COMPILER_ID MATCHES "Clang")
		remove_cc_flag(
			"-Wunused-parameter"
		)
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

		if(CMAKE_COMPILER_IS_GNUCC OR
		  (CMAKE_C_COMPILER_ID MATCHES "Clang"))

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

# External libs may need 'signed char' to be default.
macro(remove_cc_flag_unsigned_char)
	if(CMAKE_C_COMPILER_ID MATCHES "^(GNU|Clang|Intel)$")
		remove_cc_flag("-funsigned-char")
	elseif(MSVC)
		remove_cc_flag("/J")
	else()
		message(WARNING
			"Compiler '${CMAKE_C_COMPILER_ID}' failed to disable 'unsigned char' flag."
			"Build files need updating."
		)
	endif()
endmacro()

function(ADD_CHECK_C_COMPILER_FLAG
	_CFLAGS
	_CACHE_VAR
	_FLAG
	)

	include(CheckCCompilerFlag)

	CHECK_C_COMPILER_FLAG("${_FLAG}" "${_CACHE_VAR}")
	if(${_CACHE_VAR})
		# message(STATUS "Using CFLAG: ${_FLAG}")
		set(${_CFLAGS} "${${_CFLAGS}} ${_FLAG}" PARENT_SCOPE)
	else()
		message(STATUS "Unsupported CFLAG: ${_FLAG}")
	endif()
endfunction()

function(ADD_CHECK_CXX_COMPILER_FLAG
	_CXXFLAGS
	_CACHE_VAR
	_FLAG
	)

	include(CheckCXXCompilerFlag)

	CHECK_CXX_COMPILER_FLAG("${_FLAG}" "${_CACHE_VAR}")
	if(${_CACHE_VAR})
		# message(STATUS "Using CXXFLAG: ${_FLAG}")
		set(${_CXXFLAGS} "${${_CXXFLAGS}} ${_FLAG}" PARENT_SCOPE)
	else()
		message(STATUS "Unsupported CXXFLAG: ${_FLAG}")
	endif()
endfunction()

function(get_blender_version)
	# extracts header vars and defines them in the parent scope:
	#
	# - BLENDER_VERSION (major.minor)
	# - BLENDER_VERSION_MAJOR
	# - BLENDER_VERSION_MINOR
	# - BLENDER_SUBVERSION (used for internal versioning mainly)
	# - BLENDER_VERSION_CHAR (a, b, c, ...or empty string)
	# - BLENDER_VERSION_CYCLE (alpha, beta, rc, release)

	# So cmake depends on BKE_blender.h, beware of inf-loops!
	CONFIGURE_FILE(${CMAKE_SOURCE_DIR}/source/blender/blenkernel/BKE_blender_version.h
	               ${CMAKE_BINARY_DIR}/source/blender/blenkernel/BKE_blender_version.h.done)

	file(STRINGS ${CMAKE_SOURCE_DIR}/source/blender/blenkernel/BKE_blender_version.h _contents REGEX "^#define[ \t]+BLENDER_.*$")

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

	math(EXPR _out_version_major "${_out_version} / 100")
	math(EXPR _out_version_minor "${_out_version} % 100")

	# for packaging, alpha to numbers
	string(COMPARE EQUAL "${_out_version_char}" "" _out_version_char_empty)
	if(${_out_version_char_empty})
		set(_out_version_char_index "0")
	else()
		set(_char_ls a b c d e f g h i j k l m n o p q r s t u v w x y z)
		list(FIND _char_ls ${_out_version_char} _out_version_char_index)
		math(EXPR _out_version_char_index "${_out_version_char_index} + 1")
	endif()

	# output vars
	set(BLENDER_VERSION "${_out_version_major}.${_out_version_minor}" PARENT_SCOPE)
	set(BLENDER_VERSION_MAJOR "${_out_version_major}" PARENT_SCOPE)
	set(BLENDER_VERSION_MINOR "${_out_version_minor}" PARENT_SCOPE)
	set(BLENDER_SUBVERSION "${_out_subversion}" PARENT_SCOPE)
	set(BLENDER_VERSION_CHAR "${_out_version_char}" PARENT_SCOPE)
	set(BLENDER_VERSION_CHAR_INDEX "${_out_version_char_index}" PARENT_SCOPE)
	set(BLENDER_VERSION_CYCLE "${_out_version_cycle}" PARENT_SCOPE)

endfunction()


# hacks to override initial project settings
# these macros must be called directly before/after project(Blender)
macro(blender_project_hack_pre)
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
function(delayed_install
	base
	files
	destination)

	foreach(f ${files})
		if(IS_ABSOLUTE ${f})
			set_property(GLOBAL APPEND PROPERTY DELAYED_INSTALL_FILES ${f})
		else()
			set_property(GLOBAL APPEND PROPERTY DELAYED_INSTALL_FILES ${base}/${f})
		endif()
		set_property(GLOBAL APPEND PROPERTY DELAYED_INSTALL_DESTINATIONS ${destination})
	endforeach()
endfunction()

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


function(data_to_c
	file_from file_to
	list_to_add
	)

	list(APPEND ${list_to_add} ${file_to})
	set(${list_to_add} ${${list_to_add}} PARENT_SCOPE)

	get_filename_component(_file_to_path ${file_to} PATH)

	add_custom_command(
		OUTPUT ${file_to}
		COMMAND ${CMAKE_COMMAND} -E make_directory ${_file_to_path}
		COMMAND "$<TARGET_FILE:datatoc>" ${file_from} ${file_to}
		DEPENDS ${file_from} datatoc)

	set_source_files_properties(${file_to} PROPERTIES GENERATED TRUE)
endfunction()


# same as above but generates the var name and output automatic.
function(data_to_c_simple
	file_from
	list_to_add
	)

	# remove ../'s
	get_filename_component(_file_from ${CMAKE_CURRENT_SOURCE_DIR}/${file_from}   REALPATH)
	get_filename_component(_file_to   ${CMAKE_CURRENT_BINARY_DIR}/${file_from}.c REALPATH)

	list(APPEND ${list_to_add} ${_file_to})
	set(${list_to_add} ${${list_to_add}} PARENT_SCOPE)

	get_filename_component(_file_to_path ${_file_to} PATH)

	add_custom_command(
		OUTPUT  ${_file_to}
		COMMAND ${CMAKE_COMMAND} -E make_directory ${_file_to_path}
		COMMAND "$<TARGET_FILE:datatoc>" ${_file_from} ${_file_to}
		DEPENDS ${_file_from} datatoc)

	set_source_files_properties(${_file_to} PROPERTIES GENERATED TRUE)
endfunction()

# macro for converting pixmap directory to a png and then a c file
function(data_to_c_simple_icons
	path_from
	list_to_add
	)

	# Conversion steps
	#  path_from  ->  _file_from  ->  _file_to
	#  foo/*.dat  ->  foo.png     ->  foo.png.c

	get_filename_component(_path_from_abs ${path_from} ABSOLUTE)
	# remove ../'s
	get_filename_component(_file_from ${CMAKE_CURRENT_BINARY_DIR}/${path_from}.png   REALPATH)
	get_filename_component(_file_to   ${CMAKE_CURRENT_BINARY_DIR}/${path_from}.png.c REALPATH)

	list(APPEND ${list_to_add} ${_file_to})
	set(${list_to_add} ${${list_to_add}} PARENT_SCOPE)

	get_filename_component(_file_to_path ${_file_to} PATH)

	# ideally we wouldn't glob, but storing all names for all pixmaps is a bit heavy
	file(GLOB _icon_files "${path_from}/*.dat")

	add_custom_command(
		OUTPUT  ${_file_from} ${_file_to}
		COMMAND ${CMAKE_COMMAND} -E make_directory ${_file_to_path}
		#COMMAND python3 ${CMAKE_SOURCE_DIR}/source/blender/datatoc/datatoc_icon.py ${_path_from_abs} ${_file_from}
		COMMAND "$<TARGET_FILE:datatoc_icon>" ${_path_from_abs} ${_file_from}
		COMMAND "$<TARGET_FILE:datatoc>" ${_file_from} ${_file_to}
		DEPENDS
			${_icon_files}
			datatoc_icon
			datatoc
			# could be an arg but for now we only create icons depending on UI_icons.h
			${CMAKE_SOURCE_DIR}/source/blender/editors/include/UI_icons.h
		)

	set_source_files_properties(${_file_from} ${_file_to} PROPERTIES GENERATED TRUE)
endfunction()

# XXX Not used for now...
function(svg_to_png
	file_from
	file_to
	dpi
	list_to_add
	)

	# remove ../'s
	get_filename_component(_file_from ${CMAKE_CURRENT_SOURCE_DIR}/${file_from} REALPATH)
	get_filename_component(_file_to   ${CMAKE_CURRENT_SOURCE_DIR}/${file_to}   REALPATH)

	list(APPEND ${list_to_add} ${_file_to})
	set(${list_to_add} ${${list_to_add}} PARENT_SCOPE)

	find_program(INKSCAPE_EXE inkscape)
	mark_as_advanced(INKSCAPE_EXE)

	if(INKSCAPE_EXE)
		if(APPLE)
			# in OS X app bundle, the binary is a shim that doesn't take any
			# command line arguments, replace it with the actual binary
			string(REPLACE "MacOS/Inkscape" "Resources/bin/inkscape" INKSCAPE_REAL_EXE ${INKSCAPE_EXE})
			if(EXISTS "${INKSCAPE_REAL_EXE}")
				set(INKSCAPE_EXE ${INKSCAPE_REAL_EXE})
			endif()
		endif()

		add_custom_command(
			OUTPUT  ${_file_to}
			COMMAND ${INKSCAPE_EXE} ${_file_from} --export-dpi=${dpi}  --without-gui --export-png=${_file_to}
			DEPENDS ${_file_from} ${INKSCAPE_EXE}
		)
	else()
		message(WARNING "Inkscape not found, could not re-generate ${_file_to} from ${_file_from}!")
	endif()
endfunction()

function(msgfmt_simple
	file_from
	list_to_add
	)

	# remove ../'s
	get_filename_component(_file_from_we ${file_from} NAME_WE)

	get_filename_component(_file_from ${file_from} REALPATH)
	get_filename_component(_file_to ${CMAKE_CURRENT_BINARY_DIR}/${_file_from_we}.mo REALPATH)

	list(APPEND ${list_to_add} ${_file_to})
	set(${list_to_add} ${${list_to_add}} PARENT_SCOPE)

	get_filename_component(_file_to_path ${_file_to} PATH)

	add_custom_command(
		OUTPUT  ${_file_to}
		COMMAND ${CMAKE_COMMAND} -E make_directory ${_file_to_path}
		COMMAND "$<TARGET_FILE:msgfmt>" ${_file_from} ${_file_to}
		DEPENDS msgfmt ${_file_from})

	set_source_files_properties(${_file_to} PROPERTIES GENERATED TRUE)
endfunction()

function(find_python_package
	package
	)

	string(TOUPPER ${package} _upper_package)

	# set but invalid
	if((NOT ${PYTHON_${_upper_package}_PATH} STREQUAL "") AND
	   (NOT ${PYTHON_${_upper_package}_PATH} MATCHES NOTFOUND))
#		if(NOT EXISTS "${PYTHON_${_upper_package}_PATH}/${package}")
#			message(WARNING "PYTHON_${_upper_package}_PATH is invalid, ${package} not found in '${PYTHON_${_upper_package}_PATH}' "
#			                "WITH_PYTHON_INSTALL_${_upper_package} option will be ignored when installing python")
#			set(WITH_PYTHON_INSTALL${_upper_package} OFF)
#		endif()
	# not set, so initialize
	else()
		string(REPLACE "." ";" _PY_VER_SPLIT "${PYTHON_VERSION}")
		list(GET _PY_VER_SPLIT 0 _PY_VER_MAJOR)

		# re-cache
		unset(PYTHON_${_upper_package}_PATH CACHE)
		find_path(PYTHON_${_upper_package}_PATH
		  NAMES
		    ${package}
		  HINTS
		    "${PYTHON_LIBPATH}/python${PYTHON_VERSION}/"
		    "${PYTHON_LIBPATH}/python${_PY_VER_MAJOR}/"
		  PATH_SUFFIXES
		    site-packages
		    dist-packages
		   NO_DEFAULT_PATH
		)

		 if(NOT EXISTS "${PYTHON_${_upper_package}_PATH}")
			message(WARNING
				"Python package '${package}' path could not be found in:\n"
				"'${PYTHON_LIBPATH}/python${PYTHON_VERSION}/site-packages/${package}', "
				"'${PYTHON_LIBPATH}/python${_PY_VER_MAJOR}/site-packages/${package}', "
				"'${PYTHON_LIBPATH}/python${PYTHON_VERSION}/dist-packages/${package}', "
				"'${PYTHON_LIBPATH}/python${_PY_VER_MAJOR}/dist-packages/${package}', "
				"\n"
				"The 'WITH_PYTHON_INSTALL_${_upper_package}' option will be ignored when installing Python.\n"
				"The build will be usable, only add-ons that depend on this package won't be functional."
			)
			set(WITH_PYTHON_INSTALL_${_upper_package} OFF PARENT_SCOPE)
		else()
			message(STATUS "${package} found at '${PYTHON_${_upper_package}_PATH}'")
		endif()
	endif()
endfunction()

# like Python's 'print(dir())'
function(print_all_vars)
	get_cmake_property(_vars VARIABLES)
	foreach(_var ${_vars})
		message("${_var}=${${_var}}")
	endforeach()
endfunction()

macro(openmp_delayload
	projectname
	)
		if(MSVC)
			if(WITH_OPENMP)
				if(MSVC_VERSION EQUAL 1800)
					set(OPENMP_DLL_NAME "vcomp120")
				else()
					set(OPENMP_DLL_NAME "vcomp140")
				endif()
				SET_TARGET_PROPERTIES(${projectname} PROPERTIES LINK_FLAGS_RELEASE "/DELAYLOAD:${OPENMP_DLL_NAME}.dll delayimp.lib")
				SET_TARGET_PROPERTIES(${projectname} PROPERTIES LINK_FLAGS_DEBUG "/DELAYLOAD:${OPENMP_DLL_NAME}d.dll delayimp.lib")
				SET_TARGET_PROPERTIES(${projectname} PROPERTIES LINK_FLAGS_RELWITHDEBINFO "/DELAYLOAD:${OPENMP_DLL_NAME}.dll delayimp.lib")
				SET_TARGET_PROPERTIES(${projectname} PROPERTIES LINK_FLAGS_MINSIZEREL "/DELAYLOAD:${OPENMP_DLL_NAME}.dll delayimp.lib")
			endif(WITH_OPENMP)
		endif(MSVC)
endmacro()

MACRO(WINDOWS_SIGN_TARGET target)
	if(WITH_WINDOWS_CODESIGN)
		if(!SIGNTOOL_EXE)
			error("Codesigning is enabled, but signtool is not found")
		else()
			if(WINDOWS_CODESIGN_PFX_PASSWORD)
				set(CODESIGNPASSWORD /p ${WINDOWS_CODESIGN_PFX_PASSWORD})
			else()
				if($ENV{PFXPASSWORD})
					set(CODESIGNPASSWORD /p $ENV{PFXPASSWORD})
				else()
					message(FATAL_ERROR "WITH_WINDOWS_CODESIGN is on but WINDOWS_CODESIGN_PFX_PASSWORD not set, and environment variable PFXPASSWORD not found, unable to sign code.")
				endif()
			endif()
			add_custom_command(TARGET ${target}
				POST_BUILD
				COMMAND ${SIGNTOOL_EXE} sign /f ${WINDOWS_CODESIGN_PFX} ${CODESIGNPASSWORD} $<TARGET_FILE:${target}>
				VERBATIM
			)
		endif()
	endif()
ENDMACRO()
