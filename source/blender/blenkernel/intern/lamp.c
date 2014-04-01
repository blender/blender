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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/lamp.c
 *  \ingroup bke
 */

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_animsys.h"
#include "BKE_colortools.h"
#include "BKE_icons.h"
#include "BKE_global.h"
#include "BKE_lamp.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_node.h"

Lamp *BKE_lamp_add(Main *bmain, const char *name)
{
	Lamp *la;
	
	la =  BKE_libblock_alloc(bmain, ID_LA, name);
	
	la->r = la->g = la->b = la->k = 1.0f;
	la->haint = la->energy = 1.0f;
	la->dist = 25.0f;
	la->spotsize = DEG2RADF(45.0f);
	la->spotblend = 0.15f;
	la->att2 = 1.0f;
	la->mode = LA_SHAD_BUF;
	la->bufsize = 512;
	la->clipsta = 0.5f;
	la->clipend = 40.0f;
	la->samp = 3;
	la->bias = 1.0f;
	la->soft = 3.0f;
	la->compressthresh = 0.05f;
	la->ray_samp = la->ray_sampy = la->ray_sampz = 1;
	la->area_size = la->area_sizey = la->area_sizez = 0.1f;
	la->buffers = 1;
	la->buftype = LA_SHADBUF_HALFWAY;
	la->ray_samp_method = LA_SAMP_HALTON;
	la->adapt_thresh = 0.001f;
	la->preview = NULL;
	la->falloff_type = LA_FALLOFF_INVSQUARE;
	la->curfalloff = curvemapping_add(1, 0.0f, 1.0f, 1.0f, 0.0f);
	la->sun_effect_type = 0;
	la->horizon_brightness = 1.0;
	la->spread = 1.0;
	la->sun_brightness = 1.0;
	la->sun_size = 1.0;
	la->backscattered_light = 1.0f;
	la->atm_turbidity = 2.0f;
	la->atm_inscattering_factor = 1.0f;
	la->atm_extinction_factor = 1.0f;
	la->atm_distance_factor = 1.0f;
	la->sun_intensity = 1.0f;
	la->skyblendtype = MA_RAMP_ADD;
	la->skyblendfac = 1.0f;
	la->sky_colorspace = BLI_XYZ_CIE;
	la->sky_exposure = 1.0f;
	la->shadow_frustum_size = 10.0f;
	
	curvemapping_initialize(la->curfalloff);
	return la;
}

Lamp *BKE_lamp_copy(Lamp *la)
{
	Lamp *lan;
	int a;
	
	lan = BKE_libblock_copy(&la->id);

	for (a = 0; a < MAX_MTEX; a++) {
		if (lan->mtex[a]) {
			lan->mtex[a] = MEM_mallocN(sizeof(MTex), "copylamptex");
			memcpy(lan->mtex[a], la->mtex[a], sizeof(MTex));
			id_us_plus((ID *)lan->mtex[a]->tex);
		}
	}
	
	lan->curfalloff = curvemapping_copy(la->curfalloff);

	if (la->nodetree)
		lan->nodetree = ntreeCopyTree(la->nodetree);
	
	if (la->preview)
		lan->preview = BKE_previewimg_copy(la->preview);
	
	return lan;
}

Lamp *localize_lamp(Lamp *la)
{
	Lamp *lan;
	int a;
	
	lan = BKE_libblock_copy(&la->id);
	BLI_remlink(&G.main->lamp, lan);

	for (a = 0; a < MAX_MTEX; a++) {
		if (lan->mtex[a]) {
			lan->mtex[a] = MEM_mallocN(sizeof(MTex), "localize_lamp");
			memcpy(lan->mtex[a], la->mtex[a], sizeof(MTex));
			/* free lamp decrements */
			id_us_plus((ID *)lan->mtex[a]->tex);
		}
	}
	
	lan->curfalloff = curvemapping_copy(la->curfalloff);

	if (la->nodetree)
		lan->nodetree = ntreeLocalize(la->nodetree);
	
	lan->preview = NULL;
	
	return lan;
}

void BKE_lamp_make_local(Lamp *la)
{
	Main *bmain = G.main;
	Object *ob;
	bool is_local = false, is_lib = false;

	/* - only lib users: do nothing
	 * - only local users: set flag
	 * - mixed: make copy
	 */
	
	if (la->id.lib == NULL) return;
	if (la->id.us == 1) {
		id_clear_lib_data(bmain, &la->id);
		return;
	}
	
	ob = bmain->object.first;
	while (ob) {
		if (ob->data == la) {
			if (ob->id.lib) is_lib = true;
			else is_local = true;
		}
		ob = ob->id.next;
	}
	
	if (is_local && is_lib == false) {
		id_clear_lib_data(bmain, &la->id);
	}
	else if (is_local && is_lib) {
		Lamp *la_new = BKE_lamp_copy(la);
		la_new->id.us = 0;

		/* Remap paths of new ID using old library as base. */
		BKE_id_lib_local_paths(bmain, la->id.lib, &la_new->id);

		ob = bmain->object.first;
		while (ob) {
			if (ob->data == la) {
				
				if (ob->id.lib == NULL) {
					ob->data = la_new;
					la_new->id.us++;
					la->id.us--;
				}
			}
			ob = ob->id.next;
		}
	}
}

void BKE_lamp_free(Lamp *la)
{
	MTex *mtex;
	int a;

	for (a = 0; a < MAX_MTEX; a++) {
		mtex = la->mtex[a];
		if (mtex && mtex->tex) mtex->tex->id.us--;
		if (mtex) MEM_freeN(mtex);
	}
	
	BKE_free_animdata((ID *)la);

	curvemapping_free(la->curfalloff);

	/* is no lib link block, but lamp extension */
	if (la->nodetree) {
		ntreeFreeTree(la->nodetree);
		MEM_freeN(la->nodetree);
	}
	
	BKE_previewimg_free(&la->preview);
	BKE_icon_delete(&la->id);
	la->id.icon_id = 0;
}

/* Calculate all drivers for lamps, see material_drivers_update for why this is a bad hack */

static void lamp_node_drivers_update(Scene *scene, bNodeTree *ntree, float ctime)
{
	bNode *node;

	/* nodetree itself */
	if (ntree->adt && ntree->adt->drivers.first)
		BKE_animsys_evaluate_animdata(scene, &ntree->id, ntree->adt, ctime, ADT_RECALC_DRIVERS);
	
	/* nodes */
	for (node = ntree->nodes.first; node; node = node->next)
		if (node->id && node->type == NODE_GROUP)
			lamp_node_drivers_update(scene, (bNodeTree *)node->id, ctime);
}

void lamp_drivers_update(Scene *scene, Lamp *la, float ctime)
{
	/* Prevent infinite recursion by checking (and tagging the lamp) as having been visited already
	 * (see BKE_scene_update_tagged()). This assumes la->id.flag & LIB_DOIT isn't set by anything else
	 * in the meantime... [#32017] */
	if (la->id.flag & LIB_DOIT)
		return;

	la->id.flag |= LIB_DOIT;
	
	/* lamp itself */
	if (la->adt && la->adt->drivers.first)
		BKE_animsys_evaluate_animdata(scene, &la->id, la->adt, ctime, ADT_RECALC_DRIVERS);
	
	/* nodes */
	if (la->nodetree)
		lamp_node_drivers_update(scene, la->nodetree, ctime);

	la->id.flag &= ~LIB_DOIT;
}

