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

#  include "BKE_geometry_compare.hh"

static const char *rna_Lattice_unit_test_compare(Lattice *lt, Lattice *lt2, float threshold)
{
  using namespace blender::bke::compare_geometry;
  const std::optional<GeoMismatch> mismatch = compare_lattices(*lt, *lt2, threshold);

  if (!mismatch) {
    return "Same";
  }

  return mismatch_to_string(mismatch.value());
}

static void rna_Lattice_transform(Lattice *lt, const float mat[16], bool shape_keys)
{
  BKE_lattice_transform(lt, (const float (*)[4])mat, shape_keys);

  DEG_id_tag_update(&lt->id, 0);
}

static void rna_Lattice_update_gpu_tag(Lattice *lt)
{
  BKE_lattice_batch_cache_dirty_tag(lt, BKE_LATTICE_BATCH_DIRTY_ALL);
}

#else

void RNA_api_lattice(StructRNA *srna)
{
  FunctionRNA *func;
  PropertyRNA *parm;

  func = RNA_def_function(srna, "transform", "rna_Lattice_transform");
  RNA_def_function_ui_description(func, "Transform lattice by a matrix");
  parm = RNA_def_float_matrix(func, "matrix", 4, 4, nullptr, 0.0f, 0.0f, "", "Matrix", 0.0f, 0.0f);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_boolean(func, "shape_keys", false, "", "Transform Shape Keys");

  RNA_def_function(srna, "update_gpu_tag", "rna_Lattice_update_gpu_tag");

  func = RNA_def_function(srna, "unit_test_compare", "rna_Lattice_unit_test_compare");
  RNA_def_pointer(func, "lattice", "Lattice", "", "Lattice to compare to");
  RNA_def_float_factor(func,
                       "threshold",
                       FLT_EPSILON * 60,
                       0.0f,
                       FLT_MAX,
                       "Threshold",
                       "Comparison tolerance threshold",
                       0.0f,
                       FLT_MAX);
  /* return value */
  parm = RNA_def_string(
      func, "result", "nothing", 64, "Return value", "String description of result of comparison");
  RNA_def_function_return(func, parm);
}

#endif
