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
 * Contributor(s): Blender Foundation (2009), Joshua Leung, Antonio Vazquez
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_gpencil.c
 *  \ingroup RNA
 */

#include <stdlib.h>

#include "DNA_gpencil_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_string_utils.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "rna_internal.h"

#include "WM_types.h"
#include "DNA_object_types.h"
#include "ED_gpencil.h"

/* parent type */
static EnumPropertyItem parent_type_items[] = {
	{PAROBJECT, "OBJECT", 0, "Object", "The layer is parented to an object"},
	{PARSKEL, "ARMATURE", 0, "Armature", ""},
	{PARBONE, "BONE", 0, "Bone", "The layer is parented to a bone"},
	{0, NULL, 0, NULL, NULL}
};


#ifdef RNA_RUNTIME

#include "BLI_math.h"

#include "WM_api.h"

#include "BKE_animsys.h"
#include "BKE_gpencil.h"
#include "BKE_action.h"


static void rna_GPencil_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *UNUSED(ptr))
{
	WM_main_add_notifier(NC_GPENCIL | NA_EDITED, NULL);
}

static void rna_GPencil_editmode_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *UNUSED(ptr))
{
	/* Notify all places where GPencil data lives that the editing state is different */
	WM_main_add_notifier(NC_GPENCIL | NA_EDITED, NULL);
	WM_main_add_notifier(NC_SCENE | ND_MODE | NC_MOVIECLIP, NULL);
}

static void rna_GPencil_onion_skinning_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	bGPdata *gpd = (bGPdata *)ptr->id.data;
	bGPDlayer *gpl;
	bool enabled = false;
	
	/* Ensure that the datablock's onionskinning toggle flag
	 * stays in sync with the status of the actual layers
	 */
	for (gpl = gpd->layers.first; gpl; gpl = gpl->next) {
		if (gpl->flag & GP_LAYER_ONIONSKIN) {
			enabled = true;
		}
	}
	
	if (enabled)
		gpd->flag |= GP_DATA_SHOW_ONIONSKINS;
	else
		gpd->flag &= ~GP_DATA_SHOW_ONIONSKINS;
	
	
	/* Now do standard updates... */
	rna_GPencil_update(bmain, scene, ptr);
}

static void rna_GPencil_stroke_colorname_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	bGPDstroke *gps = (bGPDstroke *)ptr->data;
	gps->flag |= GP_STROKE_RECALC_COLOR;
	gps->palcolor = NULL;

	/* Now do standard updates... */
	rna_GPencil_update(bmain, scene, ptr);
}

static char *rna_GPencilLayer_path(PointerRNA *ptr)
{
	bGPDlayer *gpl = (bGPDlayer *)ptr->data;
	char name_esc[sizeof(gpl->info) * 2];
	
	BLI_strescape(name_esc, gpl->info, sizeof(name_esc));
	
	return BLI_sprintfN("layers[\"%s\"]", name_esc);
}

static int rna_GPencilLayer_active_frame_editable(PointerRNA *ptr, const char **UNUSED(r_info))
{
	bGPDlayer *gpl = (bGPDlayer *)ptr->data;

	/* surely there must be other criteria too... */
	if (gpl->flag & GP_LAYER_LOCKED)
		return 0;
	else
		return PROP_EDITABLE;
}

static void rna_GPencilLayer_line_width_range(PointerRNA *ptr, int *min, int *max,
                                              int *softmin, int *softmax)
{
	bGPDlayer *gpl = ptr->data;
	
	/* The restrictions on max width here are due to OpenGL on Windows not supporting
	 * any widths greater than 10 (for driver-drawn) strokes/points.
	 *
	 * Although most of our 2D strokes also don't suffer from this restriction,
	 * it's relatively hard to test for that. So, for now, only volumetric strokes
	 * get to be larger...
	 */

	/* From GP v2 this value is used to increase or decrease the thickness of the stroke */
	if (gpl->flag & GP_LAYER_VOLUMETRIC) {
		*min = -300;
		*max = 300;
		
		*softmin = -100;
		*softmax = 100;
	}
	else {
		*min = -10;
		*max = 10;
		
		*softmin = -10;
		*softmax = 10;
	}
}

/* set parent */
static void set_parent(bGPDlayer *gpl, Object *par, const int type, const char *substr)
{
	if (type == PAROBJECT) {
		invert_m4_m4(gpl->inverse, par->obmat);
		gpl->parent = par;
		gpl->partype |= PAROBJECT;
		gpl->parsubstr[0] = 0;
	}
	else if (type == PARSKEL) {
		invert_m4_m4(gpl->inverse, par->obmat);
		gpl->parent = par;
		gpl->partype |= PARSKEL;
		gpl->parsubstr[0] = 0;
	}
	else if (type == PARBONE) {
		bPoseChannel *pchan = BKE_pose_channel_find_name(par->pose, substr);
		if (pchan) {
			float tmp_mat[4][4];
			mul_m4_m4m4(tmp_mat, par->obmat, pchan->pose_mat);

			invert_m4_m4(gpl->inverse, tmp_mat);
			gpl->parent = par;
			gpl->partype |= PARBONE;
			BLI_strncpy(gpl->parsubstr, substr, sizeof(gpl->parsubstr));
		}
	}
}

/* set parent object and inverse matrix */
static void rna_GPencilLayer_parent_set(PointerRNA *ptr, PointerRNA value)
{
	bGPDlayer *gpl = (bGPDlayer *)ptr->data;
	Object *par = (Object *)value.data; 

	if (par != NULL) {
		set_parent(gpl, par, gpl->partype, gpl->parsubstr);
	}
	else {
		/* keep strokes in the same place, so apply current transformation */
		if (gpl->parent != NULL) {
			bGPDspoint *pt;
			int i;
			float diff_mat[4][4];
			/* calculate difference matrix */
			ED_gpencil_parent_location(gpl, diff_mat);
			for (bGPDframe *gpf = gpl->frames.first; gpf; gpf = gpf->next) {
				for (bGPDstroke *gps = gpf->strokes.first; gps; gps = gps->next) {
					for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
						mul_m4_v3(diff_mat, &pt->x);
					}
				}
			}
		}
		/* clear parent */
		gpl->parent = NULL;
	}
}

/* set parent type */
static void rna_GPencilLayer_parent_type_set(PointerRNA *ptr, int value)
{
	bGPDlayer *gpl = (bGPDlayer *)ptr->data;
	Object *par = gpl->parent;
	gpl->partype = value;

	if (par != NULL) {
		set_parent(gpl, par, value, gpl->parsubstr);
	}
}

/* set parent bone */
static void rna_GPencilLayer_parent_bone_set(PointerRNA *ptr, const char *value)
{
	bGPDlayer *gpl = (bGPDlayer *)ptr->data;

	Object *par = gpl->parent;
	gpl->partype = PARBONE;

	if (par != NULL) {
		set_parent(gpl, par, gpl->partype, value);
	}
}


/* parent types enum */
static EnumPropertyItem *rna_Object_parent_type_itemf(
        bContext *UNUSED(C), PointerRNA *ptr,
        PropertyRNA *UNUSED(prop), bool *r_free)
{
	bGPDlayer *gpl = (bGPDlayer *)ptr->data;
	EnumPropertyItem *item = NULL;
	int totitem = 0;

	RNA_enum_items_add_value(&item, &totitem, parent_type_items, PAROBJECT);

	if (gpl->parent) {
		Object *par = gpl->parent;

		if (par->type == OB_ARMATURE) {
			/* special hack: prevents this being overrided */
			RNA_enum_items_add_value(&item, &totitem, &parent_type_items[1], PARSKEL);
			RNA_enum_items_add_value(&item, &totitem, parent_type_items, PARBONE);
		}
	}

	RNA_enum_item_end(&item, &totitem);
	*r_free = true;

	return item;
}

