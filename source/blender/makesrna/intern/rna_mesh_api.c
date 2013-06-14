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

/** \file blender/makesrna/intern/rna_mesh_api.c
 *  \ingroup RNA
 */


#include <stdlib.h>
#include <stdio.h>

#include "RNA_define.h"

#include "BLI_sys_types.h"

#include "BLI_utildefines.h"

#include "BKE_mesh.h"
#include "ED_mesh.h"

#include "rna_internal.h"  /* own include */

#ifdef RNA_RUNTIME

#include "DNA_mesh_types.h"

static const char *rna_Mesh_unit_test_compare(struct Mesh *mesh, bContext *C, struct Mesh *mesh2)
{
	const char *ret = BKE_mesh_cmp(mesh, mesh2, FLT_EPSILON * 60);
	
	if (!ret)
		ret = "Same";
	
	return ret;
}

void rna_Mesh_calc_smooth_groups(struct Mesh *mesh, int *r_poly_group_len, int **r_poly_group, int *r_group_total)
{
	*r_poly_group_len = mesh->totpoly;
	*r_poly_group = BKE_mesh_calc_smoothgroups(
	                    mesh->medge, mesh->totedge,
	                    mesh->mpoly, mesh->totpoly,
	                    mesh->mloop, mesh->totloop,
	                    r_group_total);
}

#else

void RNA_api_mesh(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	func = RNA_def_function(srna, "transform", "ED_mesh_transform");
	RNA_def_function_ui_description(func, "Transform mesh vertices by a matrix");
	parm = RNA_def_float_matrix(func, "matrix", 4, 4, NULL, 0.0f, 0.0f, "", "Matrix", 0.0f, 0.0f);
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func = RNA_def_function(srna, "calc_normals", "BKE_mesh_calc_normals");
	RNA_def_function_ui_description(func, "Calculate vertex normals");

	func = RNA_def_function(srna, "calc_tessface", "ED_mesh_calc_tessface");
	RNA_def_function_ui_description(func, "Calculate face tessellation (supports editmode too)");

	func = RNA_def_function(srna, "calc_smooth_groups", "rna_Mesh_calc_smooth_groups");
	RNA_def_function_ui_description(func, "Calculate smooth groups from sharp edges");
	/* return values */
	parm = RNA_def_int_array(func, "poly_groups", 1, NULL, 0, 0, "", "Smooth Groups", 0, 0);
	RNA_def_property_flag(parm, PROP_DYNAMIC | PROP_OUTPUT);
	parm = RNA_def_int(func, "groups", 0, 0, INT_MAX, "groups", "Total number of groups", 0, INT_MAX);
	RNA_def_property_flag(parm, PROP_OUTPUT);


	func = RNA_def_function(srna, "update", "ED_mesh_update");
	RNA_def_boolean(func, "calc_edges", 0, "Calculate Edges", "Force recalculation of edges");
	RNA_def_boolean(func, "calc_tessface", 0, "Calculate Tessellation", "Force recalculation of tessellation faces");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);

	func = RNA_def_function(srna, "unit_test_compare", "rna_Mesh_unit_test_compare");
	RNA_def_pointer(func, "mesh", "Mesh", "", "Mesh to compare to");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);
	/* return value */
	parm = RNA_def_string(func, "result", "nothing", 64, "Return value", "String description of result of comparison");
	RNA_def_function_return(func, parm);

	func = RNA_def_function(srna, "validate", "BKE_mesh_validate");
	RNA_def_function_ui_description(func, "validate geometry, return True when the mesh has had "
	                                "invalid geometry corrected/removed");
	RNA_def_boolean(func, "verbose", 0, "Verbose", "Output information about the errors found");
	parm = RNA_def_boolean(func, "result", 0, "Result", "");
	RNA_def_function_return(func, parm);
}

#endif

