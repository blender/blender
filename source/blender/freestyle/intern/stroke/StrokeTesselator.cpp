/* SPDX-FileCopyrightText: 2008-2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 * \brief Class to build a Node Tree designed to be displayed from a set of strokes structure.
 */

#include "StrokeTesselator.h"
#include "StrokeAdvancedIterators.h"

#include "../scene_graph/NodeGroup.h"
#include "../scene_graph/NodeShape.h"
#include "../scene_graph/OrientedLineRep.h"

namespace Freestyle {

LineRep *StrokeTesselator::Tesselate(Stroke *iStroke)
{
  if (nullptr == iStroke) {
    return nullptr;
  }

  LineRep *line;
  line = new OrientedLineRep();

  Stroke::vertex_iterator v, vend;
  if (2 == iStroke->vertices_size()) {
    line->setStyle(LineRep::LINES);
    v = iStroke->vertices_begin();
    StrokeVertex *svA = (*v);
    v++;
    StrokeVertex *svB = (*v);
    Vec3r A((*svA)[0], (*svA)[1], 0);
    Vec3r B((*svB)[0], (*svB)[1], 0);
    line->AddVertex(A);
    line->AddVertex(B);
  }
  else {
    if (_overloadFrsMaterial) {
      line->setFrsMaterial(_FrsMaterial);
    }

    line->setStyle(LineRep::LINE_STRIP);

    for (v = iStroke->vertices_begin(), vend = iStroke->vertices_end(); v != vend; v++) {
      StrokeVertex *sv = (*v);
      Vec3r V((*sv)[0], (*sv)[1], 0);
      line->AddVertex(V);
    }
  }
  line->setId(iStroke->getId());
  line->ComputeBBox();

  return line;
}

template<class StrokeVertexIterator>
NodeGroup *StrokeTesselator::Tesselate(StrokeVertexIterator begin, StrokeVertexIterator end)
{
  NodeGroup *group = new NodeGroup;
  NodeShape *tshape = new NodeShape;
  group->AddChild(tshape);
  // tshape->material().setDiffuse(0.0f, 0.0f, 0.0f, 1.0f);
  tshape->setFrsMaterial(_FrsMaterial);

  for (StrokeVertexIterator c = begin, cend = end; c != cend; c++) {
    tshape->AddRep(Tesselate(*c));
  }

  return group;
}

} /* namespace Freestyle */
