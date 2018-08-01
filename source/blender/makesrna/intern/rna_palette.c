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

/** \file blender/makesrna/intern/rna_palette.c
 *  \ingroup RNA
 */

#include <stdlib.h>

#include "BLI_utildefines.h"

#include "RNA_define.h"
#include "RNA_access.h"

#include "rna_internal.h"

#include "WM_types.h"

#ifdef RNA_RUNTIME

#include "DNA_brush_types.h"

#include "BKE_paint.h"
#include "BKE_report.h"
static PaletteColor *rna_Palette_color_new(Palette *palette)
{
	PaletteColor *color = BKE_palette_color_add(palette);
	return color;
}

static void rna_Palette_color_remove(Palette *palette, ReportList *reports, PointerRNA *color_ptr)
{
	PaletteColor *color = color_ptr->data;

	if (BLI_findindex(&palette->colors, color) == -1) {
		BKE_reportf(reports, RPT_ERROR, "Palette '%s' does not contain color given", palette->id.name + 2);
		return;
	}

	BKE_palette_color_remove(palette, color);

	RNA_POINTER_INVALIDATE(color_ptr);
}

static void rna_Palette_color_clear(Palette *palette)
{
	BKE_palette_clear(palette);
}

static PointerRNA rna_Palette_active_color_get(PointerRNA *ptr)
{
	Palette *palette = ptr->data;
	PaletteColor *color;

	color = BLI_findlink(&palette->colors, palette->active_color);

	if (color)
		return rna_pointer_inherit_refine(ptr, &RNA_PaletteColor, color);

	return rna_pointer_inherit_refine(ptr, NULL, NULL);
}

static void rna_Palette_active_color_set(PointerRNA *ptr, PointerRNA value)
{
	Palette *palette = ptr->data;
	PaletteColor *color = value.data;

	/* -1 is ok for an unset index */
	if (color == NULL)
		palette->active_color = -1;
	else
		palette->active_color = BLI_findindex(&palette->colors, color);
}

#else

/* palette.colors */
static void rna_def_palettecolors(BlenderRNA *brna, PropertyRNA *cprop)
{
	StructRNA *srna;
	PropertyRNA *prop;

	FunctionRNA *func;
	PropertyRNA *parm;

	RNA_def_property_srna(cprop, "PaletteColors");
	srna = RNA_def_struct(brna, "PaletteColors", NULL);
	RNA_def_struct_sdna(srna, "Palette");
	RNA_def_struct_ui_text(srna, "Palette Splines", "Collection of palette colors");

	func = RNA_def_function(srna, "new", "rna_Palette_color_new");
	RNA_def_function_ui_description(func, "Add a new color to the palette");
	parm = RNA_def_pointer(func, "color", "PaletteColor", "", "The newly created color");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "remove", "rna_Palette_color_remove");
	RNA_def_function_ui_description(func, "Remove a color from the palette");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "color", "PaletteColor", "", "The color to remove");
	RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
	RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, 0);

	func = RNA_def_function(srna, "clear", "rna_Palette_color_clear");
	RNA_def_function_ui_description(func, "Remove all colors from the palette");

	prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "PaletteColor");
	RNA_def_property_pointer_funcs(prop, "rna_Palette_active_color_get", "rna_Palette_active_color_set", NULL, NULL);
	RNA_def_property_flag(prop, PROP_EDITABLE);
	RNA_def_property_ui_text(prop, "Active Palette Color", "");
}

static void rna_def_palettecolor(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "PaletteColor", NULL);
	RNA_def_struct_ui_text(srna, "Palette Color", "");

	prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR_GAMMA);
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_float_sdna(prop, NULL, "rgb");
	RNA_def_property_array(prop, 3);
	RNA_def_property_ui_text(prop, "Color", "");
	RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL);

	prop = RNA_def_property(srna, "strength", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_float_sdna(prop, NULL, "value");
	RNA_def_property_ui_text(prop, "Value", "");
	RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL);

	prop = RNA_def_property(srna, "weight", PROP_FLOAT, PROP_NONE);
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_float_sdna(prop, NULL, "value");
	RNA_def_property_ui_text(prop, "Weight", "");
	RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL);

}

static void rna_def_palette(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "Palette", "ID");
	RNA_def_struct_ui_text(srna, "Palette", "");
	RNA_def_struct_ui_icon(srna, ICON_COLOR);

	prop = RNA_def_property(srna, "colors", PROP_COLLECTION, PROP_NONE);
	RNA_def_property_struct_type(prop, "PaletteColor");
	rna_def_palettecolors(brna, prop);

}

void RNA_def_palette(BlenderRNA *brna)
{
	/* *** Non-Animated *** */
	RNA_define_animate_sdna(false);
	rna_def_palettecolor(brna);
	rna_def_palette(brna);
	RNA_define_animate_sdna(true);
}

#endif
