/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_pose_api.c
 *  \ingroup RNA
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "RNA_define.h"

#include "DNA_object_types.h"

/* #include "BLO_sys_types.h" */

#ifdef RNA_RUNTIME

/* #include "DNA_anim_types.h" */
#include "DNA_action_types.h" /* bPose */
#include "BKE_armature.h"

static float rna_PoseBone_do_envelope(bPoseChannel *chan, float *vec)
{
	Bone *bone = chan->bone;

	float scale = (bone->flag & BONE_MULT_VG_ENV) == BONE_MULT_VG_ENV ? bone->weight : 1.0f;

	return distfactor_to_bone(vec, chan->pose_head, chan->pose_tail, bone->rad_head * scale, bone->rad_tail * scale, bone->dist * scale);
}
#else

void RNA_api_pose(StructRNA *srna)
{
	/* FunctionRNA *func; */
	/* PropertyRNA *parm; */
}

void RNA_api_pose_channel(StructRNA *srna)
{
	PropertyRNA *parm;
	FunctionRNA *func;

	func = RNA_def_function(srna, "evaluate_envelope", "rna_PoseBone_do_envelope");
	RNA_def_function_ui_description(func, "Calculate bone envelope at given point");
	parm = RNA_def_float_vector_xyz(func, "point", 3, NULL, -FLT_MAX, FLT_MAX, "Point",
	                               "Position in 3d space to evaluate", -FLT_MAX, FLT_MAX);
	RNA_def_property_flag(parm, PROP_REQUIRED);
	/* return value */
	parm = RNA_def_float(func, "factor", 0, -FLT_MAX, FLT_MAX, "Factor", "Envelope factor", -FLT_MAX, FLT_MAX);
	RNA_def_function_return(func, parm);
}


#endif

