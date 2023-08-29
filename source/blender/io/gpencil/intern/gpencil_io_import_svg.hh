/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bgpencil
 */
#include "gpencil_io_import_base.hh"

struct GpencilIOParams;
struct NSVGpath;
struct NSVGshape;
struct bGPDframe;
struct bGPdata;

#define SVG_IMPORTER_NAME "SVG Import for Grease Pencil"
#define SVG_IMPORTER_VERSION "v1.0"

namespace blender::io::gpencil {

class GpencilImporterSVG : public GpencilImporter {

 public:
  GpencilImporterSVG(const char *filepath, const GpencilIOParams *iparams);

  bool read();

 protected:
 private:
  void create_stroke(bGPdata *gpd_,
                     bGPDframe *gpf,
                     NSVGshape *shape,
                     NSVGpath *path,
                     int32_t mat_index,
                     const float matrix[4][4]);

  void convert_color(int32_t color, float r_linear_rgba[4]);
};

}  // namespace blender::io::gpencil
