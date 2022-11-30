/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#include "BKE_customdata.h"

#include "../intern/ply_data.hh"
#include "IO_ply.h"
#include "ply_file_buffer.hh"

namespace blender::io::ply {

void generate_header(FileBuffer &FB, const PlyData &plyData, const PLYExportParams &export_params)
{
  FB.write_string("ply");
  StringRef format = export_params.ascii_format ? "ascii" : "binary_little_endian";
  FB.write_string("format " + format + " 1.0");
  FB.write_header_element("vertex", plyData.vertices.size());
  FB.write_header_scalar_property("float", "x");
  FB.write_header_scalar_property("float", "y");
  FB.write_header_scalar_property("float", "z");
  FB.write_string("end_header");
  FB.write_to_file();
}

}  // namespace blender::io::ply
