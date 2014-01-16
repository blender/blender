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

#include "DNA_customdata_types.h"

#include "BLI_sys_types.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "rna_internal.h"  /* own include */

#ifdef RNA_RUNTIME

#include "DNA_mesh_types.h"

#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "ED_mesh.h"

static const char *rna_Mesh_unit_test_compare(struct Mesh *mesh, struct Mesh *mesh2)
{
	const char *ret = BKE_mesh_cmp(mesh, mesh2, FLT_EPSILON * 60);
	
	if (!ret)
		ret = "Same";
	
	return ret;
}

static void rna_Mesh_calc_normals_split(Mesh *mesh, float min_angle)
{
	float (*r_loopnors)[3];
	float (*polynors)[3];
	bool free_polynors = false;

	if (CustomData_has_layer(&mesh->ldata, CD_NORMAL)) {
		r_loopnors = CustomData_get_layer(&mesh->ldata, CD_NORMAL);
		memset(r_loopnors, 0, sizeof(float[3]) * mesh->totloop);
	}
	else {
		r_loopnors = CustomData_add_layer(&mesh->ldata, CD_NORMAL, CD_CALLOC, NULL, mesh->totloop);
		CustomData_set_layer_flag(&mesh->ldata, CD_NORMAL, CD_FLAG_TEMPORARY);
	}

	if (CustomData_has_layer(&mesh->pdata, CD_NORMAL)) {
		/* This assume that layer is always up to date, not sure this is the case (esp. in Edit mode?)... */
		polynors = CustomData_get_layer(&mesh->pdata, CD_NORMAL);
		free_polynors = false;
	}
	else {
		polynors = MEM_mallocN(sizeof(float[3]) * mesh->totpoly, __func__);
		BKE_mesh_calc_normals_poly(mesh->mvert, mesh->totvert, mesh->mloop, mesh->mpoly, mesh->totloop, mesh->totpoly,
		                           polynors, false);
		free_polynors = true;
	}

	BKE_mesh_normals_loop_split(mesh->mvert, mesh->totvert, mesh->medge, mesh->totedge,
	                            mesh->mloop, r_loopnors, mesh->totloop, mesh->mpoly, polynors, mesh->totpoly,
	                            min_angle);

	if (free_polynors) {
		MEM_freeN(polynors);
	}
}

static void rna_Mesh_free_normals_split(Mesh *mesh)
{
	CustomData_free_layers(&mesh->ldata, CD_NORMAL, mesh->totloop);
}

static void rna_Mesh_calc_tangents(Mesh *mesh, ReportList *reports, const char *uvmap)
{
	float (*r_looptangents)[4];

	if (CustomData_has_layer(&mesh->ldata, CD_MLOOPTANGENT)) {
		r_looptangents = CustomData_get_layer(&mesh->ldata, CD_MLOOPTANGENT);
		memset(r_looptangents, 0, sizeof(float[4]) * mesh->totloop);
	}
	else {
		r_looptangents = CustomData_add_layer(&mesh->ldata, CD_MLOOPTANGENT, CD_CALLOC, NULL, mesh->totloop);
		CustomData_set_layer_flag(&mesh->ldata, CD_MLOOPTANGENT, CD_FLAG_TEMPORARY);
	}

	/* Compute loop normals if needed. */
	if (!CustomData_has_layer(&mesh->ldata, CD_NORMAL)) {
		rna_Mesh_calc_normals_split(mesh, (float)M_PI);
	}

	BKE_mesh_loop_tangents(mesh, uvmap, r_looptangents, reports);
}

static void rna_Mesh_free_tangents(Mesh *mesh)
{
	CustomData_free_layers(&mesh->ldata, CD_MLOOPTANGENT, mesh->totloop);
}

static void rna_Mesh_calc_smooth_groups(Mesh *mesh, int use_bitflags, int *r_poly_group_len,
                                        int **r_poly_group, int *r_group_total)
{
	*r_poly_group_len = mesh->totpoly;
	*r_poly_group = BKE_mesh_calc_smoothgroups(
	                    mesh->medge, mesh->totedge,
	                    mesh->mpoly, mesh->totpoly,
	                    mesh->mloop, mesh->totloop,
	                    r_group_total, use_bitflags);
}

static void rna_Mesh_transform(Mesh *mesh, float *mat)
{
	ED_mesh_transform(mesh, (float (*)[4])mat);
}

#else

void RNA_api_mesh(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	func = RNA_def_function(srna, "transform", "rna_Mesh_transform");
	RNA_def_function_ui_description(func, "Transform mesh vertices by a matrix");
	parm = RNA_def_float_matrix(func, "matrix", 4, 4, NULL, 0.0f, 0.0f, "", "Matrix", 0.0f, 0.0f);
	RNA_def_property_flag(parm, PROP_REQUIRED);

	func = RNA_def_function(srna, "calc_normals", "BKE_mesh_calc_normals");
	RNA_def_function_ui_description(func, "Calculate vertex normals");

	func = RNA_def_function(srna, "calc_normals_split", "rna_Mesh_calc_normals_split");
	RNA_def_function_ui_description(func, "Calculate split vertex normals, which preserve sharp edges");
	parm = RNA_def_float(func, "split_angle", M_PI, 0.0f, M_PI, "",
	                     "Angle between polys' normals above which an edge is always sharp (180° to disable)",
	                     0.0f, M_PI);
	RNA_def_property_subtype(parm, (PropertySubType)PROP_UNIT_ROTATION);

	func = RNA_def_function(srna, "free_normals_split", "rna_Mesh_free_normals_split");
	RNA_def_function_ui_description(func, "Free split vertex normals");

	func = RNA_def_function(srna, "calc_tangents", "rna_Mesh_calc_tangents");
	RNA_def_function_flag(func, FUNC_USE_REPORTS);
	RNA_def_function_ui_description(func,
	                                "Compute tangents and bitangent signs, to be used together with the split normals "
	                                "to get a complete tangent space for normal mapping "
	                                "(split normals are also computed if not yet present)");
	parm = RNA_def_string(func, "uvmap", NULL, MAX_CUSTOMDATA_LAYER_NAME, "",
	                      "Name of the UV map to use for tangent space computation");

	func = RNA_def_function(srna, "free_tangents", "rna_Mesh_free_tangents");
	RNA_def_function_ui_description(func, "Free tangents");

	func = RNA_def_function(srna, "calc_tessface", "ED_mesh_calc_tessface");
	RNA_def_function_ui_description(func, "Calculate face tessellation (supports editmode too)");

	func = RNA_def_function(srna, "calc_smooth_groups", "rna_Mesh_calc_smooth_groups");
	RNA_def_function_ui_description(func, "Calculate smooth groups from sharp edges");
	RNA_def_boolean(func, "use_bitflags", false, "", "Produce bitflags groups instead of simple numeric values");
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

