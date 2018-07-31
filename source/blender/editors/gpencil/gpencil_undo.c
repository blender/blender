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

/** \file blender/editors/gpencil/gpencil_undo.c
 *  \ingroup edgpencil
 */


#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_gpencil_types.h"
#include "DNA_object_types.h"
#include "DNA_listBase.h"
#include "DNA_windowmanager_types.h"

#include "BLI_listbase.h"

#include "BKE_blender_undo.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_gpencil.h"

#include "ED_gpencil.h"

#include "WM_api.h"
#include "WM_types.h"

#include "DEG_depsgraph.h"

#include "gpencil_intern.h"

typedef struct bGPundonode {
	struct bGPundonode *next, *prev;

	char name[BKE_UNDO_STR_MAX];
	struct bGPdata *gpd;
} bGPundonode;

static ListBase undo_nodes = {NULL, NULL};
static bGPundonode *cur_node = NULL;

int ED_gpencil_session_active(void)
{
	return (BLI_listbase_is_empty(&undo_nodes) == false);
}

int ED_undo_gpencil_step(bContext *C, int step, const char *name)
{
	bGPdata **gpd_ptr = NULL, *new_gpd = NULL;

	gpd_ptr = ED_gpencil_data_get_pointers(C, NULL);

	if (step == 1) {  /* undo */
		//printf("\t\tGP - undo step\n");
		if (cur_node->prev) {
			if (!name || STREQ(cur_node->name, name)) {
				cur_node = cur_node->prev;
				new_gpd = cur_node->gpd;
			}
		}
	}
	else if (step == -1) {
		//printf("\t\tGP - redo step\n");
		if (cur_node->next) {
			if (!name || STREQ(cur_node->name, name)) {
				cur_node = cur_node->next;
				new_gpd = cur_node->gpd;
			}
		}
	}

	if (new_gpd) {
		if (gpd_ptr) {
			if (*gpd_ptr) {
				bGPdata *gpd = *gpd_ptr;
				bGPDlayer *gpl, *gpld;

				BKE_gpencil_free_layers(&gpd->layers);

				/* copy layers */
				BLI_listbase_clear(&gpd->layers);

				for (gpl = new_gpd->layers.first; gpl; gpl = gpl->next) {
					/* make a copy of source layer and its data */
					gpld = BKE_gpencil_layer_duplicate(gpl);
					BLI_addtail(&gpd->layers, gpld);
				}
			}
		}
		/* drawing batch cache is dirty now */
		DEG_id_tag_update(&new_gpd->id, OB_RECALC_OB | OB_RECALC_DATA);
		new_gpd->flag |= GP_DATA_CACHE_IS_DIRTY;
	}

	WM_event_add_notifier(C, NC_GPENCIL | NA_EDITED, NULL);

	return OPERATOR_FINISHED;
}

void gpencil_undo_init(bGPdata *gpd)
{
	gpencil_undo_push(gpd);
}

static void gpencil_undo_free_node(bGPundonode *undo_node)
{
	/* HACK: animdata wasn't duplicated, so it shouldn't be freed here,
	 * or else the real copy will segfault when accessed
	 */
	undo_node->gpd->adt = NULL;

	BKE_gpencil_free(undo_node->gpd, false);
	MEM_freeN(undo_node->gpd);
}

void gpencil_undo_push(bGPdata *gpd)
{
	bGPundonode *undo_node;

	//printf("\t\tGP - undo push\n");

	if (cur_node) {
		/* remove all un-done nodes from stack */
		undo_node = cur_node->next;

		while (undo_node) {
			bGPundonode *next_node = undo_node->next;

			gpencil_undo_free_node(undo_node);
			BLI_freelinkN(&undo_nodes, undo_node);

			undo_node = next_node;
		}
	}

	/* limit number of undo steps to the maximum undo steps
	 *  - to prevent running out of memory during **really**
	 *    long drawing sessions (triggering swapping)
	 */
	/* TODO: Undo-memory constraint is not respected yet, but can be added if we have any need for it */
	if (U.undosteps && !BLI_listbase_is_empty(&undo_nodes)) {
		/* remove anything older than n-steps before cur_node */
		int steps = 0;

		undo_node = (cur_node) ? cur_node : undo_nodes.last;
		while (undo_node) {
			bGPundonode *prev_node = undo_node->prev;

			if (steps >= U.undosteps) {
				gpencil_undo_free_node(undo_node);
				BLI_freelinkN(&undo_nodes, undo_node);
			}

			steps++;
			undo_node = prev_node;
		}
	}

	/* create new undo node */
	undo_node = MEM_callocN(sizeof(bGPundonode), "gpencil undo node");
	undo_node->gpd = BKE_gpencil_data_duplicate(NULL, gpd, true);

	cur_node = undo_node;

	BLI_addtail(&undo_nodes, undo_node);
}

void gpencil_undo_finish(void)
{
	bGPundonode *undo_node = undo_nodes.first;

	while (undo_node) {
		gpencil_undo_free_node(undo_node);
		undo_node = undo_node->next;
	}

	BLI_freelistN(&undo_nodes);

	cur_node = NULL;
}
