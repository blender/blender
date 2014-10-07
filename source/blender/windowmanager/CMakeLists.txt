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
# Contributor(s): Jacques Beaurain.
#
# ***** END GPL LICENSE BLOCK *****

set(INC
	.
	../blenfont
	../blenkernel
	../blenlib
	../blenloader
	../compositor
	../editors/include
	../gpu
	../imbuf
	../makesdna
	../makesrna
	../nodes
	../render/extern/include
	../../gameengine/BlenderRoutines
	../../../intern/ghost
	../../../intern/guardedalloc
	../../../intern/glew-mx
	../../../intern/memutil
)

set(INC_SYS
	${ZLIB_INCLUDE_DIRS}
	${GLEW_INCLUDE_PATH}
)

set(SRC
	intern/wm.c
	intern/wm_playanim.c
	intern/wm_cursors.c
	intern/wm_dragdrop.c
	intern/wm_draw.c
	intern/wm_event_system.c
	intern/wm_files.c
	intern/wm_gesture.c
	intern/wm_init_exit.c
	intern/wm_jobs.c
	intern/wm_keymap.c
	intern/wm_operators.c
	intern/wm_subwindow.c
	intern/wm_window.c

	WM_api.h
	WM_keymap.h
	WM_types.h
	wm.h
	wm_cursors.h
	wm_draw.h
	wm_event_system.h
	wm_event_types.h
	wm_files.h
	wm_subwindow.h
	wm_window.h
)

add_definitions(${GL_DEFINITIONS})

if(WITH_INTERNATIONAL)
	add_definitions(-DWITH_INTERNATIONAL)
endif()

if(WITH_OPENCOLLADA)
	add_definitions(-DWITH_COLLADA)
endif()

if(WITH_CODEC_QUICKTIME)
	list(APPEND INC
		../quicktime
	)
	list(APPEND INC_SYS
		${QUICKTIME_INCLUDE_DIRS}
	)
	add_definitions(-DWITH_QUICKTIME)
endif()

if(WITH_CODEC_FFMPEG)
	list(APPEND INC_SYS
		${FFMPEG_INCLUDE_DIRS}
	)
	add_definitions(-DWITH_FFMPEG)
endif()

if(WITH_HEADLESS)
	add_definitions(-DWITH_HEADLESS)
elseif(WITH_X11)
	add_definitions(-DWITH_X11)
endif()

if(WITH_PYTHON)
	list(APPEND INC
		../python
	)
	add_definitions(-DWITH_PYTHON)

	if(WITH_PYTHON_SECURITY)
		add_definitions(-DWITH_PYTHON_SECURITY)
	endif()
endif()

if(WITH_GAMEENGINE)
	add_definitions(-DWITH_GAMEENGINE)
endif()

if(WITH_BUILDINFO)
	add_definitions(-DWITH_BUILDINFO)
endif()

if(WIN322)
	list(APPEND INC
		../../../intern/utfconv
	)
endif()

if(WITH_COMPOSITOR)
	add_definitions(-DWITH_COMPOSITOR)
endif()

blender_add_lib_nolist(bf_windowmanager "${SRC}" "${INC}" "${INC_SYS}")
