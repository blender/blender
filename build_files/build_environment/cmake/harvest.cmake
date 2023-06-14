# SPDX-FileCopyrightText: 2017-2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

########################################################################
# Copy all generated files to the proper structure as blender prefers
########################################################################

if(NOT DEFINED HARVEST_TARGET)
  set(HARVEST_TARGET ${CMAKE_CURRENT_SOURCE_DIR}/Harvest)
endif()
message("HARVEST_TARGET = ${HARVEST_TARGET}")

if(WIN32)

  if(BUILD_MODE STREQUAL Release)
    add_custom_target(Harvest_Release_Results
      COMMAND # JPEG rename lib-file + copy include.
      ${CMAKE_COMMAND} -E copy ${LIBDIR}/jpeg/lib/jpeg-static.lib ${HARVEST_TARGET}/jpeg/lib/libjpeg.lib &&
      ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/jpeg/include/ ${HARVEST_TARGET}/jpeg/include/ &&
      # PNG.
      ${CMAKE_COMMAND} -E copy ${LIBDIR}/png/lib/libpng16_static.lib ${HARVEST_TARGET}/png/lib/libpng.lib &&
      ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/png/include/ ${HARVEST_TARGET}/png/include/ &&
      # FREEGLUT -> OPENGL.
      ${CMAKE_COMMAND} -E copy ${LIBDIR}/freeglut/lib/freeglut_static.lib ${HARVEST_TARGET}/opengl/lib/freeglut_static.lib &&
      ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/freeglut/include/ ${HARVEST_TARGET}/opengl/include/ &&

      DEPENDS
    )
  endif()

