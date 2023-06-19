# SPDX-FileCopyrightText: 2022 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  extern

  # For internal includes.
  intern
)

set(INC_SYS
  ${wayland-client_INCLUDE_DIRS}
  ${wayland-egl_INCLUDE_DIRS}
  ${wayland-cursor_INCLUDE_DIRS}
)

set(SRC
  intern/wayland_dynload_client.c
  intern/wayland_dynload_cursor.c
  intern/wayland_dynload_egl.c
  intern/wayland_dynload_utils.c

  extern/wayland_dynload_API.h
  extern/wayland_dynload_client.h
  extern/wayland_dynload_cursor.h
  extern/wayland_dynload_egl.h
  intern/wayland_dynload_utils.h
)

if(WITH_GHOST_WAYLAND_LIBDECOR)
  list(APPEND INC_SYS
    ${libdecor_INCLUDE_DIRS}
  )
  list(APPEND SRC
    intern/wayland_dynload_libdecor.c

    extern/wayland_dynload_libdecor.h
  )
endif()

set(LIB
)


blender_add_lib(bf_intern_wayland_dynload "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
