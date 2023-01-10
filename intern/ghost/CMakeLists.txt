# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2006 Blender Foundation. All rights reserved.

set(INC
  .
  ../clog
  ../../source/blender/blenlib
  ../../source/blender/imbuf
  ../../source/blender/makesdna
)

set(INC_SYS
  ${Epoxy_INCLUDE_DIRS}
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
  intern/GHOST_PathUtils.cpp
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
  intern/GHOST_PathUtils.h
  intern/GHOST_System.h
  intern/GHOST_SystemPaths.h
  intern/GHOST_TimerManager.h
  intern/GHOST_TimerTask.h
  intern/GHOST_Util.h
  intern/GHOST_Window.h
  intern/GHOST_WindowManager.h
  intern/GHOST_utildefines.h
  intern/GHOST_utildefines_variadic.h
)

set(LIB
  ${Epoxy_LIBRARIES}
)

if(WITH_VULKAN_BACKEND)
  list(APPEND SRC
    intern/GHOST_ContextVK.cpp

    intern/GHOST_ContextVK.h
  )

  list(APPEND INC_SYS
    ${VULKAN_INCLUDE_DIRS}
  )

  list(APPEND LIB
    ${VULKAN_LIBRARIES}
  )

  add_definitions(-DWITH_VULKAN_BACKEND)
endif()

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

list(APPEND SRC
  intern/GHOST_DisplayManagerNULL.h
  intern/GHOST_SystemHeadless.h
  intern/GHOST_WindowNULL.h
)

if(WITH_HEADLESS)
  add_definitions(-DWITH_HEADLESS)
elseif(WITH_GHOST_SDL)
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

elseif(APPLE AND NOT WITH_GHOST_X11)
  if(WITH_INPUT_IME)
    add_definitions(-DWITH_INPUT_IME)
  endif()
  list(APPEND SRC
    intern/GHOST_DisplayManagerCocoa.mm
    intern/GHOST_SystemCocoa.mm
    intern/GHOST_WindowCocoa.mm

    intern/GHOST_DisplayManagerCocoa.h
    intern/GHOST_SystemCocoa.h
    intern/GHOST_WindowCocoa.h
    intern/GHOST_WindowViewCocoa.h
  )

  list(APPEND SRC
    intern/GHOST_ContextCGL.mm

    intern/GHOST_ContextCGL.h
  )

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

    list(APPEND SRC
      intern/GHOST_ContextGLX.cpp

      intern/GHOST_ContextGLX.h
    )

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
        ${X11_Xxf86vmode_INCLUDE_PATH}
      )
      list(APPEND LIB
        ${X11_Xxf86vmode_LIB}
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
    list(APPEND LIB
      ${xkbcommon_LINK_LIBRARIES}
    )

    if(WITH_GHOST_WAYLAND_DYNLOAD)
      list(APPEND INC_SYS
        ../wayland_dynload/extern
      )
      list(APPEND LIB
        bf_intern_wayland_dynload
      )
      add_definitions(-DWITH_GHOST_WAYLAND_DYNLOAD)
    else()
      list(APPEND LIB
        ${wayland-client_LINK_LIBRARIES}
        ${wayland-egl_LINK_LIBRARIES}
        ${wayland-cursor_LINK_LIBRARIES}
      )
    endif()

    if(WITH_GHOST_WAYLAND_DBUS)
      list(APPEND INC_SYS
        ${dbus_INCLUDE_DIRS}
      )
      list(APPEND LIB
        ${dbus_LINK_LIBRARIES}
      )
    endif()

    if(WITH_GHOST_WAYLAND_LIBDECOR)
      list(APPEND INC_SYS
        ${libdecor_INCLUDE_DIRS}
      )
      if(NOT WITH_GHOST_WAYLAND_DYNLOAD)
        list(APPEND LIB
          ${libdecor_LIBRARIES}
        )
      endif()
    endif()

    include(CheckSymbolExists)
    set(CMAKE_REQUIRED_DEFINITIONS "-D_GNU_SOURCE")
    check_symbol_exists(memfd_create "sys/mman.h" HAVE_MEMFD_CREATE)
    unset(CMAKE_REQUIRED_DEFINITIONS)
    if(HAVE_MEMFD_CREATE)
      add_definitions(-DHAVE_MEMFD_CREATE)
    endif()

    check_symbol_exists(poll "poll.h" HAVE_POLL)
    if(HAVE_POLL)
      add_definitions(-DHAVE_POLL)
    endif()

    list(APPEND SRC
      intern/GHOST_SystemWayland.cpp
      intern/GHOST_WindowWayland.cpp

      intern/GHOST_SystemWayland.h
      intern/GHOST_WaylandCursorSettings.h
      intern/GHOST_WaylandUtils.h
      intern/GHOST_WindowWayland.h
    )

    set(INC_DST ${CMAKE_CURRENT_BINARY_DIR}/libwayland)

    # Generate protocols bindings.
    macro(generate_protocol_bindings PROT_DEF)
      # File name without directory or extension (use for header name).
      get_filename_component(_name ${PROT_DEF} NAME_WLE)
      add_custom_command(
        OUTPUT ${INC_DST}/${_name}-client-protocol.h
        COMMAND ${CMAKE_COMMAND} -E make_directory ${INC_DST}
        COMMAND ${WAYLAND_SCANNER} client-header ${PROT_DEF} ${INC_DST}/${_name}-client-protocol.h
      )
      add_custom_command(
        OUTPUT ${INC_DST}/${_name}-client-protocol.c
        COMMAND ${CMAKE_COMMAND} -E make_directory ${INC_DST}
        COMMAND ${WAYLAND_SCANNER} private-code ${PROT_DEF} ${INC_DST}/${_name}-client-protocol.c
        DEPENDS ${INC_DST}/${_name}-client-protocol.h
      )
      list(APPEND SRC
        ${INC_DST}/${_name}-client-protocol.c
        ${INC_DST}/${_name}-client-protocol.h
      )
      unset(_name)
    endmacro()


    list(APPEND INC_SYS
      ${INC_DST}
    )

    # Used when: LIBDECOR is not needed.
    # `xdg-shell`.
    generate_protocol_bindings(
      "${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml"
    )
    # `xdg-decoration`.
    generate_protocol_bindings(
      "${WAYLAND_PROTOCOLS_DIR}/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml"
    )
    # End LIBDECOR alternative.

    # `xdg-output`.
    generate_protocol_bindings(
      "${WAYLAND_PROTOCOLS_DIR}/unstable/xdg-output/xdg-output-unstable-v1.xml"
    )
    # Pointer-constraints.
    generate_protocol_bindings(
      "${WAYLAND_PROTOCOLS_DIR}/unstable/pointer-constraints/pointer-constraints-unstable-v1.xml"
    )
    # Relative-pointer.
    generate_protocol_bindings(
      "${WAYLAND_PROTOCOLS_DIR}/unstable/relative-pointer/relative-pointer-unstable-v1.xml"
    )
    # Pointer-gestures (multi-touch).
    generate_protocol_bindings(
      "${WAYLAND_PROTOCOLS_DIR}/unstable/pointer-gestures/pointer-gestures-unstable-v1.xml"
    )
    # Tablet.
    generate_protocol_bindings(
      "${WAYLAND_PROTOCOLS_DIR}/unstable/tablet/tablet-unstable-v2.xml"
    )
    # Primary-selection.
    generate_protocol_bindings(
      "${WAYLAND_PROTOCOLS_DIR}/unstable/primary-selection/primary-selection-unstable-v1.xml"
    )

    unset(INC_DST)

    add_definitions(-DWITH_GHOST_WAYLAND)

    if(NOT WITH_GHOST_WAYLAND_APP_ID STREQUAL "")
      add_definitions(-DWITH_GHOST_WAYLAND_APP_ID=${WITH_GHOST_WAYLAND_APP_ID})
    endif()

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
    intern/GHOST_TrackpadWin32.cpp
    intern/GHOST_WindowWin32.cpp
    intern/GHOST_Wintab.cpp

    intern/GHOST_ContextD3D.h
    intern/GHOST_DisplayManagerWin32.h
    intern/GHOST_DropTargetWin32.h
    intern/GHOST_SystemWin32.h
    intern/GHOST_TaskbarWin32.h
    intern/GHOST_TrackpadWin32.h
    intern/GHOST_WindowWin32.h
    intern/GHOST_Wintab.h
  )

  list(APPEND SRC
    intern/GHOST_ContextWGL.cpp

    intern/GHOST_ContextWGL.h
  )

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

