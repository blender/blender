/* SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup intern_wayland_dynload
 *
 * Utility defines.
 */

#pragma once

#include <dlfcn.h> /* Dynamic loading. */
#include <stdbool.h>

typedef void *DynamicLibrary;

#define dynamic_library_open(path) dlopen(path, RTLD_NOW)
#define dynamic_library_close(lib) dlclose(lib)
#define dynamic_library_find(lib, symbol) dlsym(lib, symbol)

/** Loads a library from an array, printing an error when the symbol isn't found. */
DynamicLibrary dynamic_library_open_array_with_error(const char **paths,
                                                     int paths_num,
                                                     bool verbose,
                                                     int *r_path_index);

/** Find a symbol, printing an error when the symbol isn't found. */
void *dynamic_library_find_with_error(DynamicLibrary lib,
                                      const char *symbol,
                                      const char *path_lib,
                                      bool verbose);
