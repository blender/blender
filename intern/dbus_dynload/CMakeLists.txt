# SPDX-FileCopyrightText: 2026 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  extern

  # For internal includes.
  intern
)

set(INC_SYS
  ${dbus_INCLUDE_DIRS}
)

set(SRC
  intern/dbus_dynload.c
  intern/dbus_dynload_utils.c

  extern/dbus_dynload.h
  extern/dbus_dynload_API.h
  intern/dbus_dynload_utils.h
)

set(LIB
)

blender_add_lib(bf_intern_dbus_dynload "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
