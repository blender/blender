/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_wayland_dynload
 *
 * Wrapper functions for `<wayland-cursor.h>`.
 */

#include <stdlib.h> /* `atexit`. */
#include <string.h>

#include "wayland_dynload_API.h"
#include "wayland_dynload_utils.h"

#include "wayland_dynload_cursor.h" /* Own include. */

struct WaylandDynload_Cursor wayland_dynload_cursor = {NULL};

static DynamicLibrary lib = NULL;

bool wayland_dynload_cursor_init(const bool verbose)
{
  /* Library paths. */
  const char *paths[] = {
      "libwayland-cursor.so.0",
      "libwayland-cursor.so",
  };
  const int paths_num = sizeof(paths) / sizeof(*paths);
  int path_index;
  if (!(lib = dynamic_library_open_array_with_error(paths, paths_num, verbose, &path_index))) {
    return false;
  }
  if (atexit(wayland_dynload_cursor_exit)) {
    return false;
  }

#define WAYLAND_DYNLOAD_FN(symbol) \
  if (!(wayland_dynload_cursor.symbol = dynamic_library_find_with_error( \
            lib, #symbol, paths[path_index], verbose))) { \
    return false; \
  }
#include "wayland_dynload_cursor.h"
#undef WAYLAND_DYNLOAD_FN

  return true;
}

void wayland_dynload_cursor_exit(void)
{
  if (lib != NULL) {
    dynamic_library_close(lib); /*  Ignore errors. */
    lib = NULL;
  }
}

/* Validate local signatures against the original header. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wredundant-decls"
#define WAYLAND_DYNLOAD_VALIDATE
#include "wayland_dynload_cursor.h"
#pragma GCC diagnostic pop
