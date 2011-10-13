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
 * ***** END GPL LICENSE BLOCK *****
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "BLI_blenlib.h"

#include "DNA_dynamicpaint_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_blender.h"
#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_depsgraph.h"
#include "BKE_dynamicpaint.h"
#include "BKE_global.h"
#include "BKE_modifier.h"
#include "BKE_report.h"

#include "ED_mesh.h"
#include "ED_screen.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

/* Platform independend time	*/
#include "PIL_time.h"

#include "WM_types.h"
#include "WM_api.h"

static int surface_slot_add_exec(bContext *C, wmOperator *op)
{
	DynamicPaintModifierData *pmd = 0;
	Object *cObject = CTX_data_pointer_get_type(C, "object", &RNA_Object).data;
	DynamicPaintSurface *surface;

	/* Make sure we're dealing with a canvas */
	pmd = (DynamicPaintModifierData *)modifiers_findByType(cObject, eModifierType_DynamicPaint);
	if (!pmd) return OPERATOR_CANCELLED;
	if (!pmd->canvas) return OPERATOR_CANCELLED;

	surface = dynamicPaint_createNewSurface(pmd->canvas, CTX_data_scene(C));

	if (!surface) return OPERATOR_CANCELLED;

	/* set preview for this surface only and set active */
	pmd->canvas->active_sur = 0;
	for(surface=surface->prev; surface; surface=surface->prev) {
				surface->flags &= ~MOD_DPAINT_PREVIEW;
				pmd->canvas->active_sur++;
	}

	return OPERATOR_FINISHED;
}

/* add surface slot */
void DPAINT_OT_surface_slot_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Surface Slot";
	ot->idname= "DPAINT_OT_surface_slot_add";
	ot->description="Add a new Dynamic Paint surface slot";
	
	/* api callbacks */
	ot->exec= surface_slot_add_exec;
	ot->poll= ED_operator_object_active_editable;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int surface_slot_remove_exec(bContext *C, wmOperator *op)
{
	DynamicPaintModifierData *pmd = 0;
	Object *cObject = CTX_data_pointer_get_type(C, "object", &RNA_Object).data;
	DynamicPaintSurface *surface;
	int id=0;

	/* Make sure we're dealing with a canvas */
	pmd = (DynamicPaintModifierData *)modifiers_findByType(cObject, eModifierType_DynamicPaint);
	if (!pmd) return OPERATOR_CANCELLED;
	if (!pmd->canvas) return OPERATOR_CANCELLED;

	surface = pmd->canvas->surfaces.first;

	/* find active surface and remove it */
	for(; surface; surface=surface->next) {
		if(id == pmd->canvas->active_sur) {
				pmd->canvas->active_sur -= 1;
				dynamicPaint_freeSurface(surface);
				break;
			}
		id++;
	}

	dynamicPaint_resetPreview(pmd->canvas);
	DAG_id_tag_update(&cObject->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_MODIFIER, cObject);

	return OPERATOR_FINISHED;
}

/* remove surface slot */
void DPAINT_OT_surface_slot_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Remove Surface Slot";
	ot->idname= "DPAINT_OT_surface_slot_remove";
	ot->description="Remove the selected surface slot";
	
	/* api callbacks */
	ot->exec= surface_slot_remove_exec;
	ot->poll= ED_operator_object_active_editable;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int type_toggle_exec(bContext *C, wmOperator *op)
{

	Object *cObject = CTX_data_pointer_get_type(C, "object", &RNA_Object).data;
	Scene *scene = CTX_data_scene(C);
	DynamicPaintModifierData *pmd = (DynamicPaintModifierData *)modifiers_findByType(cObject, eModifierType_DynamicPaint);
	int type= RNA_enum_get(op->ptr, "type");

	if (!pmd) return OPERATOR_CANCELLED;

	/* if type is already enabled, toggle it off */
	if (type == MOD_DYNAMICPAINT_TYPE_CANVAS && pmd->canvas) {
			dynamicPaint_freeCanvas(pmd);
	}
	else if (type == MOD_DYNAMICPAINT_TYPE_BRUSH && pmd->brush) {
			dynamicPaint_freeBrush(pmd);
	}
	/* else create a new type */
	else {
		if (!dynamicPaint_createType(pmd, type, scene))
			return OPERATOR_CANCELLED;
	}
	
	/* update dependancy */
	DAG_id_tag_update(&cObject->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_MODIFIER, cObject);
	DAG_scene_sort(CTX_data_main(C), scene);

	return OPERATOR_FINISHED;
}

void DPAINT_OT_type_toggle(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name= "Toggle Type Active";
	ot->idname= "DPAINT_OT_type_toggle";
	ot->description = "Toggles whether given type is active or not";
	
	/* api callbacks */
	ot->exec= type_toggle_exec;
	ot->poll= ED_operator_object_active_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	prop= RNA_def_enum(ot->srna, "type", prop_dynamicpaint_type_items, MOD_DYNAMICPAINT_TYPE_CANVAS, "Type", "");
	ot->prop= prop;
}

