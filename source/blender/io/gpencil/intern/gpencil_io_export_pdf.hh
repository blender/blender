/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2020 Blender Foundation
 * All rights reserved.
 */
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
  GpencilExporterPDF(const char *filename, const struct GpencilIOParams *iparams);
  bool new_document();
  bool add_newpage();
  bool add_body();
  bool write();

 protected:
 private:
  /* PDF document. */
  HPDF_Doc pdf_;
  /* PDF page. */
  HPDF_Page page_;

  bool create_document();
  bool add_page();
  void export_gpencil_layers();

  void export_stroke_to_polyline(bGPDlayer *gpl,
                                 bGPDstroke *gps,
                                 const bool is_stroke,
                                 const bool do_fill,
                                 const bool normalize);
  void color_set(bGPDlayer *gpl, const bool do_fill);
};

}  // namespace blender::io::gpencil
