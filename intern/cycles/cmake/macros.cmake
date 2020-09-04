# Copyright 2011-2020 Blender Foundation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

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

# pair of macros to allow libraries to be specify files to install, but to
# only install them at the end so the directories don't get cleared with
# the files in them. used by cycles to install addon.
macro(delayed_install
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

if(NOT COMMAND remove_cc_flag)
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
endif()

macro(remove_extra_strict_flags)
  if(CMAKE_COMPILER_IS_GNUCC)
    remove_cc_flag("-Wunused-parameter")
  endif()

  if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    remove_cc_flag("-Wunused-parameter")
  endif()

  if(MSVC)
    # TODO
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
  CHECK_INCLUDE_FILE_CXX("unordered_map" HAVE_STD_UNORDERED_MAP_HEADER)
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
      message(STATUS "Found unordered_map/set in std namespace.")

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
        message(STATUS "Found unordered_map/set in std::tr1 namespace.")

        set(HAVE_UNORDERED_MAP "TRUE")
        set(UNORDERED_MAP_INCLUDE_PREFIX "")
        set(UNORDERED_MAP_NAMESPACE "std::tr1")
      else()
        message(STATUS "Found <unordered_map> but cannot find either std::unordered_map "
                "or std::tr1::unordered_map.")
      endif()
    endif()
  else()
    CHECK_INCLUDE_FILE_CXX("tr1/unordered_map" HAVE_UNORDERED_MAP_IN_TR1_NAMESPACE)
    if(HAVE_UNORDERED_MAP_IN_TR1_NAMESPACE)
      message(STATUS "Found unordered_map/set in std::tr1 namespace.")

      set(HAVE_UNORDERED_MAP "TRUE")
      set(UNORDERED_MAP_INCLUDE_PREFIX "tr1")
      set(UNORDERED_MAP_NAMESPACE "std::tr1")
    else()
      message(STATUS "Unable to find <unordered_map> or <tr1/unordered_map>. ")
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
  set(SHARED_PTR_FOUND FALSE)
  CHECK_INCLUDE_FILE_CXX(memory HAVE_STD_MEMORY_HEADER)
  if(HAVE_STD_MEMORY_HEADER)
    # Finding the memory header doesn't mean that shared_ptr is in std
    # namespace.
    #
    # In particular, MSVC 2008 has shared_ptr declared in std::tr1.  In
    # order to support this, we do an extra check to see which namespace
    # should be used.
    include(CheckCXXSourceCompiles)
    CHECK_CXX_SOURCE_COMPILES("#include <memory>
                               int main() {
                                 std::shared_ptr<int> int_ptr;
                                 return 0;
                               }"
                              HAVE_SHARED_PTR_IN_STD_NAMESPACE)

    if(HAVE_SHARED_PTR_IN_STD_NAMESPACE)
      message("-- Found shared_ptr in std namespace using <memory> header.")
      set(SHARED_PTR_FOUND TRUE)
    else()
      CHECK_CXX_SOURCE_COMPILES("#include <memory>
                                 int main() {
                                 std::tr1::shared_ptr<int> int_ptr;
                                 return 0;
                                 }"
                                HAVE_SHARED_PTR_IN_TR1_NAMESPACE)
      if(HAVE_SHARED_PTR_IN_TR1_NAMESPACE)
        message("-- Found shared_ptr in std::tr1 namespace using <memory> header.")
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
        message("-- Found shared_ptr in std::tr1 namespace using <tr1/memory> header.")
        set(SHARED_PTR_TR1_MEMORY_HEADER TRUE)
        set(SHARED_PTR_TR1_NAMESPACE TRUE)
        set(SHARED_PTR_FOUND TRUE)
      endif()
    endif()
  endif()
endmacro()

function(cycles_set_solution_folder target)
  if(WINDOWS_USE_VISUAL_STUDIO_FOLDERS)
    get_filename_component(folderdir ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY)
    string(REPLACE ${CMAKE_SOURCE_DIR} "" folderdir ${folderdir})
    set_target_properties(${target} PROPERTIES FOLDER ${folderdir})
  endif()
endfunction()

macro(cycles_add_library target library_deps)
  add_library(${target} ${ARGN})

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
  # keeps track of whiuch build type library is to provided for:
  #
  # - If "debug" or "optimized" word is found, the next element in the list is expected to be
  #   a library which will be passed to target_link_libraries() under corresponding build type.
  #
  # - If there is no "debug" or "optimized" used library is specified for all build types.
  #
  # NOTE: If separated libraries for debug and release ar eneeded every library is the list are
  # to be prefixed explicitly.
  #
  #  Use: "optimized libfoo optimized libbar debug libfoo_d debug libbar_d"
  #  NOT: "optimized libfoo libbar debug libfoo_d libbar_d"
  #
  # TODO(sergey): This is the same as Blender's side CMake. Find a way to avoid duplication
  # somehow in a way which allows to have Cycles standalone.
  if(NOT "${library_deps}" STREQUAL "")
    set(next_library_mode "")
    foreach(library ${library_deps})
      string(TOLOWER "${library}" library_lower)
      if(("${library_lower}" STREQUAL "optimized") OR
         ("${library_lower}" STREQUAL "debug"))
        set(next_library_mode "${library_lower}")
      else()
        if("${next_library_mode}" STREQUAL "optimized")
          target_link_libraries(${target} optimized ${library})
        elseif("${next_library_mode}" STREQUAL "debug")
          target_link_libraries(${target} debug ${library})
        else()
          target_link_libraries(${target} ${library})
        endif()
        set(next_library_mode "")
      endif()
    endforeach()
  endif()

  cycles_set_solution_folder(${target})
endmacro()

# Cycles library dependencies common to all executables

macro(cycles_link_directories)
  if(WITH_OPENCOLORIO)
    link_directories(${OPENCOLORIO_LIBPATH})
  endif()
  if(WITH_OPENVDB)
    link_directories(${OPENVDB_LIBPATH} ${BLOSC_LIBPATH})
  endif()
  if(WITH_OPENSUBDIV)
    link_directories(${OPENSUBDIV_LIBPATH})
  endif()
  if(WITH_OPENIMAGEDENOISE)
    link_directories(${OPENIMAGEDENOISE_LIBPATH})
  endif()

  link_directories(
    ${OPENIMAGEIO_LIBPATH}
    ${BOOST_LIBPATH}
    ${PNG_LIBPATH}
    ${JPEG_LIBPATH}
    ${ZLIB_LIBPATH}
    ${TIFF_LIBPATH}
    ${OPENEXR_LIBPATH}
    ${OPENJPEG_LIBPATH}
  )
endmacro()

macro(cycles_target_link_libraries target)
  if(WITH_CYCLES_LOGGING)
    target_link_libraries(${target} ${GLOG_LIBRARIES} ${GFLAGS_LIBRARIES})
  endif()
  if(WITH_CYCLES_OSL)
    target_link_libraries(${target} ${OSL_LIBRARIES} ${LLVM_LIBRARY})
  endif()
  if(WITH_CYCLES_EMBREE)
    target_link_libraries(${target} ${EMBREE_LIBRARIES})
  endif()
  if(WITH_OPENSUBDIV)
    target_link_libraries(${target} ${OPENSUBDIV_LIBRARIES})
  endif()
  if(WITH_OPENCOLORIO)
    target_link_libraries(${target} ${OPENCOLORIO_LIBRARIES})
  endif()
  if(WITH_OPENVDB)
    target_link_libraries(${target} ${OPENVDB_LIBRARIES} ${BLOSC_LIBRARIES})
  endif()
  if(WITH_OPENIMAGEDENOISE)
    target_link_libraries(${target} ${OPENIMAGEDENOISE_LIBRARIES})
  endif()
  target_link_libraries(
    ${target}
    ${OPENIMAGEIO_LIBRARIES}
    ${PNG_LIBRARIES}
    ${JPEG_LIBRARIES}
    ${TIFF_LIBRARY}
    ${OPENJPEG_LIBRARIES}
    ${OPENEXR_LIBRARIES}
    ${OPENEXR_LIBRARIES} # For circular dependencies between libs.
    ${PUGIXML_LIBRARIES}
    ${BOOST_LIBRARIES}
    ${ZLIB_LIBRARIES}
    ${CMAKE_DL_LIBS}
    ${PTHREADS_LIBRARIES}
    ${PLATFORM_LINKLIBS}
  )

  if(WITH_CUDA_DYNLOAD)
    target_link_libraries(${target} extern_cuew)
  else()
    target_link_libraries(${target} ${CUDA_CUDA_LIBRARY})
  endif()

  if(CYCLES_STANDALONE_REPOSITORY)
    target_link_libraries(${target} extern_numaapi)
  else()
    target_link_libraries(${target} bf_intern_numaapi)
  endif()

  if(UNIX AND NOT APPLE)
    if(CYCLES_STANDALONE_REPOSITORY)
      target_link_libraries(${target} extern_libc_compat)
    else()
      target_link_libraries(${target} bf_intern_libc_compat)
    endif()
  endif()

  if(NOT CYCLES_STANDALONE_REPOSITORY)
    target_link_libraries(${target} bf_intern_guardedalloc)
  endif()
endmacro()

macro(cycles_install_libraries target)
  # Copy DLLs for dynamically linked libraries.
  if(WIN32)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
      install(
        FILES
	${TBB_ROOT_DIR}/lib/debug/tbb_debug${CMAKE_SHARED_LIBRARY_SUFFIX}
        ${OPENVDB_ROOT_DIR}/bin/openvdb_d${CMAKE_SHARED_LIBRARY_SUFFIX}
	DESTINATION $<TARGET_FILE_DIR:${target}>)
    else()
      install(
        FILES
	${TBB_ROOT_DIR}/lib/tbb${CMAKE_SHARED_LIBRARY_SUFFIX}
        ${OPENVDB_ROOT_DIR}/bin/openvdb${CMAKE_SHARED_LIBRARY_SUFFIX}
	DESTINATION $<TARGET_FILE_DIR:${target}>)
    endif()
  endif()
endmacro()
