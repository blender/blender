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
	../../makesdna
	../../makesrna
	../../windowmanager
	../../gpu
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
	bpy_app_handlers.c
	bpy_driver.c
	bpy_interface.c
	bpy_interface_atexit.c
	bpy_intern_string.c
	bpy_library.c
	bpy_operator.c
	bpy_operator_wrap.c
	bpy_props.c
	bpy_rna.c
	bpy_rna_anim.c
	bpy_rna_array.c
	bpy_rna_callback.c
	bpy_traceback.c
	bpy_util.c
	stubs.c

	gpu.h
	bpy.h
	bpy_app.h
	bpy_app_ffmpeg.h
	bpy_app_handlers.h
	bpy_driver.h
	bpy_intern_string.h
	bpy_operator.h
	bpy_operator_wrap.h
	bpy_props.h
	bpy_rna.h
	bpy_rna_anim.h
	bpy_rna_callback.h
	bpy_traceback.h
	bpy_util.h
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

if(WITH_CYCLES)
	add_definitions(-DWITH_CYCLES)
endif()

if(WITH_INTERNATIONAL)
	add_definitions(-DWITH_INTERNATIONAL)
endif()

if(WITH_CODEC_FFMPEG)
	list(APPEND INC_SYS
		${FFMPEG_INCLUDE_DIRS}
	)

	add_definitions(-DWITH_FFMPEG)
endif()

blender_add_lib(bf_python "${SRC}" "${INC}" "${INC_SYS}")
