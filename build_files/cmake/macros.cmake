# SPDX-FileCopyrightText: 2006 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Insert `item_add` into the list after `item_check`.
#
# Modifies in parent scope:
# - `${list_id}`: the modified list.
function(list_insert_after
  list_id item_check item_add
  )
  set(_index "")
  list(FIND "${list_id}" "${item_check}" _index)
  if(${_index} EQUAL -1)
    message(FATAL_ERROR "'${list_id}' doesn't contain '${item_check}'")
  endif()
  math(EXPR _index "${_index} + 1")
  list(INSERT ${list_id} "${_index}" ${item_add})
  set(${list_id} "${${list_id}}" PARENT_SCOPE)
endfunction()

# Insert `item_add` into the list before `item_check`.
#
# Modifies in parent scope:
# - `${list_id}`: the modified list.
function(list_insert_before
  list_id item_check item_add
  )
  set(_index "")
  list(FIND "${list_id}" "${item_check}" _index)
  if(${_index} EQUAL -1)
    message(FATAL_ERROR "'${list_id}' doesn't contain '${item_check}'")
  endif()
  list(INSERT ${list_id} "${_index}" ${item_add})
  set(${list_id} "${${list_id}}" PARENT_SCOPE)
endfunction()

function(list_assert_duplicates
  list_id
  )

  # message(STATUS "list data: ${list_id}")

  list(REMOVE_ITEM list_id "PUBLIC" "PRIVATE" "INTERFACE")
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
# Return values:
# - `${path_new}`: the path with a trailing separator.
function(path_ensure_trailing_slash
  path_new path_input
  )
  file(TO_NATIVE_PATH "/" _path_sep)
  # Escape for use in regex (`string(REGEX QUOTE ...)` is only available in CMake 4.2+).
  string(REPLACE "\\" "\\\\" _path_sep_escaped "${_path_sep}")
  string(REGEX REPLACE "[${_path_sep_escaped}]+$" "" ${path_new} ${path_input})
  set(${path_new} "${${path_new}}${_path_sep}" PARENT_SCOPE)
endfunction()

# Strip trailing path separators.
#
# Return values:
# - `${path_new}`: the path without trailing separators.
function(path_strip_trailing_slash
  path_new path_input
  )
  file(TO_NATIVE_PATH "/" _path_sep)
  # Escape for use in regex (`string(REGEX QUOTE ...)` is only available in CMake 4.2+).
  string(REPLACE "\\" "\\\\" _path_sep_escaped "${_path_sep}")
  string(REGEX REPLACE "[${_path_sep_escaped}]+$" "" ${path_new} ${path_input})
  set(${path_new} "${${path_new}}" PARENT_SCOPE)
endfunction()


# foo_bar.spam --> foo_barMySuffix.spam
#
# Return values:
# - `${file_name_new}`: the file name with the suffix inserted.
function(file_suffix
  file_name_new file_name file_suffix
  )

  get_filename_component(_file_name_PATH ${file_name} PATH)
  get_filename_component(_file_name_NAME_WE ${file_name} NAME_WE)
  get_filename_component(_file_name_EXT ${file_name} EXT)
  set(${file_name_new} "${_file_name_PATH}/${_file_name_NAME_WE}${file_suffix}${_file_name_EXT}" PARENT_SCOPE)
endfunction()

# useful for adding debug suffix to library lists:
# /somepath/foo.lib --> /somepath/foo_d.lib
#
# Return values:
# - `${fp_list_new}`: the list with suffixes applied to each entry.
function(file_list_suffix
  fp_list_new fp_list fn_suffix
  )

  set(_fp_list_new "")

  foreach(_fp ${fp_list})
    file_suffix(_fp_suffixed "${_fp}" "${fn_suffix}")
    list(APPEND _fp_list_new "${_fp_suffixed}")
  endforeach()

  set(${fp_list_new} "${_fp_list_new}" PARENT_SCOPE)
endfunction()

if(UNIX AND NOT APPLE)
  # NOTE: must be a macro, forwards `${ARGV}` to `find_package()`
  # whose result variables must be visible in the caller's scope.
  macro(find_package_static)
    set(_cmake_find_library_suffixes_back ${CMAKE_FIND_LIBRARY_SUFFIXES})
    set(CMAKE_FIND_LIBRARY_SUFFIXES .a ${CMAKE_FIND_LIBRARY_SUFFIXES})
    find_package(${ARGV})
    set(CMAKE_FIND_LIBRARY_SUFFIXES ${_cmake_find_library_suffixes_back})
    unset(_cmake_find_library_suffixes_back)
  endmacro()

  # NOTE: must be a macro, forwards `${ARGV}` to `find_library()`
  # whose result variables must be visible in the caller's scope.
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
function(absolute_include_dirs
  includes_absolute)

  set(_ALL_INCS "")
  foreach(_INC ${ARGN})
    # Pass any scoping keywords as is
    if(("${_INC}" STREQUAL "PUBLIC") OR
       ("${_INC}" STREQUAL "PRIVATE") OR
       ("${_INC}" STREQUAL "INTERFACE"))
      list(APPEND _ALL_INCS ${_INC})
    else()
      get_filename_component(_ABS_INC ${_INC} ABSOLUTE)
      list(APPEND _ALL_INCS ${_ABS_INC})
      # for checking for invalid includes, disable for regular use
      # if(NOT EXISTS "${_ABS_INC}/")
      #   message(FATAL_ERROR "Include not found: ${_ABS_INC}/")
      # endif()
    endif()
  endforeach()

  set(${includes_absolute} ${_ALL_INCS} PARENT_SCOPE)
endfunction()

function(blender_target_include_dirs_impl
  target
  system
  includes
  )
  set(next_interface_mode "PRIVATE")
  foreach(_INC ${includes})
    if(("${_INC}" STREQUAL "PUBLIC") OR
       ("${_INC}" STREQUAL "PRIVATE") OR
       ("${_INC}" STREQUAL "INTERFACE"))
      set(next_interface_mode "${_INC}")
    else()
      if(system)
        target_include_directories(${target} SYSTEM ${next_interface_mode} ${_INC})
      else()
        target_include_directories(${target} ${next_interface_mode} ${_INC})
      endif()
      set(next_interface_mode "PRIVATE")
    endif()
  endforeach()
endfunction()

# Nicer makefiles with -I/1/foo/ instead of -I/1/2/3/../../foo/
# use it instead of target_include_directories()
function(blender_target_include_dirs
  target
  )
  absolute_include_dirs(_ALL_INCS ${ARGN})
  blender_target_include_dirs_impl(${target} FALSE "${_ALL_INCS}")
endfunction()

function(blender_target_include_dirs_sys
  target
  )
  absolute_include_dirs(_ALL_INCS ${ARGN})
  blender_target_include_dirs_impl(${target} TRUE "${_ALL_INCS}")
endfunction()

# Enable unity build for the given target.
function(blender_set_target_unity_build target batch_size)
  if(WITH_UNITY_BUILD)
    set_target_properties(${target} PROPERTIES
      UNITY_BUILD ON
      UNITY_BUILD_BATCH_SIZE ${batch_size}
    )
    if(WITH_NINJA_POOL_JOBS AND NINJA_MAX_NUM_PARALLEL_COMPILE_HEAVY_JOBS)
      # Unity builds are typically heavy.
      set_target_properties(${target} PROPERTIES JOB_POOL_COMPILE compile_heavy_job_pool)
    endif()
  endif()
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
    set_target_properties(
      ${name} PROPERTIES
      XCODE_ATTRIBUTE_USER_HEADER_SEARCH_PATHS "${_ALL_INCS}"
    )
  endif()
