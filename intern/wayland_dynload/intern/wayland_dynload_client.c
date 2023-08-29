/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_wayland_dynload
 *
 * Wrapper functions for `<wayland-client.h>`.
 */

#include <stdlib.h> /* `atexit`. */
#include <string.h>

#include "wayland_dynload_API.h"
#include "wayland_dynload_utils.h"

#include "wayland_dynload_client.h" /* Own include. */

/* Public handle. */
struct WaylandDynload_Client wayland_dynload_client = {NULL};

static DynamicLibrary lib = NULL;

#define WAYLAND_DYNLOAD_IFACE(symbol) \
  extern struct wl_interface symbol; \
  struct wl_interface symbol;
#include "wayland_dynload_client.h"
#undef WAYLAND_DYNLOAD_IFACE

bool wayland_dynload_client_init(const bool verbose)
{
  /* Library paths. */
  const char *paths[] = {
      "libwayland-client.so.0",
      "libwayland-client.so",
  };
  const int paths_num = sizeof(paths) / sizeof(*paths);
  int path_found;
  if (!(lib = dynamic_library_open_array_with_error(paths, paths_num, verbose, &path_found))) {
    return false;
  }
  if (atexit(wayland_dynload_client_exit)) {
    return false;
  }

#define WAYLAND_DYNLOAD_IFACE(symbol) \
  { \
    const void *symbol_val; \
    if (!(symbol_val = dynamic_library_find_with_error( \
              lib, #symbol, paths[path_found], verbose))) { \
      return false; \
    } \
    memcpy(&symbol, symbol_val, sizeof(symbol)); \
  }
#include "wayland_dynload_client.h"
#undef WAYLAND_DYNLOAD_IFACE

#define WAYLAND_DYNLOAD_FN(symbol) \
  if (!(wayland_dynload_client.symbol = dynamic_library_find_with_error( \
            lib, #symbol, paths[path_found], verbose))) { \
    return false; \
  }
#include "wayland_dynload_client.h"
#undef WAYLAND_DYNLOAD_FN

  return true;
}

void wayland_dynload_client_exit(void)
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
#include "wayland_dynload_client.h"
#pragma GCC diagnostic pop
