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
 * Contributor(s): Blender Foundation (2010)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_sequencer_api.c
 *  \ingroup RNA
 */



#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "RNA_define.h"
#include "RNA_access.h"
#include "RNA_define.h"

#include "rna_internal.h"

#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#ifdef RNA_RUNTIME

#include "BKE_report.h"
#include "BKE_sequencer.h"

static void rna_Sequence_swap_internal(Sequence *seq_self, ReportList *reports, Sequence *seq_other)
{
	const char *error_msg;
	
	if (seq_swap(seq_self, seq_other, &error_msg) == 0)
		BKE_report(reports, RPT_ERROR, error_msg);
}

#else

void RNA_api_sequence_strip(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	func = RNA_def_function(srna, "getStripElem", "give_stripelem");
	RNA_def_function_ui_description(func, "Return the strip element from a given frame or None");
	parm = RNA_def_int(func, "frame", 0, -MAXFRAME, MAXFRAME, "Frame",
	                  "The frame to get the strip element from", -MAXFRAME, MAXFRAME);
	RNA_def_property_flag(parm, PROP_REQUIRED);
	RNA_def_function_return(func, RNA_def_pointer(func, "elem", "SequenceElement", "",
	                        "strip element of the current frame"));

	func = RNA_def_function(srna, "swap", "rna_Sequence_swap_internal");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	parm = RNA_def_pointer(func, "other", "Sequence", "Other", "");
	RNA_def_property_flag(parm, PROP_REQUIRED|PROP_NEVER_NULL);
}

#endif