if(UNIX AND NOT APPLE)
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
    intern/GHOST_XrAction.cpp
    intern/GHOST_XrContext.cpp
    intern/GHOST_XrControllerModel.cpp
    intern/GHOST_XrEvent.cpp
    intern/GHOST_XrGraphicsBinding.cpp
    intern/GHOST_XrSession.cpp
    intern/GHOST_XrSwapchain.cpp

    GHOST_IXrContext.h
    intern/GHOST_IXrGraphicsBinding.h
    intern/GHOST_XrAction.h
    intern/GHOST_XrContext.h
    intern/GHOST_XrControllerModel.h
    intern/GHOST_XrException.h
    intern/GHOST_XrSession.h
    intern/GHOST_XrSwapchain.h
    intern/GHOST_Xr_intern.h
    intern/GHOST_Xr_openxr_includes.h

    # Header only library.
    ../../extern/tinygltf/tiny_gltf.h
  )
  list(APPEND INC
    ../../extern/json/include
    ../../extern/tinygltf
  )
  list(APPEND INC_SYS
    ${EIGEN3_INCLUDE_DIRS}
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
    list(APPEND XR_PLATFORM_DEFINES -DXR_OS_LINUX)
    if(WITH_GHOST_WAYLAND)
      list(APPEND XR_PLATFORM_DEFINES -DXR_USE_PLATFORM_WAYLAND)
    endif()
    if(WITH_GHOST_X11)
      list(APPEND XR_PLATFORM_DEFINES -DXR_USE_PLATFORM_EGL)
      list(APPEND XR_PLATFORM_DEFINES -DXR_USE_PLATFORM_XLIB)
    endif()
  endif()

  add_definitions(-DWITH_XR_OPENXR ${XR_PLATFORM_DEFINES})

  unset(XR_PLATFORM_DEFINES)
endif()

blender_add_lib(bf_intern_ghost "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
