/* SPDX-FileCopyrightText: 2011-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 * \brief Class to define a cell grid surrounding the projected image of a scene
 */

#include <algorithm>

#include "OccluderSource.h"

#include "BLI_sys_types.h"

#include "BKE_global.h"

namespace Freestyle {

OccluderSource::OccluderSource(const GridHelpers::Transform &t, WingedEdge &we)
    : wingedEdge(we), valid(false), transform(t)
{
  begin();
}

OccluderSource::~OccluderSource() = default;

void OccluderSource::buildCachedPolygon()
{
  vector<Vec3r> vertices(GridHelpers::enumerateVertices((*currentFace)->getEdgeList()));
  // This doesn't work, because our functor's polymorphism won't survive the copy:
  // std::transform(vertices.begin(), vertices.end(), vertices.begin(), transform);
  // so we have to do:
  for (vector<Vec3r>::iterator i = vertices.begin(); i != vertices.end(); ++i) {
    (*i) = transform(*i);
  }
  cachedPolygon = Polygon3r(vertices, transform((*currentFace)->GetNormal()));
}

void OccluderSource::begin()
{
  vector<WShape *> &wshapes = wingedEdge.getWShapes();
  currentShape = wshapes.begin();
  shapesEnd = wshapes.end();
  valid = false;
  if (currentShape != shapesEnd) {
    vector<WFace *> &wFaces = (*currentShape)->GetFaceList();
    currentFace = wFaces.begin();
    facesEnd = wFaces.end();

    if (currentFace != facesEnd) {
      buildCachedPolygon();
      valid = true;
    }
  }
}

bool OccluderSource::next()
{
  if (valid) {
    ++currentFace;
    while (currentFace == facesEnd) {
      ++currentShape;
      if (currentShape == shapesEnd) {
        valid = false;
        return false;
      }

      vector<WFace *> &wFaces = (*currentShape)->GetFaceList();
      currentFace = wFaces.begin();
      facesEnd = wFaces.end();
    }
    buildCachedPolygon();
    return true;
  }
  return false;
}

bool OccluderSource::isValid()
{
  // Or:
  // return currentShapes != shapesEnd && currentFace != facesEnd;
  return valid;
}

WFace *OccluderSource::getWFace()
{
  return valid ? *currentFace : nullptr;
}

Polygon3r OccluderSource::getCameraSpacePolygon()
{
  return Polygon3r(GridHelpers::enumerateVertices((*currentFace)->getEdgeList()),
                   (*currentFace)->GetNormal());
}

Polygon3r &OccluderSource::getGridSpacePolygon()
{
  return cachedPolygon;
}

void OccluderSource::getOccluderProscenium(real proscenium[4])
{
  begin();
  const Vec3r &initialPoint = cachedPolygon.getVertices()[0];
  proscenium[0] = proscenium[1] = initialPoint[0];
  proscenium[2] = proscenium[3] = initialPoint[1];
  while (isValid()) {
    GridHelpers::expandProscenium(proscenium, cachedPolygon);
    next();
  }
  if (G.debug & G_DEBUG_FREESTYLE) {
    cout << "Proscenium: (" << proscenium[0] << ", " << proscenium[1] << ", " << proscenium[2]
         << ", " << proscenium[3] << ")" << endl;
  }
}

real OccluderSource::averageOccluderArea()
{
  real area = 0.0;
  uint numFaces = 0;
  for (begin(); isValid(); next()) {
    Vec3r min, max;
    cachedPolygon.getBBox(min, max);
    area += (max[0] - min[0]) * (max[1] - min[1]);
    ++numFaces;
  }
  area /= numFaces;
  return area;
}

} /* namespace Freestyle */
