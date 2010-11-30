# $Id: CMakeLists.txt 12931 2007-12-17 18:20:48Z theeth $
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

SET(INC
	.
	../nodes
	../gpu
	../blenfont
	../blenlib
	../makesdna
	../makesrna
	../blenkernel
	../imbuf
	../blenloader
	../editors/include
	../render/extern/include
	../../kernel/gen_system
	../../../intern/guardedalloc
	../../../intern/memutil
	../../../intern/elbeem/extern
	../../../intern/ghost
	../../../intern/opennl/extern
	../../../extern/glew/include
	${OPENGL_INCLUDE_DIR}
)

SET(SRC
	intern/wm.c
	intern/wm_apple.c
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

ADD_DEFINITIONS(-DGLEW_STATIC)

IF(WITH_INTERNATIONAL)
	ADD_DEFINITIONS(-DINTERNATIONAL)
ENDIF(WITH_INTERNATIONAL)

IF(WITH_OPENCOLLADA)
	ADD_DEFINITIONS(-DWITH_COLLADA)
ENDIF(WITH_OPENCOLLADA)

IF(WITH_CODEC_QUICKTIME)
	LIST(APPEND INC ../quicktime ${QUICKTIME_INC})
	ADD_DEFINITIONS(-DWITH_QUICKTIME)
ENDIF(WITH_CODEC_QUICKTIME)

IF(WITH_CODEC_FFMPEG)
	LIST(APPEND INC ${FFMPEG_INC})
	ADD_DEFINITIONS(-DWITH_FFMPEG)
ENDIF(WITH_CODEC_FFMPEG)

IF(WITH_PYTHON)
	LIST(APPEND INC ../python ${PYTHON_INC})
	ADD_DEFINITIONS(-DWITH_PYTHON)
ENDIF(WITH_PYTHON)

IF(WITH_GAMEENGINE)
	ADD_DEFINITIONS(-DWITH_GAMEENGINE)
ENDIF(WITH_GAMEENGINE)

IF(WITH_COCOA)
	LIST(REMOVE_ITEM SRC "${CMAKE_CURRENT_SOURCE_DIR}/intern/wm_apple.c")
ENDIF(WITH_COCOA)

IF(WITH_BUILDINFO)
	ADD_DEFINITIONS(-DNAN_BUILDINFO)
ENDIF(WITH_BUILDINFO)

BLENDERLIB_NOLIST(bf_windowmanager "${SRC}" "${INC}")
