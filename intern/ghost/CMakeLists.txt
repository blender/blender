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
	./intern/GHOST_Buttons.cpp
	./intern/GHOST_CallbackEventConsumer.cpp
	./intern/GHOST_C-api.cpp
	./intern/GHOST_DisplayManager.cpp
	./intern/GHOST_EventManager.cpp
	./intern/GHOST_EventPrinter.cpp
	./intern/GHOST_ISystem.cpp
	./intern/GHOST_ModifierKeys.cpp
	./intern/GHOST_NDOFManager.cpp
	./intern/GHOST_Path-api.cpp
	./intern/GHOST_Rect.cpp
	./intern/GHOST_System.cpp
	./intern/GHOST_TimerManager.cpp
	./intern/GHOST_Window.cpp
	./intern/GHOST_WindowManager.cpp
)

IF(APPLE)
	IF(WITH_COCOA)
		LIST(APPEND SRC
			./intern/GHOST_DisplayManagerCocoa.mm
			./intern/GHOST_SystemCocoa.mm
			./intern/GHOST_WindowCocoa.mm
		)
	ELSE(WITH_COCOA)
		LIST(APPEND SRC
			./intern/GHOST_DisplayManagerCarbon.cpp
			./intern/GHOST_SystemCarbon.cpp
			./intern/GHOST_WindowCarbon.cpp
		)
	ENDIF(WITH_COCOA)

	IF(WITH_QUICKTIME)
		ADD_DEFINITIONS(-DWITH_QUICKTIME)
	ENDIF(WITH_QUICKTIME)
ELSEIF(WIN32)
	LIST(APPEND INC ${WINTAB_INC})

	LIST(APPEND SRC
		./intern/GHOST_DisplayManagerWin32.cpp
		./intern/GHOST_SystemWin32.cpp
		./intern/GHOST_WindowWin32.cpp
		./intern/GHOST_DropTargetWin32.cpp
	)
ELSEIF(UNIX)
	LIST(APPEND INC ${X11_X11_INCLUDE_PATH})

	LIST(APPEND SRC
		./intern/GHOST_DisplayManagerX11.cpp
		./intern/GHOST_SystemX11.cpp
		./intern/GHOST_WindowX11.cpp
	)

	ADD_DEFINITIONS(-DPREFIX="${CMAKE_INSTALL_PREFIX}")

ENDIF(APPLE)

BLENDERLIB(bf_intern_ghost "${SRC}" "${INC}")

