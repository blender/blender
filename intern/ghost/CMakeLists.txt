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
	../string
	../../source/blender/imbuf
	../../source/blender/makesdna
)

set(INC_SYS
	${GLEW_INCLUDE_PATH}
)

set(SRC
	intern/GHOST_Buttons.cpp
	intern/GHOST_C-api.cpp
	intern/GHOST_CallbackEventConsumer.cpp
	intern/GHOST_DisplayManager.cpp
	intern/GHOST_EventManager.cpp
	intern/GHOST_ISystem.cpp
	intern/GHOST_ISystemPaths.cpp
	intern/GHOST_ModifierKeys.cpp
	intern/GHOST_Path-api.cpp
	intern/GHOST_Path-api.cpp
	intern/GHOST_Rect.cpp
	intern/GHOST_System.cpp
	intern/GHOST_TimerManager.cpp
	intern/GHOST_Window.cpp
	intern/GHOST_WindowManager.cpp

	GHOST_C-api.h
	GHOST_IEvent.h
	GHOST_IEventConsumer.h
	GHOST_ISystem.h
	GHOST_ISystemPaths.h
	GHOST_ITimerTask.h
	GHOST_IWindow.h
	GHOST_Path-api.h
	GHOST_Rect.h
	GHOST_Types.h

	intern/GHOST_Buttons.h
	intern/GHOST_CallbackEventConsumer.h
	intern/GHOST_Debug.h
	intern/GHOST_DisplayManager.h
	intern/GHOST_Event.h
	intern/GHOST_EventButton.h
	intern/GHOST_EventCursor.h
	intern/GHOST_EventDragnDrop.h
	intern/GHOST_EventKey.h
	intern/GHOST_EventManager.h
	intern/GHOST_EventString.h
	intern/GHOST_EventTrackpad.h
	intern/GHOST_EventWheel.h
	intern/GHOST_ModifierKeys.h
	intern/GHOST_System.h
	intern/GHOST_SystemPaths.h
	intern/GHOST_TimerManager.h
	intern/GHOST_TimerTask.h
	intern/GHOST_Window.h
	intern/GHOST_WindowManager.h
)

if(WITH_GHOST_DEBUG)
	list(APPEND SRC
		intern/GHOST_EventPrinter.cpp

		intern/GHOST_EventPrinter.h
	)
	add_definitions(-DWITH_GHOST_DEBUG)
endif()

if(WITH_INPUT_NDOF)
	add_definitions(-DWITH_INPUT_NDOF)

	list(APPEND SRC
		intern/GHOST_NDOFManager.cpp

		intern/GHOST_EventNDOF.h
		intern/GHOST_NDOFManager.h
	)

	list(APPEND INC_SYS
		${NDOF_INCLUDE_DIRS}
	)
endif()

if(WITH_HEADLESS OR WITH_GHOST_SDL)
	if(WITH_HEADLESS)
		list(APPEND SRC
			intern/GHOST_DisplayManagerNULL.h
			intern/GHOST_SystemNULL.h
			intern/GHOST_WindowNULL.h
		)
		add_definitions(-DWITH_HEADLESS)
	else()
		list(APPEND SRC
			intern/GHOST_DisplayManagerSDL.cpp
			intern/GHOST_SystemSDL.cpp
			intern/GHOST_WindowSDL.cpp

			intern/GHOST_DisplayManagerSDL.h
			intern/GHOST_SystemSDL.h
			intern/GHOST_WindowSDL.h
		)
		add_definitions(-DWITH_GHOST_SDL)
	endif()


	# ack, this is still system dependant
	if(APPLE)
		if(WITH_COCOA)
			list(APPEND SRC
				intern/GHOST_SystemPathsCocoa.mm
				intern/GHOST_SystemPathsCocoa.h
			)
		else()
			list(APPEND SRC
				intern/GHOST_SystemPathsCarbon.cpp
				intern/GHOST_SystemPathsCarbon.h
			)
		endif()
	elseif(UNIX)
		list(APPEND SRC
			intern/GHOST_SystemPathsX11.cpp
			intern/GHOST_SystemPathsX11.h
		)

		if(NOT WITH_INSTALL_PORTABLE)
			add_definitions(-DPREFIX="${CMAKE_INSTALL_PREFIX}")
		endif()
	elseif(WIN32)

		list(APPEND SRC
			intern/GHOST_SystemPathsWin32.cpp

			intern/GHOST_SystemPathsWin32.h
		)
	endif()

	if(NOT WITH_HEADLESS)
		list(APPEND INC_SYS
			${SDL_INCLUDE_DIR}
		)
	endif()

