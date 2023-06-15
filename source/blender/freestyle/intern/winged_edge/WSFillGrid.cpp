/* SPDX-FileCopyrightText: 2008-2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 * \brief Class to fill in a grid from a SceneGraph (uses only the WingedEdge structures)
 */

#include "WSFillGrid.h"
#include "WEdge.h"

namespace Freestyle {

void WSFillGrid::fillGrid()
{
  if (!_winged_edge || !_grid) {
    return;
  }

  vector<WShape *> wshapes = _winged_edge->getWShapes();
  vector<WVertex *> fvertices;
  vector<Vec3r> vectors;
  vector<WFace *> faces;

  for (vector<WShape *>::const_iterator it = wshapes.begin(); it != wshapes.end(); ++it) {
    faces = (*it)->GetFaceList();

    for (vector<WFace *>::const_iterator f = faces.begin(); f != faces.end(); ++f) {
      (*f)->RetrieveVertexList(fvertices);

      for (vector<WVertex *>::const_iterator wv = fvertices.begin(); wv != fvertices.end(); ++wv) {
        vectors.emplace_back((*wv)->GetVertex());
      }

      // occluder will be deleted by the grid
      Polygon3r *occluder = new Polygon3r(vectors, (*f)->GetNormal());
      occluder->setId(_polygon_id++);
      occluder->userdata = (void *)(*f);
      _grid->insertOccluder(occluder);
      vectors.clear();
      fvertices.clear();
    }
    faces.clear();
  }
}

} /* namespace Freestyle */