endfunction()

function(blender_source_group
  name
  sources
  )

  # if enabled, use the sources directories as filters.
  if(IDE_GROUP_SOURCES_IN_FOLDERS)
    foreach(_SRC ${sources})
      # remove ../'s
      get_filename_component(_SRC_DIR ${_SRC} REALPATH)
      get_filename_component(_SRC_DIR ${_SRC_DIR} DIRECTORY)
      string(FIND "${_SRC_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/" _pos)
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
      if(("${_SRC_EXT}" STREQUAL ".h") OR
         ("${_SRC_EXT}" STREQUAL ".hpp") OR
         ("${_SRC_EXT}" STREQUAL ".hh"))

        set(GROUP_ID "Header Files")
      elseif("${_SRC_EXT}" STREQUAL ".glsl")
        set(GROUP_ID "Shaders")
      else()
        set(GROUP_ID "Source Files")
      endif()
      source_group("${GROUP_ID}" FILES ${_SRC})
    endforeach()
  endif()

  # if enabled, set the FOLDER property for the projects
  if(IDE_GROUP_PROJECTS_IN_FOLDERS)
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
# Optionally takes an ARGV1 passed to set(), eg `PARENT_SCOPE`.
#
# NOTE: must be a macro, modifies `CMAKE_C_FLAGS` and `CMAKE_CXX_FLAGS`
# in the caller's scope.
macro(add_c_and_cxx_flags_custom_test
  name
  )

  # NOTE: When ARGV1 is PARENT_SCOPE, propagate to the caller's parent scope.
  # `string(APPEND)` alone only modifies the local scope, and `set()` is
  # needed because `string(APPEND)` does not support PARENT_SCOPE.
  string(TOUPPER ${name} _name_upper)
  if(DEFINED CMAKE_C_FLAGS_${_name_upper})
    message(
      STATUS
      "Using custom CFLAGS: "
      "CMAKE_C_FLAGS_${_name_upper} in \"${CMAKE_CURRENT_SOURCE_DIR}\"")
    string(APPEND CMAKE_C_FLAGS " ${CMAKE_C_FLAGS_${_name_upper}}")
    # Harmless if ARGV1 isn't set.
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}" ${ARGV1})
  endif()
  if(DEFINED CMAKE_CXX_FLAGS_${_name_upper})
    message(
      STATUS
      "Using custom CXXFLAGS: "
      "CMAKE_CXX_FLAGS_${_name_upper} in \"${CMAKE_CURRENT_SOURCE_DIR}\""
    )
    string(APPEND CMAKE_CXX_FLAGS " ${CMAKE_CXX_FLAGS_${_name_upper}}")
    # Harmless if ARGV1 isn't set.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" ${ARGV1})
  endif()
  unset(_name_upper)

endmacro()

function(blender_link_libraries
  target
  library_deps
  )

  # On Windows certain libraries have two sets of binaries: one for debug builds and one for
  # release builds. The root of this requirement goes into ABI, I believe, but that's outside
  # of a scope of this comment.
  #
  # CMake have a native way of dealing with this, which is specifying what build type the
  # libraries are provided for:
  #
  #   target_link_libraries(target optimized|debug|general <libraries>)
  #
  # The build type is to be provided as a separate argument to the function.
  #
  # CMake's variables for libraries will contain build type in such cases. For example:
  #
  #   set(FOO_LIBRARIES optimized libfoo.lib debug libfoo_d.lib)
  #
  # Complications start with a single argument for library_deps: all the elements are being
  # put to a list: "${FOO_LIBRARIES}" will become "optimized;libfoo.lib;debug;libfoo_d.lib".
  # This makes it impossible to pass it as-is to target_link_libraries since it will treat
  # this argument as a list of libraries to be linked against, causing missing libraries
  # for optimized.lib.
  #
  # What this code does is traverse library_deps and extracts information about whether
  # library is to be provided as general, debug or optimized. This is a little state machine which
  # keeps track of which build type library is to be provided for:
  #
  # - If "debug" or "optimized" word is found, the next element in the list is expected to be
  #   a library which will be passed to target_link_libraries() under corresponding build type.
  #
  # - If there is no "debug" or "optimized" used library is specified for all build types.
  #
  # NOTE: If separated libraries for debug and release are needed every library in the list is
  # to be prefixed explicitly.
  #
  # Use: "optimized libfoo optimized libbar debug libfoo_d debug libbar_d"
  # NOT: "optimized libfoo libbar debug libfoo_d libbar_d"
  set(dependency_libraries "")
  if(NOT "${library_deps}" STREQUAL "")
    set(next_library_mode "")
    set(next_interface_mode "PRIVATE")
    foreach(library ${library_deps})
      string(TOLOWER "${library}" library_lower)
      if(("${library_lower}" STREQUAL "optimized") OR
         ("${library_lower}" STREQUAL "debug"))
        set(next_library_mode "${library_lower}")
      elseif(("${library}" STREQUAL "PUBLIC") OR
             ("${library}" STREQUAL "PRIVATE") OR
             ("${library}" STREQUAL "INTERFACE"))
        set(next_interface_mode "${library}")
      else()
        if("${next_library_mode}" STREQUAL "optimized")
          set(link_library ${next_interface_mode} optimized ${library})
        elseif("${next_library_mode}" STREQUAL "debug")
          set(link_library ${next_interface_mode} debug ${library})
        else()
          set(link_library ${next_interface_mode} ${library})
        endif()
        set(next_library_mode "")
        if(library MATCHES "^bf::dependencies")
          list(APPEND dependency_libraries ${link_library})
        else()
          target_link_libraries(${target} ${link_library})
        endif()
      endif()
    endforeach()
  endif()

  # Ensure external dependencies are last in the list of libraries, so that bf::extern include
  # directories have priority over system library include directories that might conflict.
  target_link_libraries(${target} ${dependency_libraries})
endfunction()

function(blender_add_lib__impl
  name
  sources
  includes
  includes_sys
  library_deps
  )

  # message(STATUS "Configuring library ${name}")

  add_library(${name} ${sources})

  # On windows vcpkg goes out of its way to make its libs the preferred
  # libs, and needs to be explicitly be told not to do that.
  if(WIN32)
    set_target_properties(${name} PROPERTIES VS_GLOBAL_VcpkgEnabled "false")
  endif()
  blender_target_include_dirs(${name} ${includes})
  blender_target_include_dirs_sys(${name} ${includes_sys})

  if(library_deps)
    blender_link_libraries(${name} "${library_deps}")
  endif()

  # works fine without having the includes
  # listed is helpful for IDE's (QtCreator/MSVC)
  blender_source_group("${name}" "${sources}")
  blender_user_header_search_paths("${name}" "${includes}")

  list_assert_duplicates("${sources}")
  list_assert_duplicates("${includes}")
  # Not for system includes because they can resolve to the same path
  # list_assert_duplicates("${includes_sys}")

  # Blender's dependency loops are longer than cmake expects and we need additional loops to
  # properly link.
  set_property(TARGET ${name} APPEND PROPERTY LINK_INTERFACE_MULTIPLICITY 3)
endfunction()