else()

  function(harvest from to)
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
    set(set_rpath_cmd python3 ${CMAKE_CURRENT_SOURCE_DIR}/darwin/set_rpath.py @loader_path)
  else()
    set(set_rpath_cmd patchelf --set-rpath $ORIGIN)
  endif()

  function(harvest_rpath_lib from to pattern)
    harvest(${from} ${to} ${pattern})

    install(CODE "\
      cmake_policy(SET CMP0009 NEW)\n
      file(GLOB_RECURSE shared_libs ${HARVEST_TARGET}/${to}/${pattern}) \n
      foreach(f \${shared_libs}) \n
        if(IS_SYMLINK \${f})\n
          if(APPLE)\n
            file(REMOVE_RECURSE \${f})
          endif()\n
        else()\n
          execute_process(COMMAND ${set_rpath_cmd} \${f}) \n
        endif()\n
      endforeach()")
  endfunction()

  # Set rpath on utility binaries assuming they are run from their install location.
  function(harvest_rpath_bin from to pattern)
    harvest(${from} ${to} ${pattern})

    install(CODE "\
      file(GLOB_RECURSE shared_libs ${HARVEST_TARGET}/${to}/${pattern}) \n
      foreach(f \${shared_libs}) \n
        execute_process(COMMAND ${set_rpath_cmd}/../lib; \${f}) \n
      endforeach()")
  endfunction()

  # Set rpath on Python module to point to the shared libraries folder in the Blender
  # installation.
  function(harvest_rpath_python from to pattern)
    harvest(${from} ${to} ${pattern})

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

  harvest(alembic/include alembic/include "*.h")
  harvest(alembic/lib/libAlembic.a alembic/lib/libAlembic.a)
  harvest_rpath_bin(alembic/bin alembic/bin "*")
  harvest(brotli/include brotli/include "*.h")
  harvest(brotli/lib brotli/lib "*.a")
  harvest(boost/include boost/include "*")
  harvest_rpath_lib(boost/lib boost/lib "*${SHAREDLIBEXT}*")
  harvest(imath/include imath/include "*.h")
  harvest_rpath_lib(imath/lib imath/lib "*${SHAREDLIBEXT}*")
  harvest(ffmpeg/include ffmpeg/include "*.h")
  harvest(ffmpeg/lib ffmpeg/lib "*.a")
  harvest(fftw3/include fftw3/include "*.h")
  harvest(fftw3/lib fftw3/lib "*.a")
  harvest(flac/lib sndfile/lib "libFLAC.a")
  harvest(freetype/include freetype/include "*.h")
  harvest(freetype/lib/libfreetype2ST.a freetype/lib/libfreetype.a)
  harvest(fribidi/include fribidi/include "*.h")
  harvest(fribidi/lib fribidi/lib "*.a")
  harvest(epoxy/include epoxy/include "*.h")
  harvest(epoxy/lib epoxy/lib "*.a")
  harvest(gmp/include gmp/include "*.h")
  harvest(gmp/lib gmp/lib "*.a")
  harvest(harfbuzz/include harfbuzz/include "*.h")
  harvest(harfbuzz/lib harfbuzz/lib "*.a")
  harvest(jemalloc/include jemalloc/include "*.h")
  harvest(jemalloc/lib jemalloc/lib "*.a")
  harvest(jpeg/include jpeg/include "*.h")
  harvest(jpeg/lib jpeg/lib "libjpeg.a")
  harvest(lame/lib ffmpeg/lib "*.a")
  if(NOT APPLE)
    harvest(level-zero/include/level_zero level-zero/include/level_zero "*.h")
    harvest(level-zero/lib level-zero/lib "*${SHAREDLIBEXT}*")
  endif()
  harvest(llvm/bin llvm/bin "clang-format")
  if(BUILD_CLANG_TOOLS)
    harvest(llvm/bin llvm/bin "clang-tidy")
    harvest(llvm/share/clang llvm/share "run-clang-tidy.py")
  endif()
  harvest(llvm/include llvm/include "*")
  harvest(llvm/bin llvm/bin "llvm-config")
  harvest(llvm/lib llvm/lib "libLLVM*.a")
  harvest(llvm/lib llvm/lib "libclang*.a")
  harvest(llvm/lib/clang llvm/lib/clang "*.h")
  if(APPLE)
    harvest(openmp/lib openmp/lib "libomp.dylib")
    harvest(openmp/include openmp/include "*.h")
  endif()
  if(BLENDER_PLATFORM_ARM)
    harvest(sse2neon sse2neon "*.h")
  endif()
  harvest(ogg/lib ffmpeg/lib "*.a")
  harvest(openal/include openal/include "*.h")
  if(UNIX AND NOT APPLE)
    harvest(openal/lib openal/lib "*.a")

    harvest(zlib/include zlib/include "*.h")
    harvest(zlib/lib zlib/lib "*.a")

    harvest(xml2/include xml2/include "*.h")
    harvest(xml2/lib xml2/lib "*.a")

    harvest(
      wayland-protocols/share/wayland-protocols
      wayland-protocols/share/wayland-protocols/
      "*.xml"
    )
    harvest(wayland/bin wayland/bin "wayland-scanner")
    harvest(wayland/include wayland/include "*.h")
    harvest(wayland_libdecor/include wayland_libdecor/include "*.h")
  else()
    harvest(blosc/lib openvdb/lib "*.a")
    harvest(xml2/lib opencollada/lib "*.a")
  endif()
  harvest(opencollada/include/opencollada opencollada/include "*.h")
  harvest(opencollada/lib/opencollada opencollada/lib "*.a")
  harvest(opencolorio/include opencolorio/include "*.h")
  harvest_rpath_lib(opencolorio/lib opencolorio/lib "*${SHAREDLIBEXT}*")
  harvest_rpath_python(
    opencolorio/lib/python${PYTHON_SHORT_VERSION}
    python/lib/python${PYTHON_SHORT_VERSION}
    "*"
  )
  harvest(openexr/include openexr/include "*.h")
  harvest_rpath_lib(openexr/lib openexr/lib "*${SHAREDLIBEXT}*")
  harvest_rpath_bin(openimageio/bin openimageio/bin "idiff")
  harvest_rpath_bin(openimageio/bin openimageio/bin "maketx")
  harvest_rpath_bin(openimageio/bin openimageio/bin "oiiotool")
  harvest(openimageio/include openimageio/include "*")
  harvest_rpath_lib(openimageio/lib openimageio/lib "*${SHAREDLIBEXT}*")
  harvest_rpath_python(
    openimageio/lib/python${PYTHON_SHORT_VERSION}
    python/lib/python${PYTHON_SHORT_VERSION}
    "*"
  )
  harvest(openimagedenoise/include openimagedenoise/include "*")
  harvest(openimagedenoise/lib openimagedenoise/lib "*.a")
  harvest(embree/include embree/include "*.h")
  harvest(embree/lib embree/lib "*.a")
  harvest_rpath_lib(embree/lib embree/lib "*${SHAREDLIBEXT}*")
  harvest(openpgl/include openpgl/include "*.h")
  harvest(openpgl/lib openpgl/lib "*.a")
  harvest(openpgl/lib/cmake/openpgl-${OPENPGL_SHORT_VERSION} openpgl/lib/cmake/openpgl "*.cmake")
  harvest(openjpeg/include/openjpeg-${OPENJPEG_SHORT_VERSION} openjpeg/include "*.h")
  harvest(openjpeg/lib openjpeg/lib "*.a")
  harvest(opensubdiv/include opensubdiv/include "*.h")
  harvest_rpath_lib(opensubdiv/lib opensubdiv/lib "*${SHAREDLIBEXT}*")
  harvest(openvdb/include/openvdb openvdb/include/openvdb "*.h")
  harvest(openvdb/include/nanovdb openvdb/include/nanovdb "*.h")
  harvest_rpath_lib(openvdb/lib openvdb/lib "*${SHAREDLIBEXT}*")
  harvest_rpath_python(
    openvdb/lib/python${PYTHON_SHORT_VERSION}
    python/lib/python${PYTHON_SHORT_VERSION}
    "*pyopenvdb*"
  )
  harvest(xr_openxr_sdk/include/openxr xr_openxr_sdk/include/openxr "*.h")
  harvest(xr_openxr_sdk/lib xr_openxr_sdk/lib "*.a")
  harvest_rpath_bin(osl/bin osl/bin "oslc")
  harvest(osl/include osl/include "*.h")
  harvest(osl/lib osl/lib "*.a")
  harvest(osl/share/OSL/shaders osl/share/OSL/shaders "*.h")
  harvest(png/include png/include "*.h")
  harvest(png/lib png/lib "*.a")
  harvest(pugixml/include pugixml/include "*.hpp")
  harvest(pugixml/lib pugixml/lib "*.a")
  harvest(python/bin python/bin "python${PYTHON_SHORT_VERSION}")
  harvest(python/include python/include "*h")
  harvest(python/lib python/lib "*")
  harvest(sdl/include/SDL2 sdl/include "*.h")
  harvest(sdl/lib sdl/lib "libSDL2.a")
  harvest(sndfile/include sndfile/include "*.h")
  harvest(sndfile/lib sndfile/lib "*.a")
  harvest(spnav/include spnav/include "*.h")
  harvest(spnav/lib spnav/lib "*.a")
  harvest(tbb/include tbb/include "*.h")
  harvest_rpath_lib(tbb/lib tbb/lib "libtbb${SHAREDLIBEXT}*")
  harvest(theora/lib ffmpeg/lib "*.a")
  harvest(tiff/include tiff/include "*.h")
  harvest(tiff/lib tiff/lib "*.a")
  harvest(vorbis/lib ffmpeg/lib "*.a")
  harvest(opus/lib ffmpeg/lib "*.a")
  harvest(vpx/lib ffmpeg/lib "*.a")
  harvest(x264/lib ffmpeg/lib "*.a")
  harvest(aom/lib ffmpeg/lib "*.a")
  harvest(webp/lib webp/lib "*.a")
  harvest(webp/include webp/include "*.h")
  harvest(usd/include usd/include "*.h")
  harvest_rpath_lib(usd/lib usd/lib "libusd_ms${SHAREDLIBEXT}")
  harvest(usd/lib/usd usd/lib/usd "*")
  harvest_rpath_python(
    usd/lib/python/pxr
    python/lib/python${PYTHON_SHORT_VERSION}/site-packages/pxr
    "*"
  )
  harvest(usd/plugin usd/plugin "*")
  harvest(materialx/include materialx/include "*.h")
  harvest_rpath_lib(materialx/lib materialx/lib "*${SHAREDLIBEXT}*")
  harvest(materialx/libraries materialx/libraries "*")
  harvest(materialx/lib/cmake/MaterialX materialx/lib/cmake/MaterialX "*.cmake")
  harvest_rpath_python(
    materialx/python/MaterialX
    python/lib/python${PYTHON_SHORT_VERSION}/site-packages/MaterialX
    "*"
  )
  # We do not need anything from the resources folder, but the MaterialX config
  # file will complain if the folder does not exist, so just copy the readme.md
  # files to ensure the folder will exist.
  harvest(materialx/resources materialx/resources "README.md")
  harvest(potrace/include potrace/include "*.h")
  harvest(potrace/lib potrace/lib "*.a")
  harvest(haru/include haru/include "*.h")
  harvest(haru/lib haru/lib "*.a")
  harvest(zstd/include zstd/include "*.h")
  harvest(zstd/lib zstd/lib "*.a")
  harvest(shaderc shaderc "*")
  harvest(vulkan_headers vulkan "*")
  harvest_rpath_lib(vulkan_loader/lib vulkan/lib "*${SHAREDLIBEXT}*")
  if(APPLE)
    harvest(vulkan_loader/loader vulkan/loader "*")
  endif()

  if(UNIX AND NOT APPLE)
    harvest(libglu/lib mesa/lib "*${SHAREDLIBEXT}*")
    harvest(mesa/lib64 mesa/lib "*${SHAREDLIBEXT}*")

    harvest(dpcpp dpcpp "*")
    harvest(igc dpcpp/lib/igc "*")
    harvest(ocloc dpcpp/lib/ocloc "*")
  endif()
endif()
