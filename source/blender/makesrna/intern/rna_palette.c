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

#include "rna_internal.h"

#include "WM_types.h"

#ifdef RNA_RUNTIME

#include "DNA_brush_types.h"

#else


static void rna_def_palettecolor(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "PaletteColor", NULL);
	RNA_def_struct_ui_text(srna, "Palette Color", "");

	prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR_GAMMA);
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_float_sdna(prop, NULL, "rgb");
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
	RNA_def_property_ui_text(prop, "Palette Color", "Colors that are part of this palette");
	RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, NULL);
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
