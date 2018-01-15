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
 * The Original Code is Copyright (C) 2016 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Mike Erwin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/gpu/intern/gpu_basic_shader.c
 *  \ingroup gpu
 */

#include "BLI_utildefines.h"

#include "GPU_batch.h"  /* own include */
#include "gpu_shader_private.h"

/* -------------------------------------------------------------------- */
/** \name Utilities
 * \{ */

void GWN_batch_program_set_builtin(Gwn_Batch *batch, GPUBuiltinShader shader_id)
{
	GPUShader *shader = GPU_shader_get_builtin_shader(shader_id);
	GWN_batch_program_set(batch, shader->program, shader->interface);
}

void gpu_batch_init(void)
{
	gpu_batch_presets_init();
}

void gpu_batch_exit(void)
{
	gpu_batch_presets_exit();
}

/** \} */

