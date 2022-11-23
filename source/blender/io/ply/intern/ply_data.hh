/** \file
 * \ingroup ply
 */

#pragma once

namespace blender::io::ply {

struct PlyData;
struct Element;
abstract struct Property;
struct ScalarProperty : Property;
struct ListProperty : Property;

enum PlyDataTypes;

}  // namespace blender::io::ply