static int rna_GPencilLayer_is_parented_get(PointerRNA *ptr)
{
	bGPDlayer *gpl = (bGPDlayer *)ptr->data;
	return (gpl->parent != NULL);
}

static PointerRNA rna_GPencil_active_layer_get(PointerRNA *ptr)
{
	bGPdata *gpd = ptr->id.data;

	if (GS(gpd->id.name) == ID_GD) { /* why would this ever be not GD */
		bGPDlayer *gl;

		for (gl = gpd->layers.first; gl; gl = gl->next) {
			if (gl->flag & GP_LAYER_ACTIVE) {
				break;
			}
		}

		if (gl) {
			return rna_pointer_inherit_refine(ptr, &RNA_GPencilLayer, gl);
		}
	}

	return rna_pointer_inherit_refine(ptr, NULL, NULL);
}

static void rna_GPencil_active_layer_set(PointerRNA *ptr, PointerRNA value)
{
	bGPdata *gpd = ptr->id.data;

	if (GS(gpd->id.name) == ID_GD) { /* why would this ever be not GD */
		bGPDlayer *gl;

		for (gl = gpd->layers.first; gl; gl = gl->next) {
			if (gl == value.data) {
				gl->flag |= GP_LAYER_ACTIVE;
			}
			else {
				gl->flag &= ~GP_LAYER_ACTIVE;
			}
		}
		
		WM_main_add_notifier(NC_GPENCIL | NA_EDITED, NULL);
	}
}

static int rna_GPencil_active_layer_index_get(PointerRNA *ptr)
{
	bGPdata *gpd = (bGPdata *)ptr->id.data;
	bGPDlayer *gpl = BKE_gpencil_layer_getactive(gpd);
	
	return BLI_findindex(&gpd->layers, gpl);
}

static void rna_GPencil_active_layer_index_set(PointerRNA *ptr, int value)
{
	bGPdata *gpd   = (bGPdata *)ptr->id.data;
	bGPDlayer *gpl = BLI_findlink(&gpd->layers, value);

	BKE_gpencil_layer_setactive(gpd, gpl);
}

static void rna_GPencil_active_layer_index_range(PointerRNA *ptr, int *min, int *max, int *softmin, int *softmax)
{
	bGPdata *gpd = (bGPdata *)ptr->id.data;

	*min = 0;
	*max = max_ii(0, BLI_listbase_count(&gpd->layers) - 1);

	*softmin = *min;
	*softmax = *max;
}

static void rna_GPencilLayer_info_set(PointerRNA *ptr, const char *value)
{
	bGPdata *gpd = ptr->id.data;
	bGPDlayer *gpl = ptr->data;

	char oldname[128] = "";
	BLI_strncpy(oldname, gpl->info, sizeof(oldname));

	/* copy the new name into the name slot */
	BLI_strncpy_utf8(gpl->info, value, sizeof(gpl->info));

	BLI_uniquename(&gpd->layers, gpl, DATA_("GP_Layer"), '.', offsetof(bGPDlayer, info), sizeof(gpl->info));

	/* now fix animation paths */
	BKE_animdata_fix_paths_rename_all(&gpd->id, "layers", oldname, gpl->info);
}

static void rna_GPencil_use_onion_skinning_set(PointerRNA *ptr, const int value)
{
	bGPdata *gpd = ptr->id.data;
	bGPDlayer *gpl;
	
	/* set new value */
	if (value) {
		/* enable on active layer (it's the one that's most likely to be of interest right now) */
		gpl = BKE_gpencil_layer_getactive(gpd);
		if (gpl) {
			gpl->flag |= GP_LAYER_ONIONSKIN;
		}
		
		gpd->flag |= GP_DATA_SHOW_ONIONSKINS;
	}
	else {
		/* disable on all layers - allowa quickly turning them all off, without having to check */
		for (gpl = gpd->layers.first; gpl; gpl = gpl->next) {
			gpl->flag &= ~GP_LAYER_ONIONSKIN;
		}
		
		gpd->flag &= ~GP_DATA_SHOW_ONIONSKINS;
	}
}

static bGPDstroke *rna_GPencil_stroke_point_find_stroke(const bGPdata *gpd, const bGPDspoint *pt, bGPDlayer **r_gpl, bGPDframe **r_gpf)
{
	bGPDlayer *gpl;
	bGPDstroke *gps;
	
	/* sanity checks */
	if (ELEM(NULL, gpd, pt)) {
		return NULL;
	}
	
	if (r_gpl) *r_gpl = NULL;
	if (r_gpf) *r_gpf = NULL;
	
	/* there's no faster alternative than just looping over everything... */
	for (gpl = gpd->layers.first; gpl; gpl = gpl->next) {
		if (gpl->actframe) {
			for (gps = gpl->actframe->strokes.first; gps; gps = gps->next) {
				if ((pt >= gps->points) && (pt < &gps->points[gps->totpoints])) {
					/* found it */
					if (r_gpl) *r_gpl = gpl;
					if (r_gpf) *r_gpf = gpl->actframe;
					
					return gps;
				}
			}
		}
	}
	
	/* didn't find it */
	return NULL;
}

static void rna_GPencil_stroke_point_select_set(PointerRNA *ptr, const int value)
{
	bGPdata *gpd = ptr->id.data;
	bGPDspoint *pt = ptr->data;
	bGPDstroke *gps = NULL;
	
	/* Ensure that corresponding stroke is set 
	 * - Since we don't have direct access, we're going to have to search
	 * - We don't apply selection value unless we can find the corresponding
	 *   stroke, so that they don't get out of sync
	 */
	gps = rna_GPencil_stroke_point_find_stroke(gpd, pt, NULL, NULL);
	if (gps) {
		/* Set the new selection state for the point */
		if (value)
			pt->flag |= GP_SPOINT_SELECT;
		else
			pt->flag &= ~GP_SPOINT_SELECT;
		
		/* Check if the stroke should be selected or not... */
		BKE_gpencil_stroke_sync_selection(gps);
	}
}

static void rna_GPencil_stroke_point_add(bGPDstroke *stroke, int count, float pressure, float strength)
{
	if (count > 0) {
		/* create space at the end of the array for extra points */
		stroke->points = MEM_recallocN_id(stroke->points,
		                                  sizeof(bGPDspoint) * (stroke->totpoints + count),
		                                  "gp_stroke_points");
		
		/* init the pressure and strength values so that old scripts won't need to
		 * be modified to give these initial values...
		 */
		for (int i = 0; i < count; i++) {
			bGPDspoint *pt = stroke->points + (stroke->totpoints + i);
			pt->pressure = pressure;
			pt->strength = strength;
		}
		
		stroke->totpoints += count;
	}
}

static void rna_GPencil_stroke_point_pop(bGPDstroke *stroke, ReportList *reports, int index)
{
	bGPDspoint *pt_tmp = stroke->points;

	/* python style negative indexing */
	if (index < 0) {
		index += stroke->totpoints;
	}

	if (stroke->totpoints <= index || index < 0) {
		BKE_report(reports, RPT_ERROR, "GPencilStrokePoints.pop: index out of range");
		return;
	}

	stroke->totpoints--;

	stroke->points = MEM_callocN(sizeof(bGPDspoint) * stroke->totpoints, "gp_stroke_points");

	if (index > 0)
		memcpy(stroke->points, pt_tmp, sizeof(bGPDspoint) * index);

	if (index < stroke->totpoints)
		memcpy(&stroke->points[index], &pt_tmp[index + 1], sizeof(bGPDspoint) * (stroke->totpoints - index));

	/* free temp buffer */
	MEM_freeN(pt_tmp);

	WM_main_add_notifier(NC_GPENCIL | NA_EDITED, NULL);
}

