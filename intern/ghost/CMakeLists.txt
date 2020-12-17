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
# ***** END GPL LICENSE BLOCK *****

set(INC
  .
  ../glew-mx
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
  intern/GHOST_Context.cpp
  intern/GHOST_ContextNone.cpp
  intern/GHOST_DisplayManager.cpp
  intern/GHOST_EventManager.cpp
  intern/GHOST_ISystem.cpp
  intern/GHOST_ISystemPaths.cpp
  intern/GHOST_ModifierKeys.cpp
  intern/GHOST_Path-api.cpp
  intern/GHOST_Rect.cpp
  intern/GHOST_System.cpp
  intern/GHOST_TimerManager.cpp
  intern/GHOST_Window.cpp
  intern/GHOST_WindowManager.cpp

  GHOST_C-api.h
  GHOST_IContext.h
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
  intern/GHOST_Context.h
  intern/GHOST_ContextNone.h
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

set(LIB
  bf_intern_glew_mx
  ${GLEW_LIBRARY}
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
  list(APPEND LIB
    ${NDOF_LIBRARIES}
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
      intern/GHOST_ContextSDL.cpp
      intern/GHOST_DisplayManagerSDL.cpp
      intern/GHOST_SystemSDL.cpp
      intern/GHOST_WindowSDL.cpp

      intern/GHOST_ContextSDL.h
      intern/GHOST_DisplayManagerSDL.h
      intern/GHOST_SystemSDL.h
      intern/GHOST_WindowSDL.h
    )
    add_definitions(-DWITH_GHOST_SDL)
  endif()

  if(NOT WITH_HEADLESS)
    list(APPEND INC_SYS
      ${SDL_INCLUDE_DIR}
    )
    if(WITH_SDL_DYNLOAD)
      list(APPEND LIB
        extern_sdlew
      )
    else()
      list(APPEND LIB
        ${SDL_LIBRARY}
      )
    endif()
  endif()

elseif(APPLE AND NOT WITH_GHOST_X11)
  list(APPEND SRC
    intern/GHOST_DisplayManagerCocoa.mm
    intern/GHOST_SystemCocoa.mm
    intern/GHOST_WindowCocoa.mm

    intern/GHOST_DisplayManagerCocoa.h
    intern/GHOST_SystemCocoa.h
    intern/GHOST_WindowCocoa.h
    intern/GHOST_WindowViewCocoa.h
  )

  if(NOT WITH_GL_EGL)
    list(APPEND SRC
      intern/GHOST_ContextCGL.mm

      intern/GHOST_ContextCGL.h
    )
  endif()

  if(WITH_INPUT_NDOF)
    list(APPEND SRC
      intern/GHOST_NDOFManagerCocoa.mm

      intern/GHOST_NDOFManagerCocoa.h
    )
  endif()

elseif(WITH_GHOST_X11 OR WITH_GHOST_WAYLAND)
  if(WITH_GHOST_X11)
    list(APPEND INC_SYS
      ${X11_X11_INCLUDE_PATH}
    )

    list(APPEND LIB
      ${X11_X11_LIB}
      ${X11_Xrender_LIB}
    )

    list(APPEND SRC
      intern/GHOST_DisplayManagerX11.cpp
      intern/GHOST_SystemX11.cpp
      intern/GHOST_TaskbarX11.cpp
      intern/GHOST_WindowX11.cpp

      intern/GHOST_DisplayManagerX11.h
      intern/GHOST_IconX11.h
      intern/GHOST_SystemX11.h
      intern/GHOST_TaskbarX11.h
      intern/GHOST_WindowX11.h
    )

    if(NOT WITH_GL_EGL)
      list(APPEND SRC
        intern/GHOST_ContextGLX.cpp

        intern/GHOST_ContextGLX.h
      )
    endif()

    if(WITH_GHOST_XDND)
      add_definitions(-DWITH_XDND)

      list(APPEND LIB
        extern_xdnd
      )

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
      list(APPEND LIB
        ${X11_Xf86vmode_LIB}
      )
    endif()

    if(WITH_X11_XFIXES)
      add_definitions(-DWITH_X11_XFIXES)
      list(APPEND INC_SYS
        ${X11_Xfixes_INCLUDE_PATH}
      )
      list(APPEND LIB
        ${X11_Xfixes_LIB}
      )
    endif()

    if(WITH_X11_ALPHA)
      add_definitions(-DWITH_X11_ALPHA)
    endif()

    if(WITH_X11_XINPUT)
      add_definitions(-DWITH_X11_XINPUT)
      list(APPEND INC_SYS
        ${X11_Xinput_INCLUDE_PATH}
      )
      list(APPEND LIB
        ${X11_Xinput_LIB}
      )
    endif()

    add_definitions(-DWITH_GHOST_X11)
  endif()

  if(WITH_GHOST_WAYLAND)
    list(APPEND INC_SYS
      ${wayland-client_INCLUDE_DIRS}
      ${wayland-egl_INCLUDE_DIRS}
      ${xkbcommon_INCLUDE_DIRS}
      ${wayland-cursor_INCLUDE_DIRS}
    )

    list(APPEND SRC
      intern/GHOST_SystemWayland.cpp
      intern/GHOST_WindowWayland.cpp

      intern/GHOST_SystemWayland.h
      intern/GHOST_WindowWayland.h
    )

    pkg_get_variable(WAYLAND_SCANNER wayland-scanner wayland_scanner)
    pkg_get_variable(WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)

    # Generate protocols bindings.
    macro(generate_protocol_bindings NAME PROT_DEF)
      add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${NAME}-client-protocol.h
        COMMAND ${WAYLAND_SCANNER} client-header ${PROT_DEF} ${NAME}-client-protocol.h
      )
      add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${NAME}-client-protocol.c
        COMMAND ${WAYLAND_SCANNER} private-code ${PROT_DEF} ${NAME}-client-protocol.c
        DEPENDS ${NAME}-client-protocol.h
      )
      list(APPEND SRC
        ${CMAKE_CURRENT_BINARY_DIR}/${NAME}-client-protocol.c
        ${CMAKE_CURRENT_BINARY_DIR}/${NAME}-client-protocol.h
      )
    endmacro()

    list(APPEND INC_SYS
      ${CMAKE_CURRENT_BINARY_DIR}
    )

    # xdg-shell.
    generate_protocol_bindings(
      xdg-shell
      "${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml"
    )
    # Pointer-constraints.
    generate_protocol_bindings(
      pointer-constraints
      "${WAYLAND_PROTOCOLS_DIR}/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml"
    )
    # Relative-pointer.
    generate_protocol_bindings(
      relative-pointer
      "${WAYLAND_PROTOCOLS_DIR}/unstable/relative-pointer/relative-pointer-unstable-v1.xml"
    )

    add_definitions(-DWITH_GHOST_WAYLAND)
  endif()

  if(WITH_INPUT_NDOF)
    list(APPEND SRC
      intern/GHOST_NDOFManagerUnix.cpp

      intern/GHOST_NDOFManagerUnix.h
    )
  endif()

  if(NOT WITH_INSTALL_PORTABLE)
    add_definitions(-DPREFIX="${CMAKE_INSTALL_PREFIX}")
  endif()


