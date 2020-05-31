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
# ***** END GPL LICENSE BLOCK *****

########################################################################
# Copy all generated files to the proper strucure as blender prefers
########################################################################

if(NOT DEFINED HARVEST_TARGET)
  set(HARVEST_TARGET ${CMAKE_CURRENT_SOURCE_DIR}/Harvest)
endif()
message("HARVEST_TARGET = ${HARVEST_TARGET}")

if(WIN32)
if(BUILD_MODE STREQUAL Release)
  add_custom_target(Harvest_Release_Results
    COMMAND # jpeg rename libfile + copy include
        ${CMAKE_COMMAND} -E copy ${LIBDIR}/jpg/lib/jpeg-static.lib ${HARVEST_TARGET}/jpeg/lib/libjpeg.lib &&
        ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/jpg/include/ ${HARVEST_TARGET}/jpeg/include/ &&
        # png
        ${CMAKE_COMMAND} -E copy ${LIBDIR}/png/lib/libpng16_static.lib ${HARVEST_TARGET}/png/lib/libpng.lib &&
        ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/png/include/ ${HARVEST_TARGET}/png/include/ &&
        # freeglut-> opengl
        ${CMAKE_COMMAND} -E copy ${LIBDIR}/freeglut/lib/freeglut_static.lib ${HARVEST_TARGET}/opengl/lib/freeglut_static.lib &&
        ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/freeglut/include/ ${HARVEST_TARGET}/opengl/include/ &&
        # glew-> opengl
        ${CMAKE_COMMAND} -E copy ${LIBDIR}/glew/lib/libglew32.lib ${HARVEST_TARGET}/opengl/lib/glew.lib &&
        ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/glew/include/ ${HARVEST_TARGET}/opengl/include/ &&
        # tiff
        ${CMAKE_COMMAND} -E copy ${LIBDIR}/tiff/lib/tiff.lib ${HARVEST_TARGET}/tiff/lib/libtiff.lib &&
        ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/tiff/include/ ${HARVEST_TARGET}/tiff/include/ &&
        # hidapi
        ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/hidapi/ ${HARVEST_TARGET}/hidapi/
    DEPENDS
  )
endif()

if(BUILD_MODE STREQUAL Debug)
  add_custom_target(Harvest_Debug_Results
    COMMAND
        # hdf5
        ${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/hdf5/lib ${HARVEST_TARGET}/hdf5/lib &&
    DEPENDS Package_Python
  )
endif()

else(WIN32)

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
      RENAME ${filename})
  else()
    install(
      DIRECTORY ${LIBDIR}/${from}/
      DESTINATION ${HARVEST_TARGET}/${to}
      USE_SOURCE_PERMISSIONS
      FILES_MATCHING PATTERN ${pattern}
      PATTERN "pkgconfig" EXCLUDE
      PATTERN "cmake" EXCLUDE
      PATTERN "__pycache__" EXCLUDE
      PATTERN "tests" EXCLUDE)
  endif()
endfunction()

harvest(alembic/include alembic/include "*.h")
harvest(alembic/lib/libAlembic.a alembic/lib/libAlembic.a)
harvest(alembic/bin alembic/bin "*")
harvest(boost/include boost/include "*")
harvest(boost/lib boost/lib "*.a")
harvest(ffmpeg/include ffmpeg/include "*.h")
harvest(ffmpeg/lib ffmpeg/lib "*.a")
harvest(fftw3/include fftw3/include "*.h")
harvest(fftw3/lib fftw3/lib "*.a")
harvest(flac/lib sndfile/lib "libFLAC.a")
harvest(freetype/include freetype/include "*.h")
harvest(freetype/lib/libfreetype2ST.a freetype/lib/libfreetype.a)
harvest(glew/include glew/include "*.h")
harvest(glew/lib glew/lib "*.a")
harvest(jemalloc/include jemalloc/include "*.h")
harvest(jemalloc/lib jemalloc/lib "*.a")
harvest(jpg/include jpeg/include "*.h")
harvest(jpg/lib jpeg/lib "libjpeg.a")
harvest(lame/lib ffmpeg/lib "*.a")
harvest(clang/bin llvm/bin "clang-format")
harvest(clang/include llvm/include "*")
harvest(llvm/include llvm/include "*")
harvest(llvm/bin llvm/bin "llvm-config")
harvest(llvm/lib llvm/lib "libLLVM*.a")
if(APPLE)
  harvest(openmp/lib openmp/lib "*")
  harvest(openmp/include openmp/include "*.h")
