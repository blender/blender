/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "BLI_utildefines.h"

#include "RNA_define.hh"

#include "rna_internal.hh" /* own include */

#ifdef RNA_RUNTIME

#  include "BKE_context.hh"
#  include "BKE_global.hh"
#  include "BLI_math_vector.h"
#  include "DNA_scene_types.h"
#  include "IMB_imbuf.hh"
#  include "IMB_imbuf_types.hh"
#  include "RE_pipeline.h"
#  include "RE_texture.h"

static void texture_evaluate(Tex *tex, const float value[3], float r_color[4])
{
  TexResult texres = {0.0f};

  /* TODO(sergey): always use color management now. */
  multitex_ext(tex, value, nullptr, nullptr, 1, &texres, 0, nullptr, true, false);

  copy_v3_v3(r_color, texres.trgba);
  r_color[3] = texres.tin;
}

#else

void RNA_api_texture(StructRNA *srna)
{
  FunctionRNA *func;
  PropertyRNA *parm;

  func = RNA_def_function(srna, "evaluate", "texture_evaluate");
  RNA_def_function_ui_description(
      func, "Evaluate the texture at the a given coordinate and returns the result");

  parm = RNA_def_float_vector(
      func,
      "value",
      3,
      nullptr,
      -FLT_MAX,
      FLT_MAX,
      "The coordinates (x,y,z) of the texture, in case of a 3D texture, the z value is the slice "
      "of the texture that is evaluated. For 2D textures such as images, the z value is ignored",
      "",
      -1e4,
      1e4);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  /* return location and normal */
  parm = RNA_def_float_vector(
      func,
      "result",
      4,
      nullptr,
      -FLT_MAX,
      FLT_MAX,
      "The result of the texture where (x,y,z,w) are (red, green, blue, intensity). "
      "For grayscale textures, often intensity only will be used",
      nullptr,
      -1e4,
      1e4);
  RNA_def_parameter_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  RNA_def_function_output(func, parm);
}

#endif
