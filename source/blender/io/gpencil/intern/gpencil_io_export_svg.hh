/* SPDX-FileCopyrightText: 2020 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
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
  GpencilExporterSVG(const char *filepath, const GpencilIOParams *iparams);
  bool add_newpage();
  bool add_body();
  bool write();

 protected:
  /**
   * Create a SVG rectangle
   * \param node: Parent node
   * \param x: X location
   * \param y: Y location
   * \param width: width of the rectangle
   * \param height: Height of the rectangle
   * \param thickness: Thickness of the line
   * \param hexcolor: Color of the line
   */
  static void add_rect(pugi::xml_node node,
                       float x,
                       float y,
                       float width,
                       float height,
                       float thickness,
                       std::string hexcolor);

  /**
   * Create SVG text
   * \param node: Parent node
   * \param x: X location
   * \param y: Y location
   * \param text: Text to include
   * \param size: Size of the text
   * \param hexcolor: Color of the text
   */
  static void add_text(
      pugi::xml_node node, float x, float y, std::string text, float size, std::string hexcolor);

 private:
  /** XML doc. */
  pugi::xml_document main_doc_;
  /** Main document node. */
  pugi::xml_node main_node_;
  /** Frame node. */
  pugi::xml_node frame_node_;
  /** Create document header and main SVG node. */
  void create_document_header();
  /** Main layer loop. */
  void export_gpencil_layers();

  /**
   * Export a stroke using SVG path
   * \param node_gpl: Node of the layer.
   * \param do_fill: True if the stroke is only fill
   */
  void export_stroke_to_path(bGPDlayer *gpl,
                             bGPDstroke *gps,
                             pugi::xml_node node_gpl,
                             bool do_fill);

  /**
   * Export a stroke using poly-line or polygon
   * \param node_gpl: Node of the layer.
   * \param do_fill: True if the stroke is only fill
   */
  void export_stroke_to_polyline(
      bGPDlayer *gpl, bGPDstroke *gps, pugi::xml_node node_gpl, bool is_stroke, bool do_fill);

  /**
   * Set color SVG string for stroke
   * \param node_gps: Stroke node.
   * \param do_fill: True if the stroke is only fill.
   */
  void color_string_set(bGPDlayer *gpl, bGPDstroke *gps, pugi::xml_node node_gps, bool do_fill);

  /** Convert a color to Hex value (#FFFFFF). */
  std::string rgb_to_hexstr(const float color[3]);
};

}  // namespace blender::io::gpencil