endif()
harvest(ogg/lib ffmpeg/lib "*.a")
harvest(openal/include openal/include "*.h")
if(UNIX AND NOT APPLE)
  harvest(openal/lib openal/lib "*.a")

  harvest(blosc/include blosc/include "*.h")
  harvest(blosc/lib blosc/lib "*.a")

  harvest(zlib/include zlib/include "*.h")
  harvest(zlib/lib zlib/lib "*.a")

  harvest(xml2/include xml2/include "*.h")
  harvest(xml2/lib xml2/lib "*.a")
else()
  harvest(blosc/lib openvdb/lib "*.a")
  harvest(xml2/lib opencollada/lib "*.a")
endif()
harvest(opencollada/include/opencollada opencollada/include "*.h")
harvest(opencollada/lib/opencollada opencollada/lib "*.a")
harvest(opencolorio/include opencolorio/include "*.h")
harvest(opencolorio/lib opencolorio/lib "*.a")
harvest(opencolorio/lib/static opencolorio/lib "*.a")
harvest(openexr/include openexr/include "*.h")
harvest(openexr/lib openexr/lib "*.a")
harvest(openimageio/bin openimageio/bin "idiff")
harvest(openimageio/bin openimageio/bin "maketx")
harvest(openimageio/bin openimageio/bin "oiiotool")
harvest(openimageio/include openimageio/include "*")
harvest(openimageio/lib openimageio/lib "*.a")
harvest(openimagedenoise/include openimagedenoise/include "*")
harvest(openimagedenoise/lib openimagedenoise/lib "*.a")
harvest(openjpeg/include/openjpeg-2.3 openjpeg/include "*.h")
harvest(openjpeg/lib openjpeg/lib "*.a")
harvest(opensubdiv/include opensubdiv/include "*.h")
harvest(opensubdiv/lib opensubdiv/lib "*.a")
harvest(openvdb/include/openvdb openvdb/include/openvdb "*.h")
harvest(openvdb/lib openvdb/lib "*.a")
harvest(xr_openxr_sdk/include/openxr xr_openxr_sdk/include/openxr "*.h")
harvest(xr_openxr_sdk/lib xr_openxr_sdk/lib "*.a")
harvest(osl/bin osl/bin "oslc")
harvest(osl/include osl/include "*.h")
harvest(osl/lib osl/lib "*.a")
harvest(osl/shaders osl/shaders "*.h")
harvest(png/include png/include "*.h")
harvest(png/lib png/lib "*.a")
harvest(python/bin python/bin "python${PYTHON_SHORT_VERSION}m")
harvest(python/include python/include "*h")
harvest(python/lib python/lib "*")
harvest(sdl/include/SDL2 sdl/include "*.h")
harvest(sdl/lib sdl/lib "libSDL2.a")
harvest(sndfile/include sndfile/include "*.h")
harvest(sndfile/lib sndfile/lib "*.a")
harvest(spnav/include spnav/include "*.h")
harvest(spnav/lib spnav/lib "*.a")
harvest(tbb/include tbb/include "*.h")
harvest(tbb/lib/libtbb_static.a tbb/lib/libtbb.a)
harvest(theora/lib ffmpeg/lib "*.a")
harvest(tiff/include tiff/include "*.h")
harvest(tiff/lib tiff/lib "*.a")
harvest(vorbis/lib ffmpeg/lib "*.a")
harvest(opus/lib ffmpeg/lib "*.a")
harvest(vpx/lib ffmpeg/lib "*.a")
harvest(webp/lib ffmpeg/lib "*.a")
harvest(x264/lib ffmpeg/lib "*.a")
harvest(xvidcore/lib ffmpeg/lib "*.a")
harvest(embree/include embree/include "*.h")
harvest(embree/lib embree/lib "*.a")
harvest(usd/include usd/include "*.h")
harvest(usd/lib/usd usd/lib/usd "*")
harvest(usd/plugin usd/plugin "*")

if(UNIX AND NOT APPLE)
  harvest(libglu/lib mesa/lib "*.so*")
  harvest(mesa/lib mesa/lib "*.so*")
endif()

endif()
