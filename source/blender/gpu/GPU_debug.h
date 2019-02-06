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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 */

/** \file \ingroup gpu
 */

#ifndef __GPU_DEBUG_H__
#define __GPU_DEBUG_H__

#include "GPU_glew.h"

#ifdef __cplusplus
extern "C" {
#endif

/* prints something if debug mode is active only */
void GPU_print_error_debug(const char *str);

/* inserts a debug marker message for the debug context messaging system */
void GPU_string_marker(const char *str);

#ifdef __cplusplus
}
#endif

#endif /* __GPU_DEBUG_H__ */
