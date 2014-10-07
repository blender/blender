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
# Contributor(s): Jacques Beaurainm, Campbell Barton
#
# ***** END GPL LICENSE BLOCK *****

set(INC 
	..
	../../blenfont
	../../blenkernel
	../../blenlib
	../../blenloader
	../../editors/include
	../../gpu
	../../makesdna
	../../makesrna
	../../windowmanager
	../../../../intern/cycles/blender
	../../../../intern/opencolorio
	../../../../intern/guardedalloc
)

set(INC_SYS
	${PYTHON_INCLUDE_DIRS}
)

set(SRC
	gpu.c
	bpy.c
	bpy_app.c
	bpy_app_ffmpeg.c
	bpy_app_build_options.c
	bpy_app_handlers.c
	bpy_app_ocio.c
	bpy_app_oiio.c
	bpy_app_translations.c
	bpy_driver.c
	bpy_interface.c
	bpy_interface_atexit.c
	bpy_intern_string.c
	bpy_library.c
	bpy_operator.c
	bpy_operator_wrap.c
	bpy_path.c
	bpy_props.c
	bpy_rna.c
	bpy_rna_anim.c
	bpy_rna_array.c
	bpy_rna_callback.c
	bpy_traceback.c
	bpy_util.c
	bpy_utils_units.c
	stubs.c

	gpu.h
	bpy.h
	bpy_app.h
	bpy_app_ffmpeg.h
	bpy_app_build_options.h
	bpy_app_handlers.h
	bpy_app_ocio.h
	bpy_app_oiio.h
	bpy_app_translations.h
	bpy_driver.h
	bpy_intern_string.h
	bpy_library.h
	bpy_operator.h
	bpy_operator_wrap.h
	bpy_path.h
	bpy_props.h
	bpy_rna.h
	bpy_rna_anim.h
	bpy_rna_callback.h
	bpy_traceback.h
	bpy_util.h
	bpy_utils_units.h
	../BPY_extern.h
)

# only to check if buildinfo is available
if(WITH_BUILDINFO)
	add_definitions(-DBUILD_DATE)
endif()

if(WITH_PYTHON_MODULE)
	add_definitions(-DWITH_PYTHON_MODULE)
endif()

if(WITH_PYTHON_SAFETY)
	add_definitions(-DWITH_PYTHON_SAFETY)
endif()



if(WITH_AUDASPACE)
	add_definitions(-DWITH_AUDASPACE)
endif()

if(WITH_BULLET)
	add_definitions(-DWITH_BULLET)
endif()

if(WITH_CODEC_AVI)
	add_definitions(-DWITH_AVI)
endif()

if(WITH_CODEC_FFMPEG)
	list(APPEND INC_SYS
		${FFMPEG_INCLUDE_DIRS}
	)
	add_definitions(-DWITH_FFMPEG)
endif()

if(WITH_CODEC_QUICKTIME)
	add_definitions(-DWITH_QUICKTIME)
endif()

if(WITH_CODEC_SNDFILE)
	add_definitions(-DWITH_SNDFILE)
endif()

if(WITH_COMPOSITOR)
	add_definitions(-DWITH_COMPOSITOR)
endif()

if(WITH_CYCLES)
	add_definitions(-DWITH_CYCLES)
endif()

if(WITH_CYCLES_OSL)
	add_definitions(-DWITH_CYCLES_OSL)
endif()

if(WITH_FREESTYLE)
	list(APPEND INC
		../../freestyle/intern/python
	)
	add_definitions(-DWITH_FREESTYLE)
endif()

if(WITH_GAMEENGINE)
	add_definitions(-DWITH_GAMEENGINE)
endif()

if(WITH_IMAGE_CINEON)
	add_definitions(-DWITH_CINEON)
endif()

if(WITH_IMAGE_DDS)
	add_definitions(-DWITH_DDS)
endif()

if(WITH_IMAGE_FRAMESERVER)
	add_definitions(-DWITH_FRAMESERVER)
endif()

if(WITH_IMAGE_HDR)
	add_definitions(-DWITH_HDR)
endif()

if(WITH_IMAGE_OPENEXR)
	add_definitions(-DWITH_OPENEXR)
endif()

if(WITH_OPENIMAGEIO)
	add_definitions(-DWITH_OPENIMAGEIO)
endif()

if(WITH_IMAGE_OPENJPEG)
	add_definitions(-DWITH_OPENJPEG)
endif()

if(WITH_IMAGE_REDCODE)
	add_definitions(-DWITH_REDCODE)
endif()

if(WITH_IMAGE_TIFF)
	add_definitions(-DWITH_TIFF)
endif()

if(WITH_INPUT_NDOF)
	add_definitions(-DWITH_INPUT_NDOF)
endif()

if(WITH_INTERNATIONAL)
	add_definitions(-DWITH_INTERNATIONAL)
endif()

if(WITH_OPENAL)
	add_definitions(-DWITH_OPENAL)
endif()

if(WITH_SDL)
	add_definitions(-DWITH_SDL)
endif()

if(WITH_JACK)
	add_definitions(-DWITH_JACK)
endif()

if(WITH_LIBMV)
	add_definitions(-DWITH_LIBMV)
endif()

if(WITH_MOD_BOOLEAN)
	add_definitions(-DWITH_MOD_BOOLEAN)
endif()

if(WITH_MOD_FLUID)
	add_definitions(-DWITH_MOD_FLUID)
endif()

if(WITH_MOD_OCEANSIM)
	add_definitions(-DWITH_OCEANSIM)
endif()

if(WITH_MOD_REMESH)
	add_definitions(-DWITH_MOD_REMESH)
endif()

if(WITH_MOD_SMOKE)
	add_definitions(-DWITH_SMOKE)
endif()

if(WITH_OPENCOLLADA)
	add_definitions(-DWITH_COLLADA)
endif()

if(WITH_OPENCOLORIO)
	add_definitions(-DWITH_OCIO)
endif()

if(WITH_OPENIMAGEIO)
	add_definitions(-DWITH_OPENIMAGEIO)
	list(APPEND INC
		../../imbuf/intern/oiio
	)
endif()

if(WITH_PLAYER)
	add_definitions(-DWITH_PLAYER)
endif()

add_definitions(${GL_DEFINITIONS})

blender_add_lib(bf_python "${SRC}" "${INC}" "${INC_SYS}")
