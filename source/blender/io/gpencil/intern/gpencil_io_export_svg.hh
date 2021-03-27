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
#include "BLI_path_util.h"

#include "gpencil_io_export_base.hh"
#include "pugixml.hpp"

struct GpencilIOParams;

#define SVG_EXPORTER_NAME "SVG Export for Grease Pencil"
#define SVG_EXPORTER_VERSION "v1.0"

namespace blender::io::gpencil {

class GpencilExporterSVG : public GpencilExporter {

 public:
  GpencilExporterSVG(const char *filename, const struct GpencilIOParams *iparams);
  bool add_newpage();
  bool add_body();
  bool write();

 protected:
  static void add_rect(pugi::xml_node node,
                       float x,
                       float y,
                       float width,
                       float height,
                       float thickness,
                       std::string hexcolor);

  static void add_text(pugi::xml_node node,
                       float x,
                       float y,
                       std::string text,
                       const float size,
                       std::string hexcolor);

 private:
  /* XML doc. */
  pugi::xml_document main_doc_;
  /* Main document node. */
  pugi::xml_node main_node_;
  /** Frame node  */
  pugi::xml_node frame_node_;
  void create_document_header();
  void export_gpencil_layers();

  void export_stroke_to_path(struct bGPDlayer *gpl,
                             struct bGPDstroke *gps,
                             pugi::xml_node node_gpl,
                             const bool do_fill);

  void export_stroke_to_polyline(struct bGPDlayer *gpl,
                                 struct bGPDstroke *gps,
                                 pugi::xml_node node_gpl,
                                 const bool is_stroke,
                                 const bool do_fill);

  void color_string_set(struct bGPDlayer *gpl,
                        struct bGPDstroke *gps,
                        pugi::xml_node node_gps,
                        const bool do_fill);

  std::string rgb_to_hexstr(const float color[3]);
};

}  // namespace blender::io::gpencil
