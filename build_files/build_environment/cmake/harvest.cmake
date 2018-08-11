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
				# Zlib Rename the lib file and copy the include/bin folders
		COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/zlib/lib/zlibstatic.lib ${HARVEST_TARGET}/zlib/lib/libz_st.lib &&
				${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/zlib/include/ ${HARVEST_TARGET}/zlib/include/ &&
				${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/zlib/bin/ ${HARVEST_TARGET}/zlib/bin/ &&
				# jpeg rename libfile + copy include
				${CMAKE_COMMAND} -E copy ${LIBDIR}/jpg/lib/jpeg-static.lib ${HARVEST_TARGET}/jpeg/lib/libjpeg.lib &&
				${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/jpg/include/ ${HARVEST_TARGET}/jpeg/include/ &&
				# pthreads, rename include dir
				${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/pthreads/inc/ ${HARVEST_TARGET}/pthreads/include/ &&
				${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/pthreads/lib/ ${HARVEST_TARGET}/pthreads/lib &&
				# ffmpeg copy include+bin
				${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/ffmpeg/include ${HARVEST_TARGET}/ffmpeg/include &&
				${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/ffmpeg/bin ${HARVEST_TARGET}/ffmpeg/lib &&
				# sdl merge bin/lib folder, copy include
				${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/sdl/include/sdl2 ${HARVEST_TARGET}/sdl/include &&
				${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/sdl/lib ${HARVEST_TARGET}/sdl/lib &&
				${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/sdl/bin ${HARVEST_TARGET}/sdl/lib &&
				# OpenImageIO
				${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/OpenImageIO/include ${HARVEST_TARGET}/OpenImageIO/include &&
				${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/OpenImageIO/lib ${HARVEST_TARGET}/OpenImageIO/lib &&
				${CMAKE_COMMAND} -E copy ${LIBDIR}/OpenImageIO/bin/idiff.exe ${HARVEST_TARGET}/OpenImageIO/bin/idiff.exe &&
				# openEXR
				${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/ilmbase ${HARVEST_TARGET}/openexr &&
				${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/openexr/lib ${HARVEST_TARGET}/openexr/lib &&
				${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/openexr/include ${HARVEST_TARGET}/openexr/include &&
				# png
				${CMAKE_COMMAND} -E copy ${LIBDIR}/png/lib/libpng16_static.lib ${HARVEST_TARGET}/png/lib/libpng.lib &&
				${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/png/include/ ${HARVEST_TARGET}/png/include/ &&
				# fftw3
				${CMAKE_COMMAND} -E copy ${LIBDIR}/fftw3/lib/libfftw3.a ${HARVEST_TARGET}/fftw3/lib/libfftw.lib &&
				${CMAKE_COMMAND} -E copy ${LIBDIR}/fftw3/include/fftw3.h ${HARVEST_TARGET}/fftw3/include/fftw3.h &&
				# freeglut-> opengl
				${CMAKE_COMMAND} -E copy ${LIBDIR}/freeglut/lib/freeglut_static.lib ${HARVEST_TARGET}/opengl/lib/freeglut_static.lib &&
				${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/freeglut/include/ ${HARVEST_TARGET}/opengl/include/ &&
				# glew-> opengl
				${CMAKE_COMMAND} -E copy ${LIBDIR}/glew/lib/libglew32.lib ${HARVEST_TARGET}/opengl/lib/glew.lib &&
				${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/glew/include/ ${HARVEST_TARGET}/opengl/include/ &&
				# sndfile
				${CMAKE_COMMAND} -E copy ${LIBDIR}/sndfile/lib/libsndfile.dll.a ${HARVEST_TARGET}/sndfile/lib/libsndfile-1.lib &&
				${CMAKE_COMMAND} -E copy ${LIBDIR}/sndfile/bin/libsndfile-1.dll ${HARVEST_TARGET}/sndfile/lib/libsndfile-1.dll &&
				${CMAKE_COMMAND} -E copy ${LIBDIR}/sndfile/include/sndfile.h ${HARVEST_TARGET}/sndfile/include/sndfile.h &&
				# tiff
				${CMAKE_COMMAND} -E copy ${LIBDIR}/tiff/lib/tiff.lib ${HARVEST_TARGET}/tiff/lib/libtiff.lib &&
				${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/tiff/include/ ${HARVEST_TARGET}/tiff/include/ &&
				# iconv
				${CMAKE_COMMAND} -E copy ${LIBDIR}/iconv/lib/libiconv.a ${HARVEST_TARGET}/iconv/lib/iconv.lib &&
				${CMAKE_COMMAND} -E copy ${LIBDIR}/iconv/include/iconv.h ${HARVEST_TARGET}/iconv/include/iconv.h &&
				# opencolorIO
				${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/OpenColorIO/ ${HARVEST_TARGET}/opencolorio &&
				${CMAKE_COMMAND} -E copy ${LIBDIR}/OpenColorIO/lib/OpenColorIO.dll ${HARVEST_TARGET}/opencolorio/bin/OpenColorIO.dll &&
				# Osl
				${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/osl/ ${HARVEST_TARGET}/osl &&
				# OpenVDB
				${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/openVDB/ ${HARVEST_TARGET}/openVDB &&
				# blosc
				${CMAKE_COMMAND} -E copy ${LIBDIR}/blosc/lib/libblosc.lib ${HARVEST_TARGET}/blosc/lib/libblosc.lib &&
				${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/blosc/include/ ${HARVEST_TARGET}/blosc/include/ &&
				# tbb
				${CMAKE_COMMAND} -E copy ${LIBDIR}/tbb/lib/tbb_static.lib ${HARVEST_TARGET}/tbb/lib/tbb.lib &&
				${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/tbb/include/ ${HARVEST_TARGET}/tbb/include/ &&
				# opencollada
				${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/opencollada/ ${HARVEST_TARGET}/opencollada/ &&
				# opensubdiv
				${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/opensubdiv ${HARVEST_TARGET}/opensubdiv &&
				# alembic
				${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/alembic ${HARVEST_TARGET}/alembic &&
				# BlendThumb
				${CMAKE_COMMAND} -E copy ${LIBDIR}/BlendThumb64/bin/blendthumb.dll ${HARVEST_TARGET}/ThumbHandler/lib/BlendThumb64.dll &&
				${CMAKE_COMMAND} -E copy ${LIBDIR}/BlendThumb32/bin/blendthumb.dll ${HARVEST_TARGET}/ThumbHandler/lib/BlendThumb.dll &&
				# hidapi
				${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/hidapi/ ${HARVEST_TARGET}/hidapi/ &&
				# webp, straight up copy
				${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/webp ${HARVEST_TARGET}/webp
		DEPENDS
	)
endif()

if(BUILD_MODE STREQUAL Debug)
	add_custom_target(Harvest_Debug_Results
				# OpenImageIO
		COMMAND ${CMAKE_COMMAND} -E copy ${LIBDIR}/openimageio/lib/OpenImageIO.lib ${HARVEST_TARGET}/openimageio/lib/OpenImageIO_d.lib &&
				${CMAKE_COMMAND} -E copy ${LIBDIR}/openimageio/lib/OpenImageIO_Util.lib ${HARVEST_TARGET}/openimageio/lib/OpenImageIO_Util_d.lib &&
				# ilmbase+openexr
				${CMAKE_COMMAND} -E copy ${LIBDIR}/ilmbase/lib/Half.lib ${HARVEST_TARGET}/openexr/lib/Half_d.lib &&
				${CMAKE_COMMAND} -E copy ${LIBDIR}/ilmbase/lib/Iex-2_2.lib ${HARVEST_TARGET}/openexr/lib/Iex-2_2_d.lib &&
				${CMAKE_COMMAND} -E copy ${LIBDIR}/ilmbase/lib/IexMath-2_2.lib ${HARVEST_TARGET}/openexr/lib/IexMath-2_2_d.lib &&
				${CMAKE_COMMAND} -E copy ${LIBDIR}/ilmbase/lib/IlmThread-2_2.lib ${HARVEST_TARGET}/openexr/lib/IlmThread-2_2_d.lib &&
				${CMAKE_COMMAND} -E copy ${LIBDIR}/ilmbase/lib/Imath-2_2.lib ${HARVEST_TARGET}/openexr/lib/Imath-2_2_d.lib &&
				${CMAKE_COMMAND} -E copy ${LIBDIR}/openexr/lib/IlmImf-2_2.lib ${HARVEST_TARGET}/openexr/lib/IlmImf-2_2_d.lib &&
				${CMAKE_COMMAND} -E copy ${LIBDIR}/openexr/lib/IlmImfUtil-2_2.lib ${HARVEST_TARGET}/openexr/lib/IlmImfUtil-2_2_d.lib &&
				# opencollada
				${CMAKE_COMMAND} -E copy ${LIBDIR}/opencollada/lib/opencollada/buffer.lib ${HARVEST_TARGET}/opencollada/lib/opencollada/buffer_d.lib &&
				${CMAKE_COMMAND} -E copy ${LIBDIR}/opencollada/lib/opencollada/ftoa.lib ${HARVEST_TARGET}/opencollada/lib/opencollada/ftoa_d.lib &&
				${CMAKE_COMMAND} -E copy ${LIBDIR}/opencollada/lib/opencollada/GeneratedSaxParser.lib ${HARVEST_TARGET}/opencollada/lib/opencollada/GeneratedSaxParser_d.lib &&
				${CMAKE_COMMAND} -E copy ${LIBDIR}/opencollada/lib/opencollada/MathMLSolver.lib ${HARVEST_TARGET}/opencollada/lib/opencollada/MathMLSolver_d.lib &&
				${CMAKE_COMMAND} -E copy ${LIBDIR}/opencollada/lib/opencollada/OpenCOLLADABaseUtils.lib ${HARVEST_TARGET}/opencollada/lib/opencollada/OpenCOLLADABaseUtils_d.lib &&
				${CMAKE_COMMAND} -E copy ${LIBDIR}/opencollada/lib/opencollada/OpenCOLLADAFramework.lib ${HARVEST_TARGET}/opencollada/lib/opencollada/OpenCOLLADAFramework_d.lib &&
				${CMAKE_COMMAND} -E copy ${LIBDIR}/opencollada/lib/opencollada/OpenCOLLADASaxFrameworkLoader.lib ${HARVEST_TARGET}/opencollada/lib/opencollada/OpenCOLLADASaxFrameworkLoader_d.lib &&
				${CMAKE_COMMAND} -E copy ${LIBDIR}/opencollada/lib/opencollada/OpenCOLLADAStreamWriter.lib ${HARVEST_TARGET}/opencollada/lib/opencollada/OpenCOLLADAStreamWriter_d.lib &&
				${CMAKE_COMMAND} -E copy ${LIBDIR}/opencollada/lib/opencollada/pcre.lib ${HARVEST_TARGET}/opencollada/lib/opencollada/pcre_d.lib &&
				${CMAKE_COMMAND} -E copy ${LIBDIR}/opencollada/lib/opencollada/UTF.lib ${HARVEST_TARGET}/opencollada/lib/opencollada/UTF_d.lib &&
				${CMAKE_COMMAND} -E copy ${LIBDIR}/opencollada/lib/opencollada/xml.lib ${HARVEST_TARGET}/opencollada/lib/opencollada/xml_d.lib &&
				# blosc
				${CMAKE_COMMAND} -E copy ${LIBDIR}/blosc/lib/libblosc_d.lib ${HARVEST_TARGET}/blosc/lib/libblosc_d.lib &&
				# osl
				${CMAKE_COMMAND} -E copy ${LIBDIR}/osl/lib/oslcomp.lib ${HARVEST_TARGET}/osl/lib/oslcomp_d.lib &&
				${CMAKE_COMMAND} -E copy ${LIBDIR}/osl/lib/oslexec.lib ${HARVEST_TARGET}/osl/lib/oslexec_d.lib &&
				${CMAKE_COMMAND} -E copy ${LIBDIR}/osl/lib/oslquery.lib ${HARVEST_TARGET}/osl/lib/oslquery_d.lib &&
				# opensubdiv
				${CMAKE_COMMAND} -E copy ${LIBDIR}/opensubdiv/lib/osdCPU.lib ${HARVEST_TARGET}/opensubdiv/lib/osdCPU_d.lib &&
				${CMAKE_COMMAND} -E copy ${LIBDIR}/opensubdiv/lib/osdGPU.lib ${HARVEST_TARGET}/opensubdiv/lib/osdGPU_d.lib &&
				# tbb
				${CMAKE_COMMAND} -E copy ${LIBDIR}/tbb/lib/tbb_static.lib ${HARVEST_TARGET}/tbb/lib/tbb_debug.lib &&
				# openvdb
				${CMAKE_COMMAND} -E copy ${LIBDIR}/openvdb/lib/openvdb.lib ${HARVEST_TARGET}/openvdb/lib/openvdb_d.lib &&
				# python
				${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/python/ ${HARVEST_TARGET}/python/ &&
				# alembic
				${CMAKE_COMMAND} -E copy ${LIBDIR}/alembic/lib/alembic.lib ${HARVEST_TARGET}/alembic/lib/alembic_d.lib &&
				# hdf5
				${CMAKE_COMMAND} -E copy_directory ${LIBDIR}/hdf5/lib ${HARVEST_TARGET}/hdf5/lib &&
				# numpy
				${CMAKE_COMMAND} -E copy ${LIBDIR}/python${PYTHON_SHORT_VERSION_NO_DOTS}_numpy_${NUMPY_SHORT_VERSION}d.tar.gz ${HARVEST_TARGET}/Release/python${PYTHON_SHORT_VERSION_NO_DOTS}_numpy_${NUMPY_SHORT_VERSION}d.tar.gz &&
				# python
				${CMAKE_COMMAND} -E copy ${LIBDIR}/python${PYTHON_SHORT_VERSION_NO_DOTS}_d.tar.gz ${HARVEST_TARGET}/Release/python${PYTHON_SHORT_VERSION_NO_DOTS}_d.tar.gz
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
			PATTERN "clang" EXCLUDE
			PATTERN "__pycache__" EXCLUDE
			PATTERN "tests" EXCLUDE)
	endif()
endfunction()

harvest(alembic/include alembic/include "*.h")
harvest(alembic/lib/libAlembic.a alembic/lib/libAlembic.a)
harvest(alembic/bin alembic/bin "*")
harvest(blosc/lib openvdb/lib "*.a")
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
harvest(ilmbase openexr "*")
harvest(ilmbase/include openexr/include "*.h")
harvest(jemalloc/include jemalloc/include "*.h")
harvest(jemalloc/lib jemalloc/lib "*.a")
harvest(jpg/include jpeg/include "*.h")
harvest(jpg/lib jpeg/lib "libjpeg.a")
harvest(lame/lib ffmpeg/lib "*.a")
harvest(llvm/bin llvm/bin "llvm-config")
harvest(llvm/lib llvm/lib "libLLVM*.a")
harvest(ogg/lib ffmpeg/lib "*.a")
harvest(openal/include openal/include "*.h")
if(UNIX AND NOT APPLE)
	harvest(openal/lib openal/lib "*.a")
endif()
harvest(opencollada/include/opencollada opencollada/include "*.h")
harvest(opencollada/lib/opencollada opencollada/lib "*.a")
harvest(opencolorio/include opencolorio/include "*.h")
harvest(opencolorio/lib opencolorio/lib "*.a")
harvest(openexr/include openexr/include "*.h")
harvest(openexr/lib openexr/lib "*.a")
harvest(openimageio/bin openimageio/bin "idiff")
harvest(openimageio/bin openimageio/bin "maketx")
harvest(openimageio/bin openimageio/bin "oiiotool")
harvest(openimageio/include openimageio/include "*")
harvest(openimageio/lib openimageio/lib "*.a")
harvest(openjpeg/include/openjpeg-2.3 openjpeg/include "*.h")
harvest(openjpeg/lib openjpeg/lib "*.a")
harvest(opensubdiv/include opensubdiv/include "*.h")
harvest(opensubdiv/lib opensubdiv/lib "*.a")
harvest(openvdb/include/openvdb/openvdb openvdb/include/openvdb "*.h")
harvest(openvdb/lib openvdb/lib "*.a")
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
harvest(vpx/lib ffmpeg/lib "*.a")
harvest(webp/lib ffmpeg/lib "*.a")
harvest(x264/lib ffmpeg/lib "*.a")
harvest(xml2/lib opencollada/lib "*.a")
harvest(xvidcore/lib ffmpeg/lib "*.a")

endif()
