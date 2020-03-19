/*
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
 */

/** \file
 * \ingroup RNA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "BLI_utildefines.h"

#include "RNA_define.h"

#include "rna_internal.h" /* own include */

#ifdef RNA_RUNTIME

#  include "BKE_context.h"
#  include "BKE_global.h"
#  include "DNA_scene_types.h"
#  include "IMB_imbuf.h"
#  include "IMB_imbuf_types.h"
#  include "RE_pipeline.h"
#  include "RE_shader_ext.h"

static void texture_evaluate(struct Tex *tex, float value[3], float r_color[4])
{
  TexResult texres = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, NULL};

  /* TODO(sergey): always use color management now.  */
  multitex_ext(tex, value, NULL, NULL, 1, &texres, 0, NULL, true, false);

  r_color[0] = texres.tr;
  r_color[1] = texres.tg;
  r_color[2] = texres.tb;
  r_color[3] = texres.tin;
}

#else

void RNA_api_texture(StructRNA *srna)
{
  FunctionRNA *func;
  PropertyRNA *parm;

  func = RNA_def_function(srna, "evaluate", "texture_evaluate");
  RNA_def_function_ui_description(func, "Evaluate the texture at the coordinates given");

  parm = RNA_def_float_vector(func, "value", 3, NULL, -FLT_MAX, FLT_MAX, "", "", -1e4, 1e4);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);

  /* return location and normal */
  parm = RNA_def_float_vector(
      func, "result", 4, NULL, -FLT_MAX, FLT_MAX, "Result", NULL, -1e4, 1e4);
  RNA_def_parameter_flags(parm, PROP_THICK_WRAP, 0);
  RNA_def_function_output(func, parm);
}

#endif
