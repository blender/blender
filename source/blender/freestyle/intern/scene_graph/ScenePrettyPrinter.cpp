/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 * \brief Class to display textual information about a scene graph.
 */

#include <iomanip>

#include "IndexedFaceSet.h"
#include "ScenePrettyPrinter.h"

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

void ScenePrettyPrinter::visitNodeShapeBefore(NodeShape &UNUSED(shape))
{
  increaseSpace();
}

void ScenePrettyPrinter::visitNodeShapeAfter(NodeShape &UNUSED(shape))
{
  decreaseSpace();
}

void ScenePrettyPrinter::visitNodeGroupBefore(NodeGroup &UNUSED(group))
{
  increaseSpace();
}

void ScenePrettyPrinter::visitNodeGroupAfter(NodeGroup &UNUSED(group))
{
  decreaseSpace();
}

void ScenePrettyPrinter::visitNodeDrawingStyleBefore(NodeDrawingStyle &UNUSED(style))
{
  increaseSpace();
}

void ScenePrettyPrinter::visitNodeDrawingStyleAfter(NodeDrawingStyle &UNUSED(style))
{
  decreaseSpace();
}

void ScenePrettyPrinter::visitNodeTransformBefore(NodeTransform &UNUSED(transform))
{
  increaseSpace();
}

void ScenePrettyPrinter::visitNodeTransformAfter(NodeTransform &UNUSED(transform))
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
  unsigned vsize = ifs.vsize();

  _ofs << _space << "IndexedFaceSet" << endl;
  const float *p = vertices;
  for (unsigned int i = 0; i < vsize / 3; i++) {
    _ofs << _space << "  " << setw(3) << setfill('0') << i << ": " << p[0] << ", " << p[1] << ", "
         << p[2] << endl;
    p += 3;
  }
}

} /* namespace Freestyle */
