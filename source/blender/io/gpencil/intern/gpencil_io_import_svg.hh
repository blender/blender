/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2020 Blender Foundation */
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
  GpencilImporterSVG(const char *filepath, const struct GpencilIOParams *iparams);

  bool read();

 protected:
 private:
  void create_stroke(struct bGPdata *gpd_,
                     struct bGPDframe *gpf,
                     struct NSVGshape *shape,
                     struct NSVGpath *path,
                     int32_t mat_index,
                     const float matrix[4][4]);

  void convert_color(int32_t color, float r_linear_rgba[4]);
};

}  // namespace blender::io::gpencil
