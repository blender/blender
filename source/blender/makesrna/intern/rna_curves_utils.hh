/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#pragma once

/* Common utility functions for CurvesGeometry */

#ifdef RNA_RUNTIME

namespace blender {

namespace bke {
class CurvesGeometry;
}

struct ReportList;

bool rna_CurvesGeometry_add_curves(bke::CurvesGeometry &curves,
                                   ReportList *reports,
                                   const int *sizes,
                                   int sizes_num);

bool rna_CurvesGeometry_remove_curves(bke::CurvesGeometry &curves,
                                      ReportList *reports,
                                      const int *indices_ptr,
                                      int indices_num);

bool rna_CurvesGeometry_resize_curves(bke::CurvesGeometry &curves,
                                      ReportList *reports,
                                      const int *sizes_ptr,
                                      int sizes_num,
                                      const int *indices_ptr,
                                      int indices_num);

bool rna_CurvesGeometry_reorder_curves(bke::CurvesGeometry &curves,
                                       ReportList *reports,
                                       const int *reorder_indices_ptr,
                                       int reorder_indices_num);

bool rna_CurvesGeometry_set_types(bke::CurvesGeometry &curves,
                                  ReportList *reports,
                                  int type,
                                  const int *indices_ptr,
                                  int indices_num);

}  // namespace blender

#endif
