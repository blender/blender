/*
 * $Id$
 *
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
 * The Original Code is Copyright (C) 2011 Blender Foundation.
 * All rights reserved.
 *
 *
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "DNA_gpencil_types.h"
#include "DNA_listBase.h"
#include "DNA_windowmanager_types.h"

#include "BKE_context.h"
#include "BKE_gpencil.h"

#include "BLI_listbase.h"

#include "ED_gpencil.h"

#include "WM_api.h"
#include "WM_types.h"

#include "gpencil_intern.h"

typedef struct bGPundonode {
	struct bGPundonode *next, *prev;

	struct bGPdata *gpd;
} bGPundonode;

static ListBase undo_nodes = {NULL, NULL};
static bGPundonode *cur_node = NULL;

int ED_undo_gpencil_step(bContext *C, int step, const char *name)
{
	bGPdata **gpd_ptr= NULL, *new_gpd= NULL;
	PointerRNA ptr;

	if(name)	/* currently unsupported */
		return OPERATOR_CANCELLED;

	gpd_ptr= gpencil_data_get_pointers(C, NULL);

	(void) step;

	if(step==1) {	/* undo */
		//printf("\t\tGP - undo step\n");
		if(cur_node->prev) {
			cur_node= cur_node->prev;
			new_gpd= cur_node->gpd;
		}
	}
	else if (step==-1) {
		//printf("\t\tGP - redo step\n");
		if(cur_node->next) {
			cur_node= cur_node->next;
			new_gpd= cur_node->gpd;
		}
	}

	if(new_gpd) {
		if(gpd_ptr) {
			if(*gpd_ptr) {
				bGPdata *gpd= *gpd_ptr;
				bGPDlayer *gpl, *gpld;

				free_gpencil_layers(&gpd->layers);

				/* copy layers */
				gpd->layers.first= gpd->layers.last= NULL;

				for (gpl= new_gpd->layers.first; gpl; gpl= gpl->next) {
					/* make a copy of source layer and its data */
					gpld= gpencil_layer_duplicate(gpl);
					BLI_addtail(&gpd->layers, gpld);
				}
			}
		}
	}

	WM_event_add_notifier(C, NC_SCREEN|ND_GPENCIL|NA_EDITED, NULL);

	return OPERATOR_FINISHED;
}

void gpencil_undo_init(bGPdata *gpd)
{
	gpencil_undo_push(gpd);
}

void gpencil_undo_push(bGPdata *gpd)
{
	bGPundonode *undo_node= MEM_callocN(sizeof(bGPundonode), "gpencil undo node");

	//printf("\t\tGP - undo push\n");

	undo_node->gpd= gpencil_data_duplicate(gpd);
	cur_node= undo_node;

	BLI_addtail(&undo_nodes, undo_node);
}

void gpencil_undo_finish(void)
{
	bGPundonode *undo_node= undo_nodes.first;

	while (undo_node) {
		free_gpencil_data(undo_node->gpd);
		MEM_freeN(undo_node->gpd);

		undo_node= undo_node->next;
	}

	BLI_freelistN(&undo_nodes);

	cur_node= NULL;
}