static bGPDstroke *rna_GPencil_stroke_new(bGPDframe *frame, const char *colorname)
{
	bGPDstroke *stroke = MEM_callocN(sizeof(bGPDstroke), "gp_stroke");
	if (colorname) {
		BLI_strncpy(stroke->colorname, colorname, sizeof(stroke->colorname));
	}
	stroke->palcolor = NULL;
	stroke->flag |= GP_STROKE_RECALC_COLOR;
	BLI_addtail(&frame->strokes, stroke);

	return stroke;
}

static void rna_GPencil_stroke_remove(bGPDframe *frame, ReportList *reports, PointerRNA *stroke_ptr)
{
	bGPDstroke *stroke = stroke_ptr->data;
	if (BLI_findindex(&frame->strokes, stroke) == -1) {
		BKE_report(reports, RPT_ERROR, "Stroke not found in grease pencil frame");
		return;
	}

	BLI_freelinkN(&frame->strokes, stroke);
	RNA_POINTER_INVALIDATE(stroke_ptr);

	WM_main_add_notifier(NC_GPENCIL | NA_EDITED, NULL);
}

static void rna_GPencil_stroke_select_set(PointerRNA *ptr, const int value)
{
	bGPDstroke *gps = ptr->data;
	bGPDspoint *pt;
	int i;
	
	/* set new value */
	if (value)
		gps->flag |= GP_STROKE_SELECT;
	else
		gps->flag &= ~GP_STROKE_SELECT;
		
	/* ensure that the stroke's points are selected in the same way */
	for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
		if (value)
			pt->flag |= GP_SPOINT_SELECT;
		else
			pt->flag &= ~GP_SPOINT_SELECT;
	}
}

static bGPDframe *rna_GPencil_frame_new(bGPDlayer *layer, ReportList *reports, int frame_number)
{
	bGPDframe *frame;

	if (BKE_gpencil_layer_find_frame(layer, frame_number)) {
		BKE_reportf(reports, RPT_ERROR, "Frame already exists on this frame number %d", frame_number);
		return NULL;
	}

	frame = BKE_gpencil_frame_addnew(layer, frame_number);

	WM_main_add_notifier(NC_GPENCIL | NA_EDITED, NULL);

	return frame;
}

static void rna_GPencil_frame_remove(bGPDlayer *layer, ReportList *reports, PointerRNA *frame_ptr)
{
	bGPDframe *frame = frame_ptr->data;
	if (BLI_findindex(&layer->frames, frame) == -1) {
		BKE_report(reports, RPT_ERROR, "Frame not found in grease pencil layer");
		return;
	}

	BKE_gpencil_layer_delframe(layer, frame);
	RNA_POINTER_INVALIDATE(frame_ptr);

	WM_main_add_notifier(NC_GPENCIL | NA_EDITED, NULL);
}

static bGPDframe *rna_GPencil_frame_copy(bGPDlayer *layer, bGPDframe *src)
{
	bGPDframe *frame = BKE_gpencil_frame_duplicate(src);

	while (BKE_gpencil_layer_find_frame(layer, frame->framenum)) {
		frame->framenum++;
	}

	BLI_addtail(&layer->frames, frame);

	WM_main_add_notifier(NC_GPENCIL | NA_EDITED, NULL);

	return frame;
}

static bGPDlayer *rna_GPencil_layer_new(bGPdata *gpd, const char *name, int setactive)
{
	bGPDlayer *gpl = BKE_gpencil_layer_addnew(gpd, name, setactive != 0);

	WM_main_add_notifier(NC_GPENCIL | ND_DATA | NA_EDITED, NULL);

	return gpl;
}

