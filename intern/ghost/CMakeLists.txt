# SPDX-FileCopyrightText: 2006 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  PUBLIC .
  ../guardedalloc
)

set(INC_SYS
)

set(SRC
  intern/GHOST_Buttons.cc
  intern/GHOST_C-api.cc
  intern/GHOST_CallbackEventConsumer.cc
  intern/GHOST_Context.cc
  intern/GHOST_ContextNone.cc
  intern/GHOST_EventManager.cc
  intern/GHOST_ISystem.cc
  intern/GHOST_ISystemPaths.cc
  intern/GHOST_ModifierKeys.cc
  intern/GHOST_Path-api.cc
  intern/GHOST_PathUtils.cc
  intern/GHOST_Rect.cc
  intern/GHOST_System.cc
  intern/GHOST_TimerManager.cc
  intern/GHOST_Window.cc
  intern/GHOST_WindowManager.cc

  GHOST_C-api.h
  GHOST_IContext.hh
  GHOST_IEvent.hh
  GHOST_IEventConsumer.hh
  GHOST_ISystem.hh
  GHOST_ISystemPaths.hh
  GHOST_ITimerTask.hh
  GHOST_IWindow.hh
  GHOST_Path-api.hh
  GHOST_Rect.hh
  GHOST_Types.h

  intern/GHOST_Buttons.hh
  intern/GHOST_CallbackEventConsumer.hh
  intern/GHOST_Context.hh
  intern/GHOST_ContextNone.hh
  intern/GHOST_Debug.hh
  intern/GHOST_Event.hh
  intern/GHOST_EventButton.hh
  intern/GHOST_EventCursor.hh
  intern/GHOST_EventDragnDrop.hh
  intern/GHOST_EventKey.hh
  intern/GHOST_EventManager.hh
  intern/GHOST_EventString.hh
  intern/GHOST_EventTrackpad.hh
  intern/GHOST_EventWheel.hh
  intern/GHOST_ModifierKeys.hh
  intern/GHOST_PathUtils.hh
  intern/GHOST_System.hh
  intern/GHOST_SystemPaths.hh
  intern/GHOST_TimerManager.hh
  intern/GHOST_TimerTask.hh
  intern/GHOST_Util.hh
  intern/GHOST_Window.hh
  intern/GHOST_WindowManager.hh
  intern/GHOST_utildefines.hh
  intern/GHOST_utildefines_variadic.hh
)

set(LIB
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  PRIVATE bf::gpu
  PRIVATE bf::imbuf
  PRIVATE bf::intern::clog
)

if(WITH_INPUT_IME)
  add_definitions(-DWITH_INPUT_IME)
endif()

if(WITH_OPENGL_BACKEND)
  list(APPEND INC_SYS
    ${Epoxy_INCLUDE_DIRS}
  )
  list(APPEND LIB
    ${Epoxy_LIBRARIES}
  )
  add_definitions(-DWITH_OPENGL_BACKEND)
endif()

if(WITH_VULKAN_BACKEND)
  list(APPEND SRC
    intern/GHOST_ContextVK.cc

    intern/GHOST_ContextVK.hh
  )
  list(APPEND INC
    ../../extern/vulkan_memory_allocator/
  )
  list(APPEND INC_SYS
    PUBLIC ${VULKAN_INCLUDE_DIRS}
  )

  list(APPEND LIB
    ${VULKAN_LIBRARIES}
  )

  add_definitions(-DWITH_VULKAN_BACKEND)
endif()

if(WITH_GHOST_DEBUG)
  list(APPEND SRC
    intern/GHOST_EventPrinter.cc

    intern/GHOST_EventPrinter.hh
  )
  add_definitions(-DWITH_GHOST_DEBUG)
endif()

if(WITH_INPUT_NDOF)
  add_definitions(-DWITH_INPUT_NDOF)

  list(APPEND SRC
    intern/GHOST_NDOFManager.cc

    intern/GHOST_EventNDOF.hh
    intern/GHOST_NDOFManager.hh
  )

  # Only some platforms define this.
  if(DEFINED NDOF_INCLUDE_DIRS)
    list(APPEND INC_SYS
      ${NDOF_INCLUDE_DIRS}
    )
  endif()
  if(DEFINED NDOF_LIBRARIES)
    list(APPEND LIB
      ${NDOF_LIBRARIES}
    )
  endif()
endif()

list(APPEND SRC
  intern/GHOST_SystemHeadless.hh
  intern/GHOST_WindowNULL.hh
)

if(WITH_HEADLESS)
  add_definitions(-DWITH_HEADLESS)
