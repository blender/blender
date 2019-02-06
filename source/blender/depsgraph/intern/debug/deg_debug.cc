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

#include "intern/debug/deg_debug.h"

#include "BLI_utildefines.h"
#include "BLI_console.h"
#include "BLI_hash.h"
#include "BLI_string.h"

#include "BKE_global.h"

namespace DEG {

bool terminal_do_color(void)
{
	return (G.debug & G_DEBUG_DEPSGRAPH_PRETTY) != 0;
}

string color_for_pointer(const void *pointer)
{
	if (!terminal_do_color()) {
		return "";
	}
	int r, g, b;
	BLI_hash_pointer_to_color(pointer, &r, &g, &b);
	char buffer[64];
	BLI_snprintf(buffer, sizeof(buffer), TRUECOLOR_ANSI_COLOR_FORMAT, r, g, b);
	return string(buffer);
}

string color_end(void)
{
	if (!terminal_do_color()) {
		return "";
	}
	return string(TRUECOLOR_ANSI_COLOR_FINISH);
}

}  // namespace DEG
