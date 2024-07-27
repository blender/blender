/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include "RNA_define.hh"

#include "rna_internal.hh" /* own include */

#ifdef RNA_RUNTIME

#  include "BKE_curves.hh"
#  include "BKE_report.hh"

#  include "BLI_index_mask.hh"

#  include "ED_curves.hh"

#  include "rna_curves_utils.hh"

/* Common `CurvesGeometry` API functions. */

bool rna_CurvesGeometry_add_curves(blender::bke::CurvesGeometry &curves,
                                   ReportList *reports,
                                   const int *sizes_ptr,
                                   const int sizes_num)
{
  using namespace blender;
  if (std::any_of(sizes_ptr, sizes_ptr + sizes_num, [](const int size) { return size < 1; })) {
    BKE_report(reports, RPT_ERROR, "Curve sizes must be greater than zero");
    return false;
  }

  ed::curves::add_curves(curves, {sizes_ptr, sizes_num});
  curves.tag_topology_changed();
  return true;
}

bool rna_CurvesGeometry_remove_curves(blender::bke::CurvesGeometry &curves,
                                      ReportList *reports,
                                      const int *indices_ptr,
                                      const int indices_num)
{
  using namespace blender;
  if (indices_ptr != nullptr) {
    const Span<int> indices(indices_ptr, indices_num);
    if (std::any_of(indices.begin(), indices.end(), [&](const int index) {
          return !curves.curves_range().contains(index);
        }))
    {
      BKE_report(reports, RPT_ERROR, "Indices must be in range");
      return false;
    }
    if (!std::is_sorted(indices.begin(), indices.end())) {
      BKE_report(reports, RPT_ERROR, "Indices must be sorted in acending order");
      return false;
    }
    if (std::adjacent_find(indices.begin(), indices.end(), std::greater_equal<int>()) !=
        indices.end())
    {
      BKE_report(reports, RPT_ERROR, "Indices can't have duplicates");
      return false;
    }
    /* Remove the curves by their indices. */
    IndexMaskMemory memory;
    IndexMask curves_to_delete = IndexMask::from_indices(indices, memory);
    curves.remove_curves(curves_to_delete, {});
  }
  else {
    /* Clear the CurvesGeometry. */
    curves = {};
  }
  return true;
}

bool rna_CurvesGeometry_resize_curves(blender::bke::CurvesGeometry &curves,
                                      ReportList *reports,
                                      const int *sizes_ptr,
                                      const int sizes_num,
                                      const int *indices_ptr,
                                      const int indices_num)
{
  using namespace blender;
  const Span<int> new_sizes(sizes_ptr, sizes_num);
  if (std::any_of(new_sizes.begin(), new_sizes.end(), [](const int size) { return size < 1; })) {
    BKE_report(reports, RPT_ERROR, "Sizes must be greater than zero");
    return false;
  }
  IndexMaskMemory memory;
  IndexMask curves_to_resize;
  if (indices_ptr != nullptr) {
    if (indices_num != sizes_num) {
      BKE_report(reports, RPT_ERROR, "Length of sizes and length of indices must be the same");
      return false;
    }
    const Span<int> indices(indices_ptr, indices_num);
    if (std::any_of(indices.begin(), indices.end(), [&](const int index) {
          return !curves.curves_range().contains(index);
        }))
    {
      BKE_report(reports, RPT_ERROR, "Indices must be in range");
      return false;
    }
    if (!std::is_sorted(indices.begin(), indices.end())) {
      BKE_report(reports, RPT_ERROR, "Indices must be sorted in acending order");
      return false;
    }
    if (std::adjacent_find(indices.begin(), indices.end(), std::greater_equal<int>()) !=
        indices.end())
    {
      BKE_report(reports, RPT_ERROR, "Indices can't have duplicates");
      return false;
    }
    curves_to_resize = IndexMask::from_indices(indices, memory);
  }
  else {
    if (sizes_num != curves.curves_num()) {
      BKE_report(reports, RPT_ERROR, "Length of sizes and number of strokes must be the same");
      return false;
    }
    curves_to_resize = curves.curves_range();
  }

  ed::curves::resize_curves(curves, curves_to_resize, {sizes_ptr, sizes_num});
  return true;
}

