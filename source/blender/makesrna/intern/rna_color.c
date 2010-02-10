/**
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Contributor(s): Blender Foundation (2008).
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <stdio.h>

#include "RNA_define.h"
#include "RNA_types.h"

#include "DNA_color_types.h"

#ifdef RNA_RUNTIME

#include "BKE_colortools.h"

static int rna_CurveMapping_curves_length(PointerRNA *ptr)
{
	CurveMapping *cumap= (CurveMapping*)ptr->data;
	int len;

	for(len=0; len<CM_TOT; len++)
		if(!cumap->cm[len].curve)
			break;
	
	return len;
}

static void rna_CurveMapping_curves_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
	CurveMapping *cumap= (CurveMapping*)ptr->data;

	rna_iterator_array_begin(iter, cumap->cm, sizeof(CurveMap), rna_CurveMapping_curves_length(ptr), 0, NULL);
}

static void rna_CurveMapping_clip_set(PointerRNA *ptr, int value)
{
	CurveMapping *cumap= (CurveMapping*)ptr->data;

	if(value) cumap->flag |= CUMA_DO_CLIP;
	else cumap->flag &= ~CUMA_DO_CLIP;

	curvemapping_changed(cumap, 0);
}

static void rna_CurveMapping_black_level_set(PointerRNA *ptr, const float *values)
{
	CurveMapping *cumap= (CurveMapping*)ptr->data;
	cumap->black[0]= values[0];
	cumap->black[1]= values[1];
	cumap->black[2]= values[2];
	curvemapping_set_black_white(cumap, NULL, NULL);
}

static void rna_CurveMapping_white_level_set(PointerRNA *ptr, const float *values)
{
	CurveMapping *cumap= (CurveMapping*)ptr->data;
	cumap->white[0]= values[0];
	cumap->white[1]= values[1];
	cumap->white[2]= values[2];
	curvemapping_set_black_white(cumap, NULL, NULL);
}

static void rna_CurveMapping_clipminx_range(PointerRNA *ptr, float *min, float *max)
{
	CurveMapping *cumap= (CurveMapping*)ptr->data;

	*min= -100.0f;
	*max= cumap->clipr.xmax;
}

static void rna_CurveMapping_clipminy_range(PointerRNA *ptr, float *min, float *max)
{
	CurveMapping *cumap= (CurveMapping*)ptr->data;

	*min= -100.0f;
	*max= cumap->clipr.ymax;
}

static void rna_CurveMapping_clipmaxx_range(PointerRNA *ptr, float *min, float *max)
{
	CurveMapping *cumap= (CurveMapping*)ptr->data;

	*min= cumap->clipr.xmin;
	*max= 100.0f;
}

static void rna_CurveMapping_clipmaxy_range(PointerRNA *ptr, float *min, float *max)
{
	CurveMapping *cumap= (CurveMapping*)ptr->data;

	*min= cumap->clipr.ymin;
	*max= 100.0f;
}

#else

static void rna_def_curvemappoint(BlenderRNA *brna)
{
	StructRNA *srna;
    PropertyRNA *prop;
	static EnumPropertyItem prop_handle_type_items[] = {
        {0, "AUTO", 0, "Auto Handle", ""},
        {CUMA_VECTOR, "VECTOR", 0, "Vector Handle", ""},
		{0, NULL, 0, NULL, NULL}
    };

	srna= RNA_def_struct(brna, "CurveMapPoint", NULL);
	RNA_def_struct_ui_text(srna, "CurveMapPoint", "Point of a curve used for a curve mapping");

	/* not editable for now, need to have CurveMapping to do curvemapping_changed */

	prop= RNA_def_property(srna, "location", PROP_FLOAT, PROP_XYZ);
	RNA_def_property_float_sdna(prop, NULL, "x");
	RNA_def_property_array(prop, 2);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Location", "X/Y coordinates of the curve point");

	prop= RNA_def_property(srna, "handle_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "flag");
	RNA_def_property_enum_items(prop, prop_handle_type_items);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Handle Type", "Curve interpolation at this point: bezier or vector");

	prop= RNA_def_property(srna, "selected", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CUMA_SELECT);
	RNA_def_property_ui_text(prop, "Selected", "Selection state of the curve point");
}

