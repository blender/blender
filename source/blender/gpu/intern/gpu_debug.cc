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

/** \file
 * \ingroup gpu
 */

#include "BLI_compiler_attrs.h"
#include "BLI_sys_types.h"
#include "BLI_system.h"
#include "BLI_utildefines.h"

#include "BKE_global.h"

#include "GPU_debug.h"
#include "GPU_glew.h"
#include "intern/gpu_private.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void GPU_print_error_debug(const char *str)
{
  if (G.debug & G_DEBUG) {
    fprintf(stderr, "GPU: %s\n", str);
  }
}
