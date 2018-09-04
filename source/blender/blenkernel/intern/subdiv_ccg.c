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
 * The Original Code is Copyright (C) 2018 by Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Sergey Sharybin.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/subdiv_ccg.c
 *  \ingroup bke
 */

#include "BKE_subdiv_ccg.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "MEM_guardedalloc.h"

#include "BKE_ccg.h"
#include "BKE_subdiv.h"

static void subdiv_ccg_init_layers(SubdivCCG *subdiv_ccg,
                                   const SubdivToCCGSettings *settings)
{
	/* CCG always contains coordinates. Rest of layers are coming after them. */
	int layer_offset = sizeof(float) * 3;
	/* Normals. */
	if (settings->need_normal) {
		subdiv_ccg->has_normal = true;
		subdiv_ccg->normal_offset = layer_offset;
		layer_offset += sizeof(float) * 3;
	}
	else {
		subdiv_ccg->has_normal = false;
		subdiv_ccg->normal_offset = -1;
	}
	/* Mask. */
	if (settings->need_mask) {
		subdiv_ccg->has_mask = true;
		subdiv_ccg->mask_offset = layer_offset;
		layer_offset += sizeof(float);
	}
	else {
		subdiv_ccg->has_mask = false;
		subdiv_ccg->mask_offset = -1;
	}
}

static int grid_size_for_level_get(const SubdivCCG *subdiv_ccg, int level)
{
	BLI_assert(level >= 1);
	BLI_assert(level <= subdiv_ccg->level);
	return (1 << (level - 1)) + 1;
}

/* Per-vertex element size in bytes. */
static int element_size_get(const SubdivCCG *subdiv_ccg)
{
	/* We always have 3 floats for coordinate. */
	int num_floats = 3;
	if (subdiv_ccg->has_normal) {
		num_floats += 3;
	}
	if (subdiv_ccg->has_mask) {
		num_floats += 1;
	}
	return sizeof(float) * num_floats;
}

SubdivCCG *BKE_subdiv_to_ccg(
        Subdiv *UNUSED(subdiv),
        const SubdivToCCGSettings *settings,
        const Mesh *UNUSED(coarse_mesh))
{
	SubdivCCG *subdiv_ccg = MEM_callocN(sizeof(SubdivCCG *), "subdiv ccg");
	subdiv_ccg->level = settings->resolution >> 1;
	subdiv_ccg->grid_size =
	        grid_size_for_level_get(subdiv_ccg, subdiv_ccg->level);
	subdiv_ccg_init_layers(subdiv_ccg, settings);
	return NULL;
}

void BKE_subdiv_ccg_destroy(SubdivCCG *subdiv_ccg)
{
	MEM_SAFE_FREE(subdiv_ccg->grids);
	MEM_SAFE_FREE(subdiv_ccg->edges);
	MEM_SAFE_FREE(subdiv_ccg->vertices);
	MEM_freeN(subdiv_ccg);
}

void BKE_subdiv_ccg_key(CCGKey *key, const SubdivCCG *subdiv_ccg, int level)
{
	key->level = level;
	key->elem_size = element_size_get(subdiv_ccg);
	key->grid_size = grid_size_for_level_get(subdiv_ccg, level);
	key->grid_area = key->grid_size * key->grid_size;
	key->grid_bytes = key->elem_size * key->grid_area;

	key->normal_offset = subdiv_ccg->normal_offset;
	key->mask_offset = subdiv_ccg->mask_offset;

	key->has_normals = subdiv_ccg->has_normal;
	key->has_mask = subdiv_ccg->has_mask;
}

void BKE_subdiv_ccg_key_top_level(CCGKey *key, const SubdivCCG *subdiv_ccg)
{
	BKE_subdiv_ccg_key(key, subdiv_ccg, subdiv_ccg->level);
}