elseif(APPLE)
	if(WITH_COCOA)
		list(APPEND SRC
			intern/GHOST_DisplayManagerCocoa.mm
			intern/GHOST_SystemCocoa.mm
			intern/GHOST_SystemPathsCocoa.mm
			intern/GHOST_WindowCocoa.mm

			intern/GHOST_DisplayManagerCocoa.h
			intern/GHOST_SystemCocoa.h
			intern/GHOST_SystemPathsCocoa.h
			intern/GHOST_WindowCocoa.h
		)

		if(WITH_INPUT_NDOF)
			list(APPEND SRC
				intern/GHOST_NDOFManagerCocoa.mm

				intern/GHOST_NDOFManagerCocoa.h
			)
		endif()

	else()
		list(APPEND SRC
			intern/GHOST_DisplayManagerCarbon.cpp
			intern/GHOST_SystemCarbon.cpp
			intern/GHOST_SystemPathsCarbon.cpp
			intern/GHOST_WindowCarbon.cpp

			intern/GHOST_DisplayManagerCarbon.h
			intern/GHOST_SystemCarbon.h
			intern/GHOST_SystemPathsCarbon.h
			intern/GHOST_WindowCarbon.h
		)
	endif()

	if(WITH_CODEC_QUICKTIME)
		add_definitions(-DWITH_QUICKTIME)
	endif()

elseif(UNIX)

	list(APPEND INC_SYS
		${X11_X11_INCLUDE_PATH}
	)

	list(APPEND SRC
		intern/GHOST_DisplayManagerX11.cpp
		intern/GHOST_SystemX11.cpp
		intern/GHOST_SystemPathsX11.cpp
		intern/GHOST_WindowX11.cpp

		intern/GHOST_DisplayManagerX11.h
		intern/GHOST_SystemX11.h
		intern/GHOST_SystemPathsX11.h
		intern/GHOST_WindowX11.h
	)

	if(WITH_GHOST_XDND)
		add_definitions(-DWITH_XDND)

		list(APPEND INC
			../../extern/xdnd
		)

		list(APPEND SRC
			intern/GHOST_DropTargetX11.cpp

			intern/GHOST_DropTargetX11.h
		)
	endif()

	if(X11_XF86keysym_INCLUDE_PATH)
		add_definitions(-DWITH_XF86KEYSYM)
		list(APPEND INC_SYS
			${X11_XF86keysym_INCLUDE_PATH}
		)
	endif()

	if(WITH_X11_XF86VMODE)
		add_definitions(-DWITH_X11_XF86VMODE)
		list(APPEND INC_SYS
			${X11_xf86vmode_INCLUDE_PATH}
		)
	endif()

	if(WITH_INPUT_NDOF)
		list(APPEND SRC
			intern/GHOST_NDOFManagerX11.cpp

			intern/GHOST_NDOFManagerX11.h
		)
	endif()

	if(NOT WITH_INSTALL_PORTABLE)
		add_definitions(-DPREFIX="${CMAKE_INSTALL_PREFIX}")
	endif()

	if(WITH_X11_XINPUT)
		add_definitions(-DWITH_X11_XINPUT)
	endif()

elseif(WIN32)
	## Warnings as errors, this is too strict!
	#if(MSVC)
	#	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /WX")
	#endif()

	list(APPEND INC_SYS
		${WINTAB_INC}
	)

	list(APPEND INC
		../utfconv
	)

	list(APPEND SRC
		intern/GHOST_DisplayManagerWin32.cpp
		intern/GHOST_SystemWin32.cpp
		intern/GHOST_SystemPathsWin32.cpp
		intern/GHOST_WindowWin32.cpp
		intern/GHOST_DropTargetWin32.cpp

		intern/GHOST_DisplayManagerWin32.h
		intern/GHOST_DropTargetWin32.h
		intern/GHOST_SystemWin32.h
		intern/GHOST_SystemPathsWin32.h
		intern/GHOST_WindowWin32.h
		intern/GHOST_TaskbarWin32.h
	)

	if(WITH_INPUT_NDOF)
		list(APPEND SRC
			intern/GHOST_NDOFManagerWin32.cpp

			intern/GHOST_NDOFManagerWin32.h
		)
	endif()

endif()

blender_add_lib(bf_intern_ghost "${SRC}" "${INC}" "${INC_SYS}")

