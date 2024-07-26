/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#pragma once

/* Common utility functions for CurvesGeometry */

#ifdef RNA_RUNTIME

namespace blender::bke {
class CurvesGeometry;
}

struct ReportList;

bool rna_CurvesGeometry_add_curves(blender::bke::CurvesGeometry &curves,
                                   ReportList *reports,
                                   const int *sizes,
                                   int sizes_num);

bool rna_CurvesGeometry_remove_curves(blender::bke::CurvesGeometry &curves,
                                      ReportList *reports,
                                      const int *indices_ptr,
                                      int indices_num);

bool rna_CurvesGeometry_resize_curves(blender::bke::CurvesGeometry &curves,
                                      ReportList *reports,
                                      const int *sizes_ptr,
                                      int sizes_num,
                                      const int *indices_ptr,
                                      int indices_num);

#endif
