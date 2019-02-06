/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 */

/** \file \ingroup depsgraph
 */

#pragma once

#include "intern/depsgraph_type.h"

#include "BKE_global.h"

#include "DEG_depsgraph_debug.h"

namespace DEG {

#define DEG_DEBUG_PRINTF(depsgraph, type, ...) \
	do { \
		if (DEG_debug_flags_get(depsgraph) & G_DEBUG_DEPSGRAPH_ ## type) { \
			DEG_debug_print_begin(depsgraph); \
			fprintf(stdout, __VA_ARGS__); \
		} \
	} while (0)

#define DEG_GLOBAL_DEBUG_PRINTF(type, ...) \
	do { \
		if (G.debug & G_DEBUG_DEPSGRAPH_ ## type) { \
			fprintf(stdout, __VA_ARGS__); \
		} \
	} while (0)

#define DEG_ERROR_PRINTF(...)               \
	do {                                    \
		fprintf(stderr, __VA_ARGS__);       \
		fflush(stderr);                     \
	} while (0)

bool terminal_do_color(void);
string color_for_pointer(const void *pointer);
string color_end(void);

}  // namespace DEG
