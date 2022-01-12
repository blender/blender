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

function(cycles_set_solution_folder target)
  if(IDE_GROUP_PROJECTS_IN_FOLDERS)
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

  if(WITH_CYCLES_DEVICE_CUDA OR WITH_CYCLES_DEVICE_OPTIX)
    if(WITH_CUDA_DYNLOAD)
      target_link_libraries(${target} extern_cuew)
    else()
      target_link_libraries(${target} ${CUDA_CUDA_LIBRARY})
    endif()
  endif()

  if(WITH_CYCLES_DEVICE_HIP AND WITH_HIP_DYNLOAD)
    target_link_libraries(${target} extern_hipew)
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
