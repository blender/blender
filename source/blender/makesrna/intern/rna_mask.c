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
 * Contributor(s): Blender Foundation,
 *                 Sergey Sharybin
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_mask.c
 *  \ingroup RNA
 */


#include <stdlib.h>
#include <limits.h>

#include "MEM_guardedalloc.h"

#include "BKE_movieclip.h"
#include "BKE_tracking.h"

#include "RNA_define.h"

#include "rna_internal.h"

#include "DNA_mask_types.h"
#include "DNA_object_types.h"	/* SELECT */
#include "DNA_scene_types.h"

#include "WM_types.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#ifdef RNA_RUNTIME

#include "DNA_mask_types.h"

#include "BKE_depsgraph.h"
#include "BKE_mask.h"

#include "RNA_access.h"

#include "WM_api.h"

static void rna_Mask_update_data(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
	Mask *mask = ptr->id.data;

	WM_main_add_notifier(NC_MASK|ND_DATA, mask);
	DAG_id_tag_update( &mask->id, 0);
}

/* note: this function exists only to avoid id refcounting */
static void rna_MaskParent_id_set(PointerRNA *ptr, PointerRNA value)
{
	MaskParent *mpar = (MaskParent*) ptr->data;

	mpar->id = value.data;
}

static StructRNA *rna_MaskParent_id_typef(PointerRNA *ptr)
{
	MaskParent *mpar = (MaskParent*) ptr->data;

	return ID_code_to_RNA_type(mpar->id_type);
}

static void rna_MaskParent_id_type_set(PointerRNA *ptr, int value)
{
	MaskParent *mpar = (MaskParent*) ptr->data;

	/* change ID-type to the new type */
	mpar->id_type = value;

	/* clear the id-block if the type is invalid */
	if ((mpar->id) && (GS(mpar->id->name) != mpar->id_type))
		mpar->id = NULL;
}

static void rna_Mask_layers_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	Mask *mask = (Mask *)ptr->id.data;

	rna_iterator_listbase_begin(iter, &mask->masklayers, NULL);
}

static int rna_Mask_layer_active_index_get(PointerRNA *ptr)
{
	Mask *mask = (Mask *)ptr->id.data;

	return mask->masklay_act;
}

static void rna_Mask_layer_active_index_set(PointerRNA *ptr, int value)
{
	Mask *mask = (Mask *)ptr->id.data;

	mask->masklay_act = value;
}

static void rna_Mask_layer_active_index_range(PointerRNA *ptr, int *min, int *max, int *softmin, int *softmax)
{
	Mask *mask = (Mask *)ptr->id.data;

	*min = 0;
	*max = mask->masklay_tot - 1;
	*max = MAX2(0, *max);

	*softmin = *min;
	*softmax = *max;
}

static char *rna_MaskLayer_path(PointerRNA *ptr)
{
	return BLI_sprintfN("layers[\"%s\"]", ((MaskLayer *)ptr->data)->name);
}

static PointerRNA rna_Mask_layer_active_get(PointerRNA *ptr)
{
	Mask *mask = (Mask *)ptr->id.data;
	MaskLayer *masklay = BKE_mask_layer_active(mask);

	return rna_pointer_inherit_refine(ptr, &RNA_MaskLayer, masklay);
}

static void rna_Mask_layer_active_set(PointerRNA *ptr, PointerRNA value)
{
	Mask *mask = (Mask *)ptr->id.data;
	MaskLayer *masklay = (MaskLayer *)value.data;

	BKE_mask_layer_active_set(mask, masklay);
}

static void rna_MaskLayer_splines_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	MaskLayer *masklay = (MaskLayer *)ptr->data;

	rna_iterator_listbase_begin(iter, &masklay->splines, NULL);
}

void rna_MaskLayer_name_set(PointerRNA *ptr, const char *value)
{
	Mask *mask = (Mask *)ptr->id.data;
	MaskLayer *masklay = (MaskLayer *)ptr->data;

	BLI_strncpy(masklay->name, value, sizeof(masklay->name));

	BKE_mask_layer_unique_name(mask, masklay);
}

static PointerRNA rna_MaskLayer_active_spline_get(PointerRNA *ptr)
{
	MaskLayer *masklay = (MaskLayer *)ptr->data;

	return rna_pointer_inherit_refine(ptr, &RNA_MaskSpline, masklay->act_spline);
}