static void rna_GPencil_layer_remove(bGPdata *gpd, ReportList *reports, PointerRNA *layer_ptr)
{
	bGPDlayer *layer = layer_ptr->data;
	if (BLI_findindex(&gpd->layers, layer) == -1) {
		BKE_report(reports, RPT_ERROR, "Layer not found in grease pencil data");
		return;
	}

	BKE_gpencil_layer_delete(gpd, layer);
	RNA_POINTER_INVALIDATE(layer_ptr);

	WM_main_add_notifier(NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
}

static void rna_GPencil_frame_clear(bGPDframe *frame)
{
	BKE_gpencil_free_strokes(frame);

	WM_main_add_notifier(NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
}

static void rna_GPencil_layer_clear(bGPDlayer *layer)
{
	BKE_gpencil_free_frames(layer);

	WM_main_add_notifier(NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
}

static void rna_GPencil_clear(bGPdata *gpd)
{
	BKE_gpencil_free_layers(&gpd->layers);

	WM_main_add_notifier(NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
}

/* Palettes */
static bGPDpalette *rna_GPencil_palette_new(bGPdata *gpd, const char *name, int setactive)
{
	bGPDpalette *palette = BKE_gpencil_palette_addnew(gpd, name, setactive != 0);

	WM_main_add_notifier(NC_GPENCIL | ND_DATA | NA_EDITED, NULL);

	return palette;
}

static void rna_GPencil_palette_remove(bGPdata *gpd, ReportList *reports, PointerRNA *palette_ptr)
{
	bGPDpalette *palette = palette_ptr->data;
	if (BLI_findindex(&gpd->palettes, palette) == -1) {
		BKE_report(reports, RPT_ERROR, "Palette not found in grease pencil data");
		return;
	}

	BKE_gpencil_palette_delete(gpd, palette);
	RNA_POINTER_INVALIDATE(palette_ptr);

	WM_main_add_notifier(NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
}

static PointerRNA rna_GPencil_active_palette_get(PointerRNA *ptr)
{
	bGPdata *gpd = ptr->id.data;

	if (GS(gpd->id.name) == ID_GD) { /* why would this ever be not GD */
		bGPDpalette *palette;

		for (palette = gpd->palettes.first; palette; palette = palette->next) {
			if (palette->flag & PL_PALETTE_ACTIVE) {
				break;
			}
		}

		if (palette) {
			return rna_pointer_inherit_refine(ptr, &RNA_GPencilPalette, palette);
		}
	}

	return rna_pointer_inherit_refine(ptr, NULL, NULL);
}

static void rna_GPencil_active_palette_set(PointerRNA *ptr, PointerRNA value)
{
	bGPdata *gpd = ptr->id.data;

	if (GS(gpd->id.name) == ID_GD) { /* why would this ever be not GD */
		bGPDpalette *palette;

		for (palette = gpd->palettes.first; palette; palette = palette->next) {
			if (palette == value.data) {
				palette->flag |= PL_PALETTE_ACTIVE;
			}
			else {
				palette->flag &= ~PL_PALETTE_ACTIVE;
			}
		}
		/* force color recalc */
		BKE_gpencil_palette_change_strokes(gpd);

		WM_main_add_notifier(NC_GPENCIL | NA_EDITED, NULL);
	}
}

static int rna_GPencilPalette_index_get(PointerRNA *ptr)
{
	bGPdata *gpd = (bGPdata *)ptr->id.data;
	bGPDpalette *palette = BKE_gpencil_palette_getactive(gpd);

	return BLI_findindex(&gpd->palettes, palette);
}

static void rna_GPencilPalette_index_set(PointerRNA *ptr, int value)
{
	bGPdata *gpd   = (bGPdata *)ptr->id.data;
	bGPDpalette *palette = BLI_findlink(&gpd->palettes, value);

	BKE_gpencil_palette_setactive(gpd, palette);
	WM_main_add_notifier(NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
}

static void rna_GPencilPalette_index_range(PointerRNA *ptr, int *min, int *max, int *softmin, int *softmax)
{
	bGPdata *gpd = (bGPdata *)ptr->id.data;

	*min = 0;
	*max = max_ii(0, BLI_listbase_count(&gpd->palettes) - 1);

	*softmin = *min;
	*softmax = *max;
}

/* Palette colors */
static bGPDpalettecolor *rna_GPencilPalette_color_new(bGPDpalette *palette)
{
	bGPDpalettecolor *color = BKE_gpencil_palettecolor_addnew(palette, DATA_("Color"), true);

	return color;
}

static void rna_GPencilPalette_color_remove(bGPDpalette *palette, ReportList *reports, PointerRNA *color_ptr)
{
	bGPDpalettecolor *color = color_ptr->data;

	if (BLI_findindex(&palette->colors, color) == -1) {
		BKE_reportf(reports, RPT_ERROR, "Palette '%s' does not contain color given", palette->info + 2);
		return;
	}

	BKE_gpencil_palettecolor_delete(palette, color);
	RNA_POINTER_INVALIDATE(color_ptr);

	WM_main_add_notifier(NC_GPENCIL | ND_DATA | NA_EDITED, NULL);
}

static PointerRNA rna_GPencilPalette_active_color_get(PointerRNA *ptr)
{
	bGPDpalette *palette = (bGPDpalette *)ptr->data;
	bGPDpalettecolor *color;

	for (color = palette->colors.first; color; color = color->next) {
		if (color->flag & PC_COLOR_ACTIVE) {
			break;
		}
	}

	if (color) {
		return rna_pointer_inherit_refine(ptr, &RNA_GPencilPaletteColor, color);
	}

	return rna_pointer_inherit_refine(ptr, NULL, NULL);
}

static void rna_GPencilPalette_active_color_set(PointerRNA *ptr, PointerRNA value)
{
	bGPDpalette *palette = (bGPDpalette *)ptr->data;
	bGPDpalettecolor *color = value.data;

	BKE_gpencil_palettecolor_setactive(palette, color);
}

static void rna_GPencilPalette_info_set(PointerRNA *ptr, const char *value)
{
	bGPdata *gpd = ptr->id.data;
	bGPDpalette *palette = ptr->data;

	char oldname[64] = "";
	BLI_strncpy(oldname, palette->info, sizeof(oldname));

	/* copy the new name into the name slot */
	BLI_strncpy_utf8(palette->info, value, sizeof(palette->info));

	BLI_uniquename(&gpd->palettes, palette, DATA_("GP_Palette"), '.', offsetof(bGPDpalette, info),
	               sizeof(palette->info));
	/* now fix animation paths */
	BKE_animdata_fix_paths_rename_all(&gpd->id, "palettes", oldname, palette->info);
}

static char *rna_GPencilPalette_path(PointerRNA *ptr)
{
	bGPDpalette *palette = ptr->data;
	char name_esc[sizeof(palette->info) * 2];
	
	BLI_strescape(name_esc, palette->info, sizeof(name_esc));
	
	return BLI_sprintfN("palettes[\"%s\"]", name_esc);
}

static char *rna_GPencilPalette_color_path(PointerRNA *ptr)
{
	bGPdata *gpd = ptr->id.data;
	bGPDpalette *palette = BKE_gpencil_palette_getactive(gpd);
	bGPDpalettecolor *palcolor = ptr->data;

	char name_palette[sizeof(palette->info) * 2];
	char name_color[sizeof(palcolor->info) * 2];

	BLI_strescape(name_palette, palette->info, sizeof(name_palette));
	BLI_strescape(name_color, palcolor->info, sizeof(name_color));

	return BLI_sprintfN("palettes[\"%s\"].colors[\"%s\"]", name_palette, name_color);
}

static void rna_GPencilPaletteColor_info_set(PointerRNA *ptr, const char *value)
{
	bGPdata *gpd = ptr->id.data;
	bGPDpalette *palette = BKE_gpencil_palette_getactive(gpd);
	bGPDpalettecolor *palcolor = ptr->data;
	
	char oldname[64] = "";
	BLI_strncpy(oldname, palcolor->info, sizeof(oldname));

	/* copy the new name into the name slot */
	BLI_strncpy_utf8(palcolor->info, value, sizeof(palcolor->info));
	BLI_uniquename(&palette->colors, palcolor, DATA_("Color"), '.', offsetof(bGPDpalettecolor, info),
	               sizeof(palcolor->info));
	
	/* rename all strokes */
	BKE_gpencil_palettecolor_changename(gpd, oldname, palcolor->info);

	/* now fix animation paths */
	BKE_animdata_fix_paths_rename_all(&gpd->id, "colors", oldname, palcolor->info);
}

static void rna_GPencilStrokeColor_info_set(PointerRNA *ptr, const char *value)
{
	bGPDstroke *gps = ptr->data;

	/* copy the new name into the name slot */
	BLI_strncpy_utf8(gps->colorname, value, sizeof(gps->colorname));
}


static int rna_GPencilPaletteColor_is_stroke_visible_get(PointerRNA *ptr)
{
	bGPDpalettecolor *pcolor = (bGPDpalettecolor *)ptr->data;
	return (pcolor->color[3] > GPENCIL_ALPHA_OPACITY_THRESH);
}

static int rna_GPencilPaletteColor_is_fill_visible_get(PointerRNA *ptr)
{
	bGPDpalettecolor *pcolor = (bGPDpalettecolor *)ptr->data;
	return (pcolor->fill[3] > GPENCIL_ALPHA_OPACITY_THRESH);
}

static int rna_GPencilPaletteColor_index_get(PointerRNA *ptr)
{
	bGPDpalette *palette = (bGPDpalette *)ptr->data;
	bGPDpalettecolor *pcolor = BKE_gpencil_palettecolor_getactive(palette);

	return BLI_findindex(&palette->colors, pcolor);
}

static void rna_GPencilPaletteColor_index_set(PointerRNA *ptr, int value)
{
	bGPDpalette *palette = (bGPDpalette *)ptr->data;
	bGPDpalettecolor *pcolor = BLI_findlink(&palette->colors, value);
	BKE_gpencil_palettecolor_setactive(palette, pcolor);
}

static void rna_GPencilPaletteColor_index_range(PointerRNA *ptr, int *min, int *max, int *softmin, int *softmax)
{
	bGPDpalette *palette = (bGPDpalette *)ptr->data;

	*min = 0;
	*max = max_ii(0, BLI_listbase_count(&palette->colors) - 1);

	*softmin = *min;
	*softmax = *max;
}

#else

static void rna_def_gpencil_stroke_point(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	
	srna = RNA_def_struct(brna, "GPencilStrokePoint", NULL);
	RNA_def_struct_sdna(srna, "bGPDspoint");
	RNA_def_struct_ui_text(srna, "Grease Pencil Stroke Point", "Data point for freehand stroke curve");
	
	prop = RNA_def_property(srna, "co", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_float_sdna(prop, NULL, "x");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Coordinates", "");
	RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");
	
	prop = RNA_def_property(srna, "pressure", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "pressure");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Pressure", "Pressure of tablet at point when drawing it");
	RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");
	
	prop = RNA_def_property(srna, "strength", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "strength");
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Strength", "Color intensity (alpha factor)");
	RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

	prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_SPOINT_SELECT);
	RNA_def_property_boolean_funcs(prop, NULL, "rna_GPencil_stroke_point_select_set");
	RNA_def_property_ui_text(prop, "Select", "Point is selected for viewport editing");
	RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");
}

static void rna_def_gpencil_stroke_points_api(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	FunctionRNA *func;

	RNA_def_property_srna(cprop, "GPencilStrokePoints");
	srna = RNA_def_struct(brna, "GPencilStrokePoints", NULL);
	RNA_def_struct_sdna(srna, "bGPDstroke");
	RNA_def_struct_ui_text(srna, "Grease Pencil Stroke Points", "Collection of grease pencil stroke points");

	func = RNA_def_function(srna, "add", "rna_GPencil_stroke_point_add");
	RNA_def_function_ui_description(func, "Add a new grease pencil stroke point");
	RNA_def_int(func, "count", 1, 0, INT_MAX, "Number", "Number of points to add to the stroke", 0, INT_MAX);
	RNA_def_float(func, "pressure", 1.0f, 0.0f, 1.0f, "Pressure", "Pressure for newly created points", 0.0f, 1.0f);
	RNA_def_float(func, "strength", 1.0f, 0.0f, 1.0f, "Strength", "Color intensity (alpha factor) for newly created points", 0.0f, 1.0f);

	func = RNA_def_function(srna, "pop", "rna_GPencil_stroke_point_pop");
	RNA_def_function_ui_description(func, "Remove a grease pencil stroke point");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	RNA_def_int(func, "index", -1, INT_MIN, INT_MAX, "Index", "point index", INT_MIN, INT_MAX);
}

/* This information is read only and it can be used by add-ons */
static void rna_def_gpencil_triangle(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "GPencilTriangle", NULL);
	RNA_def_struct_sdna(srna, "bGPDtriangle");
	RNA_def_struct_ui_text(srna, "Triangle", "Triangulation data for HQ fill");

	/* point v1 */
	prop = RNA_def_property(srna, "v1", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "v1");
	RNA_def_property_ui_text(prop, "v1", "First triangle vertex index");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	/* point v2 */
	prop = RNA_def_property(srna, "v2", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "v2");
	RNA_def_property_ui_text(prop, "v2", "Second triangle vertex index");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);

	/* point v3 */
	prop = RNA_def_property(srna, "v3", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "v3");
	RNA_def_property_ui_text(prop, "v3", "Third triangle vertex index");
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
}

static void rna_def_gpencil_stroke(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem stroke_draw_mode_items[] = {
		{0, "SCREEN", 0, "Screen", "Stroke is in screen-space"},
		{GP_STROKE_3DSPACE, "3DSPACE", 0, "3D Space", "Stroke is in 3D-space"},
		{GP_STROKE_2DSPACE, "2DSPACE", 0, "2D Space", "Stroke is in 2D-space"},
		{GP_STROKE_2DIMAGE, "2DIMAGE", 0, "2D Image", "Stroke is in 2D-space (but with special 'image' scaling)"},
		{0, NULL, 0, NULL, NULL}
	};
	
	srna = RNA_def_struct(brna, "GPencilStroke", NULL);
	RNA_def_struct_sdna(srna, "bGPDstroke");
	RNA_def_struct_ui_text(srna, "Grease Pencil Stroke", "Freehand curve defining part of a sketch");
	
	/* Points */
	prop = RNA_def_property(srna, "points", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "points", "totpoints");
	RNA_def_property_struct_type(prop, "GPencilStrokePoint");
	RNA_def_property_ui_text(prop, "Stroke Points", "Stroke data points");
	rna_def_gpencil_stroke_points_api(brna, prop);
	
	/* Triangles */
	prop = RNA_def_property(srna, "triangles", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "triangles", "tot_triangles");
	RNA_def_property_struct_type(prop, "GPencilTriangle");
	RNA_def_property_ui_text(prop, "Triangles", "Triangulation data for HQ fill");

	/* Color */
	prop = RNA_def_property(srna, "color", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "GPencilPaletteColor");
	RNA_def_property_pointer_sdna(prop, NULL, "palcolor");
	RNA_def_property_ui_text(prop, "Palette Color", "Color from palette used in Stroke");
	RNA_def_property_update(prop, 0, "rna_GPencil_update");

	/* Settings */
	prop = RNA_def_property(srna, "draw_mode", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "flag");
	RNA_def_property_enum_items(prop, stroke_draw_mode_items);
	RNA_def_property_ui_text(prop, "Draw Mode", "");
	RNA_def_property_update(prop, 0, "rna_GPencil_update");
	
	prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_STROKE_SELECT);
	RNA_def_property_boolean_funcs(prop, NULL, "rna_GPencil_stroke_select_set");
	RNA_def_property_ui_text(prop, "Select", "Stroke is selected for viewport editing");
	RNA_def_property_update(prop, 0, "rna_GPencil_update");

	/* Color Name */
	prop = RNA_def_property(srna, "colorname", PROP_STRING, PROP_NONE);
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_GPencilStrokeColor_info_set");
	RNA_def_property_ui_text(prop, "Color Name", "Palette color name");
	RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_stroke_colorname_update");

	/* Cyclic: Draw a line from end to start point */
	prop = RNA_def_property(srna, "draw_cyclic", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_STROKE_CYCLIC);
	RNA_def_property_ui_text(prop, "Cyclic", "Enable cyclic drawing, closing the stroke");
	RNA_def_property_update(prop, 0, "rna_GPencil_update");

	/* Line Thickness */
	prop = RNA_def_property(srna, "line_width", PROP_INT, PROP_PIXEL);
	RNA_def_property_int_sdna(prop, NULL, "thickness");
	RNA_def_property_range(prop, 1, 300);
	RNA_def_property_ui_range(prop, 1, 10, 1, 0);
	RNA_def_property_ui_text(prop, "Thickness", "Thickness of stroke (in pixels)");
	RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

}

static void rna_def_gpencil_strokes_api(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;

	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "GPencilStrokes");
	srna = RNA_def_struct(brna, "GPencilStrokes", NULL);
	RNA_def_struct_sdna(srna, "bGPDframe");
	RNA_def_struct_ui_text(srna, "Grease Pencil Frames", "Collection of grease pencil stroke");

	func = RNA_def_function(srna, "new", "rna_GPencil_stroke_new");
	RNA_def_function_ui_description(func, "Add a new grease pencil stroke");
	parm = RNA_def_string(func, "colorname", 0, MAX_NAME, "Color", "Name of the color");
	parm = RNA_def_pointer(func, "stroke", "GPencilStroke", "", "The newly created stroke");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "remove", "rna_GPencil_stroke_remove");
	RNA_def_function_ui_description(func, "Remove a grease pencil stroke");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "stroke", "GPencilStroke", "Stroke", "The stroke to remove");
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
	RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, 0);
}

