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

# Adds a native path separator to the end of the path:
#
# - 'example' -> 'example/'
# - '/example///' -> '/example/'
#
macro(path_ensure_trailing_slash
  path_new path_input
  )
  file(TO_NATIVE_PATH "/" _path_sep)
  string(REGEX REPLACE "[${_path_sep}]+$" "" ${path_new} ${path_input})
  set(${path_new} "${${path_new}}${_path_sep}")
  unset(_path_sep)
endmacro()

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
    target_link_libraries(${TARGET} INTERFACE optimized "${_LIB}")
  endforeach()
endfunction()

function(target_link_libraries_debug
  TARGET
  LIBS
  )

  foreach(_LIB ${LIBS})
    target_link_libraries(${TARGET} INTERFACE debug "${_LIB}")
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
    # if(NOT EXISTS "${_ABS_INC}/")
    #   message(FATAL_ERROR "Include not found: ${_ABS_INC}/")
    # endif()
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
    # if(NOT EXISTS "${_ABS_INC}/")
    #   message(FATAL_ERROR "Include not found: ${_ABS_INC}/")
    # endif()
  endforeach()
  include_directories(SYSTEM ${_ALL_INCS})
endfunction()

# Set include paths for header files included with "*.h" syntax.
# This enables auto-complete suggestions for user header files on Xcode.
# Build process is not affected since the include paths are the same
# as in HEADER_SEARCH_PATHS.
function(blender_user_header_search_paths
  name
  includes
  )

  if(XCODE)
    set(_ALL_INCS "")
    foreach(_INC ${includes})
      get_filename_component(_ABS_INC ${_INC} ABSOLUTE)
      # _ALL_INCS is a space-separated string of file paths in quotes.
      string(APPEND _ALL_INCS " \"${_ABS_INC}\"")
    endforeach()
    set_target_properties(${name} PROPERTIES XCODE_ATTRIBUTE_USER_HEADER_SEARCH_PATHS "${_ALL_INCS}")
  endif()
endfunction()

