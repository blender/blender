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
#include <string.h>

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

#define MAXUNDONAME	64

typedef struct bGPundonode {
	struct bGPundonode *next, *prev;

	char name[MAXUNDONAME];
	struct bGPdata *gpd;
} bGPundonode;

static ListBase undo_nodes = {NULL, NULL};
static bGPundonode *cur_node = NULL;

int ED_gpencil_session_active(void)
{
	return undo_nodes.first != NULL;
}

int ED_undo_gpencil_step(bContext *C, int step, const char *name)
{
	bGPdata **gpd_ptr= NULL, *new_gpd= NULL;

	gpd_ptr= gpencil_data_get_pointers(C, NULL);

	if (step==1) {	/* undo */
		//printf("\t\tGP - undo step\n");
		if (cur_node->prev) {
			if (!name || strcmp(cur_node->name, name) == 0) {
				cur_node= cur_node->prev;
				new_gpd= cur_node->gpd;
			}
		}
	}
	else if (step==-1) {
		//printf("\t\tGP - redo step\n");
		if (cur_node->next) {
			if (!name || strcmp(cur_node->name, name) == 0) {
				cur_node= cur_node->next;
				new_gpd= cur_node->gpd;
			}
		}
	}

	if (new_gpd) {
		if (gpd_ptr) {
			if (*gpd_ptr) {
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
	bGPundonode *undo_node;

	//printf("\t\tGP - undo push\n");

	if (cur_node) {
		/* remove all un-done nodes from stack */
		undo_node= cur_node->next;

		while (undo_node) {
			bGPundonode *next_node= undo_node->next;

			free_gpencil_data(undo_node->gpd);
			MEM_freeN(undo_node->gpd);

			BLI_freelinkN(&undo_nodes, undo_node);

			undo_node= next_node;
		}
	}

	/* create new undo node */
	undo_node= MEM_callocN(sizeof(bGPundonode), "gpencil undo node");
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