static int output_toggle_exec(bContext *C, wmOperator *op)
{

	Object *ob = CTX_data_pointer_get_type(C, "object", &RNA_Object).data;
	Scene *scene = CTX_data_scene(C);
	DynamicPaintModifierData *pmd = (DynamicPaintModifierData *)modifiers_findByType(ob, eModifierType_DynamicPaint);
	int index= RNA_int_get(op->ptr, "index");

	if (!pmd) return OPERATOR_CANCELLED;


	/* if type is already enabled, toggle it off */
	if (pmd->canvas) {
			DynamicPaintSurface *surface = get_activeSurface(pmd->canvas);

			if (surface->format == MOD_DPAINT_SURFACE_F_VERTEX) {
				int exists = dynamicPaint_outputLayerExists(surface, ob, index);
				char *name;
				
				if (index == 0)
					name = surface->output_name;
				else if (index == 1)
					name = surface->output_name2;

				/* Vertex Color Layer */
				if (surface->type == MOD_DPAINT_SURFACE_T_PAINT) {
					if (!exists)
						ED_mesh_color_add(C, scene, ob, ob->data, name, 1);
					else 
						ED_mesh_color_remove_named(C, ob, ob->data, name);
				}
				/* Vertex Weight Layer */
				else if (surface->type == MOD_DPAINT_SURFACE_T_WEIGHT) {
					if (!exists)
						ED_vgroup_add_name(ob, name);
					else {
						bDeformGroup *defgroup = defgroup_find_name(ob, name);
						if (defgroup) ED_vgroup_delete(ob, defgroup);
					}
				}
			}
	}

	return OPERATOR_FINISHED;
}

void DPAINT_OT_output_toggle(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name= "Toggle Output Layer";
	ot->idname= "DPAINT_OT_output_toggle";
	ot->description = "Adds or removes Dynamic Paint output data layer";
	
	/* api callbacks */
	ot->exec= output_toggle_exec;
	ot->poll= ED_operator_object_active_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	prop= RNA_def_int(ot->srna, "index", 0, 0, 1, "Index", "", 0, 1);
	ot->prop= prop;
}


/***************************** Image Sequence Baking ******************************/

/*
*	Do actual bake operation. Loop through to-be-baked frames.
*	Returns 0 on failture.
*/
static int dynamicPaint_bakeImageSequence(bContext *C, DynamicPaintSurface *surface, Object *cObject)
{
	DynamicPaintCanvasSettings *canvas = surface->canvas;
	Scene *scene= CTX_data_scene(C);
	wmWindow *win = CTX_wm_window(C);
	int frame = 1;
	int frames;

	frames = surface->end_frame - surface->start_frame + 1;
	if (frames <= 0) {sprintf(canvas->error, "No frames to bake.");printf("DynamicPaint bake failed: %s", canvas->error);return 0;}

	/*
	*	Set frame to start point (also inits modifier data)
	*/
	frame = surface->start_frame;
	scene->r.cfra = (int)frame;
	ED_update_for_newframe(CTX_data_main(C), scene, win->screen, 1);

	/* Init surface	*/
	if (!dynamicPaint_createUVSurface(surface)) return 0;

	/*
	*	Loop through selected frames
	*/
	for (frame=surface->start_frame; frame<=surface->end_frame; frame++)
	{
		float progress = (frame - surface->start_frame) / (float)frames * 100;
		surface->current_frame = frame;

		/* If user requested stop (esc), quit baking	*/
		if (blender_test_break()) return 0;

		/* Update progress bar cursor */
		WM_timecursor(win, (int)progress);
		printf("DynamicPaint: Baking frame %i\n", frame);

		/* calculate a frame */
		scene->r.cfra = (int)frame;
		ED_update_for_newframe(CTX_data_main(C), scene, win->screen, 1);
		if (!dynamicPaint_calculateFrame(surface, scene, cObject, frame)) return 0;

		/*
		*	Save output images
		*/
		{
			char filename[250];
			char pad[4];
			char dir_slash[2];
			/* OpenEXR or PNG	*/
			short format = (surface->image_fileformat & MOD_DPAINT_IMGFORMAT_OPENEXR) ? DPOUTPUT_OPENEXR : DPOUTPUT_PNG;

			/* Add frame number padding	*/
			if (frame<10) sprintf(pad,"000");
			else if (frame<100) sprintf(pad,"00");
			else if (frame<1000) sprintf(pad,"0");
			else pad[0] = '\0';

			/* make sure directory path is valid to append filename */
			if (surface->image_output_path[strlen(surface->image_output_path)-1] != 47 &&
				surface->image_output_path[strlen(surface->image_output_path)-1] != 92)
				strcpy(dir_slash,"/");
			else
				dir_slash[0] = '\0';


			/* color map	*/
			if (surface->type == MOD_DPAINT_SURFACE_T_PAINT) {
				if (surface->flags & MOD_DPAINT_OUT1) {
					sprintf(filename, "%s%s%s%s%i", surface->image_output_path, dir_slash, surface->output_name, pad, (int)frame);
					dynamicPaint_outputImage(surface, filename, format, DPOUTPUT_PAINT);
				}
				if (surface->flags & MOD_DPAINT_OUT2) {
					sprintf(filename, "%s%s%s%s%i", surface->image_output_path, dir_slash, surface->output_name2, pad, (int)frame);
					dynamicPaint_outputImage(surface, filename, format, DPOUTPUT_WET);
				}
			}

			/* displacement map	*/
			else if (surface->type == MOD_DPAINT_SURFACE_T_DISPLACE) {
				sprintf(filename, "%s%s%s%s%i", surface->image_output_path, dir_slash, surface->output_name, pad, (int)frame);
				dynamicPaint_outputImage(surface, filename, format, DPOUTPUT_DISPLACE);
			}

			/* waves	*/
			else if (surface->type == MOD_DPAINT_SURFACE_T_WAVE) {
				sprintf(filename, "%s%s%s%s%i", surface->image_output_path, dir_slash, surface->output_name, pad, (int)frame);
				dynamicPaint_outputImage(surface, filename, format, DPOUTPUT_WAVES);
			}
		}
	}
	return 1;
}


