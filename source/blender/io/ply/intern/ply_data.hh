/** \file
 * \ingroup ply
 */

#pragma once

#include "BLI_span.hh"
#include "string"
#include <optional>

namespace blender::io::ply {

enum PlyDataTypes { CHAR, UCHAR, SHORT, USHORT, INT, UINT, FLOAT, DOUBLE };

int typeSizes[8] = {1, 1, 2, 2, 4, 4, 4, 8};

struct Property {
  std::string Name;
  PlyDataTypes Type;
};

struct ListProperty : Property {
  Span<uint64_t> Values;
};

struct ScalarProperty : Property {
  uint64_t Value;
};

struct Data {
  Span<Property> Properties;
};

struct Element {
  std::string Name;
  Span<Data> Data;
};

struct PlyData {
  std::optional<Element> Vertices;
  std::optional<Element> Faces;
  std::optional<Element> Edges;
};

}  // namespace blender::io::ply
