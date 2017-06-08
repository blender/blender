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
 * Contributor(s): Blender Foundation.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_probe.c
 *  \ingroup RNA
 */

#include <stdlib.h>

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "DNA_probe_types.h"

#include "WM_types.h"

#ifdef RNA_RUNTIME

#include "MEM_guardedalloc.h"

#include "BKE_main.h"

#include "WM_api.h"
#include "WM_types.h"

#else

static EnumPropertyItem probe_type_items[] = {
	{PROBE_CUBE, "CUBE", ICON_NONE, "Cubemap", ""},
	// {PROBE_PLANAR, "PLANAR", ICON_NONE, "Planar", ""},
	// {PROBE_IMAGE, "IMAGE", ICON_NONE, "Image", ""},
	{0, NULL, 0, NULL, NULL}
};

static void rna_def_probe(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "Probe", "ID");
	RNA_def_struct_ui_text(srna, "Probe", "Probe data-block for lighting capture objects");
	RNA_def_struct_ui_icon(srna, ICON_RADIO);

	prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_items(prop, probe_type_items);
	RNA_def_property_ui_text(prop, "Type", "Type of probe");
	RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, NULL);

	prop = RNA_def_property(srna, "clip_start", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "clipsta");
	RNA_def_property_range(prop, 0.0f, 999999.0f);
	RNA_def_property_ui_text(prop, "Probe Clip Start",
	                         "Probe clip start, below which objects will not appear in reflections");
	RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, NULL);

	prop = RNA_def_property(srna, "clip_end", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "clipend");
	RNA_def_property_range(prop, 0.0f, 999999.0f);
	RNA_def_property_ui_text(prop, "Probe Clip End",
	                         "Probe clip end, beyond which objects will not appear in reflections");
	RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, NULL);

	prop = RNA_def_property(srna, "influence_distance", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "distinf");
	RNA_def_property_ui_text(prop, "Influence Distance", "Influence distance of the probe");
	RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, NULL);

	prop = RNA_def_property(srna, "influence_minimum", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_float_sdna(prop, NULL, "mininf");
	RNA_def_property_ui_text(prop, "Influence Min", "Lowest corner of the influence bounding box");
	RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, NULL);

	prop = RNA_def_property(srna, "influence_maximum", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_float_sdna(prop, NULL, "maxinf");
	RNA_def_property_ui_text(prop, "Influence Max", "Highest corner of the influence bounding box");
	RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, NULL);

	prop = RNA_def_property(srna, "falloff", PROP_FLOAT, PROP_FACTOR);
	RNA_def_property_range(prop, 0.0f, 1.0f);
	RNA_def_property_ui_text(prop, "Falloff", "Control how fast the probe influence decreases");
	RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, NULL);

	prop = RNA_def_property(srna, "parallax_distance", PROP_FLOAT, PROP_DISTANCE);
	RNA_def_property_float_sdna(prop, NULL, "distpar");
	RNA_def_property_ui_text(prop, "Parallax Radius", "Lowest corner of the parallax bounding box");
	RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, NULL);

	prop = RNA_def_property(srna, "parallax_minimum", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_float_sdna(prop, NULL, "minpar");
	RNA_def_property_ui_text(prop, "Parallax Min", "Lowest corner of the parallax bounding box");
	RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, NULL);

	prop = RNA_def_property(srna, "parallax_maximum", PROP_FLOAT, PROP_TRANSLATION);
	RNA_def_property_float_sdna(prop, NULL, "maxpar");
	RNA_def_property_ui_text(prop, "Parallax Max", "Highest corner of the parallax bounding box");
	RNA_def_property_update(prop, NC_MATERIAL | ND_SHADING, NULL);

	/* common */
	rna_def_animdata_common(srna);
}


void RNA_def_probe(BlenderRNA *brna)
{
	rna_def_probe(brna);
}

#endif

