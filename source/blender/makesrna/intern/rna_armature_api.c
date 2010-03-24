/**
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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "RNA_define.h"

#ifdef RNA_RUNTIME

#include <stddef.h>

#include "BLI_blenlib.h"

#include "ED_armature.h"

void rna_EditBone_align_roll(EditBone *ebo, float *no)
{
	if(!is_zero_v3(no)) {
		float normal[3];
		copy_v3_v3(normal, no);
		normalize_v3(normal);
		ebo->roll= ED_rollBoneToVector(ebo, normal);
	}
}

#else

void RNA_api_armature_edit_bone(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	func= RNA_def_function(srna, "align_roll", "rna_EditBone_align_roll");
	RNA_def_function_ui_description(func, "Align the bone to a localspace roll so the Z axis points in the direction of the vector given.");
	parm= RNA_def_float_vector(func, "vector", 3, NULL, -FLT_MAX, FLT_MAX, "Vector", "", -FLT_MAX, FLT_MAX);
	RNA_def_property_flag(parm, PROP_REQUIRED);
}

#endif