elseif(WIN32)
  # # Warnings as errors, this is too strict!
  # if(MSVC)
  #   string(APPEND CMAKE_CXX_FLAGS " /WX")
  # endif()

  list(APPEND INC_SYS
    ${WINTAB_INC}
  )

  list(APPEND SRC
    intern/GHOST_ContextD3D.cpp
    intern/GHOST_DisplayManagerWin32.cpp
    intern/GHOST_DropTargetWin32.cpp
    intern/GHOST_SystemWin32.cpp
    intern/GHOST_WindowWin32.cpp

    intern/GHOST_ContextD3D.h
    intern/GHOST_DisplayManagerWin32.h
    intern/GHOST_DropTargetWin32.h
    intern/GHOST_SystemWin32.h
    intern/GHOST_TaskbarWin32.h
    intern/GHOST_WindowWin32.h
  )

  if(NOT WITH_GL_EGL)
    list(APPEND SRC
      intern/GHOST_ContextWGL.cpp

      intern/GHOST_ContextWGL.h
    )
  endif()

  if(WITH_INPUT_IME)
    add_definitions(-DWITH_INPUT_IME)

    list(APPEND SRC
      intern/GHOST_ImeWin32.cpp

      intern/GHOST_ImeWin32.h
    )
  endif()

  if(WITH_INPUT_NDOF)
    list(APPEND SRC
      intern/GHOST_NDOFManagerWin32.cpp

      intern/GHOST_NDOFManagerWin32.h
    )
  endif()
