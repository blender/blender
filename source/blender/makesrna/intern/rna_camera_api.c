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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_camera_api.c
 *  \ingroup RNA
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "RNA_define.h"

#include "rna_internal.h"  /* own include */

#ifdef RNA_RUNTIME

#include "DNA_scene_types.h"

#include "BKE_camera.h"
#include "BKE_context.h"
#include "BKE_object.h"

static void rna_camera_view_frame(struct Camera *camera, struct Scene *scene,
                                  float r_vec1[3], float r_vec2[3], float r_vec3[3], float r_vec4[3])
{
	float vec[4][3];

	BKE_camera_view_frame(scene, camera, vec);

	copy_v3_v3(r_vec1, vec[0]);
	copy_v3_v3(r_vec2, vec[1]);
	copy_v3_v3(r_vec3, vec[2]);
	copy_v3_v3(r_vec4, vec[3]);
}

#else

void RNA_api_camera(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	func = RNA_def_function(srna, "view_frame", "rna_camera_view_frame");
	RNA_def_function_ui_description(func, "Return 4 points for the cameras frame (before object transformation)");

	RNA_def_pointer(func, "scene", "Scene", "", "Scene to use for aspect calculation, when omitted 1:1 aspect is used");

	/* return location and normal */
	parm = RNA_def_float_vector(func, "result_1", 3, NULL, -FLT_MAX, FLT_MAX, "Result", NULL, -1e4, 1e4);
	RNA_def_property_flag(parm, PROP_THICK_WRAP);
	RNA_def_function_output(func, parm);

	parm = RNA_def_float_vector(func, "result_2", 3, NULL, -FLT_MAX, FLT_MAX, "Result", NULL, -1e4, 1e4);
	RNA_def_property_flag(parm, PROP_THICK_WRAP);
	RNA_def_function_output(func, parm);

	parm = RNA_def_float_vector(func, "result_3", 3, NULL, -FLT_MAX, FLT_MAX, "Result", NULL, -1e4, 1e4);
	RNA_def_property_flag(parm, PROP_THICK_WRAP);
	RNA_def_function_output(func, parm);

	parm = RNA_def_float_vector(func, "result_4", 3, NULL, -FLT_MAX, FLT_MAX, "Result", NULL, -1e4, 1e4);
	RNA_def_property_flag(parm, PROP_THICK_WRAP);
	RNA_def_function_output(func, parm);
}

#endif

