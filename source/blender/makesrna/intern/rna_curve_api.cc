/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cstdlib>

#include "RNA_define.hh"

#include "rna_internal.hh" /* own include */

#ifdef RNA_RUNTIME
#  include "DNA_curve_types.h"

#  include "BLI_string.h"

#  include "BKE_curve.hh"

#  include "DEG_depsgraph.hh"

static void rna_Curve_transform(Curve *cu, const float mat[16], bool shape_keys)
{
  BKE_curve_transform(cu, (const float (*)[4])mat, shape_keys, true);

  DEG_id_tag_update(&cu->id, 0);
}

static void rna_Curve_update_gpu_tag(Curve *cu)
{
  BKE_curve_batch_cache_dirty_tag(cu, BKE_CURVE_BATCH_DIRTY_ALL);
}

static float rna_Nurb_calc_length(Nurb *nu, int resolution_u)
{
  return BKE_nurb_calc_length(nu, resolution_u);
}

static void rna_Nurb_valid_message(Nurb *nu, int direction, const char **r_result, int *result_len)
{
  const bool is_surf = nu->pntsv > 1;
  const short type = nu->type;

  int pnts;
  short order, flag;
  if (direction == 0) {
    pnts = nu->pntsu;
    order = nu->orderu;
    flag = nu->flagu;
  }
  else {
    pnts = nu->pntsv;
    order = nu->orderv;
    flag = nu->flagv;
  }

  char buf[64];
  if (BKE_nurb_valid_message(pnts, order, flag, type, is_surf, direction, buf, sizeof(buf))) {
    const int buf_len = strlen(buf);
    *r_result = BLI_strdupn(buf, buf_len);
    *result_len = buf_len;
  }
  else {
    *r_result = nullptr;
    *result_len = 0;
  }
}

#else

void RNA_api_curve(StructRNA *srna)
{
  FunctionRNA *func;
  PropertyRNA *parm;

  func = RNA_def_function(srna, "transform", "rna_Curve_transform");
  RNA_def_function_ui_description(func, "Transform curve by a matrix");
  parm = RNA_def_float_matrix(func, "matrix", 4, 4, nullptr, 0.0f, 0.0f, "", "Matrix", 0.0f, 0.0f);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_boolean(func, "shape_keys", false, "", "Transform Shape Keys");

  func = RNA_def_function(srna, "validate_material_indices", "BKE_curve_material_index_validate");
  RNA_def_function_ui_description(
      func,
      "Validate material indices of splines or letters, return True when the curve "
      "has had invalid indices corrected (to default 0)");
  parm = RNA_def_boolean(func, "result", false, "Result", "");
  RNA_def_function_return(func, parm);

  RNA_def_function(srna, "update_gpu_tag", "rna_Curve_update_gpu_tag");
}

void RNA_api_curve_nurb(StructRNA *srna)
{
  FunctionRNA *func;
  PropertyRNA *parm;

  func = RNA_def_function(srna, "calc_length", "rna_Nurb_calc_length");
  RNA_def_function_ui_description(func, "Calculate spline length");
  RNA_def_int(func,
              "resolution",
              0,
              0,
              1024,
              "Resolution",
              "Spline resolution to be used, 0 defaults to the resolution_u",
              0,
              64);
  parm = RNA_def_float_distance(func,
                                "length",
                                0.0f,
                                0.0f,
                                FLT_MAX,
                                "Length",
                                "Length of the polygonaly approximated spline",
                                0.0f,
                                FLT_MAX);
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "valid_message", "rna_Nurb_valid_message");
  RNA_def_function_ui_description(func, "Return the message");
  parm = RNA_def_int(
      func, "direction", 0, 0, 1, "Direction", "The direction where 0-1 maps to U-V", 0, 1);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return value */
  parm = RNA_def_string(func,
                        "result",
                        "nothing",
                        64,
                        "Return value",
                        "The message or an empty string when there is no error");

  RNA_def_parameter_flags(parm, PROP_DYNAMIC, PARM_OUTPUT);
  RNA_def_property_clear_flag(parm, PROP_NEVER_NULL);
}

#endif
