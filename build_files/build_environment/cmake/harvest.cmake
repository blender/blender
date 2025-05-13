# SPDX-FileCopyrightText: 2017-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

########################################################################
# Copy all generated files to the proper structure as blender prefers
########################################################################

if(NOT DEFINED HARVEST_TARGET)
  set(HARVEST_TARGET ${CMAKE_CURRENT_SOURCE_DIR}/Harvest)
endif()
message(STATUS "HARVEST_TARGET = ${HARVEST_TARGET}")

if(WIN32)

  if(BUILD_MODE STREQUAL Release)
    add_custom_target(Harvest_Release_Results
      COMMAND # JPEG rename lib-file + copy include.
      ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/jpeg/lib/jpeg-static.lib
        ${HARVEST_TARGET}/jpeg/lib/libjpeg.lib &&
      ${CMAKE_COMMAND} -E copy_directory
        ${LIBDIR}/jpeg/include/
        ${HARVEST_TARGET}/jpeg/include/ &&

      # PNG.
      ${CMAKE_COMMAND} -E copy
        ${LIBDIR}/png/lib/libpng16_static.lib
        ${HARVEST_TARGET}/png/lib/libpng.lib &&
      ${CMAKE_COMMAND} -E copy_directory
        ${LIBDIR}/png/include/
        ${HARVEST_TARGET}/png/include/
    )
  endif()

else()

  function(harvest project from to)
    set(pattern "")
    foreach(f ${ARGN})
      set(pattern ${f})
    endforeach()

    if(pattern STREQUAL "")
      get_filename_component(dirpath ${to} DIRECTORY)
      get_filename_component(filename ${to} NAME)
      install(
        FILES ${LIBDIR}/${from}
        DESTINATION ${HARVEST_TARGET}/${dirpath}
        RENAME ${filename}
      )
    else()
      install(
        DIRECTORY ${LIBDIR}/${from}/
        DESTINATION ${HARVEST_TARGET}/${to}
        USE_SOURCE_PERMISSIONS
        FILES_MATCHING PATTERN ${pattern}
        PATTERN "pkgconfig" EXCLUDE
        PATTERN "cmake" EXCLUDE
        PATTERN "__pycache__" EXCLUDE
        PATTERN "tests" EXCLUDE
        PATTERN "meson*" EXCLUDE
      )
    endif()
  endfunction()

  # Set rpath on shared libraries to $ORIGIN since all will be installed in the same
  # lib folder, and remove any absolute paths.
  #
  # Ideally this would be done as part of the Blender build since it makes assumptions
  # about where the files will be installed. However it would add patchelf as a new
  # dependency for building.
  #
  # Also removes versioned symlinks, which give errors with macOS notarization.
  if(APPLE)
    set(set_rpath_cmd python3 ${CMAKE_CURRENT_SOURCE_DIR}/utils/set_rpath.py @loader_path)
  else()
    set(set_rpath_cmd patchelf --set-rpath $ORIGIN)
  endif()

  function(harvest_rpath_lib project from to pattern)
    harvest(project ${from} ${to} ${pattern})

    install(CODE "\
      cmake_policy(SET CMP0009 NEW)\n
      file(GLOB_RECURSE shared_libs ${HARVEST_TARGET}/${to}/${pattern}) \n
      foreach(f \${shared_libs}) \n
        if((NOT IS_SYMLINK \${f}) OR APPLE)\n
          execute_process(COMMAND ${set_rpath_cmd} \${f}) \n
        endif()\n
      endforeach()")
  endfunction()

  # Set rpath on utility binaries assuming they are run from their install location.
  function(harvest_rpath_bin project from to pattern)
    harvest(project ${from} ${to} ${pattern})

    install(CODE "\
      file(GLOB_RECURSE shared_libs ${HARVEST_TARGET}/${to}/${pattern}) \n
      foreach(f \${shared_libs}) \n
        execute_process(COMMAND ${set_rpath_cmd}/../lib; \${f}) \n
      endforeach()")
  endfunction()

  # Set rpath on Python module to point to the shared libraries folder in the Blender
  # installation.
  function(harvest_rpath_python project from to pattern)
    harvest(project ${from} ${to} ${pattern})

    install(CODE "\
      file(GLOB_RECURSE shared_libs ${HARVEST_TARGET}/${to}/${pattern}\.so*) \n
      foreach(f \${shared_libs}) \n
        if(IS_SYMLINK \${f})\n
          if(APPLE)\n
            file(REMOVE_RECURSE \${f})
          endif()\n
        else()\n
          get_filename_component(f_dir \${f} DIRECTORY) \n
          file(RELATIVE_PATH relative_dir \${f_dir} ${HARVEST_TARGET}) \n
          execute_process(COMMAND ${set_rpath_cmd}/\${relative_dir}../lib \${f}) \n
        endif()\n
      endforeach()")
  endfunction()

  # Strip all shared/static libraries in the HARVEST_TARGET location.
  function(harvest_strip_all_libraries)
    install(CODE "execute_process(COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/utils/strip_libraries.py ${HARVEST_TARGET})")
  endfunction()
endif()
