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
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Jason Wilkins
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file source/blender/gpu/intern/gpu_init_exit.c
 *  \ingroup gpu
 */

#include "BLI_sys_types.h"
#include "GPU_init_exit.h"  /* interface */

#include "intern/gpu_codegen.h"
#include "intern/gpu_extensions_private.h"

/**
 * although the order of initialization and shutdown should not matter
 * (except for the extensions), I chose alphabetical and reverse alphabetical order
 */

static bool initialized = false;

void GPU_init(void)
{
	/* can't avoid calling this multiple times, see wm_window_add_ghostwindow */
	if (initialized)
		return;

	initialized = true;

	gpu_extensions_init(); /* must come first */

	gpu_codegen_init();
}



void GPU_exit(void)
{
	gpu_codegen_exit();

	gpu_extensions_exit(); /* must come last */

	initialized = false;
}
