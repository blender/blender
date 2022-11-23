
#include "ply_data.hh"
#include <span>

struct PlyData {
  Optional<Element> Vertices;
  Optional<Element> Faces;
  Optional<Element> Edges;
}

struct Element {
  String Name;
  Span<Data> Data;
}

struct Data {
  Span<Property> Properties;
}

abstract struct Property {
  String Name;
  PlyDataTypes Type;
}

struct ScalarProperty : Property {
  uint64_t Value;
}

struct ListProperty : Property {
  Span<uint64_t> Values;
}

enum PlyDataTypes {
  CHAR,
  UCHAR,
  SHORT,
  USHORT,
  INT,
  UINT,
  FLOAT,
  DOUBLE
};

int typeSizes[8] = {1, 1, 2, 2, 4, 4, 4, 8};