elseif(WITH_GHOST_SDL)
  list(APPEND SRC
    intern/GHOST_ContextSDL.cc
    intern/GHOST_SystemSDL.cc
    intern/GHOST_WindowSDL.cc

    intern/GHOST_ContextSDL.hh
    intern/GHOST_SystemSDL.hh
    intern/GHOST_WindowSDL.hh
  )
  add_definitions(-DWITH_GHOST_SDL)

  list(APPEND INC_SYS
    ${SDL_INCLUDE_DIR}
  )
  list(APPEND LIB
    ${SDL_LIBRARY}
  )
elseif(APPLE AND NOT WITH_GHOST_X11)
  list(APPEND SRC
    intern/GHOST_SystemCocoa.mm
    intern/GHOST_WindowCocoa.mm

    intern/GHOST_SystemCocoa.hh
    intern/GHOST_WindowCocoa.hh
    intern/GHOST_WindowViewCocoa.hh
  )

  if(WITH_INPUT_NDOF)
    list(APPEND SRC
      intern/GHOST_NDOFManagerCocoa.mm

      intern/GHOST_NDOFManagerCocoa.hh
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
      intern/GHOST_SystemX11.cc
      intern/GHOST_WindowX11.cc

      intern/GHOST_IconX11.hh
      intern/GHOST_SystemX11.hh
      intern/GHOST_WindowX11.hh
    )

    if(WITH_OPENGL_BACKEND)
      list(APPEND SRC
        intern/GHOST_ContextGLX.cc

        intern/GHOST_ContextGLX.hh
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
        intern/GHOST_DropTargetX11.cc

        intern/GHOST_DropTargetX11.hh
      )
    endif()

    if(X11_XF86keysym_INCLUDE_PATH)
      add_definitions(-DWITH_XF86KEYSYM)
      list(APPEND INC_SYS
        ${X11_XF86keysym_INCLUDE_PATH}
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

    if(WITH_GHOST_WAYLAND_LIBDECOR)
      # See code-comment for `USE_LIBDECOR_CONFIG_COPY_WORKAROUND`.
      check_symbol_exists(malloc_usable_size "malloc.h" HAVE_MALLOC_USABLE_SIZE)
      if(HAVE_MALLOC_USABLE_SIZE)
        add_definitions(-DHAVE_MALLOC_USABLE_SIZE)
      endif()
    endif()

    list(APPEND SRC
      intern/GHOST_SystemWayland.cc
      intern/GHOST_WindowWayland.cc

      intern/GHOST_SystemWayland.hh
      intern/GHOST_WaylandUtils.hh
      intern/GHOST_WindowWayland.hh
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

      if(CMAKE_C_COMPILER_ID MATCHES "Clang")
        # Prevent warnings/failure to compile with generated `WL_PRIVATE` declarations.
        set_source_files_properties(
          "${INC_DST}/${_name}-client-protocol.c"
          PROPERTIES COMPILE_FLAGS "-Wno-missing-variable-declarations"
        )
      endif()

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
    # `xdg-activation`.
    generate_protocol_bindings(
      "${WAYLAND_PROTOCOLS_DIR}/staging/xdg-activation/xdg-activation-v1.xml"
    )
    # Fractional scale.
    generate_protocol_bindings(
      "${WAYLAND_PROTOCOLS_DIR}/staging/fractional-scale/fractional-scale-v1.xml"
    )
    # ViewPorter (only required when fractional scale is in use).
    generate_protocol_bindings(
      "${WAYLAND_PROTOCOLS_DIR}/stable/viewporter/viewporter.xml"
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
      "${WAYLAND_PROTOCOLS_DIR}/stable/tablet/tablet-v2.xml"
    )
    # Primary-selection.
    generate_protocol_bindings(
      "${WAYLAND_PROTOCOLS_DIR}/unstable/primary-selection/primary-selection-unstable-v1.xml"
    )
    if(WITH_INPUT_IME)
      generate_protocol_bindings(
        "${WAYLAND_PROTOCOLS_DIR}/unstable/text-input/text-input-unstable-v3.xml"
      )
    endif()
    # Desktop compositor controlled cursor rendering.
    generate_protocol_bindings(
      "${WAYLAND_PROTOCOLS_DIR}/staging/cursor-shape/cursor-shape-v1.xml"
    )

    unset(INC_DST)

    add_definitions(-DWITH_GHOST_WAYLAND)
    if(NOT WITH_GHOST_WAYLAND_APP_ID STREQUAL "")
      add_definitions(-DWITH_GHOST_WAYLAND_APP_ID=${WITH_GHOST_WAYLAND_APP_ID})
    endif()

  endif()

  if(WITH_INPUT_NDOF)
    list(APPEND SRC
      intern/GHOST_NDOFManagerUnix.cc

      intern/GHOST_NDOFManagerUnix.hh
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
    intern/GHOST_ContextD3D.cc
    intern/GHOST_DropTargetWin32.cc
    intern/GHOST_SystemWin32.cc
    intern/GHOST_TrackpadWin32.cc
    intern/GHOST_WindowWin32.cc
    intern/GHOST_Wintab.cc

    intern/GHOST_ContextD3D.hh
    intern/GHOST_DropTargetWin32.hh
    intern/GHOST_SystemWin32.hh
    intern/GHOST_TaskbarWin32.hh
    intern/GHOST_TrackpadWin32.hh
    intern/GHOST_WindowWin32.hh
    intern/GHOST_Wintab.hh
  )

  list(APPEND SRC
    intern/GHOST_ContextWGL.cc

    intern/GHOST_ContextWGL.hh
  )

  if(WITH_INPUT_IME)
    list(APPEND SRC
      intern/GHOST_ImeWin32.cc

      intern/GHOST_ImeWin32.hh
    )
  endif()

  if(WITH_INPUT_NDOF)
    list(APPEND SRC
      intern/GHOST_NDOFManagerWin32.cc

      intern/GHOST_NDOFManagerWin32.hh
    )
  endif()
endif()

if(UNIX AND NOT APPLE)
  if(WITH_OPENGL_BACKEND)
    list(APPEND SRC
      intern/GHOST_ContextEGL.cc

      intern/GHOST_ContextEGL.hh
    )
  endif()
endif()

if(APPLE)
  if(WITH_METAL_BACKEND)
    list(APPEND SRC
      intern/GHOST_ContextMTL.mm

      intern/GHOST_ContextMTL.hh
    )
  endif()
endif()

if(APPLE)
  list(APPEND SRC
    intern/GHOST_SystemPathsCocoa.hh
    intern/GHOST_SystemPathsCocoa.mm
  )

elseif(UNIX)
  list(APPEND SRC
    intern/GHOST_SystemPathsUnix.cc
    intern/GHOST_SystemPathsUnix.hh
  )

  if(NOT WITH_INSTALL_PORTABLE)
    add_definitions(-DPREFIX="${CMAKE_INSTALL_PREFIX}")
  endif()

elseif(WIN32)
  list(APPEND SRC
    intern/GHOST_SystemPathsWin32.cc
    intern/GHOST_SystemPathsWin32.hh
  )

  list(APPEND INC
    ../utfconv
  )

endif()

if(WITH_XR_OPENXR)
  list(APPEND SRC
    intern/GHOST_Xr.cc
    intern/GHOST_XrAction.cc
    intern/GHOST_XrContext.cc
    intern/GHOST_XrControllerModel.cc
    intern/GHOST_XrEvent.cc
    intern/GHOST_XrGraphicsBinding.cc
    intern/GHOST_XrSession.cc
    intern/GHOST_XrSwapchain.cc

    GHOST_IXrContext.hh
    intern/GHOST_IXrGraphicsBinding.hh
    intern/GHOST_XrAction.hh
    intern/GHOST_XrContext.hh
    intern/GHOST_XrControllerModel.hh
    intern/GHOST_XrException.hh
    intern/GHOST_XrSession.hh
    intern/GHOST_XrSwapchain.hh
    intern/GHOST_Xr_intern.hh
    intern/GHOST_Xr_openxr_includes.hh

    # Header only library.
    ../../extern/tinygltf/tiny_gltf.h
  )
  if(APPLE)
    list(APPEND SRC
      intern/GHOST_XrGraphicsBindingMetal.mm

      intern/GHOST_XrGraphicsBindingMetal.hh
    )
  endif()

  if(WIN32)
    list(APPEND SRC
      intern/GHOST_XrGraphicsBindingD3D.cc

      intern/GHOST_XrGraphicsBindingD3D.hh
    )
  endif()

  list(APPEND INC_SYS
    ../../extern/json/include
    ../../extern/tinygltf
  )
  list(APPEND INC_SYS
    ${XR_OPENXR_SDK_INCLUDE_DIR}
  )
  list(APPEND LIB
    ${XR_OPENXR_SDK_LIBRARIES}
    PRIVATE bf::dependencies::eigen
  )

  set(XR_PLATFORM_DEFINES
  )
  if(WITH_OPENGL_BACKEND)
    list(APPEND XR_PLATFORM_DEFINES -DXR_USE_GRAPHICS_API_OPENGL)
  endif()
  if(WITH_VULKAN_BACKEND)
    list(APPEND XR_PLATFORM_DEFINES -DXR_USE_GRAPHICS_API_VULKAN)
    list(APPEND SRC
      intern/GHOST_XrGraphicsBindingVulkan.cc

      intern/GHOST_XrGraphicsBindingVulkan.hh
    )
  endif()
  if(WITH_METAL_BACKEND)
    list(APPEND XR_PLATFORM_DEFINES -DXR_USE_GRAPHICS_API_METAL)
  endif()

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