static void rna_def_gpencil_frame(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	FunctionRNA *func;
	
	srna = RNA_def_struct(brna, "GPencilFrame", NULL);
	RNA_def_struct_sdna(srna, "bGPDframe");
	RNA_def_struct_ui_text(srna, "Grease Pencil Frame", "Collection of related sketches on a particular frame");
	
	/* Strokes */
	prop = RNA_def_property(srna, "strokes", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "strokes", NULL);
	RNA_def_property_struct_type(prop, "GPencilStroke");
	RNA_def_property_ui_text(prop, "Strokes", "Freehand curves defining the sketch on this frame");
	rna_def_gpencil_strokes_api(brna, prop);

	/* Frame Number */
	prop = RNA_def_property(srna, "frame_number", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "framenum");
	/* XXX note: this cannot occur on the same frame as another sketch */
	RNA_def_property_range(prop, -MAXFRAME, MAXFRAME);
	RNA_def_property_ui_text(prop, "Frame Number", "The frame on which this sketch appears");
	
	/* Flags */
	prop = RNA_def_property(srna, "is_edited", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_FRAME_PAINT); /* XXX should it be editable? */
	RNA_def_property_ui_text(prop, "Paint Lock", "Frame is being edited (painted on)");
	
	prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_FRAME_SELECT);
	RNA_def_property_ui_text(prop, "Select", "Frame is selected for editing in the Dope Sheet");
	
	
	/* API */
	func = RNA_def_function(srna, "clear", "rna_GPencil_frame_clear");
	RNA_def_function_ui_description(func, "Remove all the grease pencil frame data");
}

