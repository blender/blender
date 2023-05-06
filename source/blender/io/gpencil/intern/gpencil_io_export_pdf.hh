/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2020 Blender Foundation */
#pragma once

/** \file
 * \ingroup bgpencil
 */

#include "gpencil_io_export_base.hh"
#include "hpdf.h"

struct GpencilIOParams;
struct bGPDlayer;
struct bGPDstroke;

#define PDF_EXPORTER_NAME "PDF Exporter for Grease Pencil"
#define PDF_EXPORTER_VERSION "v1.0"

namespace blender::io::gpencil {

class GpencilExporterPDF : public GpencilExporter {

 public:
  GpencilExporterPDF(const char *filepath, const struct GpencilIOParams *iparams);
  bool new_document();
  bool add_newpage();
  bool add_body();
  bool write();

 protected:
 private:
  /** PDF document. */
  HPDF_Doc pdf_;
  /** PDF page. */
  HPDF_Page page_;

  /** Create PDF document. */
  bool create_document();
  /** Add page. */
  bool add_page();
  /** Main layer loop. */
  void export_gpencil_layers();

  /**
   * Export a stroke using poly-line or polygon
   * \param do_fill: True if the stroke is only fill
   */
  void export_stroke_to_polyline(
      bGPDlayer *gpl, bGPDstroke *gps, bool is_stroke, bool do_fill, bool normalize);
  /**
   * Set color.
   * \param do_fill: True if the stroke is only fill.
   */
  void color_set(bGPDlayer *gpl, bool do_fill);
};

}  // namespace blender::io::gpencil
