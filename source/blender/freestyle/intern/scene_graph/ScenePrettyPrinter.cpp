/* SPDX-FileCopyrightText: 2012-2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 * \brief Class to display textual information about a scene graph.
 */

#include <iomanip>

#include "IndexedFaceSet.h"
#include "ScenePrettyPrinter.h"

#include "BLI_sys_types.h"

namespace Freestyle {

#define VISIT(CLASS) \
  void ScenePrettyPrinter::visit##CLASS(CLASS &) \
  { \
    _ofs << _space << #CLASS << endl; \
  }

VISIT(Node)
VISIT(NodeShape)
VISIT(NodeGroup)
VISIT(NodeLight)
VISIT(NodeDrawingStyle)
VISIT(NodeTransform)

void ScenePrettyPrinter::visitNodeShapeBefore(NodeShape & /*shape*/)
{
  increaseSpace();
}

void ScenePrettyPrinter::visitNodeShapeAfter(NodeShape & /*shape*/)
{
  decreaseSpace();
}

void ScenePrettyPrinter::visitNodeGroupBefore(NodeGroup & /*group*/)
{
  increaseSpace();
}

void ScenePrettyPrinter::visitNodeGroupAfter(NodeGroup & /*group*/)
{
  decreaseSpace();
}

void ScenePrettyPrinter::visitNodeDrawingStyleBefore(NodeDrawingStyle & /*style*/)
{
  increaseSpace();
}

void ScenePrettyPrinter::visitNodeDrawingStyleAfter(NodeDrawingStyle & /*style*/)
{
  decreaseSpace();
}

void ScenePrettyPrinter::visitNodeTransformBefore(NodeTransform & /*transform*/)
{
  increaseSpace();
}

void ScenePrettyPrinter::visitNodeTransformAfter(NodeTransform & /*transform*/)
{
  decreaseSpace();
}

VISIT(LineRep)
VISIT(OrientedLineRep)
VISIT(TriangleRep)
VISIT(VertexRep)

void ScenePrettyPrinter::visitIndexedFaceSet(IndexedFaceSet &ifs)
{
  const float *vertices = ifs.vertices();
  uint vsize = ifs.vsize();

  _ofs << _space << "IndexedFaceSet" << endl;
  const float *p = vertices;
  for (uint i = 0; i < vsize / 3; i++) {
    _ofs << _space << "  " << setw(3) << setfill('0') << i << ": " << p[0] << ", " << p[1] << ", "
         << p[2] << endl;
    p += 3;
  }
}

} /* namespace Freestyle */
