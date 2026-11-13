/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#pragma once

#include <optional>

#include "BKE_attribute_enums.hh"

#include "ply_data.hh"

namespace blender::io::ply {

class FileBuffer;

struct PlyCustomAttributeProperty {
  std::string name;
  PlyDataTypes type;
  const char *data;
  int stride;
};

struct PlyCustomAttributeLayout {
  PlyDataTypes scalar_type;
  Span<StringRef> suffixes;
};

std::optional<PlyCustomAttributeLayout> custom_attribute_layout(bke::AttrType type);
Vector<PlyCustomAttributeProperty> custom_attribute_properties(const PlyData &ply_data);

void write_vertices(FileBuffer &buffer, const PlyData &ply_data);

void write_faces(FileBuffer &buffer, const PlyData &ply_data);

void write_edges(FileBuffer &buffer, const PlyData &ply_data);

}  // namespace blender::io::ply