static void rna_def_gpencil_frames_api(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;

	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "GPencilFrames");
	srna = RNA_def_struct(brna, "GPencilFrames", NULL);
	RNA_def_struct_sdna(srna, "bGPDlayer");
	RNA_def_struct_ui_text(srna, "Grease Pencil Frames", "Collection of grease pencil frames");

	func = RNA_def_function(srna, "new", "rna_GPencil_frame_new");
	RNA_def_function_ui_description(func, "Add a new grease pencil frame");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	parm = RNA_def_int(func, "frame_number", 1, MINAFRAME, MAXFRAME, "Frame Number",
	                   "The frame on which this sketch appears", MINAFRAME, MAXFRAME);
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	parm = RNA_def_pointer(func, "frame", "GPencilFrame", "", "The newly created frame");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "remove", "rna_GPencil_frame_remove");
	RNA_def_function_ui_description(func, "Remove a grease pencil frame");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "frame", "GPencilFrame", "Frame", "The frame to remove");
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
	RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, 0);

	func = RNA_def_function(srna, "copy", "rna_GPencil_frame_copy");
	RNA_def_function_ui_description(func, "Copy a grease pencil frame");
	parm = RNA_def_pointer(func, "source", "GPencilFrame", "Source", "The source frame");
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
	parm = RNA_def_pointer(func, "copy", "GPencilFrame", "", "The newly copied frame");
	RNA_def_function_return(func, parm);
}

static void rna_def_gpencil_layer(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	FunctionRNA *func;
	
	srna = RNA_def_struct(brna, "GPencilLayer", NULL);
	RNA_def_struct_sdna(srna, "bGPDlayer");
	RNA_def_struct_ui_text(srna, "Grease Pencil Layer", "Collection of related sketches");
	RNA_def_struct_path_func(srna, "rna_GPencilLayer_path");
	
	/* Name */
	prop = RNA_def_property(srna, "info", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Info", "Layer name");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_GPencilLayer_info_set");
	RNA_def_struct_name_property(srna, prop);
	RNA_def_property_update(prop, NC_GPENCIL | ND_DATA | NA_RENAME, NULL);

	/* Frames */
	prop = RNA_def_property(srna, "frames", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "frames", NULL);
	RNA_def_property_struct_type(prop, "GPencilFrame");
	RNA_def_property_ui_text(prop, "Frames", "Sketches for this layer on different frames");
	rna_def_gpencil_frames_api(brna, prop);

	/* Active Frame */
	prop = RNA_def_property(srna, "active_frame", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_sdna(prop, NULL, "actframe");
	RNA_def_property_ui_text(prop, "Active Frame", "Frame currently being displayed for this layer");
	RNA_def_property_editable_func(prop, "rna_GPencilLayer_active_frame_editable");
	RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, NULL);

	/* Draw Style */
	// TODO: replace these with a "draw type" combo (i.e. strokes only, filled strokes, strokes + fills, volumetric)?
	prop = RNA_def_property(srna, "use_volumetric_strokes", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_LAYER_VOLUMETRIC);
	RNA_def_property_ui_text(prop, "Volumetric Strokes",
	                         "Draw strokes as a series of circular blobs, resulting in a volumetric effect");
	RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");
	
	prop = RNA_def_property(srna, "opacity", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "opacity");
	RNA_def_property_range(prop, 0.0, 1.0f);
	RNA_def_property_ui_text(prop, "Opacity", "Layer Opacity");
	RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");
	
	/* Tint Color */
	prop = RNA_def_property(srna, "tint_color", PROP_FLOAT, PROP_COLOR_GAMMA);
	RNA_def_property_float_sdna(prop, NULL, "tintcolor");
	RNA_def_property_array(prop, 3);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Tint Color", "Color for tinting stroke colors");
	RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");
	
	/* Tint factor */
	prop = RNA_def_property(srna, "tint_factor", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "tintcolor[3]");
	RNA_def_property_range(prop, 0.0, 1.0f);
	RNA_def_property_ui_text(prop, "Tint Factor", "Factor of tinting color");
	RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");
	
	/* Line Thickness change */
	prop = RNA_def_property(srna, "line_change", PROP_INT, PROP_PIXEL);
	RNA_def_property_int_sdna(prop, NULL, "thickness");
	//RNA_def_property_range(prop, 1, 10); /* 10 px limit comes from Windows OpenGL limits for natively-drawn strokes */
	RNA_def_property_int_funcs(prop, NULL, NULL, "rna_GPencilLayer_line_width_range");
	RNA_def_property_ui_text(prop, "Thickness", "Thickness change to apply to current strokes (in pixels)");
	RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");
	
	/* Onion-Skinning */
	prop = RNA_def_property(srna, "use_onion_skinning", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_LAYER_ONIONSKIN);
	RNA_def_property_ui_text(prop, "Onion Skinning", "Ghost frames on either side of frame");
	RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_onion_skinning_update");
	
	prop = RNA_def_property(srna, "ghost_before_range", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "gstep");
	RNA_def_property_range(prop, -1, 120);
	RNA_def_property_ui_text(prop, "Frames Before",
	                         "Maximum number of frames to show before current frame "
	                         "(0 = show only the previous sketch, -1 = don't show any frames before current)");
	RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");
	
	prop = RNA_def_property(srna, "ghost_after_range", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "gstep_next");
	RNA_def_property_range(prop, -1, 120);
	RNA_def_property_ui_text(prop, "Frames After",
	                         "Maximum number of frames to show after current frame "
	                         "(0 = show only the next sketch, -1 = don't show any frames after current)");
	RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");
	
	prop = RNA_def_property(srna, "use_ghost_custom_colors", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_LAYER_GHOST_PREVCOL | GP_LAYER_GHOST_NEXTCOL);
	RNA_def_property_ui_text(prop, "Use Custom Ghost Colors", "Use custom colors for ghost frames");
	RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");
	
	prop = RNA_def_property(srna, "before_color", PROP_FLOAT, PROP_COLOR_GAMMA);
	RNA_def_property_float_sdna(prop, NULL, "gcolor_prev");
	RNA_def_property_array(prop, 3);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Before Color", "Base color for ghosts before the active frame");
	RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");
	
	prop = RNA_def_property(srna, "after_color", PROP_FLOAT, PROP_COLOR_GAMMA);
	RNA_def_property_float_sdna(prop, NULL, "gcolor_next");
	RNA_def_property_array(prop, 3);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "After Color", "Base color for ghosts after the active frame");
	RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");
	
	prop = RNA_def_property(srna, "use_ghosts_always", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_LAYER_GHOST_ALWAYS);
	RNA_def_property_ui_text(prop, "Always Show Ghosts",
	                         "Ghosts are shown in renders and animation playback. Useful for special effects (e.g. motion blur)");
	RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");
	
	
	/* Flags */
	prop = RNA_def_property(srna, "hide", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_LAYER_HIDE);
	RNA_def_property_ui_icon(prop, ICON_RESTRICT_VIEW_OFF, 1);
	RNA_def_property_ui_text(prop, "Hide", "Set layer Visibility");
	RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");
	
	prop = RNA_def_property(srna, "lock", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_LAYER_LOCKED);
	RNA_def_property_ui_icon(prop, ICON_UNLOCKED, 1);
	RNA_def_property_ui_text(prop, "Locked", "Protect layer from further editing and/or frame changes");
	RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");
	
	prop = RNA_def_property(srna, "lock_frame", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_LAYER_FRAMELOCK);
	RNA_def_property_ui_icon(prop, ICON_UNLOCKED, 1);
	RNA_def_property_ui_text(prop, "Frame Locked", "Lock current frame displayed by layer");
	RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

	/* Unlock colors */
	prop = RNA_def_property(srna, "unlock_color", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_LAYER_UNLOCK_COLOR);
	RNA_def_property_ui_icon(prop, ICON_RESTRICT_COLOR_OFF, 1);
	RNA_def_property_ui_text(prop, "Unlock Color",
	                         "Unprotect selected colors from further editing and/or frame changes");
	RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, NULL);


	/* expose as layers.active */
