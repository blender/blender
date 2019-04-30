/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup freestyle
 * \brief Class to define a cell grid surrounding the projected image of a scene
 */

#include "CulledOccluderSource.h"

#include "../geometry/GridHelpers.h"

#include "BKE_global.h"

namespace Freestyle {

CulledOccluderSource::CulledOccluderSource(const GridHelpers::Transform &t,
                                           WingedEdge &we,
                                           ViewMap &viewMap,
                                           bool extensiveFEdgeSearch)
    : OccluderSource(t, we), rejected(0), gridSpaceOccluderProsceniumInitialized(false)
{
  cullViewEdges(viewMap, extensiveFEdgeSearch);

  // If we have not found any visible FEdges during our cull, then there is nothing to iterate
  // over. Short-circuit everything.
  valid = gridSpaceOccluderProsceniumInitialized;

  if (valid && !testCurrent()) {
    next();
  }
}

CulledOccluderSource::~CulledOccluderSource()
{
}

bool CulledOccluderSource::testCurrent()
{
  if (valid) {
    // The test for gridSpaceOccluderProsceniumInitialized should not be necessary
    return gridSpaceOccluderProsceniumInitialized &&
           GridHelpers::insideProscenium(gridSpaceOccluderProscenium, cachedPolygon);
  }
  return false;
}

bool CulledOccluderSource::next()
{
  while (OccluderSource::next()) {
    if (testCurrent()) {
      ++rejected;
      return true;
    }
  }
  if (G.debug & G_DEBUG_FREESTYLE) {
    std::cout << "Finished generating occluders.  Rejected " << rejected << " faces." << std::endl;
  }
  return false;
}

void CulledOccluderSource::getOccluderProscenium(real proscenium[4])
{
  for (unsigned int i = 0; i < 4; ++i) {
    proscenium[i] = gridSpaceOccluderProscenium[i];
  }
}

static inline real distance2D(const Vec3r &point, const real origin[2])
{
  return ::hypot((point[0] - origin[0]), (point[1] - origin[1]));
}

static inline bool crossesProscenium(real proscenium[4], FEdge *fe)
{
  Vec2r min(proscenium[0], proscenium[2]);
  Vec2r max(proscenium[1], proscenium[3]);
  Vec2r A(fe->vertexA()->getProjectedX(), fe->vertexA()->getProjectedY());
  Vec2r B(fe->vertexB()->getProjectedX(), fe->vertexB()->getProjectedY());

  return GeomUtils::intersect2dSeg2dArea(min, max, A, B);
}

static inline bool insideProscenium(real proscenium[4], const Vec3r &point)
{
  return !(point[0] < proscenium[0] || point[0] > proscenium[1] || point[1] < proscenium[2] ||
           point[1] > proscenium[3]);
}

void CulledOccluderSource::cullViewEdges(ViewMap &viewMap, bool extensiveFEdgeSearch)
{
  // Cull view edges by marking them as non-displayable.
  // This avoids the complications of trying to delete edges from the ViewMap.

  // Non-displayable view edges will be skipped over during visibility calculation.

  // View edges will be culled according to their position w.r.t. the viewport proscenium (viewport
  // + 5% border, or some such).

  // Get proscenium boundary for culling
  real viewProscenium[4];
  GridHelpers::getDefaultViewProscenium(viewProscenium);
  real prosceniumOrigin[2];
  prosceniumOrigin[0] = (viewProscenium[1] - viewProscenium[0]) / 2.0;
  prosceniumOrigin[1] = (viewProscenium[3] - viewProscenium[2]) / 2.0;
  if (G.debug & G_DEBUG_FREESTYLE) {
    cout << "Proscenium culling:" << endl;
    cout << "Proscenium: [" << viewProscenium[0] << ", " << viewProscenium[1] << ", "
         << viewProscenium[2] << ", " << viewProscenium[3] << "]" << endl;
    cout << "Origin: [" << prosceniumOrigin[0] << ", " << prosceniumOrigin[1] << "]" << endl;
  }

  // A separate occluder proscenium will also be maintained, starting out the same as the viewport
  // proscenium, and expanding as necessary so that it encompasses the center point of at least one
  // feature edge in each retained view edge. The occluder proscenium will be used later to cull
  // occluding triangles before they are inserted into the Grid. The occluder proscenium starts out
  // the same size as the view proscenium
  GridHelpers::getDefaultViewProscenium(occluderProscenium);

  // XXX Freestyle is inconsistent in its use of ViewMap::viewedges_container and
  // vector<ViewEdge*>::iterator. Probably all occurences of vector<ViewEdge*>::iterator should be
  // replaced ViewMap::viewedges_container throughout the code. For each view edge
  ViewMap::viewedges_container::iterator ve, veend;

  for (ve = viewMap.ViewEdges().begin(), veend = viewMap.ViewEdges().end(); ve != veend; ve++) {
    // Overview:
    //    Search for a visible feature edge
    //    If none: mark view edge as non-displayable
    //    Otherwise:
    //        Find a feature edge with center point inside occluder proscenium.
    //        If none exists, find the feature edge with center point closest to viewport origin.
    //            Expand occluder proscenium to enclose center point.

    // For each feature edge, while bestOccluderTarget not found and view edge not visibile
    bool bestOccluderTargetFound = false;
    FEdge *bestOccluderTarget = NULL;
    real bestOccluderDistance = 0.0;
    FEdge *festart = (*ve)->fedgeA();
    FEdge *fe = festart;
    // All ViewEdges start culled
    (*ve)->setIsInImage(false);

    // For simple visibility calculation: mark a feature edge that is known to have a center point
    // inside the occluder proscenium. Cull all other feature edges.
    do {
      // All FEdges start culled
      fe->setIsInImage(false);

      // Look for the visible edge that can most easily be included in the occluder proscenium.
      if (!bestOccluderTargetFound) {
        // If center point is inside occluder proscenium,
        if (insideProscenium(occluderProscenium, fe->center2d())) {
          // Use this feature edge for visibility deterimination
          fe->setIsInImage(true);
          expandGridSpaceOccluderProscenium(fe);
          // Mark bestOccluderTarget as found
          bestOccluderTargetFound = true;
          bestOccluderTarget = fe;
        }
        else {
          real d = distance2D(fe->center2d(), prosceniumOrigin);
          // If center point is closer to viewport origin than current target
          if (bestOccluderTarget == NULL || d < bestOccluderDistance) {
            // Then store as bestOccluderTarget
            bestOccluderDistance = d;
            bestOccluderTarget = fe;
          }
        }
      }

      // If feature edge crosses the view proscenium
      if (!(*ve)->isInImage() && crossesProscenium(viewProscenium, fe)) {
        // Then the view edge will be included in the image
        (*ve)->setIsInImage(true);
      }
      fe = fe->nextEdge();
    } while (fe != NULL && fe != festart && !(bestOccluderTargetFound && (*ve)->isInImage()));

    // Either we have run out of FEdges, or we already have the one edge we need to determine
    // visibility Cull all remaining edges.
    while (fe != NULL && fe != festart) {
      fe->setIsInImage(false);
      fe = fe->nextEdge();
    }

    // If bestOccluderTarget was not found inside the occluder proscenium,
    // we need to expand the occluder proscenium to include it.
    if ((*ve)->isInImage() && bestOccluderTarget != NULL && !bestOccluderTargetFound) {
      // Expand occluder proscenium to enclose bestOccluderTarget
      Vec3r point = bestOccluderTarget->center2d();
      if (point[0] < occluderProscenium[0]) {
        occluderProscenium[0] = point[0];
      }
      else if (point[0] > occluderProscenium[1]) {
        occluderProscenium[1] = point[0];
      }
      if (point[1] < occluderProscenium[2]) {
        occluderProscenium[2] = point[1];
      }
      else if (point[1] > occluderProscenium[3]) {
        occluderProscenium[3] = point[1];
      }
      // Use bestOccluderTarget for visibility determination
      bestOccluderTarget->setIsInImage(true);
    }
  }

  // We are done calculating the occluder proscenium.
  // Expand the occluder proscenium by an epsilon to avoid rounding errors.
  const real epsilon = 1.0e-6;
  occluderProscenium[0] -= epsilon;
  occluderProscenium[1] += epsilon;
  occluderProscenium[2] -= epsilon;
  occluderProscenium[3] += epsilon;

  // For "Normal" or "Fast" style visibility computation only:

  // For more detailed visibility calculation, make a second pass through the view map, marking all
  // feature edges with center points inside the final occluder proscenium. All of these feature
  // edges can be considered during visibility calculation.

  // So far we have only found one FEdge per ViewEdge. The "Normal" and "Fast" styles of visibility
  // computation want to consider many FEdges for each ViewEdge. Here we re-scan the view map to
  // find any usable FEdges that we skipped on the first pass, or that have become usable because
  // the occluder proscenium has been expanded since the edge was visited on the first pass.
  if (extensiveFEdgeSearch) {
    // For each view edge,
    for (ve = viewMap.ViewEdges().begin(), veend = viewMap.ViewEdges().end(); ve != veend; ve++) {
      if (!(*ve)->isInImage()) {
        continue;
      }
      // For each feature edge,
      FEdge *festart = (*ve)->fedgeA();
      FEdge *fe = festart;
      do {
        // If not (already) visible and center point inside occluder proscenium,
        if (!fe->isInImage() && insideProscenium(occluderProscenium, fe->center2d())) {
          // Use the feature edge for visibility determination
          fe->setIsInImage(true);
          expandGridSpaceOccluderProscenium(fe);
        }
        fe = fe->nextEdge();
      } while (fe != NULL && fe != festart);
    }
  }

  // Up until now, all calculations have been done in camera space.
  // However, the occluder source's iteration and the grid that consumes the occluders both work in
  // gridspace, so we need a version of the occluder proscenium in gridspace. Set the gridspace
  // occlude proscenium
}

void CulledOccluderSource::expandGridSpaceOccluderProscenium(FEdge *fe)
{
  if (gridSpaceOccluderProsceniumInitialized) {
    GridHelpers::expandProscenium(gridSpaceOccluderProscenium, transform(fe->center3d()));
  }
  else {
    const Vec3r &point = transform(fe->center3d());
    gridSpaceOccluderProscenium[0] = gridSpaceOccluderProscenium[1] = point[0];
    gridSpaceOccluderProscenium[2] = gridSpaceOccluderProscenium[3] = point[1];
    gridSpaceOccluderProsceniumInitialized = true;
  }
}

} /* namespace Freestyle */
