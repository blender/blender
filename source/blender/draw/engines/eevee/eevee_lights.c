/*
 * Copyright 2016, Blender Foundation.
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
 * Contributor(s): Blender Institute
 *
 */

/** \file eevee_lights.c
 *  \ingroup DNA
 */

#include "DRW_render.h"

#include "eevee_private.h"

#define MAX_LIGHT 210 /* TODO : find size by dividing UBO max size by light data size */

typedef struct EEVEE_Light {
	float position[3], pad;
	float color[3], spec;
	float spot_size, spot_blend, area_x, area_y;
} EEVEE_Light;

static struct {
	ListBase lamps; /* Lamps gathered during cache iteration */
} g_data = {NULL}; /* Transient data */

void EEVEE_lights_init(EEVEE_StorageList *stl)
{
	stl->lights_info = MEM_callocN(sizeof(EEVEE_LightsInfo), "EEVEE_LightsInfo");
	stl->lights_data = MEM_mallocN(sizeof(EEVEE_Light) * MAX_LIGHT, "EEVEE_LightsUboStorage");
	stl->lights_ref  = MEM_mallocN(sizeof(Object *) * MAX_LIGHT, "EEVEE lights_ref");
	stl->lights_ubo  = DRW_uniformbuffer_create(sizeof(EEVEE_Light) * MAX_LIGHT, NULL);
}

void EEVEE_lights_cache_init(EEVEE_StorageList *stl)
{
	BLI_listbase_clear(&g_data.lamps);
	stl->lights_info->light_count = 0;
}

void EEVEE_lights_cache_add(EEVEE_StorageList *stl, Object *ob)
{
	BLI_addtail(&g_data.lamps, BLI_genericNodeN(ob));
	stl->lights_info->light_count += 1;
}

void EEVEE_lights_cache_finish(EEVEE_StorageList *stl)
{
	int light_ct = stl->lights_info->light_count;

	if (light_ct > MAX_LIGHT) {
		printf("Too much lamps in the scene !!!\n");
		stl->lights_info->light_count = MAX_LIGHT;
	}

	if (light_ct > 0) {
		int i = 0;
		for (LinkData *link = g_data.lamps.first; link && i < MAX_LIGHT; link = link->next, i++) {
			Object *ob = (Object *)link->data;
			stl->lights_ref[i] = ob;
		}
	}
	BLI_freelistN(&g_data.lamps);

	/* We changed light data so we need to upload it */
	EEVEE_lights_update(stl);
}

void EEVEE_lights_update(EEVEE_StorageList *stl)
{
	int light_ct = stl->lights_info->light_count;

	/* TODO only update if data changes */
	/* Update buffer with lamp data */
	for (int i = 0; i < light_ct; ++i) {
		EEVEE_Light *evli = stl->lights_data + i;
		Object *ob = stl->lights_ref[i];
		Lamp *la = (Lamp *)ob->data;

		copy_v3_v3(evli->position, ob->obmat[3]);
		evli->color[0] = la->r * la->energy;
		evli->color[1] = la->g * la->energy;
		evli->color[2] = la->b * la->energy;
	}

	/* Upload buffer to GPU */
	DRW_uniformbuffer_update(stl->lights_ubo, stl->lights_data);
}
