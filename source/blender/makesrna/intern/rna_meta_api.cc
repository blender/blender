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

#  include "DNA_meta_types.h"

#  include "BKE_mball.hh"

#  include "DEG_depsgraph.hh"

static void rna_Meta_transform(MetaBall *mb, const float mat[16])
{
  BKE_mball_transform(mb, (const float (*)[4])mat, true);

  DEG_id_tag_update(&mb->id, 0);
}

static void rna_Mball_update_gpu_tag(MetaBall *mb)
{
  DEG_id_tag_update(&mb->id, ID_RECALC_SHADING);
}
#else

void RNA_api_meta(StructRNA *srna)
{
  FunctionRNA *func;
  PropertyRNA *parm;

  func = RNA_def_function(srna, "transform", "rna_Meta_transform");
  RNA_def_function_ui_description(func, "Transform metaball elements by a matrix");
  parm = RNA_def_float_matrix(func, "matrix", 4, 4, nullptr, 0.0f, 0.0f, "", "Matrix", 0.0f, 0.0f);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  RNA_def_function(srna, "update_gpu_tag", "rna_Mball_update_gpu_tag");
}

#endif
