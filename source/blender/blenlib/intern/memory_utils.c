/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenlib/intern/memory_utils.c
 *  \ingroup bli
 *  \brief Generic memory manipulation API.
 *
 * This is to extend on existing functions
 * such as ``memcpy`` & ``memcmp``.
 */
#include <string.h>

#include "BLI_sys_types.h"
#include "BLI_utildefines.h"

#include "BLI_memory_utils.h"

#include "BLI_strict_flags.h"

/**
 * Check if memory is zero'd, as with memset(s, 0, nbytes)
 */
bool BLI_memory_is_zero(const void *s, const size_t nbytes)
{
	const char *s_byte = s;
	const char *s_end = (const char *)s + nbytes;

	while ((s_byte != s_end) && (*s_byte == 0)) {
		s_byte++;
	}

	return (s_byte == s_end);
}