endif()

if(WITH_GL_EGL AND NOT (WITH_HEADLESS OR WITH_GHOST_SDL))
  list(APPEND SRC
    intern/GHOST_ContextEGL.cpp

    intern/GHOST_ContextEGL.h
  )
endif()

if(APPLE)
  list(APPEND SRC
    intern/GHOST_SystemPathsCocoa.h
    intern/GHOST_SystemPathsCocoa.mm
  )

elseif(UNIX)
  list(APPEND SRC
    intern/GHOST_SystemPathsUnix.cpp
    intern/GHOST_SystemPathsUnix.h
  )

  if(NOT WITH_INSTALL_PORTABLE)
    add_definitions(-DPREFIX="${CMAKE_INSTALL_PREFIX}")
  endif()

elseif(WIN32)
  list(APPEND SRC
    intern/GHOST_SystemPathsWin32.cpp
    intern/GHOST_SystemPathsWin32.h
  )

  list(APPEND INC
    ../utfconv
  )

endif()

if(WITH_XR_OPENXR)
  list(APPEND SRC
    intern/GHOST_Xr.cpp
    intern/GHOST_XrContext.cpp
    intern/GHOST_XrEvent.cpp
    intern/GHOST_XrGraphicsBinding.cpp
    intern/GHOST_XrSession.cpp
    intern/GHOST_XrSwapchain.cpp

    GHOST_IXrContext.h
    intern/GHOST_IXrGraphicsBinding.h
    intern/GHOST_XrContext.h
    intern/GHOST_XrException.h
    intern/GHOST_XrSession.h
    intern/GHOST_XrSwapchain.h
    intern/GHOST_Xr_intern.h
    intern/GHOST_Xr_openxr_includes.h
  )
  list(APPEND INC_SYS
    ${XR_OPENXR_SDK_INCLUDE_DIR}
  )
  list(APPEND LIB
    ${XR_OPENXR_SDK_LIBRARIES}
  )

  set(XR_PLATFORM_DEFINES -DXR_USE_GRAPHICS_API_OPENGL)

  # Add compiler defines as required by the OpenXR specification.
  if(WIN32)
    list(APPEND XR_PLATFORM_DEFINES
      -DXR_USE_PLATFORM_WIN32
      -DXR_USE_GRAPHICS_API_D3D11
    )
    list(APPEND LIB
      shlwapi
    )
  elseif(UNIX AND NOT APPLE)
    list(APPEND XR_PLATFORM_DEFINES
      -DXR_OS_LINUX
      -DXR_USE_PLATFORM_XLIB
    )
  endif()

  add_definitions(-DWITH_XR_OPENXR ${XR_PLATFORM_DEFINES})

  unset(XR_PLATFORM_DEFINES)
endif()

add_definitions(${GL_DEFINITIONS})

blender_add_lib(bf_intern_ghost "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