static void rna_def_curvemap(BlenderRNA *brna)
{
	StructRNA *srna;
    PropertyRNA *prop;
	static EnumPropertyItem prop_extend_items[] = {
        {0, "HORIZONTAL", 0, "Horizontal", ""},
        {CUMA_EXTEND_EXTRAPOLATE, "EXTRAPOLATED", 0, "Extrapolated", ""},
		{0, NULL, 0, NULL, NULL}
    };

	srna= RNA_def_struct(brna, "CurveMap", NULL);
	RNA_def_struct_ui_text(srna, "CurveMap", "Curve in a curve mapping");

	/* not editable for now, need to have CurveMapping to do curvemapping_changed */

	prop= RNA_def_property(srna, "extend", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_bitflag_sdna(prop, NULL, "flag");
	RNA_def_property_enum_items(prop, prop_extend_items);
	RNA_def_property_clear_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Extend", "Extrapolate the curve or extend it horizontally");

	prop= RNA_def_property(srna, "points", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_sdna(prop, NULL, "curve", "totpoint");
	RNA_def_property_struct_type(prop, "CurveMapPoint");
	RNA_def_property_ui_text(prop, "Points", "");
}

static void rna_def_curvemapping(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna= RNA_def_struct(brna, "CurveMapping", NULL);
	RNA_def_struct_ui_text(srna, "CurveMapping", "Curve mapping to map color, vector and scalar values to other values using a user defined curve");
	
	prop= RNA_def_property(srna, "clip", PROP_BOOLEAN, PROP_NONE);
	RNA_def_property_boolean_sdna(prop, NULL, "flag", CUMA_DO_CLIP);
	RNA_def_property_ui_text(prop, "Clip", "Force the curve view to fit a defined boundary");
	RNA_def_property_boolean_funcs(prop, NULL, "rna_CurveMapping_clip_set");

	prop= RNA_def_property(srna, "clip_min_x", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "clipr.xmin");
	RNA_def_property_range(prop, -100.0f, 100.0f);
	RNA_def_property_ui_text(prop, "Clip Min X", "");
	RNA_def_property_float_funcs(prop, NULL, NULL, "rna_CurveMapping_clipminx_range");

	prop= RNA_def_property(srna, "clip_min_y", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "clipr.ymin");
	RNA_def_property_range(prop, -100.0f, 100.0f);
	RNA_def_property_ui_text(prop, "Clip Min Y", "");
	RNA_def_property_float_funcs(prop, NULL, NULL, "rna_CurveMapping_clipminy_range");

	prop= RNA_def_property(srna, "clip_max_x", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "clipr.xmax");
	RNA_def_property_range(prop, -100.0f, 100.0f);
	RNA_def_property_ui_text(prop, "Clip Max X", "");
	RNA_def_property_float_funcs(prop, NULL, NULL, "rna_CurveMapping_clipmaxx_range");

	prop= RNA_def_property(srna, "clip_max_y", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "clipr.ymax");
	RNA_def_property_range(prop, -100.0f, 100.0f);
	RNA_def_property_ui_text(prop, "Clip Max Y", "");
	RNA_def_property_float_funcs(prop, NULL, NULL, "rna_CurveMapping_clipmaxy_range");

	prop= RNA_def_property(srna, "curves", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_collection_funcs(prop, "rna_CurveMapping_curves_begin", "rna_iterator_array_next", "rna_iterator_array_end", "rna_iterator_array_get", "rna_CurveMapping_curves_length", 0, 0);
	RNA_def_property_struct_type(prop, "CurveMap");
	RNA_def_property_ui_text(prop, "Curves", "");

	prop= RNA_def_property(srna, "black_level", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, NULL, "black");
	RNA_def_property_range(prop, -1000.0f, 1000.0f);
	RNA_def_property_ui_text(prop, "Black Level", "For RGB curves, the color that black is mapped to");
	RNA_def_property_float_funcs(prop, NULL, "rna_CurveMapping_black_level_set", NULL);

	prop= RNA_def_property(srna, "white_level", PROP_FLOAT, PROP_COLOR);
	RNA_def_property_float_sdna(prop, NULL, "white");
	RNA_def_property_range(prop, -1000.0f, 1000.0f);
	RNA_def_property_ui_text(prop, "White Level", "For RGB curves, the color that white is mapped to");
	RNA_def_property_float_funcs(prop, NULL, "rna_CurveMapping_white_level_set", NULL);
}

static void rna_def_histogram(BlenderRNA *brna)
{
	StructRNA *srna;
	
	srna= RNA_def_struct(brna, "Histogram", NULL);
	RNA_def_struct_ui_text(srna, "Histogram", "Statistical view of the levels of color in an image");
	
}

void RNA_def_color(BlenderRNA *brna)
{
	rna_def_curvemappoint(brna);
	rna_def_curvemap(brna);
	rna_def_curvemapping(brna);
	rna_def_histogram(brna);
}

#endif

