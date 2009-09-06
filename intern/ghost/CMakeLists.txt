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
# Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
#
# The Original Code is Copyright (C) 2006, Blender Foundation
# All rights reserved.
#
# The Original Code is: all of this file.
#
# Contributor(s): Jacques Beaurain.
#
# ***** END GPL LICENSE BLOCK *****

SET(INC . ../string)

FILE(GLOB SRC intern/*.cpp)

IF(APPLE)
	LIST(REMOVE_ITEM SRC "${CMAKE_CURRENT_SOURCE_DIR}/intern/GHOST_DisplayManagerWin32.cpp")
	LIST(REMOVE_ITEM SRC "${CMAKE_CURRENT_SOURCE_DIR}/intern/GHOST_SystemWin32.cpp")
	LIST(REMOVE_ITEM SRC "${CMAKE_CURRENT_SOURCE_DIR}/intern/GHOST_WindowWin32.cpp")
	LIST(REMOVE_ITEM SRC "${CMAKE_CURRENT_SOURCE_DIR}/intern/GHOST_DisplayManagerX11.cpp")
	LIST(REMOVE_ITEM SRC "${CMAKE_CURRENT_SOURCE_DIR}/intern/GHOST_SystemX11.cpp")
	LIST(REMOVE_ITEM SRC "${CMAKE_CURRENT_SOURCE_DIR}/intern/GHOST_WindowX11.cpp")
ELSE(APPLE)
	IF(WIN32)
		SET(INC ${INC} ${WINTAB_INC})
		LIST(REMOVE_ITEM SRC "${CMAKE_CURRENT_SOURCE_DIR}/intern/GHOST_DisplayManagerCarbon.cpp")
		LIST(REMOVE_ITEM SRC "${CMAKE_CURRENT_SOURCE_DIR}/intern/GHOST_SystemCarbon.cpp")
		LIST(REMOVE_ITEM SRC "${CMAKE_CURRENT_SOURCE_DIR}/intern/GHOST_WindowCarbon.cpp")
		LIST(REMOVE_ITEM SRC "${CMAKE_CURRENT_SOURCE_DIR}/intern/GHOST_DisplayManagerX11.cpp")
		LIST(REMOVE_ITEM SRC "${CMAKE_CURRENT_SOURCE_DIR}/intern/GHOST_SystemX11.cpp")
		LIST(REMOVE_ITEM SRC "${CMAKE_CURRENT_SOURCE_DIR}/intern/GHOST_WindowX11.cpp")
	ELSE(WIN32)
		SET(INC ${INC} ${X11_X11_INCLUDE_PATH})
		LIST(REMOVE_ITEM SRC "${CMAKE_CURRENT_SOURCE_DIR}/intern/GHOST_DisplayManagerWin32.cpp")
		LIST(REMOVE_ITEM SRC "${CMAKE_CURRENT_SOURCE_DIR}/intern/GHOST_SystemWin32.cpp")
		LIST(REMOVE_ITEM SRC "${CMAKE_CURRENT_SOURCE_DIR}/intern/GHOST_WindowWin32.cpp")
		LIST(REMOVE_ITEM SRC "${CMAKE_CURRENT_SOURCE_DIR}/intern/GHOST_DisplayManagerCarbon.cpp")
		LIST(REMOVE_ITEM SRC "${CMAKE_CURRENT_SOURCE_DIR}/intern/GHOST_SystemCarbon.cpp")
		LIST(REMOVE_ITEM SRC "${CMAKE_CURRENT_SOURCE_DIR}/intern/GHOST_WindowCarbon.cpp")
	ENDIF(WIN32)
ENDIF(APPLE)

BLENDERLIB(bf_ghost "${SRC}" "${INC}")
#, libtype=['core','player'], priority = [25,15] ) 
