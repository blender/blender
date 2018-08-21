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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2018 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Sergey Sharybin.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/multires_subdiv.c
 *  \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_scene_types.h"

#include "BLI_utildefines.h"

#include "BKE_library.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_subdiv.h"

void BKE_multires_subdiv_settings_init(
        SubdivSettings *settings,
        const MultiresModifierData *mmd)
{
	settings->is_simple = (mmd->simple != 0);
	settings->is_adaptive = !settings->is_simple;
	settings->level = mmd->quality;
	settings->fvar_linear_interpolation =
	        BKE_subdiv_fvar_interpolation_from_uv_smooth(mmd->uv_smooth);
}

void BKE_multires_subdiv_mesh_settings_init(
        SubdivToMeshSettings *mesh_settings,
        const Scene *scene,
        const Object *object,
        const MultiresModifierData *mmd,
        const bool use_render_params)
{
	const int level = multires_get_level(
	        scene, object, mmd, use_render_params, false);
	mesh_settings->resolution = (1 << level) + 1;
}