function(blender_add_lib_nolist
  name
  sources
  includes
  includes_sys
  library_deps
  )

  add_c_and_cxx_flags_custom_test(${name} PARENT_SCOPE)

  blender_add_lib__impl(${name} "${sources}" "${includes}" "${includes_sys}" "${library_deps}")
endfunction()

function(blender_add_lib
  name
  sources
  includes
  includes_sys
  library_deps
  )

  add_c_and_cxx_flags_custom_test(${name} PARENT_SCOPE)

  blender_add_lib__impl(${name} "${sources}" "${includes}" "${includes_sys}" "${library_deps}")

  set_property(GLOBAL APPEND PROPERTY BLENDER_LINK_LIBS ${name})
endfunction()

# Ninja only: assign 'heavy pool' to some targets that are especially RAM-consuming to build.
function(setup_heavy_lib_pool)
  if(WITH_NINJA_POOL_JOBS AND NINJA_MAX_NUM_PARALLEL_COMPILE_HEAVY_JOBS)
    set(_HEAVY_LIBS "")
    set(_HEAVY_FILES "")
    set(_TARGET "")
    if(WITH_CYCLES)
      list(APPEND _HEAVY_LIBS "cycles_device" "cycles_kernel" "cycles_hydra")
    endif()
    if(WITH_LIBMV)
      list(APPEND _HEAVY_LIBS "extern_ceres" "bf_intern_libmv")
    endif()
    if(WITH_OPENVDB)
      list(APPEND _HEAVY_LIBS "bf_intern_openvdb")
    endif()

    # A few specific files are very heavy to compile in Clang or GCC
    # (several GB of RAM required in debug + ASAN builds e.g.).
    list(APPEND _HEAVY_FILES "source/blender/blenkernel/intern/volume.cc")
    list(APPEND _HEAVY_FILES "source/blender/blenkernel/intern/volume_to_mesh.cc")
    list(APPEND _HEAVY_FILES "source/blender/blenkernel/intern/volume_grid.cc")
    list(APPEND _HEAVY_FILES "source/blender/modifiers/intern/MOD_volume_displace.cc")
    list(APPEND _HEAVY_FILES "source/blender/geometry/intern/mesh_to_volume.cc")

    foreach(_TARGET ${_HEAVY_LIBS})
      if(TARGET ${_TARGET})
        set_property(TARGET ${_TARGET} PROPERTY JOB_POOL_COMPILE compile_heavy_job_pool)
      endif()
    endforeach()
    if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.2.0")
      foreach(_FILE ${_HEAVY_FILES})
        set_property(SOURCE ${_FILE} PROPERTY JOB_POOL_COMPILE compile_heavy_job_pool)
      endforeach()
    endif()
    unset(_TARGET)
    unset(_HEAVY_LIBS)
    unset(_HEAVY_FILES)
  endif()
endfunction()

# Platform specific linker flags for targets.
function(setup_platform_linker_flags
  target)
  set_property(
    TARGET ${target} APPEND_STRING PROPERTY
    LINK_FLAGS " ${PLATFORM_LINKFLAGS}"
  )
  set_property(
    TARGET ${target} APPEND_STRING PROPERTY
    LINK_FLAGS_RELEASE " ${PLATFORM_LINKFLAGS_RELEASE}"
  )
  set_property(
    TARGET ${target} APPEND_STRING PROPERTY
    LINK_FLAGS_DEBUG " ${PLATFORM_LINKFLAGS_DEBUG}"
  )

  get_target_property(target_type ${target} TYPE)
  if(target_type STREQUAL "EXECUTABLE")
    set_property(
      TARGET ${target} APPEND_STRING PROPERTY
      LINK_FLAGS " ${PLATFORM_LINKFLAGS_EXECUTABLE}"
    )
  endif()
endfunction()

# Hide internal symbols for targets that might otherwise conflict with plugins.
function(setup_platform_linker_symbol_hiding target)
  if(DEFINED PLATFORM_LINKFLAGS_SYMBOL_HIDING)
    set_property(
      TARGET ${target} APPEND_STRING PROPERTY
      LINK_FLAGS " ${PLATFORM_LINKFLAGS_SYMBOL_HIDING}"
    )
  endif()

  if(DEFINED PLATFORM_SYMBOLS_MAP)
    set_target_properties(${target} PROPERTIES LINK_DEPENDS ${PLATFORM_SYMBOLS_MAP})
  endif()
endfunction()

# Platform specific libraries for targets.
function(setup_platform_linker_libs
  target
  )
  # TBB malloc must be early in the list, to be before PTHREAD (see #57998).
  if(WITH_TBB_MALLOC_PROXY)
    target_link_libraries(${target}
      PRIVATE ${TBB_MALLOC_LIBRARIES}
      PRIVATE ${TBB_MALLOC_PROXY_LIBRARIES}
    )
  endif()

  if(WIN32 AND NOT UNIX)
    if(DEFINED PTHREADS_LIBRARIES)
      target_link_libraries(${target} PRIVATE ${PTHREADS_LIBRARIES})
    endif()
  endif()

  # target_link_libraries(${target} ${PLATFORM_LINKLIBS} ${CMAKE_DL_LIBS})
  target_link_libraries(${target} PRIVATE ${PLATFORM_LINKLIBS})
endfunction()

# Return values:
# - `${_simd_flags}`: compiler flags to enable SIMD support.
function(get_compiler_simd_flags
  _simd_flags)

  if(CMAKE_SYSTEM_PROCESSOR MATCHES "(x86_64)|(AMD64)" OR CMAKE_OSX_ARCHITECTURES MATCHES x86_64)
    # message(STATUS "Detecting SIMD support")
    if((CMAKE_C_COMPILER_ID STREQUAL "GNU") OR (CMAKE_C_COMPILER_ID MATCHES "Clang"))
      set(${_simd_flags} "-march=x86-64-v2" PARENT_SCOPE)
    elseif(MSVC)
      # MSVC has no specific compile flags for SSE42 (only for AVX).
      set(${_simd_flags} PARENT_SCOPE)
      # It also doesn't define __SSE__/__MMX__ flags and only does the AVX and higher flags.
      # For consistency we define these flags for MSVC.
      add_compile_definitions(__MMX__ __SSE__ __SSE2__ __SSE3__ __SSE4_1__ __SSE4_2__)
    elseif(CMAKE_C_COMPILER_ID STREQUAL "Intel")
      if(WIN32)
        set(${_simd_flags} "/QxSSE4.2" PARENT_SCOPE)
      else()
        set(${_simd_flags} "-xsse4.2" PARENT_SCOPE)
      endif()
    else()
      message(WARNING "SIMD flags for this compiler: '${CMAKE_C_COMPILER_ID}' not known")
      set(${_simd_flags} PARENT_SCOPE)
    endif()
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64|arm64" OR CMAKE_OSX_ARCHITECTURES MATCHES "arm64")
    if((CMAKE_C_COMPILER_ID STREQUAL "GNU") OR (CMAKE_C_COMPILER_ID MATCHES "Clang"))
      if(UNIX AND NOT APPLE)
        # Target ARMv8.2-A with dot product and half float.
        set(${_simd_flags} "-march=armv8.2-a+dotprod+fp16+lse" PARENT_SCOPE)
      else()
        set(${_simd_flags} PARENT_SCOPE)
      endif()
    else()
      set(${_simd_flags} PARENT_SCOPE)
    endif()
  else()
    # Not a supported system, don't set any SIMD compiler flags.
    set(${_simd_flags} PARENT_SCOPE)
  endif()
