/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_wayland_dynload
 */

#include <stdio.h>

#include "wayland_dynload_utils.h"

DynamicLibrary dynamic_library_open_array_with_error(const char **paths,
                                                     const int paths_num,
                                                     const bool verbose,
                                                     int *r_path_index)
{
  DynamicLibrary lib = NULL;
  for (int a = 0; a < paths_num; a++) {
    lib = dynamic_library_open(paths[a]);
    if (lib) {
      *r_path_index = a;
      break;
    }
  }
  if (lib == NULL) {
    /* Use the last path as it's likely to be least specific. */
    if (verbose) {
      fprintf(stderr, "Unable to find '%s'\n", paths[paths_num - 1]);
    }
  }
  return lib;
}

void *dynamic_library_find_with_error(DynamicLibrary lib,
                                      const char *symbol,
                                      const char *path_lib,
                                      const bool verbose)
{
  void *symbol_var = dynamic_library_find(lib, symbol);
  if (symbol_var == NULL) {
    if (verbose) {
      fprintf(stderr, "Unable to find '%s' in '%s'.\n", symbol, path_lib);
    }
  }
  return symbol_var;
}