function(blender_source_group
  name
  sources
  )

  # if enabled, use the sources directories as filters.
  if(WINDOWS_USE_VISUAL_STUDIO_SOURCE_FOLDERS)
    foreach(_SRC ${sources})
      # remove ../'s
      get_filename_component(_SRC_DIR ${_SRC} REALPATH)
      get_filename_component(_SRC_DIR ${_SRC_DIR} DIRECTORY)
      string(FIND ${_SRC_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/" _pos)
      if(NOT _pos EQUAL -1)
        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" GROUP_ID ${_SRC_DIR})
        string(REPLACE "/" "\\" GROUP_ID ${GROUP_ID})
        source_group("${GROUP_ID}" FILES ${_SRC})
      endif()
      unset(_pos)
    endforeach()
  else()
    # Group by location on disk
    source_group("Source Files" FILES CMakeLists.txt)
    foreach(_SRC ${sources})
      get_filename_component(_SRC_EXT ${_SRC} EXT)
      if((${_SRC_EXT} MATCHES ".h") OR
         (${_SRC_EXT} MATCHES ".hpp") OR
         (${_SRC_EXT} MATCHES ".hh"))

        set(GROUP_ID "Header Files")
      elseif(${_SRC_EXT} MATCHES ".glsl$")
        set(GROUP_ID "Shaders")
      else()
        set(GROUP_ID "Source Files")
      endif()
      source_group("${GROUP_ID}" FILES ${_SRC})
    endforeach()
  endif()

  # if enabled, set the FOLDER property for visual studio projects
  if(WINDOWS_USE_VISUAL_STUDIO_PROJECT_FOLDERS)
    get_filename_component(FolderDir ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY)
    string(REPLACE ${CMAKE_SOURCE_DIR} "" FolderDir ${FolderDir})
    set_target_properties(${name} PROPERTIES FOLDER ${FolderDir})
  endif()
endfunction()


# Support per-target CMake flags
# Read from: CMAKE_C_FLAGS_**** (made upper case) when set.
#
# 'name' should always match the target name,
# use this macro before add_library or add_executable.
#
# Optionally takes an arg passed to set(), eg PARENT_SCOPE.
macro(add_cc_flags_custom_test
  name
  )

  string(TOUPPER ${name} _name_upper)
  if(DEFINED CMAKE_C_FLAGS_${_name_upper})
    message(STATUS "Using custom CFLAGS: CMAKE_C_FLAGS_${_name_upper} in \"${CMAKE_CURRENT_SOURCE_DIR}\"")
    string(APPEND CMAKE_C_FLAGS " ${CMAKE_C_FLAGS_${_name_upper}}" ${ARGV1})
  endif()
  if(DEFINED CMAKE_CXX_FLAGS_${_name_upper})
    message(STATUS "Using custom CXXFLAGS: CMAKE_CXX_FLAGS_${_name_upper} in \"${CMAKE_CURRENT_SOURCE_DIR}\"")
    string(APPEND CMAKE_CXX_FLAGS " ${CMAKE_CXX_FLAGS_${_name_upper}}" ${ARGV1})
  endif()
  unset(_name_upper)

endmacro()


# only MSVC uses SOURCE_GROUP
function(blender_add_lib__impl
  name
  sources
  includes
  includes_sys
  library_deps
  )

  # message(STATUS "Configuring library ${name}")

  # include_directories(${includes})
  # include_directories(SYSTEM ${includes_sys})
  blender_include_dirs("${includes}")
  blender_include_dirs_sys("${includes_sys}")

  add_library(${name} ${sources})

  # On Windows certain libraries have two sets of binaries: one for debug builds and one for
  # release builds. The root of this requirement goes into ABI, I believe, but that's outside
  # of a scope of this comment.
  #
  # CMake have a native way of dealing with this, which is specifying what build type the
  # libraries are provided for:
  #
  #   target_link_libraries(tagret optimized|debug|general <libraries>)
  #
  # The build type is to be provided as a separate argument to the function.
  #
  # CMake's variables for libraries will contain build type in such cases. For example:
  #
  #   set(FOO_LIBRARIES optimized libfoo.lib debug libfoo_d.lib)
  #
  # Complications starts with a single argument for library_deps: all the elements are being
  # put to a list: "${FOO_LIBRARIES}" will become "optimized;libfoo.lib;debug;libfoo_d.lib".
  # This makes it impossible to pass it as-is to target_link_libraries sine it will treat
  # this argument as a list of libraries to be linked against, causing missing libraries
  # for optimized.lib.
  #
  # What this code does it traverses library_deps and extracts information about whether
  # library is to provided as general, debug or optimized. This is a little state machine which
  # keeps track of which build type library is to provided for:
  #
  # - If "debug" or "optimized" word is found, the next element in the list is expected to be
  #   a library which will be passed to target_link_libraries() under corresponding build type.
  #
  # - If there is no "debug" or "optimized" used library is specified for all build types.
  #
  # NOTE: If separated libraries for debug and release are needed every library is the list are
  # to be prefixed explicitly.
  #
  #  Use: "optimized libfoo optimized libbar debug libfoo_d debug libbar_d"
  #  NOT: "optimized libfoo libbar debug libfoo_d libbar_d"
  if(NOT "${library_deps}" STREQUAL "")
    set(next_library_mode "")
    foreach(library ${library_deps})
      string(TOLOWER "${library}" library_lower)
      if(("${library_lower}" STREQUAL "optimized") OR
         ("${library_lower}" STREQUAL "debug"))
        set(next_library_mode "${library_lower}")
      else()
        if("${next_library_mode}" STREQUAL "optimized")
          target_link_libraries(${name} INTERFACE optimized ${library})
        elseif("${next_library_mode}" STREQUAL "debug")
          target_link_libraries(${name} INTERFACE debug ${library})
        else()
          target_link_libraries(${name} INTERFACE ${library})
        endif()
        set(next_library_mode "")
      endif()
    endforeach()
  endif()

  # works fine without having the includes
  # listed is helpful for IDE's (QtCreator/MSVC)
  blender_source_group("${name}" "${sources}")
  blender_user_header_search_paths("${name}" "${includes}")

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
  library_deps
  )

  add_cc_flags_custom_test(${name} PARENT_SCOPE)

  blender_add_lib__impl(${name} "${sources}" "${includes}" "${includes_sys}" "${library_deps}")
endfunction()

function(blender_add_lib
  name
  sources
  includes
  includes_sys
  library_deps
  )

  add_cc_flags_custom_test(${name} PARENT_SCOPE)

  blender_add_lib__impl(${name} "${sources}" "${includes}" "${includes_sys}" "${library_deps}")

  set_property(GLOBAL APPEND PROPERTY BLENDER_LINK_LIBS ${name})
endfunction()

function(blender_add_test_suite)
  if(ARGC LESS 1)
    message(FATAL_ERROR "No arguments supplied to blender_add_test_suite()")
  endif()

  # Parse the arguments
  set(oneValueArgs TARGET SUITE_NAME)
  set(multiValueArgs SOURCES)
  cmake_parse_arguments(ARGS "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  # Figure out the release dir, as some tests need files from there.
  GET_BLENDER_TEST_INSTALL_DIR(TEST_INSTALL_DIR)
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
  include(GTest)
  gtest_add_tests(
    TARGET ${ARGS_TARGET}
    SOURCES "${ARGS_SOURCES}"
    TEST_PREFIX ${ARGS_SUITE_NAME}
    WORKING_DIRECTORY "${TEST_INSTALL_DIR}"
    EXTRA_ARGS
      --test-assets-dir "${CMAKE_SOURCE_DIR}/../lib/tests"
      --test-release-dir "${_test_release_dir}"
  )

  unset(_test_release_dir)
endfunction()

# Add tests for a Blender library, to be called in tandem with blender_add_lib().
# The tests will be part of the blender_test executable (see tests/gtests/runner).
function(blender_add_test_lib
  name
  sources
  includes
  includes_sys
  library_deps
  )

  add_cc_flags_custom_test(${name} PARENT_SCOPE)

  # Otherwise external projects will produce warnings that we cannot fix.
  remove_strict_flags()

  # This duplicates logic that's also in GTestTesting.cmake, macro BLENDER_SRC_GTEST_EX.
  # TODO(Sybren): deduplicate after the general approach in D7649 has been approved.
  LIST(APPEND includes
    ${CMAKE_SOURCE_DIR}/tests/gtests
  )
  LIST(APPEND includes_sys
    ${GLOG_INCLUDE_DIRS}
    ${GFLAGS_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/extern/gtest/include
    ${CMAKE_SOURCE_DIR}/extern/gmock/include
  )
  add_definitions(-DBLENDER_GFLAGS_NAMESPACE=${GFLAGS_NAMESPACE})
  add_definitions(${GFLAGS_DEFINES})
  add_definitions(${GLOG_DEFINES})

  blender_add_lib__impl(${name} "${sources}" "${includes}" "${includes_sys}" "${library_deps}")

  set_property(GLOBAL APPEND PROPERTY BLENDER_TEST_LIBS ${name})

  blender_add_test_suite(
    TARGET blender_test
    SUITE_NAME ${name}
    SOURCES "${sources}"
  )
endfunction()


# Add tests for a Blender library, to be called in tandem with blender_add_lib().
# Test will be compiled into a ${name}_test executable.
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

  add_cc_flags_custom_test(${name} PARENT_SCOPE)

  ## Otherwise external projects will produce warnings that we cannot fix.
  remove_strict_flags()

  include_directories(${includes})
  include_directories(${includes_sys})
  setup_libdirs()

  BLENDER_SRC_GTEST_EX(
    NAME ${name}
    SRC "${sources}"
    EXTRA_LIBS "${library_deps}"
    SKIP_ADD_TEST
  )

  blender_add_test_suite(
    TARGET ${name}_test
    SUITE_NAME ${name}
    SOURCES "${sources}"
  )
endfunction()

# Ninja only: assign 'heavy pool' to some targets that are especially RAM-consuming to build.
function(setup_heavy_lib_pool)
  if(WITH_NINJA_POOL_JOBS AND NINJA_MAX_NUM_PARALLEL_COMPILE_HEAVY_JOBS)
    if(WITH_CYCLES)
      list(APPEND _HEAVY_LIBS "cycles_device" "cycles_kernel")
    endif()
    if(WITH_LIBMV)
      list(APPEND _HEAVY_LIBS "extern_ceres" "bf_intern_libmv")
    endif()
    if(WITH_OPENVDB)
      list(APPEND _HEAVY_LIBS "bf_intern_openvdb")
    endif()

    foreach(TARGET ${_HEAVY_LIBS})
      if(TARGET ${TARGET})
        set_property(TARGET ${TARGET} PROPERTY JOB_POOL_COMPILE compile_heavy_job_pool)
      endif()
    endforeach()
  endif()
endfunction()

function(SETUP_LIBDIRS)

  # NOTE: For all new libraries, use absolute library paths.
  # This should eventually be phased out.
  # APPLE plaform uses full paths for linking libraries, and avoids link_directories.
  if(NOT MSVC AND NOT APPLE)
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
    if(WITH_OPENIMAGEDENOISE)
      link_directories(${OPENIMAGEDENOISE_LIBPATH})
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
    if(WITH_PULSEAUDIO AND NOT WITH_PULSEAUDIO_DYNLOAD)
      link_directories(${LIBPULSE_LIBPATH})
    endif()
    if(WITH_CODEC_SNDFILE)
      link_directories(${LIBSNDFILE_LIBPATH})
    endif()
    if(WITH_FFTW3)
      link_directories(${FFTW3_LIBPATH})
    endif()
    if(WITH_OPENCOLLADA)
      link_directories(${OPENCOLLADA_LIBPATH})
      # # Never set
      # link_directories(${PCRE_LIBPATH})
      # link_directories(${EXPAT_LIBPATH})
    endif()
    if(WITH_LLVM)
      link_directories(${LLVM_LIBPATH})
    endif()

    if(WITH_ALEMBIC)
      link_directories(${ALEMBIC_LIBPATH})
    endif()

    if(WITH_GMP)
      link_directories(${GMP_LIBPATH})
    endif()

    if(WITH_GHOST_WAYLAND)
      link_directories(
        ${wayland-client_LIBRARY_DIRS}
        ${wayland-egl_LIBRARY_DIRS}
        ${xkbcommon_LIBRARY_DIRS}
        ${wayland-cursor_LIBRARY_DIRS})
    endif()

    if(WIN32 AND NOT UNIX)
      link_directories(${PTHREADS_LIBPATH})
    endif()
  endif()
endfunction()

# Platform specific linker flags for targets.
function(setup_platform_linker_flags
  target)
  set_property(TARGET ${target} APPEND_STRING PROPERTY LINK_FLAGS " ${PLATFORM_LINKFLAGS}")
  set_property(TARGET ${target} APPEND_STRING PROPERTY LINK_FLAGS_RELEASE " ${PLATFORM_LINKFLAGS_RELEASE}")
  set_property(TARGET ${target} APPEND_STRING PROPERTY LINK_FLAGS_DEBUG " ${PLATFORM_LINKFLAGS_DEBUG}")
endfunction()

# Platform specific libraries for targets.
function(setup_platform_linker_libs
  target
  )
  # jemalloc must be early in the list, to be before pthread (see T57998)
  if(WITH_MEM_JEMALLOC)
    target_link_libraries(${target} ${JEMALLOC_LIBRARIES})
  endif()

  if(WIN32 AND NOT UNIX)
    target_link_libraries(${target} ${PTHREADS_LIBRARIES})
  endif()

  # target_link_libraries(${target} ${PLATFORM_LINKLIBS} ${CMAKE_DL_LIBS})
  target_link_libraries(${target} ${PLATFORM_LINKLIBS})
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
    set(${_sse2_flags} "")  # icc defaults to -msse2
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
  endif()

  if(NOT DEFINED SUPPORT_SSE2_BUILD)
    # result cached
    check_c_source_runs("
      #include <emmintrin.h>
      int main(void) { __m128d v = _mm_setzero_pd(); return 0; }"
    SUPPORT_SSE2_BUILD)
  endif()

  unset(CMAKE_REQUIRED_FLAGS)
endmacro()

macro(TEST_NEON_SUPPORT)
  if(NOT DEFINED SUPPORT_NEON_BUILD)
    include(CheckCXXSourceCompiles)
    check_cxx_source_compiles(
      "#include <arm_neon.h>
       int main() {return vaddvq_s32(vdupq_n_s32(1));}"
      SUPPORT_NEON_BUILD)
  endif()
endmacro()

# Only print message if running CMake first time
macro(message_first_run)
  if(FIRST_RUN)
    message(${ARGV})
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

macro(add_c_flag
  flag)

  string(APPEND CMAKE_C_FLAGS " ${flag}")
  string(APPEND CMAKE_CXX_FLAGS " ${flag}")
endmacro()

macro(add_cxx_flag
  flag)

  string(APPEND CMAKE_CXX_FLAGS " ${flag}")
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
    add_c_flag("${C_REMOVE_STRICT_FLAGS}")
    add_cxx_flag("${CXX_REMOVE_STRICT_FLAGS}")
  endif()

  if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    remove_cc_flag(
      "-Wunused-parameter"
      "-Wunused-variable"
      "-Werror=[^ ]+"
      "-Werror"
    )

    # negate flags implied by '-Wall'
    add_c_flag("${C_REMOVE_STRICT_FLAGS}")
    add_cxx_flag("${CXX_REMOVE_STRICT_FLAGS}")
  endif()

  if(MSVC)
    remove_cc_flag(/w34189) # Restore warn C4189 (unused variable) back to w4
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
macro(remove_strict_c_flags_file
  filenames)
  foreach(_SOURCE ${ARGV})
    if(CMAKE_COMPILER_IS_GNUCC OR
       (CMAKE_C_COMPILER_ID MATCHES "Clang"))
      set_source_files_properties(${_SOURCE}
        PROPERTIES
          COMPILE_FLAGS "${C_REMOVE_STRICT_FLAGS}"
      )
    endif()
    if(MSVC)
      # TODO
    endif()
  endforeach()
  unset(_SOURCE)
endmacro()

macro(remove_strict_cxx_flags_file
  filenames)
  remove_strict_c_flags_file(${filenames} ${ARHV})
  foreach(_SOURCE ${ARGV})
    if(CMAKE_COMPILER_IS_GNUCC OR
       (CMAKE_CXX_COMPILER_ID MATCHES "Clang"))
      set_source_files_properties(${_SOURCE}
        PROPERTIES
          COMPILE_FLAGS "${CXX_REMOVE_STRICT_FLAGS}"
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
  if(CMAKE_COMPILER_IS_GNUCC OR
     (CMAKE_C_COMPILER_ID MATCHES "Clang") OR
     (CMAKE_C_COMPILER_ID MATCHES "Intel"))
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
  # - BLENDER_VERSION_PATCH
  # - BLENDER_VERSION_CYCLE (alpha, beta, rc, release)

  # So cmake depends on BKE_blender.h, beware of inf-loops!
  CONFIGURE_FILE(${CMAKE_SOURCE_DIR}/source/blender/blenkernel/BKE_blender_version.h
                 ${CMAKE_BINARY_DIR}/source/blender/blenkernel/BKE_blender_version.h.done)

  file(STRINGS ${CMAKE_SOURCE_DIR}/source/blender/blenkernel/BKE_blender_version.h _contents REGEX "^#define[ \t]+BLENDER_.*$")

  string(REGEX REPLACE ".*#define[ \t]+BLENDER_VERSION[ \t]+([0-9]+).*" "\\1" _out_version "${_contents}")
  string(REGEX REPLACE ".*#define[ \t]+BLENDER_VERSION_PATCH[ \t]+([0-9]+).*" "\\1" _out_version_patch "${_contents}")
  string(REGEX REPLACE ".*#define[ \t]+BLENDER_VERSION_CYCLE[ \t]+([a-z]+).*" "\\1" _out_version_cycle "${_contents}")

  if(NOT ${_out_version} MATCHES "[0-9]+")
    message(FATAL_ERROR "Version parsing failed for BLENDER_VERSION")
  endif()

  if(NOT ${_out_version_patch} MATCHES "[0-9]+")
    message(FATAL_ERROR "Version parsing failed for BLENDER_VERSION_PATCH")
  endif()

  if(NOT ${_out_version_cycle} MATCHES "[a-z]+")
    message(FATAL_ERROR "Version parsing failed for BLENDER_VERSION_CYCLE")
  endif()

  math(EXPR _out_version_major "${_out_version} / 100")
  math(EXPR _out_version_minor "${_out_version} % 100")

  # output vars
  set(BLENDER_VERSION "${_out_version_major}.${_out_version_minor}" PARENT_SCOPE)
  set(BLENDER_VERSION_MAJOR "${_out_version_major}" PARENT_SCOPE)
  set(BLENDER_VERSION_MINOR "${_out_version_minor}" PARENT_SCOPE)
  set(BLENDER_VERSION_PATCH "${_out_version_patch}" PARENT_SCOPE)
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
  unset(f)
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
      if(NOT IS_ABSOLUTE ${d})
        install(FILES ${f} DESTINATION ${targetdir}/${d})
      else()
        install(FILES ${f} DESTINATION ${d})
      endif()
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
  source_group(Generated FILES ${_file_to})
  list(APPEND ${list_to_add} ${file_from})
  set(${list_to_add} ${${list_to_add}} PARENT_SCOPE)

  get_filename_component(_file_to_path ${_file_to} PATH)

  add_custom_command(
    OUTPUT  ${_file_to}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${_file_to_path}
    COMMAND "$<TARGET_FILE:datatoc>" ${_file_from} ${_file_to}
    DEPENDS ${_file_from} datatoc)

  set_source_files_properties(${_file_to} PROPERTIES GENERATED TRUE)
endfunction()

# Function for converting pixmap directory to a '.png' and then a '.c' file.
function(data_to_c_simple_icons
  path_from icon_prefix icon_names
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

  # Construct a list of absolute paths from input
  set(_icon_files)
  foreach(_var ${icon_names})
    list(APPEND _icon_files "${_path_from_abs}/${icon_prefix}${_var}.dat")
  endforeach()

  add_custom_command(
    OUTPUT  ${_file_from} ${_file_to}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${_file_to_path}
    # COMMAND python3 ${CMAKE_SOURCE_DIR}/source/blender/datatoc/datatoc_icon.py ${_path_from_abs} ${_file_from}
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
  relative_include_dir
  )

  string(TOUPPER ${package} _upper_package)

  # set but invalid
  if((NOT ${PYTHON_${_upper_package}_PATH} STREQUAL "") AND
     (NOT ${PYTHON_${_upper_package}_PATH} MATCHES NOTFOUND))
#       if(NOT EXISTS "${PYTHON_${_upper_package}_PATH}/${package}")
#           message(WARNING "PYTHON_${_upper_package}_PATH is invalid, ${package} not found in '${PYTHON_${_upper_package}_PATH}' "
#                           "WITH_PYTHON_INSTALL_${_upper_package} option will be ignored when installing python")
#           set(WITH_PYTHON_INSTALL${_upper_package} OFF)
#       endif()
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
        "${PYTHON_LIBPATH}/"
        "${PYTHON_LIBPATH}/python${PYTHON_VERSION}/"
        "${PYTHON_LIBPATH}/python${_PY_VER_MAJOR}/"
      PATH_SUFFIXES
        site-packages
        dist-packages
        vendor-packages
      NO_DEFAULT_PATH
      DOC
        "Path to python site-packages or dist-packages containing '${package}' module"
    )
    mark_as_advanced(PYTHON_${_upper_package}_PATH)

    if(NOT EXISTS "${PYTHON_${_upper_package}_PATH}")
      message(WARNING
        "Python package '${package}' path could not be found in:\n"
        "'${PYTHON_LIBPATH}/python${PYTHON_VERSION}/site-packages/${package}', "
        "'${PYTHON_LIBPATH}/python${_PY_VER_MAJOR}/site-packages/${package}', "
        "'${PYTHON_LIBPATH}/python${PYTHON_VERSION}/dist-packages/${package}', "
        "'${PYTHON_LIBPATH}/python${_PY_VER_MAJOR}/dist-packages/${package}', "
        "'${PYTHON_LIBPATH}/python${PYTHON_VERSION}/vendor-packages/${package}', "
        "'${PYTHON_LIBPATH}/python${_PY_VER_MAJOR}/vendor-packages/${package}', "
        "\n"
        "The 'WITH_PYTHON_INSTALL_${_upper_package}' option will be ignored when installing Python.\n"
        "The build will be usable, only add-ons that depend on this package won't be functional."
      )
      set(WITH_PYTHON_INSTALL_${_upper_package} OFF PARENT_SCOPE)
    else()
      message(STATUS "${package} found at '${PYTHON_${_upper_package}_PATH}'")

      if(NOT "${relative_include_dir}" STREQUAL "")
        set(_relative_include_dir "${package}/${relative_include_dir}")
        unset(PYTHON_${_upper_package}_INCLUDE_DIRS CACHE)
        find_path(PYTHON_${_upper_package}_INCLUDE_DIRS
          NAMES
            "${_relative_include_dir}"
          HINTS
            "${PYTHON_LIBPATH}/"
            "${PYTHON_LIBPATH}/python${PYTHON_VERSION}/"
            "${PYTHON_LIBPATH}/python${_PY_VER_MAJOR}/"
          PATH_SUFFIXES
            "site-packages/"
            "dist-packages/"
            "vendor-packages/"
          NO_DEFAULT_PATH
          DOC
            "Path to python site-packages or dist-packages containing '${package}' module header files"
        )
        mark_as_advanced(PYTHON_${_upper_package}_INCLUDE_DIRS)

        if(NOT EXISTS "${PYTHON_${_upper_package}_INCLUDE_DIRS}")
          message(WARNING
            "Python package '${package}' include dir path could not be found in:\n"
            "'${PYTHON_LIBPATH}/python${PYTHON_VERSION}/site-packages/${_relative_include_dir}', "
            "'${PYTHON_LIBPATH}/python${_PY_VER_MAJOR}/site-packages/${_relative_include_dir}', "
            "'${PYTHON_LIBPATH}/python${PYTHON_VERSION}/dist-packages/${_relative_include_dir}', "
            "'${PYTHON_LIBPATH}/python${_PY_VER_MAJOR}/dist-packages/${_relative_include_dir}', "
            "'${PYTHON_LIBPATH}/python${PYTHON_VERSION}/vendor-packages/${_relative_include_dir}', "
            "'${PYTHON_LIBPATH}/python${_PY_VER_MAJOR}/vendor-packages/${_relative_include_dir}', "
            "\n"
            "The 'WITH_PYTHON_${_upper_package}' option will be disabled.\n"
            "The build will be usable, only add-ons that depend on this package won't be functional."
          )
          set(WITH_PYTHON_${_upper_package} OFF PARENT_SCOPE)
        else()
          set(_temp "${PYTHON_${_upper_package}_INCLUDE_DIRS}/${package}/${relative_include_dir}")
          unset(PYTHON_${_upper_package}_INCLUDE_DIRS CACHE)
          set(PYTHON_${_upper_package}_INCLUDE_DIRS "${_temp}"
              CACHE PATH "Path to the include directory of the ${package} module")

          message(STATUS "${package} include files found at '${PYTHON_${_upper_package}_INCLUDE_DIRS}'")
        endif()
      endif()
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
        if(MSVC_CLANG)
          set(OPENMP_DLL_NAME "libomp")
        elseif(MSVC_VERSION EQUAL 1800)
          set(OPENMP_DLL_NAME "vcomp120")
        else()
          set(OPENMP_DLL_NAME "vcomp140")
        endif()
        set_property(TARGET ${projectname} APPEND_STRING  PROPERTY LINK_FLAGS_RELEASE " /DELAYLOAD:${OPENMP_DLL_NAME}.dll delayimp.lib")
        set_property(TARGET ${projectname} APPEND_STRING  PROPERTY LINK_FLAGS_DEBUG " /DELAYLOAD:${OPENMP_DLL_NAME}d.dll delayimp.lib")
        set_property(TARGET ${projectname} APPEND_STRING  PROPERTY LINK_FLAGS_RELWITHDEBINFO " /DELAYLOAD:${OPENMP_DLL_NAME}.dll delayimp.lib")
        set_property(TARGET ${projectname} APPEND_STRING  PROPERTY LINK_FLAGS_MINSIZEREL " /DELAYLOAD:${OPENMP_DLL_NAME}.dll delayimp.lib")
      endif()
    endif()
endmacro()

macro(blender_precompile_headers target cpp header)
  if(MSVC)
    # get the name for the pch output file
    get_filename_component(pchbase ${cpp} NAME_WE)
    set(pchfinal "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/${pchbase}.pch")

    # mark the cpp as the one outputting the pch
    set_property(SOURCE ${cpp} APPEND PROPERTY OBJECT_OUTPUTS "${pchfinal}")

    # get all sources for the target
    get_target_property(sources ${target} SOURCES)

    # make all sources depend on the pch to enforce the build order
    foreach(src ${sources})
      set_property(SOURCE ${src} APPEND PROPERTY OBJECT_DEPENDS "${pchfinal}")
    endforeach()

    target_sources(${target} PRIVATE ${cpp} ${header})
    set_target_properties(${target} PROPERTIES COMPILE_FLAGS "/Yu${header} /Fp${pchfinal} /FI${header}")
    set_source_files_properties(${cpp} PROPERTIES COMPILE_FLAGS "/Yc${header} /Fp${pchfinal}")
  endif()
endmacro()

macro(set_and_warn_dependency
  _dependency _setting _val)
  # when $_dependency is disabled, forces $_setting = $_val
  if(NOT ${${_dependency}} AND ${${_setting}})
    message(STATUS "'${_dependency}' is disabled: forcing 'set(${_setting} ${_val})'")
    set(${_setting} ${_val})
  endif()
endmacro()

macro(without_system_libs_begin)
  set(CMAKE_IGNORE_PATH "${CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES};${CMAKE_SYSTEM_INCLUDE_PATH};${CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES};${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES}")
  if(APPLE)
    # Avoid searching for headers in frameworks (like Mono), and libraries in LIBDIR.
    set(CMAKE_FIND_FRAMEWORK NEVER)
  endif()
endmacro()

macro(without_system_libs_end)
  unset(CMAKE_IGNORE_PATH)
  if(APPLE)
    # FIRST is the default.
    set(CMAKE_FIND_FRAMEWORK FIRST)
  endif()
endmacro()