endfunction()

function(test_neon_support)
  if(NOT DEFINED SUPPORTS_NEON_BUILD)
    include(CheckCXXSourceCompiles)
    check_cxx_source_compiles(
      "#include <arm_neon.h>
       int main() {return vaddvq_s32(vdupq_n_s32(1));}"
      SUPPORTS_NEON_BUILD)
  endif()
endfunction()

# Only print message if running CMake first time
function(message_first_run)
  if(FIRST_RUN)
    message(${ARGV})
  endif()
endfunction()

# when we have warnings as errors applied globally this
# needs to be removed for some external libs which we don't maintain.


# NOTE: must be a macro, modifies `CMAKE_C_FLAGS` and per-config
# variants in the caller's scope.
macro(remove_c_flag
  _flag)

  foreach(f ${ARGV})
    string(REGEX REPLACE ${f} "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    string(REGEX REPLACE ${f} "" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
    string(REGEX REPLACE ${f} "" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
    string(REGEX REPLACE ${f} "" CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL}")
    string(REGEX REPLACE ${f} "" CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
  endforeach()
  unset(f)
endmacro()

# NOTE: must be a macro, modifies `CMAKE_CXX_FLAGS` and per-config
# variants in the caller's scope.
macro(remove_cxx_flag
  _flag)

  foreach(f ${ARGV})
    string(REGEX REPLACE ${f} "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REGEX REPLACE ${f} "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    string(REGEX REPLACE ${f} "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
    string(REGEX REPLACE ${f} "" CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL}")
    string(REGEX REPLACE ${f} "" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
  endforeach()
  unset(f)
endmacro()

# NOTE: must be a macro, calls `remove_c_flag`/`remove_cxx_flag`
# which modify `CMAKE_*_FLAGS` in the caller's scope.
macro(remove_c_and_cxx_flag
  _flag)

  remove_c_flag(${ARGV})
  remove_cxx_flag(${ARGV})
endmacro()

# NOTE: must be a macro, modifies `CMAKE_C_FLAGS` in the caller's scope.
macro(add_c_flag
  flag)

  string(APPEND CMAKE_C_FLAGS " ${flag}")
endmacro()

# NOTE: must be a macro, modifies `CMAKE_CXX_FLAGS` in the caller's scope.
macro(add_cxx_flag
  flag)

  string(APPEND CMAKE_CXX_FLAGS " ${flag}")
endmacro()

# NOTE: must be a macro, calls `add_c_flag`/`add_cxx_flag`
# which modify `CMAKE_*_FLAGS` in the caller's scope.
macro(add_c_and_cxx_flag
  flag)

  add_c_flag("${flag}")
  add_cxx_flag("${flag}")
endmacro()

# Needed to "negate" options: `-Wno-example`
# as this doesn't work when added to `CMAKE_CXX_FLAGS`.
#
# NOTE: must be a macro, modifies per-config `CMAKE_C_FLAGS_*`
# variants in the caller's scope.
macro(add_c_flag_per_config
  flag)

  string(APPEND CMAKE_C_FLAGS_DEBUG " ${flag}")
  string(APPEND CMAKE_C_FLAGS_RELEASE " ${flag}")
  string(APPEND CMAKE_C_FLAGS_MINSIZEREL " ${flag}")
  string(APPEND CMAKE_C_FLAGS_RELWITHDEBINFO " ${flag}")
endmacro()

# NOTE: must be a macro, modifies per-config `CMAKE_CXX_FLAGS_*`
# variants in the caller's scope.
macro(add_cxx_flag_per_config
  flag)

  string(APPEND CMAKE_CXX_FLAGS_DEBUG " ${flag}")
  string(APPEND CMAKE_CXX_FLAGS_RELEASE " ${flag}")
  string(APPEND CMAKE_CXX_FLAGS_MINSIZEREL " ${flag}")
  string(APPEND CMAKE_CXX_FLAGS_RELWITHDEBINFO " ${flag}")
endmacro()

# NOTE: must be a macro, calls `remove_cc_flag`/`add_c_flag`/`add_cxx_flag`/`add_cc_flag`
# which modify `CMAKE_*_FLAGS` in the caller's scope.
macro(remove_strict_flags)

  if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    remove_c_and_cxx_flag(
      "-Wstrict-prototypes"
      "-Wsuggest-attribute=format"
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
      "-Wextra"
      "-Werror=[^ ]+"
      "-Werror"
    )

    # negate flags implied by '-Wall'
    add_c_flag("${C_REMOVE_STRICT_FLAGS}")
    add_cxx_flag("${CXX_REMOVE_STRICT_FLAGS}")
    add_c_and_cxx_flag("${C_AND_CXX_REMOVE_STRICT_FLAGS}")
  endif()

  if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    remove_c_and_cxx_flag(
      "-Wunused-parameter"
      "-Wunused-variable"
      "-Werror=[^ ]+"
      "-Werror"
    )

    # negate flags implied by '-Wall'
    add_c_flag("${C_REMOVE_STRICT_FLAGS}")
    add_cxx_flag("${CXX_REMOVE_STRICT_FLAGS}")
    add_c_and_cxx_flag("${C_AND_CXX_REMOVE_STRICT_FLAGS}")
  endif()

  if(MSVC)
    add_cxx_flag(
      # Warning C5038: data member 'foo' will be initialized after data member 'bar'.
      "/wd5038"
    )
    remove_c_and_cxx_flag(
      # Restore warn C4100 (unreferenced formal parameter) back to w4.
      "/w34100"
      # Restore warn C4189 (unused variable) back to w4.
      "/w34189"
    )
  endif()

endmacro()

# NOTE: must be a macro, calls `remove_cc_flag`
# which modifies `CMAKE_*_FLAGS` in the caller's scope.
macro(remove_extra_strict_flags)
  if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    remove_c_and_cxx_flag(
      "-Wunused-parameter"
    )
  endif()

  if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    remove_c_and_cxx_flag(
      "-Wunused-parameter"
    )
  endif()

  if(MSVC)
    remove_c_and_cxx_flag(
      # Restore warn C4100 (unreferenced formal parameter) back to w4.
      "/w34100"
    )
  endif()
endmacro()

# note, we can only append flags on a single file so we need to negate the options.
# at the moment we can't shut up ffmpeg deprecations, so use this, but will
# probably add more removals here.
function(remove_strict_c_flags_file
  filenames)
  foreach(_SOURCE ${ARGV})
    if((CMAKE_C_COMPILER_ID STREQUAL "GNU") OR
       (CMAKE_C_COMPILER_ID MATCHES "Clang"))
      set_source_files_properties(
        ${_SOURCE} PROPERTIES
        COMPILE_FLAGS "${C_REMOVE_STRICT_FLAGS} ${C_AND_CXX_REMOVE_STRICT_FLAGS}"
      )
    endif()
    if(MSVC)
      # TODO
    endif()
  endforeach()
endfunction()

function(remove_strict_cxx_flags_file
  filenames)
  foreach(_SOURCE ${ARGV})
    if((CMAKE_CXX_COMPILER_ID STREQUAL "GNU") OR
       (CMAKE_CXX_COMPILER_ID MATCHES "Clang"))
      set_source_files_properties(
        ${_SOURCE} PROPERTIES
        COMPILE_FLAGS "${CXX_REMOVE_STRICT_FLAGS} ${C_AND_CXX_REMOVE_STRICT_FLAGS}"
      )
    endif()
    if(MSVC)
      # TODO
    endif()
  endforeach()
endfunction()

# External libs may need 'signed char' to be default.
#
# NOTE: must be a macro, calls `remove_c_and_cxx_flag`
# which modifies `CMAKE_*_FLAGS` in the caller's scope.
macro(remove_c_and_cxx_flag_unsigned_char)
  if((CMAKE_C_COMPILER_ID STREQUAL "GNU") OR
     (CMAKE_C_COMPILER_ID MATCHES "Clang") OR
     (CMAKE_C_COMPILER_ID STREQUAL "Intel"))
    remove_c_and_cxx_flag("-funsigned-char")
  elseif(MSVC)
    remove_c_and_cxx_flag("/J")
  else()
    message(WARNING
      "Compiler '${CMAKE_C_COMPILER_ID}' failed to disable 'unsigned char' flag."
      "Build files need updating."
    )
  endif()
endmacro()

function(add_check_c_compiler_flag_impl
  _CFLAGS
  _CACHE_VAR
  _FLAG
  )

  include(CheckCCompilerFlag)

  set(_is_new TRUE)
  if(DEFINED CACHE{${_CACHE_VAR}})
    set(_is_new FALSE)
  endif()

  check_c_compiler_flag("${_FLAG}" "${_CACHE_VAR}")
  if(${_CACHE_VAR})
    # message(STATUS "Using CFLAG: ${_FLAG}")
    set(${_CFLAGS} "${${_CFLAGS}} ${_FLAG}" PARENT_SCOPE)
  else()
    if(_is_new)
      message(STATUS "Unsupported CFLAG: ${_FLAG}")
    endif()
  endif()
endfunction()

function(add_check_cxx_compiler_flag_impl
  _CXXFLAGS
  _CACHE_VAR
  _FLAG
  )

  include(CheckCXXCompilerFlag)

  set(_is_new TRUE)
  if(DEFINED CACHE{${_CACHE_VAR}})
    set(_is_new FALSE)
  endif()

  check_cxx_compiler_flag("${_FLAG}" "${_CACHE_VAR}")
  if(${_CACHE_VAR})
    # message(STATUS "Using CXXFLAG: ${_FLAG}")
    set(${_CXXFLAGS} "${${_CXXFLAGS}} ${_FLAG}" PARENT_SCOPE)
  else()
    if(_is_new)
      message(STATUS "Unsupported CXXFLAG: ${_FLAG}")
    endif()
  endif()
endfunction()

function(ADD_CHECK_C_COMPILER_FLAGS _CFLAGS)
  # Iterate over pairs & check each.
  set(cache_var "")
  foreach(arg ${ARGN})
    if(cache_var)
      add_check_c_compiler_flag_impl("${_CFLAGS}" "${cache_var}" "${arg}")
      set(cache_var "")
    else()
      set(cache_var "${arg}")
    endif()
  endforeach()
  set(${_CFLAGS} "${${_CFLAGS}}" PARENT_SCOPE)
endfunction()

function(ADD_CHECK_CXX_COMPILER_FLAGS _CXXFLAGS)
  # Iterate over pairs & check each.
  set(cache_var "")
  foreach(arg ${ARGN})
    if(cache_var)
      add_check_cxx_compiler_flag_impl("${_CXXFLAGS}" "${cache_var}" "${arg}")
      set(cache_var "")
    else()
      set(cache_var "${arg}")
    endif()
  endforeach()
  set(${_CXXFLAGS} "${${_CXXFLAGS}}" PARENT_SCOPE)
endfunction()

function(get_blender_version)
  # extracts header vars and defines them in the parent scope:
  #
  # - BLENDER_VERSION (major.minor)
  # - BLENDER_VERSION_MAJOR
  # - BLENDER_VERSION_MINOR
  # - BLENDER_VERSION_PATCH
  # - BLENDER_VERSION_CYCLE (alpha, beta, rc, release)

  # So CMAKE depends on `BKE_blender_version.h`, beware of infinite-loops!
  configure_file(
    ${CMAKE_SOURCE_DIR}/source/blender/blenkernel/BKE_blender_version.h
    ${CMAKE_BINARY_DIR}/source/blender/blenkernel/BKE_blender_version.h.done
  )

  file(
    STRINGS ${CMAKE_SOURCE_DIR}/source/blender/blenkernel/BKE_blender_version.h
    _contents REGEX "^#define[ \t]+BLENDER_.*$"
  )

  string(
    REGEX REPLACE ".*#define[ \t]+BLENDER_VERSION[ \t]+([0-9]+).*" "\\1"
    _out_version "${_contents}"
  )
  string(
    REGEX REPLACE ".*#define[ \t]+BLENDER_VERSION_PATCH[ \t]+([0-9]+).*" "\\1"
    _out_version_patch "${_contents}"
  )
  string(
    REGEX REPLACE ".*#define[ \t]+BLENDER_VERSION_CYCLE[ \t]+([a-z]+).*" "\\1"
    _out_version_cycle "${_contents}"
  )

  if(NOT ${_out_version} MATCHES "^[0-9]+$")
    message(FATAL_ERROR "Version parsing failed for BLENDER_VERSION")
  endif()

  if(NOT ${_out_version_patch} MATCHES "^[0-9]+$")
    message(FATAL_ERROR "Version parsing failed for BLENDER_VERSION_PATCH")
  endif()

  if(NOT ${_out_version_cycle} MATCHES "^[a-z]+$")
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
#
# NOTE: must be a macro pair, `_pre` sets `_reset_standard_c*flags_rel`
# variables that `_post` reads from the same scope, and `_post` modifies
# `CMAKE_C/CXX_FLAGS_RELEASE` cache entries.
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
endmacro()

# pair of macros to allow libraries to be specify files to install, but to
# only install them at the end so the directories don't get cleared with
# the files in them. used by cycles to install addon.
function(delayed_install
  base
  files
  destination)

  foreach(f ${files})
    if(IS_ABSOLUTE ${f} OR "${base}" STREQUAL "")
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

# Same as above but generates the var name and output automatic.
function(data_to_c
  file_from file_to
  list_to_add
  # Optional 4th argument: override the symbol name used in the generated C file,
  # useful when different files share the same basename and would produce conflicting symbols.
  # When omitted the symbol name is derived from the filename.
  symbol_name_override
  )

  list(APPEND ${list_to_add} ${file_to})
  set(${list_to_add} ${${list_to_add}} PARENT_SCOPE)

  add_custom_command(
    OUTPUT ${file_to}
    COMMAND "$<TARGET_FILE:datatoc>" ${file_from} ${file_to} ${symbol_name_override}
    DEPENDS ${file_from} datatoc)

  set_source_files_properties(${file_to} PROPERTIES GENERATED TRUE)
endfunction()


# Same as above but generates the var name and output automatic.
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

  add_custom_command(
    OUTPUT  ${_file_to}
    COMMAND "$<TARGET_FILE:datatoc>" ${_file_from} ${_file_to}
    DEPENDS ${_file_from} datatoc)

  set_source_files_properties(${_file_to} PROPERTIES GENERATED TRUE)
endfunction()


# Process glsl file and convert it to c
function(glsl_to_c
  file_from
  list_to_add
  include_list
  )

  # remove ../'s
  get_filename_component(_file_from ${CMAKE_CURRENT_SOURCE_DIR}/${file_from}    REALPATH)
  get_filename_component(_file_tmp  ${CMAKE_CURRENT_BINARY_DIR}/${file_from}.tmp   REALPATH)
  get_filename_component(_file_meta ${CMAKE_CURRENT_BINARY_DIR}/${file_from}.hh REALPATH)
  get_filename_component(_file_info ${CMAKE_CURRENT_BINARY_DIR}/${file_from}.info  REALPATH)
  get_filename_component(_file_to   ${CMAKE_CURRENT_BINARY_DIR}/${file_from}.c  REALPATH)
  get_filename_component(_file_dep  ${CMAKE_CURRENT_BINARY_DIR}/${file_from}.d  REALPATH)

  # Turn include directories into absolute paths
  set(_inc_list "")
  foreach(path IN LISTS ${include_list})
    get_filename_component(_inc_path ${CMAKE_CURRENT_SOURCE_DIR}/${path} REALPATH)
    list(APPEND _inc_list ${_inc_path})
  endforeach()

  list(APPEND ${list_to_add} ${_file_to})
  source_group(Generated FILES ${_file_to})
  list(APPEND ${list_to_add} ${file_from})
  set(${list_to_add} ${${list_to_add}} PARENT_SCOPE)

  add_custom_command(
    OUTPUT  ${_file_tmp} ${_file_meta} ${_file_info} ${_file_dep}
    DEPFILE ${_file_dep}
    COMMAND "$<TARGET_FILE:shader_tool>" ${_file_from} ${_file_tmp} ${_file_meta} ${_file_info} ${_file_dep} ${_inc_list}
    DEPENDS ${_file_from} shader_tool)

  add_custom_command(
    OUTPUT  ${_file_to}
    COMMAND "$<TARGET_FILE:datatoc>" ${_file_tmp} ${_file_to}
    DEPENDS ${_file_tmp} datatoc)

  set_source_files_properties(${_file_tmp} PROPERTIES GENERATED TRUE)
  set_source_files_properties(${_file_to}  PROPERTIES GENERATED TRUE)
  set_source_files_properties(${_file_meta}  PROPERTIES GENERATED TRUE)
  set_source_files_properties(${_file_info}  PROPERTIES GENERATED TRUE)
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

    COMMAND ${CMAKE_COMMAND} -E
    make_directory ${_file_to_path}

    COMMAND ${CMAKE_COMMAND} -E
    env ${PLATFORM_ENV_BUILD} "$<TARGET_FILE:msgfmt>" ${_file_from} ${_file_to}

    DEPENDS msgfmt ${_file_from})

  set_source_files_properties(${_file_to} PROPERTIES GENERATED TRUE)
endfunction()

function(find_python_package
    package
    relative_inc_dir
  )

  string(TOUPPER ${package} _upper_package)

  # Set but invalid.
  if((NOT ${PYTHON_${_upper_package}_PATH} STREQUAL "") AND
     (NOT ${PYTHON_${_upper_package}_PATH} MATCHES NOTFOUND))
    # if(NOT EXISTS "${PYTHON_${_upper_package}_PATH}/${package}")
    #   message(
    #     WARNING
    #     "PYTHON_${_upper_package}_PATH is invalid, ${package} not found in "
    #     "'${PYTHON_${_upper_package}_PATH}' "
    #     "WITH_PYTHON_INSTALL_${_upper_package} option will be ignored when installing Python"
    #   )
    #   set(WITH_PYTHON_INSTALL${_upper_package} OFF)
    # endif()
    # Not set, so initialize.
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
        "The 'WITH_PYTHON_INSTALL_${_upper_package}' option will be ignored "
        "when installing Python.\n"
        "The build will be usable, only add-ons that depend on this package won't be functional."
      )
      set(WITH_PYTHON_INSTALL_${_upper_package} OFF PARENT_SCOPE)
    else()
      message(STATUS "${package} found at '${PYTHON_${_upper_package}_PATH}'")

      if(NOT "${relative_inc_dir}" STREQUAL "")
        set(_relative_inc_dir "${package}/${relative_inc_dir}")
        unset(PYTHON_${_upper_package}_INCLUDE_DIRS CACHE)
        find_path(PYTHON_${_upper_package}_INCLUDE_DIRS
          NAMES
            "${_relative_inc_dir}"
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
            "\
Path to python site-packages or dist-packages containing '${package}' module header files"
        )
        mark_as_advanced(PYTHON_${_upper_package}_INCLUDE_DIRS)

        if(NOT EXISTS "${PYTHON_${_upper_package}_INCLUDE_DIRS}")
          message(WARNING
            "Python package '${package}' include dir path could not be found in:\n"
            "'${PYTHON_LIBPATH}/python${PYTHON_VERSION}/site-packages/${_relative_inc_dir}', "
            "'${PYTHON_LIBPATH}/python${_PY_VER_MAJOR}/site-packages/${_relative_inc_dir}', "
            "'${PYTHON_LIBPATH}/python${PYTHON_VERSION}/dist-packages/${_relative_inc_dir}', "
            "'${PYTHON_LIBPATH}/python${_PY_VER_MAJOR}/dist-packages/${_relative_inc_dir}', "
            "'${PYTHON_LIBPATH}/python${PYTHON_VERSION}/vendor-packages/${_relative_inc_dir}', "
            "'${PYTHON_LIBPATH}/python${_PY_VER_MAJOR}/vendor-packages/${_relative_inc_dir}', "
            "\n"
            "The 'WITH_PYTHON_${_upper_package}' option will be disabled.\n"
            "The build will be usable, only add-ons that depend on this package "
            "won't be functional."
          )
          set(WITH_PYTHON_${_upper_package} OFF PARENT_SCOPE)
        else()
          set(_temp "${PYTHON_${_upper_package}_INCLUDE_DIRS}/${package}/${relative_inc_dir}")
          unset(PYTHON_${_upper_package}_INCLUDE_DIRS CACHE)
          set(PYTHON_${_upper_package}_INCLUDE_DIRS "${_temp}"
              CACHE PATH "Path to the include directory of the ${package} module")

          message(STATUS
            "${package} include files found at '${PYTHON_${_upper_package}_INCLUDE_DIRS}'"
          )
        endif()
      endif()
    endif()
  endif()
endfunction()

# Find a file in Python's module path and cache it.
# Re-generating cache upon changes to the Python installation.
#
# Return values:
# - `${out_var_abs}`: absolute path (cached).
# - `${out_var_rel}`: `PYTHON_ROOT` relative path (not cached).
function(find_python_module_file
  module_file
  out_var_abs
  out_var_rel
  )

  # Reset if the file isn't found.
  if(DEFINED ${out_var_abs})
    if(NOT EXISTS ${${out_var_abs}})
      unset(${out_var_abs} CACHE)
    endif()
  endif()

  # Reset if the version number or Python path changes.
  set(_python_mod_file_deps_test "${PYTHON_LIBPATH};${PYTHON_VERSION}")
  if(DEFINED _${out_var_abs}_DEPS)
    if(NOT (_${out_var_abs}_DEPS STREQUAL _python_mod_file_deps_test))
      unset(${out_var_abs} CACHE)
    endif()
  else()
    unset(${out_var_abs} CACHE)
  endif()

  path_strip_trailing_slash(_python_root "${PYTHON_LIBPATH}")
  set(_python_base "${_python_root}/python${PYTHON_VERSION}")
  # This always moves up one level (even if there is a trailing slash).
  get_filename_component(_python_root "${_python_root}" DIRECTORY)
  path_ensure_trailing_slash(_python_root "${_python_root}")

  if(NOT (DEFINED ${out_var_abs}))
    message(STATUS "Finding Python Module File: ${module_file}")
    find_file(${out_var_abs}
      NAMES
        "${module_file}"
      PATHS
        "${_python_base}"
      PATH_SUFFIXES
        "site-packages"
        "dist-packages"
        "vendor-packages"
        ""
      NO_DEFAULT_PATH
    )
    if(${out_var_abs})
      # Internal because this is only to track changes (users never need to manipulate it).
      set(_${out_var_abs}_DEPS "${_python_mod_file_deps_test}" CACHE INTERNAL "")
    endif()
  endif()

  if(${out_var_abs})
    string(LENGTH "${_python_root}" _python_root_len)
    string(SUBSTRING ${${out_var_abs}} ${_python_root_len} -1 ${out_var_rel})
    set(${out_var_rel} "${${out_var_rel}}" PARENT_SCOPE)
  endif()
endfunction()


# like Python's 'print(dir())'
function(print_all_vars)
  get_cmake_property(_vars VARIABLES)
  foreach(_var ${_vars})
    message(STATUS "${_var}=${${_var}}")
  endforeach()
endfunction()

# When `_dependency` is disabled, force `_setting` to `_val`.
#
# Modifies in parent scope:
# - `${_setting}`: set to `_val` when the dependency is disabled.
function(set_and_warn_dependency
  _dependency _setting _val)
  if(NOT ${${_dependency}} AND ${${_setting}})
    if(WITH_STRICT_BUILD_OPTIONS)
      message(SEND_ERROR "${_dependency} disabled but required by ${_setting}")
    else()
      message(STATUS "${_dependency} is disabled, setting ${_setting}=${_val}")
    endif()
    set(${_setting} ${_val} PARENT_SCOPE)
  endif()
endfunction()

# When `_dependency` is enabled, force `_setting` to `_val`.
# Both should be defined, warn if they're not.
#
# Modifies in parent scope:
# - `${_setting}`: set to `_val` when the dependency is enabled.
function(set_and_warn_incompatible
  _dependency _setting _val)
  if(NOT DEFINED ${_dependency})
    message(STATUS "${_dependency} not defined!")
  elseif(NOT DEFINED ${_setting})
    message(STATUS "${_setting} not defined!")
  elseif(${${_dependency}} AND ${${_setting}})
    if(WITH_STRICT_BUILD_OPTIONS)
      message(SEND_ERROR "${_dependency} enabled but incompatible with ${_setting}")
    else()
      message(STATUS "${_dependency} is enabled but incompatible, setting ${_setting}=${_val}")
    endif()
    set(${_setting} ${_val} PARENT_SCOPE)
  endif()
endfunction()

# Disable `_setting` when the library is not found.
#
# Modifies in parent scope:
# - `${_setting}`: set to `OFF` when the library is not found.
function(set_and_warn_library_found
  _library_name _library_found _setting)
  if(((NOT ${_library_found}) OR (NOT ${${_library_found}})) AND ${${_setting}})
    if(WITH_STRICT_BUILD_OPTIONS)
      message(SEND_ERROR "${_library_name} required but not found")
    else()
      message(STATUS "${_library_name} not found, disabling ${_setting}")
    endif()
    set(${_setting} OFF PARENT_SCOPE)
  endif()
endfunction()

# Modifies in parent scope:
# - `CMAKE_IGNORE_PATH`: set to system implicit paths.
function(without_system_libs_begin)
  set(CMAKE_IGNORE_PATH
    "${CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES}"
    "${CMAKE_SYSTEM_INCLUDE_PATH}"
    "${CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES}"
    "${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES}"
    PARENT_SCOPE
  )
endfunction()

# Modifies in parent scope:
# - `CMAKE_IGNORE_PATH`: unset.
function(without_system_libs_end)
  unset(CMAKE_IGNORE_PATH PARENT_SCOPE)
endfunction()

# Utility to gather and install precompiled shared libraries.
#
# Modifies in parent scope:
# - `PLATFORM_BUNDLED_LIBRARIES`: appended with found libraries.
# - `PLATFORM_BUNDLED_LIBRARY_DIRS`: appended with the library directory.
function(add_bundled_libraries library_dir)
  if(DEFINED LIBDIR)
    set(_library_dir ${LIBDIR}/${library_dir})
    if(WIN32)
      file(GLOB _all_library_versions ${_library_dir}/*\.dll)
    elseif(APPLE)
      file(GLOB _all_library_versions ${_library_dir}/*\.dylib*)
    else()
      file(GLOB _all_library_versions ${_library_dir}/*\.so*)
    endif()
    list(APPEND PLATFORM_BUNDLED_LIBRARIES ${_all_library_versions})
    list(APPEND PLATFORM_BUNDLED_LIBRARY_DIRS ${_library_dir})
    set(PLATFORM_BUNDLED_LIBRARIES "${PLATFORM_BUNDLED_LIBRARIES}" PARENT_SCOPE)
    set(PLATFORM_BUNDLED_LIBRARY_DIRS "${PLATFORM_BUNDLED_LIBRARY_DIRS}" PARENT_SCOPE)
  endif()
endfunction()

# Modifies in parent scope:
# - `WINDOWS_SHARED_MANIFEST_DEBUG`: appended with files (when debug).
# - `WINDOWS_SHARED_MANIFEST_RELEASE`: appended with files (when release).
function(windows_install_shared_manifest)
  set(options OPTIONAL DEBUG RELEASE ALL)
  set(oneValueArgs "")
  set(multiValueArgs FILES)
  cmake_parse_arguments(
    WINDOWS_INSTALL
    "${options}"
    "${oneValueArgs}"
    "${multiValueArgs}"
    ${ARGN}
  )
  # If none of the options are set assume ALL.
  unset(WINDOWS_CONFIGURATIONS)
  if(NOT WINDOWS_INSTALL_ALL AND
     NOT WINDOWS_INSTALL_DEBUG AND
     NOT WINDOWS_INSTALL_RELEASE)
    set(WINDOWS_INSTALL_ALL TRUE)
  endif()
  # If all is set, turn both DEBUG and RELEASE on.
  if(WINDOWS_INSTALL_ALL)
    set(WINDOWS_INSTALL_DEBUG TRUE)
    set(WINDOWS_INSTALL_RELEASE TRUE)
  endif()
  if(WINDOWS_INSTALL_DEBUG)
    set(WINDOWS_CONFIGURATIONS "${WINDOWS_CONFIGURATIONS};Debug")
  endif()
  if(WINDOWS_INSTALL_RELEASE)
    set(WINDOWS_CONFIGURATIONS "${WINDOWS_CONFIGURATIONS};Release;RelWithDebInfo;MinSizeRel")
  endif()
  if(NOT WITH_PYTHON_MODULE)
    # Blender executable with manifest.
    if(WINDOWS_INSTALL_DEBUG)
      list(APPEND WINDOWS_SHARED_MANIFEST_DEBUG ${WINDOWS_INSTALL_FILES})
    endif()
    if(WINDOWS_INSTALL_RELEASE)
      list(APPEND WINDOWS_SHARED_MANIFEST_RELEASE ${WINDOWS_INSTALL_FILES})
    endif()
    set(WINDOWS_SHARED_MANIFEST_DEBUG "${WINDOWS_SHARED_MANIFEST_DEBUG}" PARENT_SCOPE)
    set(WINDOWS_SHARED_MANIFEST_RELEASE "${WINDOWS_SHARED_MANIFEST_RELEASE}" PARENT_SCOPE)
    install(
      FILES ${WINDOWS_INSTALL_FILES}
      DESTINATION "blender.shared"
      CONFIGURATIONS ${WINDOWS_CONFIGURATIONS}
    )
  else()
    # Python module without manifest.
    install(
      FILES ${WINDOWS_INSTALL_FILES}
      DESTINATION "bpy"
      CONFIGURATIONS ${WINDOWS_CONFIGURATIONS}
    )
  endif()
endfunction()

function(windows_generate_manifest)
  set(options "")
  set(oneValueArgs OUTPUT NAME)
  set(multiValueArgs FILES)
  cmake_parse_arguments(
    WINDOWS_MANIFEST
    "${options}"
    "${oneValueArgs}"
    "${multiValueArgs}"
    ${ARGN}
  )
  set(MANIFEST_LIBS "")
  foreach(lib ${WINDOWS_MANIFEST_FILES})
    get_filename_component(filename ${lib} NAME)
    set(MANIFEST_LIBS "${MANIFEST_LIBS}    <file name=\"${filename}\"/>\n")
  endforeach()
  configure_file(
    ${CMAKE_SOURCE_DIR}/release/windows/manifest/blender.manifest.in
    ${WINDOWS_MANIFEST_OUTPUT}
    @ONLY
  )
endfunction()

function(windows_generate_shared_manifest)
  if(WINDOWS_SHARED_MANIFEST_DEBUG)
    windows_generate_manifest(
      FILES "${WINDOWS_SHARED_MANIFEST_DEBUG}"
      OUTPUT "${CMAKE_BINARY_DIR}/Debug/blender.shared.manifest"
      NAME "blender.shared"
    )
    install(
      FILES ${CMAKE_BINARY_DIR}/Debug/blender.shared.manifest
      DESTINATION "blender.shared"
      CONFIGURATIONS Debug
    )
  endif()
  if(WINDOWS_SHARED_MANIFEST_RELEASE)
    windows_generate_manifest(
      FILES "${WINDOWS_SHARED_MANIFEST_RELEASE}"
      OUTPUT "${CMAKE_BINARY_DIR}/Release/blender.shared.manifest"
      NAME "blender.shared"
    )
    install(
      FILES ${CMAKE_BINARY_DIR}/Release/blender.shared.manifest
      DESTINATION "blender.shared"
      CONFIGURATIONS Release;RelWithDebInfo;MinSizeRel
    )
  endif()
endfunction()

# Modifies in parent scope:
# - `WINDOWS_SHARED_MANIFEST_DEBUG`: accumulated via `windows_install_shared_manifest`.
# - `WINDOWS_SHARED_MANIFEST_RELEASE`: accumulated via `windows_install_shared_manifest`.
function(windows_process_platform_bundled_libraries library_deps)
  if(NOT "${library_deps}" STREQUAL "")
    set(next_library_mode "ALL")
    foreach(library ${library_deps})
      string(TOUPPER "${library}" library_upper)
      if(("${library_upper}" STREQUAL "RELEASE") OR
         ("${library_upper}" STREQUAL "DEBUG") OR
         ("${library_upper}" STREQUAL "ALL"))
        set(next_library_mode "${library_upper}")
      else()
        windows_install_shared_manifest(
          FILES ${library}
          ${next_library_mode}
        )
        set(next_library_mode "ALL")
      endif()
    endforeach()
    set(WINDOWS_SHARED_MANIFEST_DEBUG "${WINDOWS_SHARED_MANIFEST_DEBUG}" PARENT_SCOPE)
    set(WINDOWS_SHARED_MANIFEST_RELEASE "${WINDOWS_SHARED_MANIFEST_RELEASE}" PARENT_SCOPE)
  endif()
endfunction()

function(compile_sources_as_cpp
  library
  sources
  define
  )

  # On Windows, MSVC/Clang echo the filename being compiled to the console.
  # Ninja suppresses this only for recognized C++ extensions (.cc/.cpp etc.),
  # not .glsl. Generate a .cc wrapper per source so Ninja suppresses the echo.
  # On other platforms, just set LANGUAGE CXX directly on each source file.

  if(WIN32)
    foreach(glsl_file ${sources})
      get_filename_component(_file_from ${CMAKE_CURRENT_SOURCE_DIR}/${glsl_file}   REALPATH)
      get_filename_component(_file_to   ${CMAKE_CURRENT_BINARY_DIR}/${glsl_file}.cc REALPATH)
      file(WRITE "${_file_to}" "#include \"${_file_from}\"\n")
      list(APPEND sources ${_file_to})
      # Mark the original file as header only, so no attempt will be made at compiling it
      # regardless of extention.
      set_source_files_properties(${glsl_file} PROPERTIES HEADER_FILE_ONLY TRUE)
    endforeach()
  else()
    foreach(glsl_file ${sources})
      set_source_files_properties(${glsl_file} PROPERTIES LANGUAGE CXX)
    endforeach()
  endif()

  add_library(${library} OBJECT ${sources})
  set_target_properties(${library} PROPERTIES LINKER_LANGUAGE CXX)
  target_include_directories(${library} PUBLIC ${INC_GLSL})
  target_compile_definitions(${library} PRIVATE ${define} -DGPU_SHADER)

  # avoid noisy warnings
  if((CMAKE_C_COMPILER_ID STREQUAL "GNU") OR (CMAKE_C_COMPILER_ID MATCHES "Clang"))
    target_compile_options(${library} PRIVATE "-Wno-unused-result")
    target_compile_options(${library} PRIVATE "-Wno-missing-declarations")
    # Would be nice to enable the warning once we support references.
    target_compile_options(${library} PRIVATE "-Wno-uninitialized")
    # To compile libraries.
    target_compile_options(${library} PRIVATE "-Wno-pragma-once-outside-header")
    target_compile_options(${library} PRIVATE "-Wno-unknown-pragmas")
  elseif(MSVC)
    # Equivalent to "-Wno-uninitialized"
    target_compile_options(${library} PRIVATE "/wd4700")
    # Disable "potential divide by 0" warning
    target_compile_options(${library} PRIVATE "/wd4723")
    # Disable unkown pragma warning
    target_compile_options(${library} PRIVATE "/wd4068")
    # Disable unknown attribute warning
    target_compile_options(${library} PRIVATE "/wd5030")
    target_compile_options(${library} PRIVATE "/wd5222")
  endif()
  if(WIN32 AND NOT MSVC_CLANG)
    set_target_properties(${library} PROPERTIES STATIC_LIBRARY_OPTIONS "-ignore:4006")
  endif()
endfunction()

# NOTE: must be a macro, calls `remove_cc_flag` on Windows
# which modifies `CMAKE_*_FLAGS` in the caller's scope.
macro(optimize_debug_target executable)
  if(WITH_OPTIMIZED_BUILD_TOOLS)
    if(WIN32)
      remove_c_and_cxx_flag("/Od" "/RTC1")
      target_compile_options(${executable} PRIVATE "/Ox")
      target_compile_definitions(${executable} PRIVATE "_ITERATOR_DEBUG_LEVEL=0")
    else()
      target_compile_options(${executable} PRIVATE "-O2")
    endif()
  endif()
endmacro()
