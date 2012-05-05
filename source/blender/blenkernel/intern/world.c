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

/** \file blender/blenkernel/intern/world.c
 *  \ingroup bke
 */


#include <string.h>
#include <math.h>
#include "MEM_guardedalloc.h"

#include "DNA_world_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"

#include "BLI_listbase.h"
#include "BLI_utildefines.h"
#include "BLI_bpath.h"

#include "BKE_animsys.h"
#include "BKE_global.h"
#include "BKE_icons.h"
#include "BKE_library.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_world.h"

void BKE_world_free(World *wrld)
{
	MTex *mtex;
	int a;
	
	for (a=0; a<MAX_MTEX; a++) {
		mtex= wrld->mtex[a];
		if (mtex && mtex->tex) mtex->tex->id.us--;
		if (mtex) MEM_freeN(mtex);
	}
	BKE_previewimg_free(&wrld->preview);

	BKE_free_animdata((ID *)wrld);

	/* is no lib link block, but world extension */
	if (wrld->nodetree) {
		ntreeFreeTree(wrld->nodetree);
		MEM_freeN(wrld->nodetree);
	}

	BKE_icon_delete((struct ID*)wrld);
	wrld->id.icon_id = 0;
}


World *add_world(const char *name)
{
	Main *bmain= G.main;
	World *wrld;

	wrld= BKE_libblock_alloc(&bmain->world, ID_WO, name);
	
	wrld->horr= 0.05f;
	wrld->horg= 0.05f;
	wrld->horb= 0.05f;
	wrld->zenr= 0.01f;
	wrld->zeng= 0.01f;
	wrld->zenb= 0.01f;
	wrld->skytype= 0;
	wrld->stardist= 15.0f;
	wrld->starsize= 2.0f;
	
	wrld->exp= 0.0f;
	wrld->exposure=wrld->range= 1.0f;

	wrld->aodist= 10.0f;
	wrld->aosamp= 5;
	wrld->aoenergy= 1.0f;
	wrld->ao_env_energy= 1.0f;
	wrld->ao_indirect_energy= 1.0f;
	wrld->ao_indirect_bounces= 1;
	wrld->aobias= 0.05f;
	wrld->ao_samp_method = WO_AOSAMP_HAMMERSLEY;	
	wrld->ao_approx_error= 0.25f;
	
	wrld->preview = NULL;
	wrld->miststa = 5.0f;
	wrld->mistdist = 25.0f;

	return wrld;
}

World *BKE_world_copy(World *wrld)
{
	World *wrldn;
	int a;
	
	wrldn= BKE_libblock_copy(&wrld->id);
	
	for (a=0; a<MAX_MTEX; a++) {
		if (wrld->mtex[a]) {
			wrldn->mtex[a]= MEM_mallocN(sizeof(MTex), "BKE_world_copy");
			memcpy(wrldn->mtex[a], wrld->mtex[a], sizeof(MTex));
			id_us_plus((ID *)wrldn->mtex[a]->tex);
		}
	}

	if (wrld->nodetree)
		wrldn->nodetree= ntreeCopyTree(wrld->nodetree);
	
	if (wrld->preview)
		wrldn->preview = BKE_previewimg_copy(wrld->preview);

	return wrldn;
}

World *localize_world(World *wrld)
{
	World *wrldn;
	int a;
	
	wrldn= BKE_libblock_copy(&wrld->id);
	BLI_remlink(&G.main->world, wrldn);
	
	for (a=0; a<MAX_MTEX; a++) {
		if (wrld->mtex[a]) {
			wrldn->mtex[a]= MEM_mallocN(sizeof(MTex), "localize_world");
			memcpy(wrldn->mtex[a], wrld->mtex[a], sizeof(MTex));
			/* free world decrements */
			id_us_plus((ID *)wrldn->mtex[a]->tex);
		}
	}

	if (wrld->nodetree)
		wrldn->nodetree= ntreeLocalize(wrld->nodetree);
	
	wrldn->preview= NULL;
	
	return wrldn;
}

void BKE_world_make_local(World *wrld)
{
	Main *bmain= G.main;
	Scene *sce;
	int is_local= FALSE, is_lib= FALSE;

	/* - only lib users: do nothing
	 * - only local users: set flag
	 * - mixed: make copy
	 */
	
	if (wrld->id.lib==NULL) return;
	if (wrld->id.us==1) {
		id_clear_lib_data(bmain, &wrld->id);
		return;
	}
	
	for (sce= bmain->scene.first; sce && ELEM(FALSE, is_lib, is_local); sce= sce->id.next) {
		if (sce->world == wrld) {
			if (sce->id.lib) is_lib= TRUE;
			else is_local= TRUE;
		}
	}

	if (is_local && is_lib==FALSE) {
		id_clear_lib_data(bmain, &wrld->id);
	}
	else if (is_local && is_lib) {
		World *wrld_new= BKE_world_copy(wrld);
		wrld_new->id.us= 0;

		/* Remap paths of new ID using old library as base. */
		BKE_id_lib_local_paths(bmain, wrld->id.lib, &wrld_new->id);

		for (sce= bmain->scene.first; sce; sce= sce->id.next) {
			if (sce->world == wrld) {
				if (sce->id.lib==NULL) {
					sce->world= wrld_new;
					wrld_new->id.us++;
					wrld->id.us--;
				}
			}
		}
	}
}