static void rna_MaskLayer_active_spline_set(PointerRNA *ptr, PointerRNA value)
{
	MaskLayer *masklay = (MaskLayer *)ptr->data;
	MaskSpline *spline = (MaskSpline *)value.data;
	int index = BLI_findindex(&masklay->splines, spline);

	if (index >= 0)
		masklay->act_spline = spline;
	else
		masklay->act_spline = NULL;
}

static PointerRNA rna_MaskLayer_active_spline_point_get(PointerRNA *ptr)
{
	MaskLayer *masklay = (MaskLayer *)ptr->data;

	return rna_pointer_inherit_refine(ptr, &RNA_MaskSplinePoint, masklay->act_point);
}

static void rna_MaskLayer_active_spline_point_set(PointerRNA *ptr, PointerRNA value)
{
	MaskLayer *masklay = (MaskLayer *)ptr->data;
	MaskSpline *spline;
	MaskSplinePoint *point = (MaskSplinePoint *)value.data;

	masklay->act_point = NULL;

	for (spline = masklay->splines.first; spline; spline = spline->next) {
		if (point >= spline->points && point < spline->points + spline->tot_point) {
			masklay->act_point = point;

			break;
		}
	}
}

static void rna_MaskSplinePoint_handle1_get(PointerRNA *ptr, float *values)
{
	MaskSplinePoint *point = (MaskSplinePoint*) ptr->data;
	BezTriple *bezt = &point->bezt;

	values[0] = bezt->vec[0][0];
	values[1] = bezt->vec[0][1];
	values[2] = bezt->vec[0][2];
}

static void rna_MaskSplinePoint_handle1_set(PointerRNA *ptr, const float *values)
{
	MaskSplinePoint *point = (MaskSplinePoint*) ptr->data;
	BezTriple *bezt = &point->bezt;

	bezt->vec[0][0] = values[0];
	bezt->vec[0][1] = values[1];
	bezt->vec[0][2] = values[2];
}

static void rna_MaskSplinePoint_handle2_get(PointerRNA *ptr, float *values)
{
	MaskSplinePoint *point = (MaskSplinePoint*) ptr->data;
	BezTriple *bezt = &point->bezt;

	values[0] = bezt->vec[2][0];
	values[1] = bezt->vec[2][1];
	values[2] = bezt->vec[2][2];
}

static void rna_MaskSplinePoint_handle2_set(PointerRNA *ptr, const float *values)
{
	MaskSplinePoint *point = (MaskSplinePoint*) ptr->data;
	BezTriple *bezt = &point->bezt;

	bezt->vec[2][0] = values[0];
	bezt->vec[2][1] = values[1];
	bezt->vec[2][2] = values[2];
}

static void rna_MaskSplinePoint_ctrlpoint_get(PointerRNA *ptr, float *values)
{
	MaskSplinePoint *point = (MaskSplinePoint*) ptr->data;
	BezTriple *bezt = &point->bezt;

	values[0] = bezt->vec[1][0];
	values[1] = bezt->vec[1][1];
	values[2] = bezt->vec[1][2];
}

static void rna_MaskSplinePoint_ctrlpoint_set(PointerRNA *ptr, const float *values)
{
	MaskSplinePoint *point = (MaskSplinePoint*) ptr->data;
	BezTriple *bezt = &point->bezt;

	bezt->vec[1][0] = values[0];
	bezt->vec[1][1] = values[1];
	bezt->vec[1][2] = values[2];
}

static int rna_MaskSplinePoint_handle_type_get(PointerRNA *ptr)
{
	MaskSplinePoint *point = (MaskSplinePoint*) ptr->data;
	BezTriple *bezt = &point->bezt;

	return bezt->h1;
}

static void rna_MaskSplinePoint_handle_type_set(PointerRNA *ptr, int value)
{
	MaskSplinePoint *point = (MaskSplinePoint*) ptr->data;
	BezTriple *bezt = &point->bezt;

	bezt->h1 = bezt->h2 = value;
}

/* ** API **  */

static MaskLayer *rna_Mask_layer_new(Mask *mask, const char *name)
{
	MaskLayer *masklay = BKE_mask_layer_new(mask, name);

	WM_main_add_notifier(NC_MASK|NA_EDITED, mask);

	return masklay;
}

void rna_Mask_layer_remove(Mask *mask, MaskLayer *masklay)
{
	BKE_mask_layer_remove(mask, masklay);

	WM_main_add_notifier(NC_MASK|NA_EDITED, mask);
}

