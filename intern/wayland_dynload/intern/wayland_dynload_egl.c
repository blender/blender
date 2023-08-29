/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_wayland_dynload
 *
 * Wrapper functions for `<wayland-egl.h>`.
 */

#include <stdlib.h> /* `atexit`. */

#include "wayland_dynload_API.h"
#include "wayland_dynload_utils.h"

#include "wayland_dynload_egl.h" /* Own include. */

/* Public handle. */
struct WaylandDynload_EGL wayland_dynload_egl = {NULL};

static DynamicLibrary lib = NULL;

bool wayland_dynload_egl_init(const bool verbose)
{
  /* Library paths. */
  const char *paths[] = {
      "libwayland-egl.so.1",
      "libwayland-egl.so",
  };
  const int paths_num = sizeof(paths) / sizeof(*paths);
  int path_found = 0;
  if (!(lib = dynamic_library_open_array_with_error(paths, paths_num, verbose, &path_found))) {
    return false;
  }
  if (atexit(wayland_dynload_egl_exit)) {
    return false;
  }

#define WAYLAND_DYNLOAD_FN(symbol) \
  if (!(wayland_dynload_egl.symbol = dynamic_library_find_with_error( \
            lib, #symbol, paths[path_found], verbose))) { \
    return false; \
  }
#include "wayland_dynload_egl.h"
#undef WAYLAND_DYNLOAD_FN

  return true;
}

void wayland_dynload_egl_exit(void)
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
#include "wayland_dynload_egl.h"
#pragma GCC diagnostic pop