#if 0
	prop = RNA_def_property(srna, "active", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_LAYER_ACTIVE);
	RNA_def_property_boolean_funcs(prop, NULL, "rna_GPencilLayer_active_set");
	RNA_def_property_ui_text(prop, "Active", "Set active layer for editing");
	RNA_def_property_update(prop, NC_GPENCIL | ND_DATA | NA_SELECTED, NULL);
#endif

	prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_LAYER_SELECT);
	RNA_def_property_ui_text(prop, "Select", "Layer is selected for editing in the Dope Sheet");
	RNA_def_property_update(prop, NC_GPENCIL | ND_DATA | NA_SELECTED, "rna_GPencil_update");
	
	/* XXX keep this option? */
	prop = RNA_def_property(srna, "show_points", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_LAYER_DRAWDEBUG);
	RNA_def_property_ui_text(prop, "Show Points", "Draw the points which make up the strokes (for debugging purposes)");
	RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");
	
	/* X-Ray */
	prop = RNA_def_property(srna, "show_x_ray", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", GP_LAYER_NO_XRAY);
	RNA_def_property_ui_text(prop, "X Ray", "Make the layer draw in front of objects");
	RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");
	
	/* Parent object */
	prop = RNA_def_property(srna, "parent", PROP_POINTER, PROP_NONE);
	RNA_def_property_pointer_funcs(prop, NULL, "rna_GPencilLayer_parent_set", NULL, NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_SELF_CHECK); 
	RNA_def_property_ui_text(prop, "Parent", "Parent Object");
	RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

	/* parent type */
	prop = RNA_def_property(srna, "parent_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "partype");
	RNA_def_property_enum_items(prop, parent_type_items);
	RNA_def_property_enum_funcs(prop, NULL, "rna_GPencilLayer_parent_type_set", "rna_Object_parent_type_itemf");
	RNA_def_property_ui_text(prop, "Parent Type", "Type of parent relation");
	RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

	/* parent bone */
	prop = RNA_def_property(srna, "parent_bone", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "parsubstr");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_GPencilLayer_parent_bone_set");
	RNA_def_property_ui_text(prop, "Parent Bone", "Name of parent bone in case of a bone parenting relation");
	RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

	/* matrix */
	prop = RNA_def_property(srna, "matrix_inverse", PROP_FLOAT, PROP_MATRIX);
	RNA_def_property_float_sdna(prop, NULL, "inverse");
	RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_ui_text(prop, "Inverse Matrix", "Parent inverse transformation matrix");
	RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

	/* read only parented flag */
	prop = RNA_def_property(srna, "is_parented", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_GPencilLayer_is_parented_get", NULL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Is Parented", "True when the layer parent object is set");

	/* Layers API */
	func = RNA_def_function(srna, "clear", "rna_GPencil_layer_clear");
	RNA_def_function_ui_description(func, "Remove all the grease pencil layer data");
}

static void rna_def_gpencil_layers_api(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	PropertyRNA *prop;

	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "GreasePencilLayers");
	srna = RNA_def_struct(brna, "GreasePencilLayers", NULL);
	RNA_def_struct_sdna(srna, "bGPdata");
	RNA_def_struct_ui_text(srna, "Grease Pencil Layers", "Collection of grease pencil layers");

	func = RNA_def_function(srna, "new", "rna_GPencil_layer_new");
	RNA_def_function_ui_description(func, "Add a new grease pencil layer");
	parm = RNA_def_string(func, "name", "GPencilLayer", MAX_NAME, "Name", "Name of the layer");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	RNA_def_boolean(func, "set_active", true, "Set Active", "Set the newly created layer to the active layer");
	parm = RNA_def_pointer(func, "layer", "GPencilLayer", "", "The newly created layer");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "remove", "rna_GPencil_layer_remove");
	RNA_def_function_ui_description(func, "Remove a grease pencil layer");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "layer", "GPencilLayer", "", "The layer to remove");
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
	RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, 0);

	prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "GPencilLayer");
	RNA_def_property_pointer_funcs(prop, "rna_GPencil_active_layer_get", "rna_GPencil_active_layer_set", NULL, NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Active Layer", "Active grease pencil layer");
	RNA_def_property_update(prop, NC_GPENCIL | ND_DATA | NA_SELECTED, NULL);
	
	prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_funcs(prop,
	                           "rna_GPencil_active_layer_index_get", 
	                           "rna_GPencil_active_layer_index_set", 
	                           "rna_GPencil_active_layer_index_range");
	RNA_def_property_ui_text(prop, "Active Layer Index", "Index of active grease pencil layer");
	RNA_def_property_update(prop, NC_GPENCIL | ND_DATA | NA_SELECTED, NULL);
}

static void rna_def_gpencil_palettecolor(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "GPencilPaletteColor", NULL);
	RNA_def_struct_sdna(srna, "bGPDpalettecolor");
	RNA_def_struct_ui_text(srna, "Grease Pencil Palette color", "Collection of related colors");
	RNA_def_struct_path_func(srna, "rna_GPencilPalette_color_path");

	/* Stroke Drawing Color */
	prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR_GAMMA);
	RNA_def_property_float_sdna(prop, NULL, "color");
	RNA_def_property_array(prop, 3);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Color", "Color for strokes");
	RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

	prop = RNA_def_property(srna, "alpha", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "color[3]");
	RNA_def_property_range(prop, 0.0, 1.0f);
	RNA_def_property_ui_text(prop, "Opacity", "Color Opacity");
	RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

	/* Name */
	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "info");
	RNA_def_property_ui_text(prop, "Name", "Color name");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_GPencilPaletteColor_info_set");
	RNA_def_struct_name_property(srna, prop);
	RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

	/* Fill Drawing Color */
	prop = RNA_def_property(srna, "fill_color", PROP_FLOAT, PROP_COLOR_GAMMA);
	RNA_def_property_float_sdna(prop, NULL, "fill");
	RNA_def_property_array(prop, 3);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Fill Color", "Color for filling region bounded by each stroke");
	RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

	/* Fill alpha */
	prop = RNA_def_property(srna, "fill_alpha", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "fill[3]");
	RNA_def_property_range(prop, 0.0, 1.0f);
	RNA_def_property_ui_text(prop, "Fill Opacity", "Opacity for filling region bounded by each stroke");
	RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

	/* Flags */
	prop = RNA_def_property(srna, "hide", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PC_COLOR_HIDE);
	RNA_def_property_ui_icon(prop, ICON_RESTRICT_VIEW_OFF, 1);
	RNA_def_property_ui_text(prop, "Hide", "Set color Visibility");
	RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

	prop = RNA_def_property(srna, "lock", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PC_COLOR_LOCKED);
	RNA_def_property_ui_icon(prop, ICON_UNLOCKED, 1);
	RNA_def_property_ui_text(prop, "Locked", "Protect color from further editing and/or frame changes");
	RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

	prop = RNA_def_property(srna, "ghost", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PC_COLOR_ONIONSKIN);
	RNA_def_property_ui_icon(prop, ICON_GHOST_ENABLED, 0);
	RNA_def_property_ui_text(prop, "Show in Ghosts", "Display strokes using this color when showing onion skins");
	RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

	/* Draw Style */
	prop = RNA_def_property(srna, "use_volumetric_strokes", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PC_COLOR_VOLUMETRIC);
	RNA_def_property_ui_text(prop, "Volumetric Strokes", "Draw strokes as a series of circular blobs, resulting in "
	                         "a volumetric effect");
	RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

	/* Use High quality fill */
	prop = RNA_def_property(srna, "use_hq_fill", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", PC_COLOR_HQ_FILL);
	RNA_def_property_ui_text(prop, "High Quality Fill", "Fill strokes using high quality to avoid glitches "
	                         "(slower fps during animation play)");
	RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

	/* Read-only state props (for simpler UI code) */
	prop = RNA_def_property(srna, "is_stroke_visible", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_GPencilPaletteColor_is_stroke_visible_get", NULL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Is Stroke Visible", "True when opacity of stroke is set high enough to be visible");

	prop = RNA_def_property(srna, "is_fill_visible", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_funcs(prop, "rna_GPencilPaletteColor_is_fill_visible_get", NULL);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Is Fill Visible", "True when opacity of fill is set high enough to be visible");
}

/* palette colors api */
static void rna_def_gpencil_palettecolors_api(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	PropertyRNA *prop;

	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "GPencilPaletteColors");
	srna = RNA_def_struct(brna, "GPencilPaletteColors", NULL);
	RNA_def_struct_sdna(srna, "bGPDpalette");
	RNA_def_struct_ui_text(srna, "Palette colors", "Collection of palette colors");

	func = RNA_def_function(srna, "new", "rna_GPencilPalette_color_new");
	RNA_def_function_ui_description(func, "Add a new color to the palette");
	parm = RNA_def_pointer(func, "color", "GPencilPaletteColor", "", "The newly created color");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "remove", "rna_GPencilPalette_color_remove");
	RNA_def_function_ui_description(func, "Remove a color from the palette");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "color", "GPencilPaletteColor", "", "The color to remove");
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
	RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, 0);

	prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "GPencilPaletteColor");
	RNA_def_property_pointer_funcs(prop, "rna_GPencilPalette_active_color_get", "rna_GPencilPalette_active_color_set", NULL, NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Active Palette Color", "Current active color");

	prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_funcs(prop,
		"rna_GPencilPaletteColor_index_get",
		"rna_GPencilPaletteColor_index_set",
		"rna_GPencilPaletteColor_index_range");
	RNA_def_property_ui_text(prop, "Active color Index", "Index of active palette color");
}

