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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 *
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_animation_api.c
 *  \ingroup RNA
 */


#include <stdlib.h>
#include <stdio.h>

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "rna_internal.h"  /* own include */

#ifdef RNA_RUNTIME

#include "BKE_context.h"
#include "BKE_report.h"

#include "ED_keyframing.h"

static void rna_KeyingSet_context_refresh(KeyingSet *ks, bContext *C, ReportList *reports)
{
	/* TODO: enable access to providing a list of overrides (dsources)? */
	int success = ANIM_validate_keyingset(C, NULL, ks);
	
	if (success != 0) {
		switch (success) {
			case MODIFYKEY_INVALID_CONTEXT:
				BKE_report(reports, RPT_ERROR, "Invalid context for keying set");
				break;
				
			case MODIFYKEY_MISSING_TYPEINFO:
				BKE_report(reports, RPT_ERROR, "Incomplete built-in keying set, appears to be missing type info");
				break;
		}
	}
}

#else

void RNA_api_keyingset(StructRNA *srna)
{
	FunctionRNA *func;
	/*PropertyRNA *parm; */
	
	/* validate relative Keying Set (used to ensure paths are ok for context) */
	func = RNA_def_function(srna, "refresh", "rna_KeyingSet_context_refresh");
	RNA_def_function_ui_description(func,
	                                "Refresh Keying Set to ensure that it is valid for the current context "
	                                "(call before each use of one)");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
}

#endif
