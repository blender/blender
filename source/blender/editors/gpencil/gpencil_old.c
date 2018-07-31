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
 * The Original Code is Copyright (C) 2018, Blender Foundation,
 * This is a new part of Blender
 *
 * Contributor(s): Antonio Vazquez
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * Use deprecated data to convert old 2.7x files
 */

/** \file blender/editors/gpencil/gpencil_old.c
 *  \ingroup edgpencil
 */

 /* allow to use deprecated functionality */
#define DNA_DEPRECATED_ALLOW

#include <stdio.h>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"

#include "DNA_gpencil_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_main.h"
#include "BKE_brush.h"
#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_gpencil.h"
#include "BKE_object.h"
#include "BKE_material.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_object.h"
#include "ED_gpencil.h"

#include "gpencil_intern.h"

 /* Free all of a gp-colors */
static void free_gpencil_colors(bGPDpalette *palette)
{
	/* error checking */
	if (palette == NULL) {
		return;
	}

	/* free colors */
	BLI_freelistN(&palette->colors);
}

/* Free all of the gp-palettes and colors */
static void free_palettes(ListBase *list)
{
	bGPDpalette *palette_next;

	/* error checking */
	if (list == NULL) {
		return;
	}

	/* delete palettes */
	for (bGPDpalette *palette = list->first; palette; palette = palette_next) {
		palette_next = palette->next;
		/* free palette colors */
		free_gpencil_colors(palette);

		MEM_freeN(palette);
	}
	BLI_listbase_clear(list);
}

/* ***************** Convert old 2.7 files to 2.8 ************************ */
static bool gpencil_convert_old_files_poll(bContext *C)
{
	Scene *scene = CTX_data_scene(C);

	return (int) (scene->gpd != NULL);
}

static int gpencil_convert_old_files_exec(bContext *C, wmOperator *UNUSED(op))
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	ToolSettings *ts = CTX_data_tool_settings(C);
	ViewLayer *view_layer = CTX_data_view_layer(C);

	/* Convert grease pencil scene datablock to GP object */
	if ((scene->gpd) && (view_layer != NULL)) {
		Object *ob;
		ob = BKE_object_add_for_data(bmain, view_layer, OB_GPENCIL, "GP_Scene", &scene->gpd->id, false);
		zero_v3(ob->loc);

		Paint *paint = BKE_brush_get_gpencil_paint(ts);
		/* if not exist, create a new one */
		if (paint->brush == NULL) {
			/* create new brushes */
			BKE_brush_gpencil_presets(C);
		}

		/* convert grease pencil palettes (version >= 2.78)  to materials and weights */
		bGPdata *gpd = scene->gpd;
		for (const bGPDpalette *palette = gpd->palettes.first; palette; palette = palette->next) {
			for (bGPDpalettecolor *palcolor = palette->colors.first; palcolor; palcolor = palcolor->next) {

				/* create material slot */
				BKE_object_material_slot_add(bmain, ob);
				Material *ma = BKE_material_add_gpencil(bmain, palcolor->info);
				assign_material(bmain, ob, ma, ob->totcol, BKE_MAT_ASSIGN_EXISTING);

				/* copy color settings */
				MaterialGPencilStyle *gp_style = ma->gp_style;
				copy_v4_v4(gp_style->stroke_rgba, palcolor->color);
				copy_v4_v4(gp_style->fill_rgba, palcolor->fill);
				gp_style->flag = palcolor->flag;

				/* fix strokes */
				for (bGPDlayer *gpl = gpd->layers.first; gpl; gpl = gpl->next) {
					for (bGPDframe *gpf = gpl->frames.first; gpf; gpf = gpf->next) {
						for (bGPDstroke *gps = gpf->strokes.first; gps; gps = gps->next) {
							if ((gps->colorname[0] != '\0') &&
							    (STREQ(gps->colorname, palcolor->info)))
							{
								gps->mat_nr = ob->totcol - 1;
								gps->colorname[0] = '\0';
								/* create weights array */
								gps->dvert = MEM_callocN(sizeof(MDeformVert) * gps->totpoints, "gp_stroke_weights");
							}
						}
					}
				}
			}
		}

		/* free palettes */
		free_palettes(&gpd->palettes);

		/* disable all GP modes */
		ED_gpencil_setup_modes(C, gpd, 0);

		/* set cache as dirty */
		BKE_gpencil_batch_cache_dirty(ob->data);

		scene->gpd = NULL;
	}

#if 0 /* GPXX */
	/* Handle object-linked grease pencil datablocks */
	for (Object *ob = bmain->object.first; ob; ob = ob->id.next) {
		if (ob->gpd) {
			if (ob->type == OB_GPENCIL) {
				/* GP Object - remap the links */
				ob->data = ob->gpd;
				ob->gpd = NULL;
			}
			else if (ob->type == OB_EMPTY) {
				/* Empty with GP data - This should be able to be converted
				* to a GP object with little data loss
				*/
				ob->data = ob->gpd;
				ob->gpd = NULL;
				ob->type = OB_GPENCIL;
			}
			else {
				/* FIXME: What to do in this case?
				*
				* We cannot create new objects for these, as we don't have a scene & scene layer
				* to put them into from here...
				*/
				printf("WARNING: Old Grease Pencil data ('%s') still exists on Object '%s'\n",
					ob->gpd->id.name + 2, ob->id.name + 2);
			}
		}
	}
#endif

	/* notifiers */
	WM_event_add_notifier(C, NC_GPENCIL | ND_DATA | NA_EDITED, NULL);

	return OPERATOR_FINISHED;
}

void GPENCIL_OT_convert_old_files(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Convert 2.7 Grease Pencil File";
	ot->idname = "GPENCIL_OT_convert_old_files";
	ot->description = "Convert 2.7x grease pencil files to 2.8";

	/* callbacks */
	ot->exec = gpencil_convert_old_files_exec;
	ot->poll = gpencil_convert_old_files_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}
