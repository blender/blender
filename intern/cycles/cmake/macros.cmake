# SPDX-FileCopyrightText: 2011-2022 Blender Foundation
#
# SPDX-License-Identifier: Apache-2.0

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
  # Use: "optimized libfoo optimized libbar debug libfoo_d debug libbar_d"
  # NOT: "optimized libfoo libbar debug libfoo_d libbar_d"
  #
  # TODO(sergey): This is the same as Blender's side CMake. Find a way to avoid duplication
  # somehow in a way which allows to have Cycles standalone.
  if(NOT "${library_deps}" STREQUAL "")
    set(next_library_mode "")
    set(next_library_scope "PUBLIC")
    foreach(library ${library_deps})
      string(TOLOWER "${library}" library_lower)
      if(("${library_lower}" STREQUAL "optimized") OR
         ("${library_lower}" STREQUAL "debug"))
        set(next_library_mode "${library_lower}")
      elseif(("${library}" STREQUAL "PUBLIC") OR
             ("${library}" STREQUAL "PRIVATE") OR
             ("${library}" STREQUAL "INTERFACE"))
        set(next_library_scope "${library}")
      else()
        if("${next_library_mode}" STREQUAL "optimized")
          target_link_libraries(${target} ${next_library_scope} optimized ${library})
        elseif("${next_library_mode}" STREQUAL "debug")
          target_link_libraries(${target} ${next_library_scope} debug ${library})
        else()
          target_link_libraries(${target} ${next_library_scope} ${library})
        endif()
        set(next_library_mode "")
      endif()
    endforeach()
  endif()

  # On windows vcpkg goes out of its way to make its libs the preferred
  # libs, and needs to be explicitly be told not to do that.
  if(WIN32)
    set_target_properties(${target} PROPERTIES VS_GLOBAL_VcpkgEnabled "false")
  endif()

  cycles_set_solution_folder(${target})
endmacro()

macro(cycles_external_libraries_append libraries)
  # Dependencies with modern targets, these always exist even when optional deps are disabled.
  list(APPEND ${libraries}
    bf::dependencies::openimageio
    bf::dependencies::pthreads
    bf::dependencies::zlib
    bf::dependencies::optional::embree
    bf::dependencies::optional::opencolorio
    bf::dependencies::optional::openexr
    bf::dependencies::optional::openimagedenoise
    bf::dependencies::optional::openpgl
    bf::dependencies::optional::opensubdiv
    bf::dependencies::optional::openvdb
    bf::dependencies::optional::osl
    bf::dependencies::optional::pugixml
    bf::dependencies::optional::python
    bf::dependencies::optional::webp
    ${CMAKE_DL_LIBS}
    ${PLATFORM_LINKLIBS}
  )

  # Platform-specific frameworks and libraries.
  if(APPLE)
    list(APPEND ${libraries} "-framework Foundation")
    if(WITH_USD)
      list(APPEND ${libraries} "-framework CoreVideo -framework Cocoa -framework OpenGL")
    endif()
    if(WITH_OPENCOLORIO)
      list(APPEND ${libraries} "-framework IOKit")
      list(APPEND ${libraries} "-framework Carbon")
    endif()
    if(WITH_OPENIMAGEDENOISE)
      if("${CMAKE_OSX_ARCHITECTURES}" STREQUAL "arm64")
        list(APPEND ${libraries} "-framework Accelerate")
      endif()
    endif()
  elseif(WIN32)
    if(WITH_USD)
      list(APPEND ${libraries} "opengl32")
    endif()
  endif()
  if(UNIX AND NOT APPLE)
    list(APPEND ${libraries} "-lm -lc -lutil")
  endif()

  # GPU backends.
  if(WITH_CYCLES_DEVICE_CUDA OR WITH_CYCLES_DEVICE_OPTIX)
    if(WITH_CUDA_DYNLOAD)
      list(APPEND ${libraries} extern_cuew)
    else()
      list(APPEND ${libraries} ${CUDA_CUDA_LIBRARY})
    endif()
  endif()

  if(WITH_CYCLES_DEVICE_HIP AND WITH_HIP_DYNLOAD)
    list(APPEND ${libraries} extern_hipew)
  endif()

  if(WITH_CYCLES_DEVICE_ONEAPI AND WITH_CYCLES_EMBREE  AND EMBREE_SYCL_SUPPORT)
    list(APPEND ${libraries} ${SYCL_LIBRARIES})
  endif()

  # Compatibility libraries.
  if(UNIX AND NOT APPLE)
    if(CYCLES_STANDALONE_REPOSITORY)
      list(APPEND ${libraries} extern_libc_compat)
      # Hack to solve linking order issue where external libs depend
      # on our compatibility lib.
      list(APPEND ${libraries} $<TARGET_FILE:extern_libc_compat>)
    else()
      list(APPEND ${libraries} bf_intern_libc_compat)
    endif()
  endif()

  if(NOT CYCLES_STANDALONE_REPOSITORY)
    list(APPEND ${libraries} bf_intern_guardedalloc)
  endif()
endmacro()

macro(cycles_install_libraries target)
  # Copy DLLs for dynamically linked libraries.
  if(WIN32)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
      install(
        FILES
        ${TBB_ROOT_DIR}/bin/tbb_debug${CMAKE_SHARED_LIBRARY_SUFFIX}
        ${OPENVDB_ROOT_DIR}/bin/openvdb_d${CMAKE_SHARED_LIBRARY_SUFFIX}
        DESTINATION ${CMAKE_INSTALL_PREFIX})
    else()
      install(
        FILES
        ${TBB_ROOT_DIR}/bin/tbb${CMAKE_SHARED_LIBRARY_SUFFIX}
        ${OPENVDB_ROOT_DIR}/bin/openvdb${CMAKE_SHARED_LIBRARY_SUFFIX}
        DESTINATION ${CMAKE_INSTALL_PREFIX})
    endif()
  endif()
endmacro()
