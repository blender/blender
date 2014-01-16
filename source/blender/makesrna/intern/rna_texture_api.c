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
 * Contributor(s): Tom Edwards
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_texture_api.c
 *  \ingroup RNA
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "BLI_utildefines.h"
#include "BLI_path_util.h"

#include "RNA_define.h"

#include "rna_internal.h"  /* own include */

#ifdef RNA_RUNTIME

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
#include "DNA_scene_types.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "RE_pipeline.h"
#include "RE_shader_ext.h"

static void save_envmap(struct EnvMap *env, bContext *C, ReportList *reports, const char *filepath,
                        struct Scene *scene, float layout[12])
{
	if (scene == NULL) {
		scene = CTX_data_scene(C);
	}

	RE_WriteEnvmapResult(reports, scene, env, filepath, scene->r.im_format.imtype, layout);
}

static void clear_envmap(struct EnvMap *env, bContext *C)
{
	Main *bmain = CTX_data_main(C);
	Tex *tex;

	BKE_free_envmapdata(env);
	
	for (tex = bmain->tex.first; tex; tex = tex->id.next)
		if (tex->env == env) {
			WM_event_add_notifier(C, NC_TEXTURE | NA_EDITED, tex);
			break;
		}
}

static void texture_evaluate(struct Tex *tex, float value[3], float color_r[4])
{
	TexResult texres = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, NULL};

	/* TODO(sergey): always use color management now.  */
	multitex_ext(tex, value, NULL, NULL, 1, &texres, NULL, true);

	color_r[0] = texres.tr;
	color_r[1] = texres.tg;
	color_r[2] = texres.tb;
	color_r[3] = texres.tin;
}

#else

void RNA_api_texture(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	func = RNA_def_function(srna, "evaluate", "texture_evaluate");
	RNA_def_function_ui_description(func, "Evaluate the texture at the coordinates given");

	parm = RNA_def_float_vector(func, "value", 3, NULL, -FLT_MAX, FLT_MAX, "", "", -1e4, 1e4);
	RNA_def_property_flag(parm, PROP_REQUIRED);

	/* return location and normal */
	parm = RNA_def_float_vector(func, "result", 4, NULL, -FLT_MAX, FLT_MAX, "Result", NULL, -1e4, 1e4);
	RNA_def_property_flag(parm, PROP_THICK_WRAP);
	RNA_def_function_output(func, parm);

}

void RNA_api_environment_map(StructRNA *srna)
{
	FunctionRNA *func;
	PropertyRNA *parm;

	static const float default_layout[] = {0, 0, 1, 0, 2, 0, 0, 1, 1, 1, 2, 1};

	func = RNA_def_function(srna, "clear", "clear_envmap");
	RNA_def_function_ui_description(func, "Discard the environment map and free it from memory");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT);


	func = RNA_def_function(srna, "save", "save_envmap");
	RNA_def_function_ui_description(func, "Save the environment map to disc using the scene render settings");
	RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_REPORTS);

	parm = RNA_def_string_file_name(func, "filepath", NULL, FILE_MAX, "File path", "Location of the output file");
	RNA_def_property_flag(parm, PROP_REQUIRED);

	RNA_def_pointer(func, "scene", "Scene", "", "Overrides the scene from which image parameters are taken");

	RNA_def_float_array(func, "layout", 12, default_layout, 0.0f, 1000.0f, "File layout",
	                    "Flat array describing the X,Y position of each cube face in the "
	                    "output image, where 1 is the size of a face - order is [+Z -Z +Y -X -Y +X] "
	                    "(use -1 to skip a face)", 0.0f, 1000.0f);
}

#endif
