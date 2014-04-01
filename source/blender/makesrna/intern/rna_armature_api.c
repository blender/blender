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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_armature_api.c
 *  \ingroup RNA
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "RNA_define.h"

#include "rna_internal.h"  /* own include */

#ifdef RNA_RUNTIME

#include <stddef.h>

#include "BKE_armature.h"

static void rna_EditBone_align_roll(EditBone *ebo, float no[3])
{
	ebo->roll = ED_rollBoneToVector(ebo, no, false);
}

static float rna_Bone_do_envelope(Bone *bone, float *vec)
{
	float scale = (bone->flag & BONE_MULT_VG_ENV) == BONE_MULT_VG_ENV ? bone->weight : 1.0f;
	return distfactor_to_bone(vec, bone->arm_head, bone->arm_tail, bone->rad_head * scale,
	                          bone->rad_tail * scale, bone->dist * scale);
}

#else

void RNA_api_armature_edit_bone(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	func = RNA_def_function(srna, "align_roll", "rna_EditBone_align_roll");
	RNA_def_function_ui_description(func, "Align the bone to a localspace roll so the Z axis "
	                                "points in the direction of the vector given");
	parm = RNA_def_float_vector(func, "vector", 3, NULL, -FLT_MAX, FLT_MAX, "Vector", "", -FLT_MAX, FLT_MAX);
	RNA_def_property_flag(parm, PROP_REQUIRED);
}

void RNA_api_bone(StructRNA *srna)
{
	PropertyRNA *parm;
	FunctionRNA *func;

	func = RNA_def_function(srna, "evaluate_envelope", "rna_Bone_do_envelope");
	RNA_def_function_ui_description(func, "Calculate bone envelope at given point");
	parm = RNA_def_float_vector_xyz(func, "point", 3, NULL, -FLT_MAX, FLT_MAX, "Point",
	                                "Position in 3d space to evaluate", -FLT_MAX, FLT_MAX);
	RNA_def_property_flag(parm, PROP_REQUIRED);
	/* return value */
	parm = RNA_def_float(func, "factor", 0, -FLT_MAX, FLT_MAX, "Factor", "Envelope factor", -FLT_MAX, FLT_MAX);
	RNA_def_function_return(func, parm);
}

#endif