static void rna_MaskLayer_spline_add(ID *id, MaskLayer *masklay, int number)
{
	Mask *mask = (Mask*) id;
	int i;

	for (i = 0; i < number; i++)
		BKE_mask_spline_add(masklay);

	WM_main_add_notifier(NC_MASK|NA_EDITED, mask);
}

static void rna_Mask_start_frame_set(PointerRNA *ptr, int value)
{
	Mask *data = (Mask *)ptr->data;
	/* MINFRAME not MINAFRAME, since some output formats can't taken negative frames */
	CLAMP(value, MINFRAME, MAXFRAME);
	data->sfra = value;

	if (data->sfra >= data->efra) {
		data->efra = MIN2(data->sfra, MAXFRAME);
	}
}

static void rna_Mask_end_frame_set(PointerRNA *ptr, int value)
{
	Mask *data = (Mask *)ptr->data;
	CLAMP(value, MINFRAME, MAXFRAME);
	data->efra = value;

	if (data->sfra >= data->efra) {
		data->sfra = MAX2(data->efra, MINFRAME);
	}
}

#else

static void rna_def_maskParent(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem mask_id_type_items[] = {
		{ID_MC, "MOVIECLIP", ICON_SEQUENCE, "Movie Clip", ""},
		{0, NULL, 0, NULL, NULL}};

	srna = RNA_def_struct(brna, "MaskParent", NULL);
	RNA_def_struct_ui_text(srna, "Mask Parent", "Parenting settings for masking element");

	/* Target Properties - ID-block to Drive */
	prop = RNA_def_property(srna, "id", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "ID");
	RNA_def_property_flag(prop, PROP_EDITABLE);
	// RNA_def_property_editable_func(prop, "rna_maskSpline_id_editable");
	/* note: custom set function is ONLY to avoid rna setting a user for this. */
	RNA_def_property_pointer_funcs(prop, NULL, "rna_MaskParent_id_set", "rna_MaskParent_id_typef", NULL);
	RNA_def_property_ui_text(prop, "ID", "ID-block to which masking element would be parented to or to it's property");
	RNA_def_property_update(prop, 0, "rna_Mask_update_data");

	prop = RNA_def_property(srna, "id_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "id_type");
	RNA_def_property_enum_items(prop, mask_id_type_items);
	RNA_def_property_enum_default(prop, ID_MC);
	RNA_def_property_enum_funcs(prop, NULL, "rna_MaskParent_id_type_set", NULL);
	//RNA_def_property_editable_func(prop, "rna_MaskParent_id_type_editable");
	RNA_def_property_ui_text(prop, "ID Type", "Type of ID-block that can be used");
	RNA_def_property_update(prop, 0, "rna_Mask_update_data");

	/* parent */
	prop = RNA_def_property(srna, "parent", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Parent", "Name of parent object in specified data block to which parenting happens");
	RNA_def_property_string_maxlength(prop, MAX_ID_NAME - 2);
	RNA_def_property_update(prop, 0, "rna_Mask_update_data");

	/* sub_parent */
	prop = RNA_def_property(srna, "sub_parent", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Sub Parent", "Name of parent sub-object in specified data block to which parenting happens");
	RNA_def_property_string_maxlength(prop, MAX_ID_NAME - 2);
	RNA_def_property_update(prop, 0, "rna_Mask_update_data");
}

static void rna_def_maskSplinePointUW(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "MaskSplinePointUW", NULL);
	RNA_def_struct_ui_text(srna, "Mask Spline UW Point", "Single point in spline segment defining feather");

	/* u */
	prop = RNA_def_property(srna, "u", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "u");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "U", "U coordinate of point along spline segment");
	RNA_def_property_update(prop, 0, "rna_Mask_update_data");

	/* weight */
	prop = RNA_def_property(srna, "weight", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "w");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_text(prop, "Weight", "Weight of feather point");
	RNA_def_property_update(prop, 0, "rna_Mask_update_data");

	/* select */
	prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", SELECT);
	RNA_def_property_ui_text(prop, "Select", "Selection status");
	RNA_def_property_update(prop, 0, "rna_Mask_update_data");
}

static void rna_def_maskSplinePoint(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	static EnumPropertyItem handle_type_items[] = {
		{HD_AUTO, "AUTO", 0, "Auto", ""},
		{HD_VECT, "VECTOR", 0, "Vector", ""},
		{HD_ALIGN, "ALIGNED", 0, "Aligned", ""},
		{0, NULL, 0, NULL, NULL}};

	rna_def_maskSplinePointUW(brna);

	srna = RNA_def_struct(brna, "MaskSplinePoint", NULL);
	RNA_def_struct_ui_text(srna, "Mask Spline Point", "Single point in spline used for defining mask");

	/* Vector values */
	prop = RNA_def_property(srna, "handle_left", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_array(prop, 3);
	RNA_def_property_float_funcs(prop, "rna_MaskSplinePoint_handle1_get", "rna_MaskSplinePoint_handle1_set", NULL);
	RNA_def_property_ui_text(prop, "Handle 1", "Coordinates of the first handle");
	RNA_def_property_update(prop, 0, "rna_Mask_update_data");

	prop = RNA_def_property(srna, "co", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_array(prop, 3);
	RNA_def_property_float_funcs(prop, "rna_MaskSplinePoint_ctrlpoint_get", "rna_MaskSplinePoint_ctrlpoint_set", NULL);
	RNA_def_property_ui_text(prop, "Control Point", "Coordinates of the control point");
	RNA_def_property_update(prop, 0, "rna_Mask_update_data");

	prop = RNA_def_property(srna, "handle_right", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_array(prop, 3);
	RNA_def_property_float_funcs(prop, "rna_MaskSplinePoint_handle2_get", "rna_MaskSplinePoint_handle2_set", NULL);
	RNA_def_property_ui_text(prop, "Handle 2", "Coordinates of the second handle");
	RNA_def_property_update(prop, 0, "rna_Mask_update_data");

	/* handle_type */
	prop = RNA_def_property(srna, "handle_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_funcs(prop, "rna_MaskSplinePoint_handle_type_get", "rna_MaskSplinePoint_handle_type_set", NULL);
	RNA_def_property_enum_items(prop, handle_type_items);
	RNA_def_property_ui_text(prop, "Handle Type", "Handle type");
	RNA_def_property_update(prop, 0, "rna_Mask_update_data");

	/* select */
	prop = RNA_def_property(srna, "select", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "bezt.f1", SELECT);
	RNA_def_property_ui_text(prop, "Select", "Selection status");
	RNA_def_property_update(prop, 0, "rna_Mask_update_data");

	/* parent */
	prop = RNA_def_property(srna, "parent", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "MaskParent");

	/* feather points */
	prop = RNA_def_property(srna, "feather_points", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "MaskSplinePointUW");
	RNA_def_property_collection_sdna(prop, NULL, "uw", "tot_uw");
	RNA_def_property_ui_text(prop, "Feather Points", "Points defining feather");
}

static void rna_def_mask_splines(BlenderRNA *brna)
{
	StructRNA *srna;
	FunctionRNA *func;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "MaskSplines", NULL);
	RNA_def_struct_sdna(srna, "MaskLayer");
	RNA_def_struct_ui_text(srna, "Mask Splines", "Collection of masking splines");

	func = RNA_def_function(srna, "add", "rna_MaskLayer_spline_add");
	RNA_def_function_flag(func, FUNC_USE_SELF_ID);
	RNA_def_function_ui_description(func, "Add a number of splines to mask layer");
	RNA_def_int(func, "count", 1, 0, INT_MAX, "Number", "Number of splines to add to the layer", 0, INT_MAX);

	/* active spline */
	prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "MaskSpline");
	RNA_def_property_pointer_funcs(prop, "rna_MaskLayer_active_spline_get", "rna_MaskLayer_active_spline_set", NULL, NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE|PROP_NEVER_UNLINK);
	RNA_def_property_ui_text(prop, "Active Spline", "Active spline of masking layer");

	/* active point */
	prop = RNA_def_property(srna, "active_point", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "MaskSplinePoint");
	RNA_def_property_pointer_funcs(prop, "rna_MaskLayer_active_spline_point_get", "rna_MaskLayer_active_spline_point_set", NULL, NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE|PROP_NEVER_UNLINK);
	RNA_def_property_ui_text(prop, "Active Spline", "Active spline of masking layer");
}

static void rna_def_maskSpline(BlenderRNA *brna)
{
	static EnumPropertyItem spline_interpolation_items[] = {
		{MASK_SPLINE_INTERP_LINEAR, "LINEAR", 0, "Linear", ""},
		{MASK_SPLINE_INTERP_EASE, "EASE", 0, "Ease", ""},
		{0, NULL, 0, NULL, NULL}
	};

	StructRNA *srna;
	PropertyRNA *prop;

	rna_def_maskSplinePoint(brna);

	srna = RNA_def_struct(brna, "MaskSpline", NULL);
	RNA_def_struct_ui_text(srna, "Mask spline", "Single spline used for defining mask shape");

	/* weight interpolation */
	prop = RNA_def_property(srna, "weight_interpolation", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "weight_interp");
	RNA_def_property_enum_items(prop, spline_interpolation_items);
	RNA_def_property_ui_text(prop, "Weight Interpolation", "The type of weight interpolation for spline");
	RNA_def_property_update(prop, 0, "rna_Mask_update_data");

	/* cyclic */
	prop = RNA_def_property(srna, "use_cyclic", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", MASK_SPLINE_CYCLIC);
	RNA_def_property_ui_text(prop, "Cyclic", "Make this spline a closed loop");
	RNA_def_property_update(prop, 0, "rna_Mask_update_data");
}

static void rna_def_mask_layer(BlenderRNA *brna)
{
	static EnumPropertyItem masklay_blend_mode_items[] = {
		{MASK_BLEND_ADD, "ADD", 0, "Add", ""},
		{MASK_BLEND_SUBTRACT, "SUBTRACT", 0, "Subtract", ""},
		{0, NULL, 0, NULL, NULL}
	};

	StructRNA *srna;
	PropertyRNA *prop;

	rna_def_maskSpline(brna);
	rna_def_mask_splines(brna);

	srna = RNA_def_struct(brna, "MaskLayer", NULL);
	RNA_def_struct_ui_text(srna, "Mask Layer", "Single layer used for masking pixels");
	RNA_def_struct_path_func(srna, "rna_MaskLayer_path");

	/* name */
	prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
	RNA_def_property_ui_text(prop, "Name", "Unique name of layer");
	RNA_def_property_string_funcs(prop, NULL, NULL, "rna_MaskLayer_name_set");
	RNA_def_property_string_maxlength(prop, MAX_ID_NAME - 2);
	RNA_def_property_update(prop, 0, "rna_Mask_update_data");
	RNA_def_struct_name_property(srna, prop);

	/* splines */
	prop = RNA_def_property(srna, "splines", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_funcs(prop, "rna_MaskLayer_splines_begin", "rna_iterator_listbase_next", "rna_iterator_listbase_end", "rna_iterator_listbase_get", 0, 0, 0, 0);
	RNA_def_property_struct_type(prop, "MaskSpline");
	RNA_def_property_ui_text(prop, "Splines", "Collection of splines which defines this layer");
	RNA_def_property_srna(prop, "MaskSplines");

	/* restrict */
	prop = RNA_def_property(srna, "hide", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "restrictflag", MASK_RESTRICT_VIEW);
	RNA_def_property_ui_text(prop, "Restrict View", "Restrict visibility in the viewport");
	RNA_def_property_ui_icon(prop, ICON_RESTRICT_VIEW_OFF, 1);
	RNA_def_property_update(prop, NC_MASK | ND_DRAW, NULL);

	prop = RNA_def_property(srna, "hide_select", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "restrictflag", MASK_RESTRICT_SELECT);
	RNA_def_property_ui_text(prop, "Restrict Select", "Restrict selection in the viewport");
	RNA_def_property_ui_icon(prop, ICON_RESTRICT_SELECT_OFF, 1);
	RNA_def_property_update(prop, NC_MASK | ND_DRAW, NULL);

	prop = RNA_def_property(srna, "hide_render", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "restrictflag", MASK_RESTRICT_RENDER);
	RNA_def_property_ui_text(prop, "Restrict Render", "Restrict renderability");
	RNA_def_property_ui_icon(prop, ICON_RESTRICT_RENDER_OFF, 1);
	RNA_def_property_update(prop, NC_MASK | NA_EDITED, NULL);

	/* render settings */
	prop = RNA_def_property(srna, "alpha", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "alpha");
	RNA_def_property_range(prop, 0.0, 1.0f);
	RNA_def_property_ui_text(prop, "Opacity", "Render Opacity");
	RNA_def_property_update(prop, NC_MASK | NA_EDITED, NULL);

	/* weight interpolation */
	prop = RNA_def_property(srna, "blend", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "blend");
	RNA_def_property_enum_items(prop, masklay_blend_mode_items);
	RNA_def_property_ui_text(prop, "Blend", "Method of blending mask layers");
	RNA_def_property_update(prop, 0, "rna_Mask_update_data");
	RNA_def_property_update(prop, NC_MASK | NA_EDITED, NULL);

	prop = RNA_def_property(srna, "invert", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "blend_flag", MASK_BLENDFLAG_INVERT);
	RNA_def_property_ui_text(prop, "Restrict View", "Invert the mask black/white");
	RNA_def_property_update(prop, NC_MASK | NA_EDITED, NULL);

}

static void rna_def_masklayers(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	PropertyRNA *prop;

	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "MaskLayers");
	srna = RNA_def_struct(brna, "MaskLayers", NULL);
	RNA_def_struct_sdna(srna, "Mask");
	RNA_def_struct_ui_text(srna, "Mask Layers", "Collection of layers used by mask");

	func = RNA_def_function(srna, "new", "rna_Mask_layer_new");
	RNA_def_function_ui_description(func, "Add layer to this mask");
	RNA_def_string(func, "name", "", 0, "Name", "Name of new layer");
	parm = RNA_def_pointer(func, "layer", "MaskLayer", "", "New mask layer");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "remove", "rna_Mask_layer_remove");
	RNA_def_function_ui_description(func, "Remove layer from this mask");
	RNA_def_pointer(func, "layer", "MaskLayer", "", "Shape to be removed");

	/* active layer */
	prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "MaskLayer");
	RNA_def_property_pointer_funcs(prop, "rna_Mask_layer_active_get", "rna_Mask_layer_active_set", NULL, NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE|PROP_NEVER_UNLINK);
	RNA_def_property_ui_text(prop, "Active Shape", "Active layer in this mask");
}

static void rna_def_mask(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	rna_def_mask_layer(brna);

	srna = RNA_def_struct(brna, "Mask", "ID");
	RNA_def_struct_ui_text(srna, "Mask", "Mask datablock defining mask for compositing");
	RNA_def_struct_ui_icon(srna, ICON_MOD_MASK);

	/* mask layers */
	prop = RNA_def_property(srna, "layers", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_funcs(prop, "rna_Mask_layers_begin", "rna_iterator_listbase_next", "rna_iterator_listbase_end", "rna_iterator_listbase_get", 0, 0, 0, 0);
	RNA_def_property_struct_type(prop, "MaskLayer");
	RNA_def_property_ui_text(prop, "Layers", "Collection of layers which defines this mask");
	rna_def_masklayers(brna, prop);

	/* active masklay index */
	prop = RNA_def_property(srna, "active_layer_index", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "masklay_act");
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_int_funcs(prop, "rna_Mask_layer_active_index_get", "rna_Mask_layer_active_index_set", "rna_Mask_layer_active_index_range");
	RNA_def_property_ui_text(prop, "Active Shape Index", "Index of active layer in list of all mask's layers");

	/* frame range */
	prop = RNA_def_property(srna, "frame_start", PROP_INT, PROP_TIME);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_int_sdna(prop, NULL, "sfra");
	RNA_def_property_int_funcs(prop, NULL, "rna_Mask_start_frame_set", NULL);
	RNA_def_property_range(prop, MINFRAME, MAXFRAME);
	RNA_def_property_ui_text(prop, "Start Frame", "First frame of the mask (used for sequencer)");
	RNA_def_property_update(prop, NC_SCENE | ND_FRAME_RANGE, NULL);

	prop = RNA_def_property(srna, "frame_end", PROP_INT, PROP_TIME);
	RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	RNA_def_property_int_sdna(prop, NULL, "efra");
	RNA_def_property_int_funcs(prop, NULL, "rna_Mask_end_frame_set", NULL);
	RNA_def_property_range(prop, MINFRAME, MAXFRAME);
	RNA_def_property_ui_text(prop, "End Frame", "Final frame of the mask (used for sequencer)");
	RNA_def_property_update(prop, NC_SCENE | ND_FRAME_RANGE, NULL);

	/* pointers */
	rna_def_animdata_common(srna);
}

void RNA_def_mask(BlenderRNA *brna)
{
	rna_def_maskParent(brna);
	rna_def_mask(brna);
}

#endif
