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

#ifdef RNA_RUNTIME

#include "MEM_guardedalloc.h"

#include "BKE_depsgraph.h"
#include "BKE_main.h"

#include "WM_api.h"
#include "WM_types.h"

#else

static void rna_def_probe(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "Probe", "ID");
	RNA_def_struct_ui_text(srna, "Probe", "Probe data-block for lighting capture objects");
	RNA_def_struct_ui_icon(srna, ICON_RADIO);

	// prop = RNA_def_property(srna, "influence", PROP_FLOAT, PROP_NONE);
	// RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
	// RNA_def_property_range(prop, 0.0f, 1.0f);
	// RNA_def_property_ui_text(prop, "Maximum Volume", "Maximum volume, no matter how near the object is");
	/* RNA_def_property_float_funcs(prop, NULL, "rna_Speaker_volume_max_set", NULL); */
	/* RNA_def_property_update(prop, 0, "rna_Speaker_update"); */

	/* common */
	rna_def_animdata_common(srna);
}


void RNA_def_probe(BlenderRNA *brna)
{
	rna_def_probe(brna);
}

#endif