/*
*	Bake Dynamic Paint image sequence surface
*/
int dynamicPaint_initBake(struct bContext *C, struct wmOperator *op)
{
	DynamicPaintModifierData *pmd = NULL;
	Object *ob = CTX_data_pointer_get_type(C, "object", &RNA_Object).data;
	int status = 0;
	double timer = PIL_check_seconds_timer();
	DynamicPaintSurface *surface;

	/*
	*	Get modifier data
	*/
	pmd = (DynamicPaintModifierData *)modifiers_findByType(ob, eModifierType_DynamicPaint);
	if (!pmd) {
		BKE_report(op->reports, RPT_ERROR, "Bake Failed: No Dynamic Paint modifier found.");
		return 0;
	}

	/* Make sure we're dealing with a canvas */
	if (!pmd->canvas) {
		BKE_report(op->reports, RPT_ERROR, "Bake Failed: Invalid Canvas.");
		return 0;
	}
	surface = get_activeSurface(pmd->canvas);

	/* Set state to baking and init surface */
	pmd->canvas->error[0] = '\0';
	pmd->canvas->flags |= MOD_DPAINT_BAKING;
	G.afbreek= 0;	/* reset blender_test_break*/

	/*  Bake Dynamic Paint	*/
	status = dynamicPaint_bakeImageSequence(C, surface, ob);
	/* Clear bake */
	pmd->canvas->flags &= ~MOD_DPAINT_BAKING;
	WM_cursor_restore(CTX_wm_window(C));
	dynamicPaint_freeSurfaceData(surface);

	/* Bake was successful:
	*  Report for ended bake and how long it took */
	if (status) {

		/* Format time string	*/
		char timestr[30];
		double time = PIL_check_seconds_timer() - timer;
		int tmp_val;
		timestr[0] = '\0';

		/* days (just in case someone actually has a very slow pc)	*/
		tmp_val = (int)floor(time / 86400.0f);
		if (tmp_val > 0) sprintf(timestr, "%i Day(s) - ", tmp_val);
		/* hours	*/
		time -= 86400.0f * tmp_val;
		tmp_val = (int)floor(time / 3600.0f);
		if (tmp_val > 0) sprintf(timestr, "%s%i h ", timestr, tmp_val);
		/* minutes	*/
		time -= 3600.0f * tmp_val;
		tmp_val = (int)floor(time / 60.0f);
		if (tmp_val > 0) sprintf(timestr, "%s%i min ", timestr, tmp_val);
		/* seconds	*/
		time -= 60.0f * tmp_val;
		tmp_val = (int)ceil(time);
		sprintf(timestr, "%s%i s", timestr, tmp_val);

		/* Show bake info */
		sprintf(pmd->canvas->ui_info, "Bake Complete! (Time: %s)", timestr);
		printf("%s\n", pmd->canvas->ui_info);
	}
	else {
		if (strlen(pmd->canvas->error)) { /* If an error occured */
			sprintf(pmd->canvas->ui_info, "Bake Failed: %s", pmd->canvas->error);
			BKE_report(op->reports, RPT_ERROR, pmd->canvas->ui_info);
		}
		else {	/* User cancelled the bake */
			sprintf(pmd->canvas->ui_info, "Baking Cancelled!");
			BKE_report(op->reports, RPT_WARNING, pmd->canvas->ui_info);
		}

		/* Print failed bake to console */
		printf("Baking Cancelled!\n");
	}

	return status;
}

static int dynamicpaint_bake_exec(bContext *C, wmOperator *op)
{
	/* Bake dynamic paint */
	if(!dynamicPaint_initBake(C, op)) {
		return OPERATOR_CANCELLED;}

	return OPERATOR_FINISHED;
}

void DPAINT_OT_bake(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Dynamic Paint Bake";
	ot->description= "Bake dynamic paint image sequence surface";
	ot->idname= "DPAINT_OT_bake";
	
	/* api callbacks */
	ot->exec= dynamicpaint_bake_exec;
	ot->poll= ED_operator_object_active_editable;
}