static void rna_def_gpencil_palette(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "GPencilPalette", NULL);
	RNA_def_struct_sdna(srna, "bGPDpalette");
	RNA_def_struct_ui_text(srna, "Grease Pencil Palette", "Collection of related palettes");
	RNA_def_struct_path_func(srna, "rna_GPencilPalette_path");
	RNA_def_struct_ui_icon(srna, ICON_COLOR);

	/* Name */
	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_string_sdna(prop, NULL, "info");
	RNA_def_property_ui_text(prop, "Name", "Palette name");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_GPencilPalette_info_set");
	RNA_def_struct_name_property(srna, prop);
	RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

	/* Colors */
	prop = RNA_def_property(srna, "colors", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "colors", NULL);
	RNA_def_property_struct_type(prop, "GPencilPaletteColor");
	RNA_def_property_ui_text(prop, "Colors", "Colors of the palette");
	rna_def_gpencil_palettecolors_api(brna, prop);

}

static void rna_def_gpencil_palettes_api(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	PropertyRNA *prop;

	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "GreasePencilPalettes");
	srna = RNA_def_struct(brna, "GreasePencilPalettes", NULL);
	RNA_def_struct_sdna(srna, "bGPdata");
	RNA_def_struct_ui_text(srna, "Grease Pencil Palettes", "Collection of grease pencil palettes");

	func = RNA_def_function(srna, "new", "rna_GPencil_palette_new");
	RNA_def_function_ui_description(func, "Add a new grease pencil palette");
	parm = RNA_def_string(func, "name", "GPencilPalette", MAX_NAME, "Name", "Name of the palette");
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	RNA_def_boolean(func, "set_active", true, "Set Active", "Activate the newly created palette");
	parm = RNA_def_pointer(func, "palette", "GPencilPalette", "", "The newly created palette");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "remove", "rna_GPencil_palette_remove");
	RNA_def_function_ui_description(func, "Remove a grease pencil palette");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "palette", "GPencilPalette", "", "The palette to remove");
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
	RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, 0);

	prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "GPencilPalette");
	RNA_def_property_pointer_funcs(prop, "rna_GPencil_active_palette_get", "rna_GPencil_active_palette_set",
	                               NULL, NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Active Palette", "Current active palette");

	prop = RNA_def_property(srna, "active_index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_funcs(prop,
		"rna_GPencilPalette_index_get",
		"rna_GPencilPalette_index_set",
		"rna_GPencilPalette_index_range");
	RNA_def_property_ui_text(prop, "Active Palette Index", "Index of active palette");
}

static void rna_def_gpencil_data(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;
	FunctionRNA *func;

	srna = RNA_def_struct(brna, "GreasePencil", "ID");
	RNA_def_struct_sdna(srna, "bGPdata");
	RNA_def_struct_ui_text(srna, "Grease Pencil", "Freehand annotation sketchbook");
	RNA_def_struct_ui_icon(srna, ICON_GREASEPENCIL);
	
	/* Layers */
	prop = RNA_def_property(srna, "layers", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "layers", NULL);
	RNA_def_property_struct_type(prop, "GPencilLayer");
	RNA_def_property_ui_text(prop, "Layers", "");
	rna_def_gpencil_layers_api(brna, prop);
	
	/* Palettes */
	prop = RNA_def_property(srna, "palettes", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "palettes", NULL);
	RNA_def_property_struct_type(prop, "GPencilPalette");
	RNA_def_property_ui_text(prop, "Palettes", "");
	rna_def_gpencil_palettes_api(brna, prop);

	/* Animation Data */
	rna_def_animdata_common(srna);
	
	/* Flags */
	prop = RNA_def_property(srna, "use_stroke_edit_mode", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_DATA_STROKE_EDITMODE);
	RNA_def_property_ui_text(prop, "Stroke Edit Mode", "Edit Grease Pencil strokes instead of viewport data");
	RNA_def_property_update(prop, NC_GPENCIL | ND_DATA | ND_GPENCIL_EDITMODE, "rna_GPencil_editmode_update");
	
	prop = RNA_def_property(srna, "use_onion_skinning", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_DATA_SHOW_ONIONSKINS);
	RNA_def_property_boolean_funcs(prop, NULL, "rna_GPencil_use_onion_skinning_set");
	RNA_def_property_ui_text(prop, "Onion Skins", 
	                         "Show ghosts of the frames before and after the current frame, toggle to enable on active layer or disable all");
	RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, NULL);
	
	prop = RNA_def_property(srna, "show_stroke_direction", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", GP_DATA_SHOW_DIRECTION);
	RNA_def_property_ui_text(prop, "Show Direction", "Show stroke drawing direction with a bigger green dot (start) "
	                         "and smaller red dot (end) points");
	RNA_def_property_update(prop, NC_GPENCIL | ND_DATA, "rna_GPencil_update");

	/* API Functions */
	func = RNA_def_function(srna, "clear", "rna_GPencil_clear");
	RNA_def_function_ui_description(func, "Remove all the grease pencil data");
}

/* --- */

void RNA_def_gpencil(BlenderRNA *brna)
{
	rna_def_gpencil_data(brna);

	rna_def_gpencil_layer(brna);
	rna_def_gpencil_frame(brna);
	rna_def_gpencil_triangle(brna);
	rna_def_gpencil_stroke(brna);
	rna_def_gpencil_stroke_point(brna);

	rna_def_gpencil_palette(brna);
	rna_def_gpencil_palettecolor(brna);
}

#endif
