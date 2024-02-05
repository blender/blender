/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "RNA_define.hh"

#include "rna_internal.hh" /* own include */

#ifdef RNA_RUNTIME

#  include "DNA_scene_types.h"

#  include "BLI_math_vector.h"

#  include "BKE_camera.h"
#  include "BKE_context.hh"
#  include "BKE_object.hh"

static void rna_camera_view_frame(Camera *camera,
                                  Scene *scene,
                                  float r_vec1[3],
                                  float r_vec2[3],
                                  float r_vec3[3],
                                  float r_vec4[3])
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
  RNA_def_function_ui_description(
      func, "Return 4 points for the cameras frame (before object transformation)");

  RNA_def_pointer(func,
                  "scene",
                  "Scene",
                  "",
                  "Scene to use for aspect calculation, when omitted 1:1 aspect is used");

  /* return location and normal */
  parm = RNA_def_float_vector(
      func, "result_1", 3, nullptr, -FLT_MAX, FLT_MAX, "Result", nullptr, -1e4, 1e4);
  RNA_def_property_flag(parm, PROP_THICK_WRAP);
  RNA_def_function_output(func, parm);

  parm = RNA_def_float_vector(
      func, "result_2", 3, nullptr, -FLT_MAX, FLT_MAX, "Result", nullptr, -1e4, 1e4);
  RNA_def_property_flag(parm, PROP_THICK_WRAP);
  RNA_def_function_output(func, parm);

  parm = RNA_def_float_vector(
      func, "result_3", 3, nullptr, -FLT_MAX, FLT_MAX, "Result", nullptr, -1e4, 1e4);
  RNA_def_property_flag(parm, PROP_THICK_WRAP);
  RNA_def_function_output(func, parm);

  parm = RNA_def_float_vector(
      func, "result_4", 3, nullptr, -FLT_MAX, FLT_MAX, "Result", nullptr, -1e4, 1e4);
  RNA_def_property_flag(parm, PROP_THICK_WRAP);
  RNA_def_function_output(func, parm);
}

#endif
