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
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_curve_api.c
 *  \ingroup RNA
 */


#include <stdlib.h>
#include <stdio.h>

#include "RNA_define.h"

#include "BLI_sys_types.h"

#include "BLI_utildefines.h"

#include "BKE_curve.h"

#include "rna_internal.h"  /* own include */

#ifdef RNA_RUNTIME
static void rna_Curve_transform(Curve *cu, float *mat, bool shape_keys)
{
	BKE_curve_transform(cu, (float (*)[4])mat, shape_keys, true);

	DEG_id_tag_update(&cu->id, 0);
}

static void rna_Curve_update_gpu_tag(Curve *cu)
{
	BKE_curve_batch_cache_dirty(cu, BKE_CURVE_BATCH_DIRTY_ALL);
}

static float rna_Nurb_calc_length(Nurb *nu, int resolution_u)
{
	return BKE_nurb_calc_length(nu, resolution_u);
}

#else

void RNA_api_curve(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	func = RNA_def_function(srna, "transform", "rna_Curve_transform");
	RNA_def_function_ui_description(func, "Transform curve by a matrix");
	parm = RNA_def_float_matrix(func, "matrix", 4, 4, NULL, 0.0f, 0.0f, "", "Matrix", 0.0f, 0.0f);
	RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
	RNA_def_boolean(func, "shape_keys", 0, "", "Transform Shape Keys");

	func = RNA_def_function(srna, "validate_material_indices", "BKE_curve_material_index_validate");
	RNA_def_function_ui_description(func, "Validate material indices of splines or letters, return True when the curve "
	                                "has had invalid indices corrected (to default 0)");
	parm = RNA_def_boolean(func, "result", 0, "Result", "");
	RNA_def_function_return(func, parm);

	RNA_def_function(srna, "update_gpu_tag", "rna_Curve_update_gpu_tag");
}

void RNA_api_curve_nurb(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	func = RNA_def_function(srna, "calc_length", "rna_Nurb_calc_length");
	RNA_def_function_ui_description(func, "Calculate spline length");
	RNA_def_int(
	        func, "resolution", 0, 0, 1024, "Resolution",
	        "Spline resolution to be used, 0 defaults to the resolution_u", 0, 64);
	parm = RNA_def_float_distance(
	        func, "length", 0.0f, 0.0f, FLT_MAX, "Length",
	        "Length of the polygonaly approximated spline", 0.0f, FLT_MAX);
	RNA_def_function_return(func, parm);
}

#endif