/* Curves API functions. */

static void rna_Curves_add_curves(Curves *curves_id,
                                  ReportList *reports,
                                  const int *sizes,
                                  const int sizes_num)
{
  using namespace blender;
  bke::CurvesGeometry &curves = curves_id->geometry.wrap();
  if (!rna_CurvesGeometry_add_curves(curves, reports, sizes, sizes_num)) {
    return;
  }

  /* Avoid updates for importers creating curves. */
  if (curves_id->id.us > 0) {
    DEG_id_tag_update(&curves_id->id, ID_RECALC_GEOMETRY);
    WM_main_add_notifier(NC_GEOM | ND_DATA, curves_id);
  }
}

static void rna_Curves_remove_curves(Curves *curves_id,
                                     ReportList *reports,
                                     const int *indices_ptr,
                                     const int indices_num)
{
  using namespace blender;
  bke::CurvesGeometry &curves = curves_id->geometry.wrap();
  if (!rna_CurvesGeometry_remove_curves(curves, reports, indices_ptr, indices_num)) {
    return;
  }

  /* Avoid updates for importers creating curves. */
  if (curves_id->id.us > 0) {
    DEG_id_tag_update(&curves_id->id, ID_RECALC_GEOMETRY);
    WM_main_add_notifier(NC_GEOM | ND_DATA, curves_id);
  }
}

static void rna_Curves_resize_curves(Curves *curves_id,
                                     ReportList *reports,
                                     const int *sizes_ptr,
                                     const int sizes_num,
                                     const int *indices_ptr,
                                     const int indices_num)
{
  using namespace blender;
  bke::CurvesGeometry &curves = curves_id->geometry.wrap();
  if (!rna_CurvesGeometry_resize_curves(
          curves, reports, sizes_ptr, sizes_num, indices_ptr, indices_num))
  {
    return;
  }

  /* Avoid updates for importers creating curves. */
  if (curves_id->id.us > 0) {
    DEG_id_tag_update(&curves_id->id, ID_RECALC_GEOMETRY);
    WM_main_add_notifier(NC_GEOM | ND_DATA, curves_id);
  }
}

#else

void RNA_api_curves(StructRNA *srna)
{
  FunctionRNA *func;
  PropertyRNA *parm;

  func = RNA_def_function(srna, "add_curves", "rna_Curves_add_curves");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_int_array(func,
                           "sizes",
                           1,
                           nullptr,
                           0,
                           INT_MAX,
                           "Sizes",
                           "The number of points in each curve",
                           1,
                           10000);
  RNA_def_parameter_flags(parm, PROP_DYNAMIC, PARM_REQUIRED);

  func = RNA_def_function(srna, "remove_curves", "rna_Curves_remove_curves");
  RNA_def_function_ui_description(func,
                                  "Remove all curves. If indices are provided, remove only the "
                                  "curves with the given indices.");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_int_array(func,
                           "indices",
                           1,
                           nullptr,
                           0,
                           INT_MAX,
                           "Indices",
                           "The indices of the curves to remove",
                           0,
                           10000);
  RNA_def_parameter_flags(parm, PROP_DYNAMIC, ParameterFlag(0));

  func = RNA_def_function(srna, "resize_curves", "rna_Curves_resize_curves");
  RNA_def_function_ui_description(
      func,
      "Resize all existing curves. If indices are provided, resize only the curves with the given "
      "indices. If the new size for a curve is smaller, the curve is trimmed. If "
      "the new size for a curve is larger, the new end values are default initialized.");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_int_array(func,
                           "sizes",
                           1,
                           nullptr,
                           1,
                           INT_MAX,
                           "Sizes",
                           "The number of points in each curve",
                           1,
                           10000);
  RNA_def_parameter_flags(parm, PROP_DYNAMIC, PARM_REQUIRED);
  parm = RNA_def_int_array(func,
                           "indices",
                           1,
                           nullptr,
                           0,
                           INT_MAX,
                           "Indices",
                           "The indices of the curves to resize",
                           0,
                           10000);
  RNA_def_parameter_flags(parm, PROP_DYNAMIC, ParameterFlag(0));
}

#endif
