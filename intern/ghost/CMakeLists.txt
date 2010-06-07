# $Id$
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
	../string
	../../extern/glew/include
	../../source/blender/imbuf
	../../source/blender/makesdna
)

SET(SRC
	${CMAKE_CURRENT_SOURCE_DIR}/intern/GHOST_Buttons.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/intern/GHOST_CallbackEventConsumer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/intern/GHOST_C-api.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/intern/GHOST_DisplayManager.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/intern/GHOST_EventManager.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/intern/GHOST_EventPrinter.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/intern/GHOST_ISystem.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/intern/GHOST_ModifierKeys.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/intern/GHOST_NDOFManager.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/intern/GHOST_Rect.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/intern/GHOST_System.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/intern/GHOST_TimerManager.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/intern/GHOST_Window.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/intern/GHOST_WindowManager.cpp
)

IF(APPLE)
	IF(WITH_COCOA)
		SET(SRC ${SRC}
			${CMAKE_CURRENT_SOURCE_DIR}/intern/GHOST_DisplayManagerCocoa.mm
			${CMAKE_CURRENT_SOURCE_DIR}/intern/GHOST_SystemCocoa.mm
			${CMAKE_CURRENT_SOURCE_DIR}/intern/GHOST_WindowCocoa.mm
		)
	ELSE(WITH_COCOA)
		SET(SRC ${SRC}
			${CMAKE_CURRENT_SOURCE_DIR}/intern/GHOST_DisplayManagerCarbon.cpp
			${CMAKE_CURRENT_SOURCE_DIR}/intern/GHOST_SystemCarbon.cpp
			${CMAKE_CURRENT_SOURCE_DIR}/intern/GHOST_WindowCarbon.cpp
		)
	ENDIF(WITH_COCOA)

	IF(WITH_QUICKTIME)
		ADD_DEFINITIONS(-DWITH_QUICKTIME)
	ENDIF(WITH_QUICKTIME)
ELSEIF(WIN32)
	SET(INC ${INC} ${WINTAB_INC})

	SET(SRC ${SRC}
		${CMAKE_CURRENT_SOURCE_DIR}/intern/GHOST_DisplayManagerWin32.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/intern/GHOST_SystemWin32.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/intern/GHOST_WindowWin32.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/intern/GHOST_DropTargetWin32.cpp
	)
ELSEIF(UNIX)
	SET(INC ${INC} ${X11_X11_INCLUDE_PATH})

	SET(SRC ${SRC}
		${CMAKE_CURRENT_SOURCE_DIR}/intern/GHOST_DisplayManagerX11.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/intern/GHOST_SystemX11.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/intern/GHOST_WindowX11.cpp
	)
ENDIF(APPLE)

BLENDERLIB(bf_ghost "${SRC}" "${INC}")

