/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 *
 * API to manage `Library` data-blocks.
 */

#include "BLI_compiler_attrs.h"

struct Library;
struct Main;

void BKE_library_filepath_set(Main *bmain, Library *lib, const char *filepath);

/**
 * Rebuild the hierarchy of libraries, after e.g. deleting or relocating one, often some indirectly
 * linked libraries lose their 'parent' pointer, making them wrongly directly used ones.
 */
void BKE_library_main_rebuild_hierarchy(Main *bmain);
