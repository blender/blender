/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include "DNA_curves_types.h"

#include "RNA_define.hh"

#include "RNA_enum_types.hh"

#include "rna_internal.hh" /* own include */

#ifdef RNA_RUNTIME

#  include "BKE_curves.hh"
#  include "BKE_report.hh"

#  include "BLI_index_mask.hh"

#  include "ED_curves.hh"

#  include "rna_curves_utils.hh"

/* Common `CurvesGeometry` API functions. */

using blender::IndexMask;
using blender::IndexMaskMemory;
using blender::IndexRange;
using blender::Span;

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

static std::optional<IndexMask> rna_indices_to_mask(const IndexRange universe,
                                                    const int *indices_ptr,
                                                    const int indices_num,
                                                    ReportList &reports,
                                                    IndexMaskMemory &memory)
{
  if (!indices_ptr) {
    return IndexMask(universe);
  }
  const Span<int> indices(indices_ptr, indices_num);
  if (std::any_of(indices.begin(), indices.end(), [&](const int index) {
        return !universe.contains(index);
      }))
  {
    BKE_report(&reports, RPT_ERROR, "Indices must be in range");
    return std::nullopt;
  }
  if (!std::is_sorted(indices.begin(), indices.end())) {
    BKE_report(&reports, RPT_ERROR, "Indices must be sorted in ascending order");
    return std::nullopt;
  }
  if (std::adjacent_find(indices.begin(), indices.end(), std::greater_equal<int>()) !=
      indices.end())
  {
    BKE_report(&reports, RPT_ERROR, "Indices can't have duplicates");
    return std::nullopt;
  }
  return IndexMask::from_indices(indices, memory);
}

bool rna_CurvesGeometry_remove_curves(blender::bke::CurvesGeometry &curves,
                                      ReportList *reports,
                                      const int *indices_ptr,
                                      const int indices_num)
{
  using namespace blender;
  IndexMaskMemory memory;
  const std::optional<IndexMask> curves_to_delete = rna_indices_to_mask(
      curves.curves_range(), indices_ptr, indices_num, *reports, memory);
  if (!curves_to_delete) {
    return false;
  }
  curves.remove_curves(*curves_to_delete, {});
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
  const std::optional<IndexMask> curves_to_resize = rna_indices_to_mask(
      curves.curves_range(), indices_ptr, indices_num, *reports, memory);
  if (!curves_to_resize) {
    return false;
  }
  if (curves_to_resize->size() != sizes_num) {
    BKE_report(reports, RPT_ERROR, "Length of sizes must be the same as the selection size");
    return false;
  }
  ed::curves::resize_curves(curves, *curves_to_resize, {sizes_ptr, sizes_num});
  return true;
}

bool rna_CurvesGeometry_set_types(blender::bke::CurvesGeometry &curves,
                                  ReportList *reports,
                                  const int type,
                                  const int *indices_ptr,
                                  const int indices_num)
{
  IndexMaskMemory memory;
  const std::optional<IndexMask> selection = rna_indices_to_mask(
      curves.curves_range(), indices_ptr, indices_num, *reports, memory);
  if (!selection) {
    return false;
  }
  curves.fill_curve_types(*selection, CurveType(type));
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

static void rna_Curves_set_types(Curves *curves_id,
                                 ReportList *reports,
                                 const int type,
                                 const int *indices_ptr,
                                 const int indices_num)
{
  using namespace blender;
  bke::CurvesGeometry &curves = curves_id->geometry.wrap();
  if (!rna_CurvesGeometry_set_types(curves, reports, type, indices_ptr, indices_num)) {
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

  func = RNA_def_function(srna, "set_types", "rna_Curves_set_types");
  RNA_def_function_ui_description(func,
                                  "Set the curve type. If indices are provided, set only the "
                                  "types with the given curve indices.");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_enum(func, "type", rna_enum_curves_type_items, CURVE_TYPE_CATMULL_ROM, "Type", "");
  parm = RNA_def_int_array(func,
                           "indices",
                           1,
                           nullptr,
                           0,
                           INT_MAX,
                           "Indices",
                           "The indices of the curves to resize",
                           0,
                           INT_MAX);
  RNA_def_parameter_flags(parm, PROP_DYNAMIC, ParameterFlag(0));
}

#endif
