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
 * \brief Class to build silhouette edges from a Winged-Edge structure
 */

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <sstream>

#include "FRS_freestyle.h"

#include "BoxGrid.h"
#include "CulledOccluderSource.h"
#include "HeuristicGridDensityProviderFactory.h"
#include "OccluderSource.h"
#include "SphericalGrid.h"
#include "ViewMapBuilder.h"

#include "../geometry/GridHelpers.h"
#include "../geometry/GeomUtils.h"

#include "../winged_edge/WFillGrid.h"

#include "BKE_global.h"

namespace Freestyle {

// XXX Grmll... G is used as template's typename parameter :/
static const Global &_global = G;

#define LOGGING 0

using namespace std;

template<typename G, typename I>
static void findOccludee(FEdge *fe,
                         G & /*grid*/,
                         I &occluders,
                         real epsilon,
                         WFace **oaWFace,
                         Vec3r &u,
                         Vec3r &A,
                         Vec3r &origin,
                         Vec3r &edgeDir,
                         vector<WVertex *> &faceVertices)
{
  WFace *face = NULL;
  if (fe->isSmooth()) {
    FEdgeSmooth *fes = dynamic_cast<FEdgeSmooth *>(fe);
    face = (WFace *)fes->face();
  }
  WFace *oface;
  bool skipFace;

  WVertex::incoming_edge_iterator ie;

  *oaWFace = NULL;
  if (((fe)->getNature() & Nature::SILHOUETTE) || ((fe)->getNature() & Nature::BORDER)) {
    // we cast a ray from A in the same direction but looking behind
    Vec3r v(-u[0], -u[1], -u[2]);
    bool noIntersection = true;
    real mint = FLT_MAX;

    for (occluders.initAfterTarget(); occluders.validAfterTarget(); occluders.nextOccludee()) {
#if LOGGING
      if (_global.debug & G_DEBUG_FREESTYLE) {
        cout << "\t\tEvaluating intersection for occludee " << occluders.getWFace() << " and ray "
             << A << " * " << u << endl;
      }
#endif
      oface = occluders.getWFace();
      Polygon3r *p = occluders.getCameraSpacePolygon();
      real d = -((p->getVertices())[0] * p->getNormal());
      real t, t_u, t_v;

      if (0 != face) {
        skipFace = false;

        if (face == oface) {
          continue;
        }

        if (faceVertices.empty()) {
          continue;
        }

        for (vector<WVertex *>::iterator fv = faceVertices.begin(), fvend = faceVertices.end();
             fv != fvend;
             ++fv) {
          if ((*fv)->isBoundary()) {
            continue;
          }
          WVertex::incoming_edge_iterator iebegin = (*fv)->incoming_edges_begin();
          WVertex::incoming_edge_iterator ieend = (*fv)->incoming_edges_end();
          for (ie = iebegin; ie != ieend; ++ie) {
            if ((*ie) == 0) {
              continue;
            }

            WFace *sface = (*ie)->GetbFace();
            if (sface == oface) {
              skipFace = true;
              break;
            }
          }
          if (skipFace) {
            break;
          }
        }
        if (skipFace) {
          continue;
        }
      }
      else {
        // check whether the edge and the polygon plane are coincident:
        //-------------------------------------------------------------
        // first let us compute the plane equation.
        if (GeomUtils::COINCIDENT ==
            GeomUtils::intersectRayPlane(origin, edgeDir, p->getNormal(), d, t, epsilon)) {
#if LOGGING
          if (_global.debug & G_DEBUG_FREESTYLE) {
            cout << "\t\tRejecting occluder for target coincidence." << endl;
          }
#endif
          continue;
        }
      }

      if (p->rayIntersect(A, v, t, t_u, t_v)) {
#if LOGGING
        if (_global.debug & G_DEBUG_FREESTYLE) {
          cout << "\t\tRay " << A << " * " << v << " intersects at time " << t << endl;
          cout << "\t\t(v * normal) == " << (v * p->getNormal()) << " for normal "
               << p->getNormal() << endl;
        }
#endif
        if (fabs(v * p->getNormal()) > 0.0001) {
          if ((t > 0.0) /* && (t<1.0) */) {
            if (t < mint) {
              *oaWFace = oface;
              mint = t;
              noIntersection = false;
              fe->setOccludeeIntersection(Vec3r(A + t * v));
#if LOGGING
              if (_global.debug & G_DEBUG_FREESTYLE) {
                cout << "\t\tIs occludee" << endl;
              }
#endif
            }
          }
        }

        occluders.reportDepth(A, v, t);
      }
    }

    if (noIntersection) {
      *oaWFace = NULL;
    }
  }
}

template<typename G, typename I>
static void findOccludee(FEdge *fe, G &grid, real epsilon, ViewEdge * /*ve*/, WFace **oaFace)
{
  Vec3r A;
  Vec3r edgeDir;
  Vec3r origin;
  A = Vec3r(((fe)->vertexA()->point3D() + (fe)->vertexB()->point3D()) / 2.0);
  edgeDir = Vec3r((fe)->vertexB()->point3D() - (fe)->vertexA()->point3D());
  edgeDir.normalize();
  origin = Vec3r((fe)->vertexA()->point3D());
  Vec3r u;
  if (grid.orthographicProjection()) {
    u = Vec3r(0.0, 0.0, grid.viewpoint().z() - A.z());
  }
  else {
    u = Vec3r(grid.viewpoint() - A);
  }
  u.normalize();

  vector<WVertex *> faceVertices;

  WFace *face = NULL;
  if (fe->isSmooth()) {
    FEdgeSmooth *fes = dynamic_cast<FEdgeSmooth *>(fe);
    face = (WFace *)fes->face();
  }

  if (face) {
    face->RetrieveVertexList(faceVertices);
  }

  I occluders(grid, A, epsilon);
  findOccludee<G, I>(fe, grid, occluders, epsilon, oaFace, u, A, origin, edgeDir, faceVertices);
}

// computeVisibility takes a pointer to foundOccluders, instead of using a reference,
// so that computeVeryFastVisibility can skip the AddOccluders step with minimal overhead.
template<typename G, typename I>
static int computeVisibility(ViewMap *viewMap,
                             FEdge *fe,
                             G &grid,
                             real epsilon,
                             ViewEdge * /*ve*/,
                             WFace **oaWFace,
                             set<ViewShape *> *foundOccluders)
{
  int qi = 0;

  Vec3r center;
  Vec3r edgeDir;
  Vec3r origin;

  center = fe->center3d();
  edgeDir = Vec3r(fe->vertexB()->point3D() - fe->vertexA()->point3D());
  edgeDir.normalize();
  origin = Vec3r(fe->vertexA()->point3D());

  Vec3r vp;
  if (grid.orthographicProjection()) {
    vp = Vec3r(center.x(), center.y(), grid.viewpoint().z());
  }
  else {
    vp = Vec3r(grid.viewpoint());
  }
  Vec3r u(vp - center);
  real raylength = u.norm();
  u.normalize();

  WFace *face = NULL;
  if (fe->isSmooth()) {
    FEdgeSmooth *fes = dynamic_cast<FEdgeSmooth *>(fe);
    face = (WFace *)fes->face();
  }
  vector<WVertex *> faceVertices;
  WVertex::incoming_edge_iterator ie;

  WFace *oface;
  bool skipFace;

  if (face) {
    face->RetrieveVertexList(faceVertices);
  }

  I occluders(grid, center, epsilon);

  for (occluders.initBeforeTarget(); occluders.validBeforeTarget(); occluders.nextOccluder()) {
    // If we're dealing with an exact silhouette, check whether we must take care of this occluder
    // of not. (Indeed, we don't consider the occluders that share at least one vertex with the
    // face containing this edge).
    //-----------
    oface = occluders.getWFace();
    Polygon3r *p = occluders.getCameraSpacePolygon();
    real t, t_u, t_v;
#if LOGGING
    if (_global.debug & G_DEBUG_FREESTYLE) {
      cout << "\t\tEvaluating intersection for occluder " << (p->getVertices())[0]
           << (p->getVertices())[1] << (p->getVertices())[2] << endl
           << "\t\t\tand ray " << vp << " * " << u << " (center " << center << ")" << endl;
    }
#endif

#if LOGGING
    Vec3r v(vp - center);
    real rl = v.norm();
    v.normalize();
    vector<Vec3r> points;
    // Iterate over vertices, storing projections in points
    for (vector<WOEdge *>::const_iterator woe = oface->getEdgeList().begin(),
                                          woend = oface->getEdgeList().end();
         woe != woend;
         woe++) {
      points.push_back(Vec3r((*woe)->GetaVertex()->GetVertex()));
    }
    Polygon3r p1(points, oface->GetNormal());
    Vec3r v1((p1.getVertices())[0]);
    real d = -(v1 * p->getNormal());
    if (_global.debug & G_DEBUG_FREESTYLE) {
      cout << "\t\tp:  " << (p->getVertices())[0] << (p->getVertices())[1] << (p->getVertices())[2]
           << ", norm: " << p->getNormal() << endl;
      cout << "\t\tp1: " << (p1.getVertices())[0] << (p1.getVertices())[1] << (p1.getVertices())[2]
           << ", norm: " << p1.getNormal() << endl;
    }
#else
    real d = -((p->getVertices())[0] * p->getNormal());
#endif

    if (face) {
#if LOGGING
      if (_global.debug & G_DEBUG_FREESTYLE) {
        cout << "\t\tDetermining face adjacency...";
      }
#endif
      skipFace = false;

      if (face == oface) {
#if LOGGING
        if (_global.debug & G_DEBUG_FREESTYLE) {
          cout << "  Rejecting occluder for face concurrency." << endl;
        }
#endif
        continue;
      }

      for (vector<WVertex *>::iterator fv = faceVertices.begin(), fvend = faceVertices.end();
           fv != fvend;
           ++fv) {
        if ((*fv)->isBoundary()) {
          continue;
        }

        WVertex::incoming_edge_iterator iebegin = (*fv)->incoming_edges_begin();
        WVertex::incoming_edge_iterator ieend = (*fv)->incoming_edges_end();
        for (ie = iebegin; ie != ieend; ++ie) {
          if ((*ie) == 0) {
            continue;
          }

          WFace *sface = (*ie)->GetbFace();
          // WFace *sfacea = (*ie)->GetaFace();
          // if ((sface == oface) || (sfacea == oface))
          if (sface == oface) {
            skipFace = true;
            break;
          }
        }
        if (skipFace) {
          break;
        }
      }
      if (skipFace) {
#if LOGGING
        if (_global.debug & G_DEBUG_FREESTYLE) {
          cout << "  Rejecting occluder for face adjacency." << endl;
        }
#endif
        continue;
      }
    }
    else {
      // check whether the edge and the polygon plane are coincident:
      //-------------------------------------------------------------
      // first let us compute the plane equation.
      if (GeomUtils::COINCIDENT ==
          GeomUtils::intersectRayPlane(origin, edgeDir, p->getNormal(), d, t, epsilon)) {
#if LOGGING
        if (_global.debug & G_DEBUG_FREESTYLE) {
          cout << "\t\tRejecting occluder for target coincidence." << endl;
        }
#endif
        continue;
      }
    }

#if LOGGING
    if (_global.debug & G_DEBUG_FREESTYLE) {
      real x;
      if (p1.rayIntersect(center, v, x, t_u, t_v)) {
        cout << "\t\tRay should intersect at time " << (rl - x) << ". Center: " << center
             << ", V: " << v << ", RL: " << rl << ", T:" << x << endl;
      }
      else {
        cout << "\t\tRay should not intersect. Center: " << center << ", V: " << v
             << ", RL: " << rl << endl;
      }
    }
#endif

    if (p->rayIntersect(center, u, t, t_u, t_v)) {
#if LOGGING
      if (_global.debug & G_DEBUG_FREESTYLE) {
        cout << "\t\tRay " << center << " * " << u << " intersects at time " << t
             << " (raylength is " << raylength << ")" << endl;
        cout << "\t\t(u * normal) == " << (u * p->getNormal()) << " for normal " << p->getNormal()
             << endl;
      }
#endif
      if (fabs(u * p->getNormal()) > 0.0001) {
        if ((t > 0.0) && (t < raylength)) {
#if LOGGING
          if (_global.debug & G_DEBUG_FREESTYLE) {
            cout << "\t\tIs occluder" << endl;
          }
#endif
          if (foundOccluders != NULL) {
            ViewShape *vshape = viewMap->viewShape(oface->GetVertex(0)->shape()->GetId());
            foundOccluders->insert(vshape);
          }
          ++qi;

          if (!grid.enableQI()) {
            break;
          }
        }

        occluders.reportDepth(center, u, t);
      }
    }
  }

  // Find occludee
  findOccludee<G, I>(
      fe, grid, occluders, epsilon, oaWFace, u, center, origin, edgeDir, faceVertices);

  return qi;
}

// computeCumulativeVisibility returns the lowest x such that the majority of FEdges have QI <= x
//
// This was probably the original intention of the "normal" algorithm on which
// computeDetailedVisibility is based. But because the "normal" algorithm chooses the most popular
// QI, without considering any other values, a ViewEdge with FEdges having QIs of 0, 21, 22, 23, 24
// and 25 will end up having a total QI of 0, even though most of the FEdges are heavily occluded.
// computeCumulativeVisibility will treat this case as a QI of 22 because 3 out of 6 occluders have
// QI <= 22.

template<typename G, typename I>
static void computeCumulativeVisibility(ViewMap *ioViewMap,
                                        G &grid,
                                        real epsilon,
                                        RenderMonitor *iRenderMonitor)
{
  vector<ViewEdge *> &vedges = ioViewMap->ViewEdges();

  FEdge *fe, *festart;
  int nSamples = 0;
  vector<WFace *> wFaces;
  WFace *wFace = NULL;
  unsigned cnt = 0;
  unsigned cntStep = (unsigned)ceil(0.01f * vedges.size());
  unsigned tmpQI = 0;
  unsigned qiClasses[256];
  unsigned maxIndex, maxCard;
  unsigned qiMajority;
  for (vector<ViewEdge *>::iterator ve = vedges.begin(), veend = vedges.end(); ve != veend; ve++) {
    if (iRenderMonitor) {
      if (iRenderMonitor->testBreak()) {
        break;
      }
      if (cnt % cntStep == 0) {
        stringstream ss;
        ss << "Freestyle: Visibility computations " << (100 * cnt / vedges.size()) << "%";
        iRenderMonitor->setInfo(ss.str());
        iRenderMonitor->progress((float)cnt / vedges.size());
      }
      cnt++;
    }
#if LOGGING
    if (_global.debug & G_DEBUG_FREESTYLE) {
      cout << "Processing ViewEdge " << (*ve)->getId() << endl;
    }
#endif
    // Find an edge to test
    if (!(*ve)->isInImage()) {
      // This view edge has been proscenium culled
      (*ve)->setQI(255);
      (*ve)->setaShape(0);
#if LOGGING
      if (_global.debug & G_DEBUG_FREESTYLE) {
        cout << "\tCulled." << endl;
      }
#endif
      continue;
    }

    // Test edge
    festart = (*ve)->fedgeA();
    fe = (*ve)->fedgeA();
    qiMajority = 0;
    do {
      if (fe != NULL && fe->isInImage()) {
        qiMajority++;
      }
      fe = fe->nextEdge();
    } while (fe && fe != festart);

    if (qiMajority == 0) {
      // There are no occludable FEdges on this ViewEdge
      // This should be impossible.
      if (_global.debug & G_DEBUG_FREESTYLE) {
        cout << "View Edge in viewport without occludable FEdges: " << (*ve)->getId() << endl;
      }
      // We can recover from this error:
      // Treat this edge as fully visible with no occludee
      (*ve)->setQI(0);
      (*ve)->setaShape(0);
      continue;
    }
    else {
      ++qiMajority;
      qiMajority >>= 1;
    }
#if LOGGING
    if (_global.debug & G_DEBUG_FREESTYLE) {
      cout << "\tqiMajority: " << qiMajority << endl;
    }
#endif

    tmpQI = 0;
    maxIndex = 0;
    maxCard = 0;
    nSamples = 0;
    memset(qiClasses, 0, 256 * sizeof(*qiClasses));
    set<ViewShape *> foundOccluders;

    fe = (*ve)->fedgeA();
    do {
      if (!fe || !fe->isInImage()) {
        fe = fe->nextEdge();
        continue;
      }
      if ((maxCard < qiMajority)) {
        // ARB: change &wFace to wFace and use reference in called function
        tmpQI = computeVisibility<G, I>(
            ioViewMap, fe, grid, epsilon, *ve, &wFace, &foundOccluders);
#if LOGGING
        if (_global.debug & G_DEBUG_FREESTYLE) {
          cout << "\tFEdge: visibility " << tmpQI << endl;
        }
#endif

        // ARB: This is an error condition, not an alert condition.
        // Some sort of recovery or abort is necessary.
        if (tmpQI >= 256) {
          cerr << "Warning: too many occluding levels" << endl;
          // ARB: Wild guess: instead of aborting or corrupting memory, treat as tmpQI == 255
          tmpQI = 255;
        }

        if (++qiClasses[tmpQI] > maxCard) {
          maxCard = qiClasses[tmpQI];
          maxIndex = tmpQI;
        }
      }
      else {
        // ARB: FindOccludee is redundant if ComputeRayCastingVisibility has been called
        // ARB: change &wFace to wFace and use reference in called function
        findOccludee<G, I>(fe, grid, epsilon, *ve, &wFace);
#if LOGGING
        if (_global.debug & G_DEBUG_FREESTYLE) {
          cout << "\tFEdge: occludee only (" << (wFace != NULL ? "found" : "not found") << ")"
               << endl;
        }
#endif
      }

      // Store test results
      if (wFace) {
        vector<Vec3r> vertices;
        for (int i = 0, numEdges = wFace->numberOfEdges(); i < numEdges; ++i) {
          vertices.push_back(Vec3r(wFace->GetVertex(i)->GetVertex()));
        }
        Polygon3r poly(vertices, wFace->GetNormal());
        poly.userdata = (void *)wFace;
        fe->setaFace(poly);
        wFaces.push_back(wFace);
        fe->setOccludeeEmpty(false);
#if LOGGING
        if (_global.debug & G_DEBUG_FREESTYLE) {
          cout << "\tFound occludee" << endl;
        }
#endif
      }
      else {
        fe->setOccludeeEmpty(true);
      }

      ++nSamples;
      fe = fe->nextEdge();
    } while ((maxCard < qiMajority) && (fe) && (fe != festart));

#if LOGGING
    if (_global.debug & G_DEBUG_FREESTYLE) {
      cout << "\tFinished with " << nSamples << " samples, maxCard = " << maxCard << endl;
    }
#endif

    // ViewEdge
    // qi --
    // Find the minimum value that is >= the majority of the QI
    for (unsigned count = 0, i = 0; i < 256; ++i) {
      count += qiClasses[i];
      if (count >= qiMajority) {
        (*ve)->setQI(i);
        break;
      }
    }
    // occluders --
    // I would rather not have to go through the effort of creating this set and then copying out
    // its contents. Is there a reason why ViewEdge::_Occluders cannot be converted to a set<>?
    for (set<ViewShape *>::iterator o = foundOccluders.begin(), oend = foundOccluders.end();
         o != oend;
         ++o) {
      (*ve)->AddOccluder((*o));
    }
#if LOGGING
    if (_global.debug & G_DEBUG_FREESTYLE) {
      cout << "\tConclusion: QI = " << maxIndex << ", " << (*ve)->occluders_size() << " occluders."
           << endl;
    }
#else
    (void)maxIndex;
#endif
    // occludee --
    if (!wFaces.empty()) {
      if (wFaces.size() <= (float)nSamples / 2.0f) {
        (*ve)->setaShape(0);
      }
      else {
        ViewShape *vshape = ioViewMap->viewShape(
            (*wFaces.begin())->GetVertex(0)->shape()->GetId());
        (*ve)->setaShape(vshape);
      }
    }

    wFaces.clear();
  }
  if (iRenderMonitor && vedges.size()) {
    stringstream ss;
    ss << "Freestyle: Visibility computations " << (100 * cnt / vedges.size()) << "%";
    iRenderMonitor->setInfo(ss.str());
    iRenderMonitor->progress((float)cnt / vedges.size());
  }
}

template<typename G, typename I>
static void computeDetailedVisibility(ViewMap *ioViewMap,
                                      G &grid,
                                      real epsilon,
                                      RenderMonitor *iRenderMonitor)
{
  vector<ViewEdge *> &vedges = ioViewMap->ViewEdges();

  FEdge *fe, *festart;
  int nSamples = 0;
  vector<WFace *> wFaces;
  WFace *wFace = NULL;
  unsigned tmpQI = 0;
  unsigned qiClasses[256];
  unsigned maxIndex, maxCard;
  unsigned qiMajority;
  for (vector<ViewEdge *>::iterator ve = vedges.begin(), veend = vedges.end(); ve != veend; ve++) {
    if (iRenderMonitor && iRenderMonitor->testBreak()) {
      break;
    }
#if LOGGING
    if (_global.debug & G_DEBUG_FREESTYLE) {
      cout << "Processing ViewEdge " << (*ve)->getId() << endl;
    }
#endif
    // Find an edge to test
    if (!(*ve)->isInImage()) {
      // This view edge has been proscenium culled
      (*ve)->setQI(255);
      (*ve)->setaShape(0);
#if LOGGING
      if (_global.debug & G_DEBUG_FREESTYLE) {
        cout << "\tCulled." << endl;
      }
#endif
      continue;
    }

    // Test edge
    festart = (*ve)->fedgeA();
    fe = (*ve)->fedgeA();
    qiMajority = 0;
    do {
      if (fe != NULL && fe->isInImage()) {
        qiMajority++;
      }
      fe = fe->nextEdge();
    } while (fe && fe != festart);

    if (qiMajority == 0) {
      // There are no occludable FEdges on this ViewEdge
      // This should be impossible.
      if (_global.debug & G_DEBUG_FREESTYLE) {
        cout << "View Edge in viewport without occludable FEdges: " << (*ve)->getId() << endl;
      }
      // We can recover from this error:
      // Treat this edge as fully visible with no occludee
      (*ve)->setQI(0);
      (*ve)->setaShape(0);
      continue;
    }
    else {
      ++qiMajority;
      qiMajority >>= 1;
    }
#if LOGGING
    if (_global.debug & G_DEBUG_FREESTYLE) {
      cout << "\tqiMajority: " << qiMajority << endl;
    }
#endif

    tmpQI = 0;
    maxIndex = 0;
    maxCard = 0;
    nSamples = 0;
    memset(qiClasses, 0, 256 * sizeof(*qiClasses));
    set<ViewShape *> foundOccluders;

    fe = (*ve)->fedgeA();
    do {
      if (fe == NULL || !fe->isInImage()) {
        fe = fe->nextEdge();
        continue;
      }
      if ((maxCard < qiMajority)) {
        // ARB: change &wFace to wFace and use reference in called function
        tmpQI = computeVisibility<G, I>(
            ioViewMap, fe, grid, epsilon, *ve, &wFace, &foundOccluders);
#if LOGGING
        if (_global.debug & G_DEBUG_FREESTYLE) {
          cout << "\tFEdge: visibility " << tmpQI << endl;
        }
#endif

        // ARB: This is an error condition, not an alert condition.
        // Some sort of recovery or abort is necessary.
        if (tmpQI >= 256) {
          cerr << "Warning: too many occluding levels" << endl;
          // ARB: Wild guess: instead of aborting or corrupting memory, treat as tmpQI == 255
          tmpQI = 255;
        }

        if (++qiClasses[tmpQI] > maxCard) {
          maxCard = qiClasses[tmpQI];
          maxIndex = tmpQI;
        }
      }
      else {
        // ARB: FindOccludee is redundant if ComputeRayCastingVisibility has been called
        // ARB: change &wFace to wFace and use reference in called function
        findOccludee<G, I>(fe, grid, epsilon, *ve, &wFace);
#if LOGGING
        if (_global.debug & G_DEBUG_FREESTYLE) {
          cout << "\tFEdge: occludee only (" << (wFace != NULL ? "found" : "not found") << ")"
               << endl;
        }
#endif
      }

      // Store test results
      if (wFace) {
        vector<Vec3r> vertices;
        for (int i = 0, numEdges = wFace->numberOfEdges(); i < numEdges; ++i) {
          vertices.push_back(Vec3r(wFace->GetVertex(i)->GetVertex()));
        }
        Polygon3r poly(vertices, wFace->GetNormal());
        poly.userdata = (void *)wFace;
        fe->setaFace(poly);
        wFaces.push_back(wFace);
        fe->setOccludeeEmpty(false);
#if LOGGING
        if (_global.debug & G_DEBUG_FREESTYLE) {
          cout << "\tFound occludee" << endl;
        }
#endif
      }
      else {
        fe->setOccludeeEmpty(true);
      }

      ++nSamples;
      fe = fe->nextEdge();
    } while ((maxCard < qiMajority) && (fe) && (fe != festart));

#if LOGGING
    if (_global.debug & G_DEBUG_FREESTYLE) {
      cout << "\tFinished with " << nSamples << " samples, maxCard = " << maxCard << endl;
    }
#endif

    // ViewEdge
    // qi --
    (*ve)->setQI(maxIndex);
    // occluders --
    // I would rather not have to go through the effort of creating this this set and then copying
    // out its contents. Is there a reason why ViewEdge::_Occluders cannot be converted to a set<>?
    for (set<ViewShape *>::iterator o = foundOccluders.begin(), oend = foundOccluders.end();
         o != oend;
         ++o) {
      (*ve)->AddOccluder((*o));
    }
#if LOGGING
    if (_global.debug & G_DEBUG_FREESTYLE) {
      cout << "\tConclusion: QI = " << maxIndex << ", " << (*ve)->occluders_size() << " occluders."
           << endl;
    }
#endif
    // occludee --
    if (!wFaces.empty()) {
      if (wFaces.size() <= (float)nSamples / 2.0f) {
        (*ve)->setaShape(0);
      }
      else {
        ViewShape *vshape = ioViewMap->viewShape(
            (*wFaces.begin())->GetVertex(0)->shape()->GetId());
        (*ve)->setaShape(vshape);
      }
    }

    wFaces.clear();
  }
}

template<typename G, typename I>
static void computeFastVisibility(ViewMap *ioViewMap, G &grid, real epsilon)
{
  vector<ViewEdge *> &vedges = ioViewMap->ViewEdges();

  FEdge *fe, *festart;
  unsigned nSamples = 0;
  vector<WFace *> wFaces;
  WFace *wFace = NULL;
  unsigned tmpQI = 0;
  unsigned qiClasses[256];
  unsigned maxIndex, maxCard;
  unsigned qiMajority;
  bool even_test;
  for (vector<ViewEdge *>::iterator ve = vedges.begin(), veend = vedges.end(); ve != veend; ve++) {
    // Find an edge to test
    if (!(*ve)->isInImage()) {
      // This view edge has been proscenium culled
      (*ve)->setQI(255);
      (*ve)->setaShape(0);
      continue;
    }

    // Test edge
    festart = (*ve)->fedgeA();
    fe = (*ve)->fedgeA();

    even_test = true;
    qiMajority = 0;
    do {
      if (even_test && fe && fe->isInImage()) {
        qiMajority++;
        even_test = !even_test;
      }
      fe = fe->nextEdge();
    } while (fe && fe != festart);

    if (qiMajority == 0) {
      // There are no occludable FEdges on this ViewEdge
      // This should be impossible.
      if (_global.debug & G_DEBUG_FREESTYLE) {
        cout << "View Edge in viewport without occludable FEdges: " << (*ve)->getId() << endl;
      }
      // We can recover from this error:
      // Treat this edge as fully visible with no occludee
      (*ve)->setQI(0);
      (*ve)->setaShape(0);
      continue;
    }
    else {
      ++qiMajority;
      qiMajority >>= 1;
    }

    even_test = true;
    maxIndex = 0;
    maxCard = 0;
    nSamples = 0;
    memset(qiClasses, 0, 256 * sizeof(*qiClasses));
    set<ViewShape *> foundOccluders;

    fe = (*ve)->fedgeA();
    do {
      if (!fe || !fe->isInImage()) {
        fe = fe->nextEdge();
        continue;
      }
      if (even_test) {
        if ((maxCard < qiMajority)) {
          // ARB: change &wFace to wFace and use reference in called function
          tmpQI = computeVisibility<G, I>(
              ioViewMap, fe, grid, epsilon, *ve, &wFace, &foundOccluders);

          // ARB: This is an error condition, not an alert condition.
          // Some sort of recovery or abort is necessary.
          if (tmpQI >= 256) {
            cerr << "Warning: too many occluding levels" << endl;
            // ARB: Wild guess: instead of aborting or corrupting memory, treat as tmpQI == 255
            tmpQI = 255;
          }

          if (++qiClasses[tmpQI] > maxCard) {
            maxCard = qiClasses[tmpQI];
            maxIndex = tmpQI;
          }
        }
        else {
          // ARB: FindOccludee is redundant if ComputeRayCastingVisibility has been called
          // ARB: change &wFace to wFace and use reference in called function
          findOccludee<G, I>(fe, grid, epsilon, *ve, &wFace);
        }

        if (wFace) {
          vector<Vec3r> vertices;
          for (int i = 0, numEdges = wFace->numberOfEdges(); i < numEdges; ++i) {
            vertices.push_back(Vec3r(wFace->GetVertex(i)->GetVertex()));
          }
          Polygon3r poly(vertices, wFace->GetNormal());
          poly.userdata = (void *)wFace;
          fe->setaFace(poly);
          wFaces.push_back(wFace);
        }
        ++nSamples;
      }

      even_test = !even_test;
      fe = fe->nextEdge();
    } while ((maxCard < qiMajority) && (fe) && (fe != festart));

    // qi --
    (*ve)->setQI(maxIndex);

    // occluders --
    for (set<ViewShape *>::iterator o = foundOccluders.begin(), oend = foundOccluders.end();
         o != oend;
         ++o) {
      (*ve)->AddOccluder((*o));
    }

    // occludee --
    if (!wFaces.empty()) {
      if (wFaces.size() < nSamples / 2) {
        (*ve)->setaShape(0);
      }
      else {
        ViewShape *vshape = ioViewMap->viewShape(
            (*wFaces.begin())->GetVertex(0)->shape()->GetId());
        (*ve)->setaShape(vshape);
      }
    }

    wFaces.clear();
  }
}

template<typename G, typename I>
static void computeVeryFastVisibility(ViewMap *ioViewMap, G &grid, real epsilon)
{
  vector<ViewEdge *> &vedges = ioViewMap->ViewEdges();

  FEdge *fe;
  unsigned qi = 0;
  WFace *wFace = 0;

  for (vector<ViewEdge *>::iterator ve = vedges.begin(), veend = vedges.end(); ve != veend; ve++) {
    // Find an edge to test
    if (!(*ve)->isInImage()) {
      // This view edge has been proscenium culled
      (*ve)->setQI(255);
      (*ve)->setaShape(0);
      continue;
    }
    fe = (*ve)->fedgeA();
    // Find a FEdge inside the occluder proscenium to test for visibility
    FEdge *festart = fe;
    while (fe && !fe->isInImage() && fe != festart) {
      fe = fe->nextEdge();
    }

    // Test edge
    if (!fe || !fe->isInImage()) {
      // There are no occludable FEdges on this ViewEdge
      // This should be impossible.
      if (_global.debug & G_DEBUG_FREESTYLE) {
        cout << "View Edge in viewport without occludable FEdges: " << (*ve)->getId() << endl;
      }
      // We can recover from this error:
      // Treat this edge as fully visible with no occludee
      qi = 0;
      wFace = NULL;
    }
    else {
      qi = computeVisibility<G, I>(ioViewMap, fe, grid, epsilon, *ve, &wFace, NULL);
    }

    // Store test results
    if (wFace) {
      vector<Vec3r> vertices;
      for (int i = 0, numEdges = wFace->numberOfEdges(); i < numEdges; ++i) {
        vertices.push_back(Vec3r(wFace->GetVertex(i)->GetVertex()));
      }
      Polygon3r poly(vertices, wFace->GetNormal());
      poly.userdata = (void *)wFace;
      fe->setaFace(poly);  // This works because setaFace *copies* the polygon
      ViewShape *vshape = ioViewMap->viewShape(wFace->GetVertex(0)->shape()->GetId());
      (*ve)->setaShape(vshape);
    }
    else {
      (*ve)->setaShape(0);
    }
    (*ve)->setQI(qi);
  }
}

void ViewMapBuilder::BuildGrid(WingedEdge &we, const BBox<Vec3r> &bbox, unsigned int sceneNumFaces)
{
  _Grid->clear();
  Vec3r size;
  for (unsigned int i = 0; i < 3; i++) {
    size[i] = fabs(bbox.getMax()[i] - bbox.getMin()[i]);
    // let make the grid 1/10 bigger to avoid numerical errors while computing triangles/cells
    // intersections.
    size[i] += size[i] / 10.0;
    if (size[i] == 0) {
      if (_global.debug & G_DEBUG_FREESTYLE) {
        cout << "Warning: the bbox size is 0 in dimension " << i << endl;
      }
    }
  }
  _Grid->configure(Vec3r(bbox.getMin() - size / 20.0), size, sceneNumFaces);

  // Fill in the grid:
  WFillGrid fillGridRenderer(_Grid, &we);
  fillGridRenderer.fillGrid();

  // DEBUG
  _Grid->displayDebug();
}

ViewMap *ViewMapBuilder::BuildViewMap(WingedEdge &we,
                                      visibility_algo iAlgo,
                                      real epsilon,
                                      const BBox<Vec3r> &bbox,
                                      unsigned int sceneNumFaces)
{
  _ViewMap = new ViewMap;
  _currentId = 1;
  _currentFId = 0;
  _currentSVertexId = 0;

  // Builds initial view edges
  computeInitialViewEdges(we);

  // Detects cusps
  computeCusps(_ViewMap);

  // Compute intersections
  ComputeIntersections(_ViewMap, sweep_line, epsilon);

  // Compute visibility
  ComputeEdgesVisibility(_ViewMap, we, bbox, sceneNumFaces, iAlgo, epsilon);

  return _ViewMap;
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

void ViewMapBuilder::CullViewEdges(ViewMap *ioViewMap,
                                   real viewProscenium[4],
                                   real occluderProscenium[4],
                                   bool extensiveFEdgeSearch)
{
  // Cull view edges by marking them as non-displayable.
  // This avoids the complications of trying to delete edges from the ViewMap.

  // Non-displayable view edges will be skipped over during visibility calculation.

  // View edges will be culled according to their position w.r.t. the viewport proscenium (viewport
  // + 5% border, or some such).

  // Get proscenium boundary for culling
  GridHelpers::getDefaultViewProscenium(viewProscenium);
  real prosceniumOrigin[2];
  prosceniumOrigin[0] = (viewProscenium[1] - viewProscenium[0]) / 2.0;
  prosceniumOrigin[1] = (viewProscenium[3] - viewProscenium[2]) / 2.0;
  if (_global.debug & G_DEBUG_FREESTYLE) {
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

  // N.B. Freestyle is inconsistent in its use of ViewMap::viewedges_container and
  // vector<ViewEdge*>::iterator. Probably all occurences of vector<ViewEdge*>::iterator should be
  // replaced ViewMap::viewedges_container throughout the code. For each view edge
  ViewMap::viewedges_container::iterator ve, veend;

  for (ve = ioViewMap->ViewEdges().begin(), veend = ioViewMap->ViewEdges().end(); ve != veend;
       ve++) {
    // Overview:
    //    Search for a visible feature edge
    //    If none: mark view edge as non-displayable
    //    Otherwise:
    //        Find a feature edge with center point inside occluder proscenium.
    //        If none exists, find the feature edge with center point closest to viewport origin.
    //            Expand occluder proscenium to enclose center point.

    // For each feature edge, while bestOccluderTarget not found and view edge not visible
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
    } while (fe && fe != festart && !(bestOccluderTargetFound && (*ve)->isInImage()));

    // Either we have run out of FEdges, or we already have the one edge we need to determine
    // visibility Cull all remaining edges.
    while (fe && fe != festart) {
      fe->setIsInImage(false);
      fe = fe->nextEdge();
    }

    // If bestOccluderTarget was not found inside the occluder proscenium, we need to expand the
    // occluder proscenium to include it.
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

  // So far we have only found one FEdge per ViewEdge.  The "Normal" and "Fast" styles of
  // visibility computation want to consider many FEdges for each ViewEdge. Here we re-scan the
  // view map to find any usable FEdges that we skipped on the first pass, or that have become
  // usable because the occluder proscenium has been expanded since the edge was visited on the
  // first pass.
  if (extensiveFEdgeSearch) {
    // For each view edge,
    for (ve = ioViewMap->ViewEdges().begin(), veend = ioViewMap->ViewEdges().end(); ve != veend;
         ve++) {
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
        }
        fe = fe->nextEdge();
      } while (fe && fe != festart);
    }
  }
}

void ViewMapBuilder::computeInitialViewEdges(WingedEdge &we)
{
  vector<WShape *> wshapes = we.getWShapes();
  SShape *psShape;

  for (vector<WShape *>::const_iterator it = wshapes.begin(); it != wshapes.end(); it++) {
    if (_pRenderMonitor && _pRenderMonitor->testBreak()) {
      break;
    }

    // create the embedding
    psShape = new SShape;
    psShape->setId((*it)->GetId());
    psShape->setName((*it)->getName());
    psShape->setLibraryPath((*it)->getLibraryPath());
    psShape->setFrsMaterials((*it)->frs_materials());  // FIXME

    // create the view shape
    ViewShape *vshape = new ViewShape(psShape);
    // add this view shape to the view map:
    _ViewMap->AddViewShape(vshape);

    // we want to number the view edges in a unique way for the while scene.
    _pViewEdgeBuilder->setCurrentViewId(_currentId);
    // we want to number the feature edges in a unique way for the while scene.
    _pViewEdgeBuilder->setCurrentFId(_currentFId);
    // we want to number the SVertex in a unique way for the while scene.
    _pViewEdgeBuilder->setCurrentSVertexId(_currentFId);
    _pViewEdgeBuilder->BuildViewEdges(dynamic_cast<WXShape *>(*it),
                                      vshape,
                                      _ViewMap->ViewEdges(),
                                      _ViewMap->ViewVertices(),
                                      _ViewMap->FEdges(),
                                      _ViewMap->SVertices());

    _currentId = _pViewEdgeBuilder->currentViewId() + 1;
    _currentFId = _pViewEdgeBuilder->currentFId() + 1;
    _currentSVertexId = _pViewEdgeBuilder->currentSVertexId() + 1;

    psShape->ComputeBBox();
  }
}

void ViewMapBuilder::computeCusps(ViewMap *ioViewMap)
{
  vector<ViewVertex *> newVVertices;
  vector<ViewEdge *> newVEdges;
  ViewMap::viewedges_container &vedges = ioViewMap->ViewEdges();
  ViewMap::viewedges_container::iterator ve = vedges.begin(), veend = vedges.end();
  for (; ve != veend; ++ve) {
    if (_pRenderMonitor && _pRenderMonitor->testBreak()) {
      break;
    }
    if ((!((*ve)->getNature() & Nature::SILHOUETTE)) || (!((*ve)->fedgeA()->isSmooth()))) {
      continue;
    }
    FEdge *fe = (*ve)->fedgeA();
    FEdge *fefirst = fe;
    bool first = true;
    bool positive = true;
    do {
      FEdgeSmooth *fes = dynamic_cast<FEdgeSmooth *>(fe);
      Vec3r A((fes)->vertexA()->point3d());
      Vec3r B((fes)->vertexB()->point3d());
      Vec3r AB(B - A);
      AB.normalize();
      Vec3r m((A + B) / 2.0);
      Vec3r crossP(AB ^ (fes)->normal());
      crossP.normalize();
      Vec3r viewvector;
      if (_orthographicProjection) {
        viewvector = Vec3r(0.0, 0.0, m.z() - _viewpoint.z());
      }
      else {
        viewvector = Vec3r(m - _viewpoint);
      }
      viewvector.normalize();
      if (first) {
        if (((crossP) * (viewvector)) > 0) {
          positive = true;
        }
        else {
          positive = false;
        }
        first = false;
      }
      // If we're in a positive part, we need a stronger negative value to change
      NonTVertex *cusp = NULL;
      if (positive) {
        if (((crossP) * (viewvector)) < -0.1) {
          // state changes
          positive = false;
          // creates and insert cusp
          cusp = dynamic_cast<NonTVertex *>(
              ioViewMap->InsertViewVertex(fes->vertexA(), newVEdges));
          if (cusp) {
            cusp->setNature(cusp->getNature() | Nature::CUSP);
          }
        }
      }
      else {
        // If we're in a negative part, we need a stronger negative value to change
        if (((crossP) * (viewvector)) > 0.1) {
          positive = true;
          cusp = dynamic_cast<NonTVertex *>(
              ioViewMap->InsertViewVertex(fes->vertexA(), newVEdges));
          if (cusp) {
            cusp->setNature(cusp->getNature() | Nature::CUSP);
          }
        }
      }
      fe = fe->nextEdge();
    } while (fe && fe != fefirst);
  }
  for (ve = newVEdges.begin(), veend = newVEdges.end(); ve != veend; ++ve) {
    (*ve)->viewShape()->AddEdge(*ve);
    vedges.push_back(*ve);
  }
}

void ViewMapBuilder::ComputeCumulativeVisibility(ViewMap *ioViewMap,
                                                 WingedEdge &we,
                                                 const BBox<Vec3r> &bbox,
                                                 real epsilon,
                                                 bool cull,
                                                 GridDensityProviderFactory &factory)
{
  AutoPtr<GridHelpers::Transform> transform;
  AutoPtr<OccluderSource> source;

  if (_orthographicProjection) {
    transform.reset(new BoxGrid::Transform);
  }
  else {
    transform.reset(new SphericalGrid::Transform);
  }

  if (cull) {
    source.reset(new CulledOccluderSource(*transform, we, *ioViewMap, true));
  }
  else {
    source.reset(new OccluderSource(*transform, we));
  }

  AutoPtr<GridDensityProvider> density(factory.newGridDensityProvider(*source, bbox, *transform));

  if (_orthographicProjection) {
    BoxGrid grid(*source, *density, ioViewMap, _viewpoint, _EnableQI);
    computeCumulativeVisibility<BoxGrid, BoxGrid::Iterator>(
        ioViewMap, grid, epsilon, _pRenderMonitor);
  }
  else {
    SphericalGrid grid(*source, *density, ioViewMap, _viewpoint, _EnableQI);
    computeCumulativeVisibility<SphericalGrid, SphericalGrid::Iterator>(
        ioViewMap, grid, epsilon, _pRenderMonitor);
  }
}

void ViewMapBuilder::ComputeDetailedVisibility(ViewMap *ioViewMap,
                                               WingedEdge &we,
                                               const BBox<Vec3r> &bbox,
                                               real epsilon,
                                               bool cull,
                                               GridDensityProviderFactory &factory)
{
  AutoPtr<GridHelpers::Transform> transform;
  AutoPtr<OccluderSource> source;

  if (_orthographicProjection) {
    transform.reset(new BoxGrid::Transform);
  }
  else {
    transform.reset(new SphericalGrid::Transform);
  }

  if (cull) {
    source.reset(new CulledOccluderSource(*transform, we, *ioViewMap, true));
  }
  else {
    source.reset(new OccluderSource(*transform, we));
  }

  AutoPtr<GridDensityProvider> density(factory.newGridDensityProvider(*source, bbox, *transform));

  if (_orthographicProjection) {
    BoxGrid grid(*source, *density, ioViewMap, _viewpoint, _EnableQI);
    computeDetailedVisibility<BoxGrid, BoxGrid::Iterator>(
        ioViewMap, grid, epsilon, _pRenderMonitor);
  }
  else {
    SphericalGrid grid(*source, *density, ioViewMap, _viewpoint, _EnableQI);
    computeDetailedVisibility<SphericalGrid, SphericalGrid::Iterator>(
        ioViewMap, grid, epsilon, _pRenderMonitor);
  }
}

void ViewMapBuilder::ComputeEdgesVisibility(ViewMap *ioViewMap,
                                            WingedEdge &we,
                                            const BBox<Vec3r> &bbox,
                                            unsigned int sceneNumFaces,
                                            visibility_algo iAlgo,
                                            real epsilon)
{
#if 0
  iAlgo = ray_casting;  // for testing algorithms equivalence
#endif
  switch (iAlgo) {
    case ray_casting:
      if (_global.debug & G_DEBUG_FREESTYLE) {
        cout << "Using ordinary ray casting" << endl;
      }
      BuildGrid(we, bbox, sceneNumFaces);
      ComputeRayCastingVisibility(ioViewMap, epsilon);
      break;
    case ray_casting_fast:
      if (_global.debug & G_DEBUG_FREESTYLE) {
        cout << "Using fast ray casting" << endl;
      }
      BuildGrid(we, bbox, sceneNumFaces);
      ComputeFastRayCastingVisibility(ioViewMap, epsilon);
      break;
    case ray_casting_very_fast:
      if (_global.debug & G_DEBUG_FREESTYLE) {
        cout << "Using very fast ray casting" << endl;
      }
      BuildGrid(we, bbox, sceneNumFaces);
      ComputeVeryFastRayCastingVisibility(ioViewMap, epsilon);
      break;
    case ray_casting_culled_adaptive_traditional:
      if (_global.debug & G_DEBUG_FREESTYLE) {
        cout << "Using culled adaptive grid with heuristic density and traditional QI calculation"
             << endl;
      }
      try {
        HeuristicGridDensityProviderFactory factory(0.5f, sceneNumFaces);
        ComputeDetailedVisibility(ioViewMap, we, bbox, epsilon, true, factory);
      }
      catch (...) {
        // Last resort catch to make sure RAII semantics hold for OptimizedGrid. Can be replaced
        // with try...catch block around main() if the program as a whole is converted to RAII

        // This is the little-mentioned caveat of RAII: RAII does not work unless destructors are
        // always called, but destructors are only called if all exceptions are caught (or
        // std::terminate() is replaced).

        // We don't actually handle the exception here, so re-throw it now that our destructors
        // have had a chance to run.
        throw;
      }
      break;
    case ray_casting_adaptive_traditional:
      if (_global.debug & G_DEBUG_FREESTYLE) {
        cout
            << "Using unculled adaptive grid with heuristic density and traditional QI calculation"
            << endl;
      }
      try {
        HeuristicGridDensityProviderFactory factory(0.5f, sceneNumFaces);
        ComputeDetailedVisibility(ioViewMap, we, bbox, epsilon, false, factory);
      }
      catch (...) {
        throw;
      }
      break;
    case ray_casting_culled_adaptive_cumulative:
      if (_global.debug & G_DEBUG_FREESTYLE) {
        cout << "Using culled adaptive grid with heuristic density and cumulative QI calculation"
             << endl;
      }
      try {
        HeuristicGridDensityProviderFactory factory(0.5f, sceneNumFaces);
        ComputeCumulativeVisibility(ioViewMap, we, bbox, epsilon, true, factory);
      }
      catch (...) {
        throw;
      }
      break;
    case ray_casting_adaptive_cumulative:
      if (_global.debug & G_DEBUG_FREESTYLE) {
        cout << "Using unculled adaptive grid with heuristic density and cumulative QI calculation"
             << endl;
      }
      try {
        HeuristicGridDensityProviderFactory factory(0.5f, sceneNumFaces);
        ComputeCumulativeVisibility(ioViewMap, we, bbox, epsilon, false, factory);
      }
      catch (...) {
        throw;
      }
      break;
    default:
      break;
  }
}

static const unsigned gProgressBarMaxSteps = 10;
static const unsigned gProgressBarMinSize = 2000;

void ViewMapBuilder::ComputeRayCastingVisibility(ViewMap *ioViewMap, real epsilon)
{
  vector<ViewEdge *> &vedges = ioViewMap->ViewEdges();
  bool progressBarDisplay = false;
  unsigned progressBarStep = 0;
  unsigned vEdgesSize = vedges.size();
  unsigned fEdgesSize = ioViewMap->FEdges().size();

  if (_pProgressBar != NULL && fEdgesSize > gProgressBarMinSize) {
    unsigned progressBarSteps = min(gProgressBarMaxSteps, vEdgesSize);
    progressBarStep = vEdgesSize / progressBarSteps;
    _pProgressBar->reset();
    _pProgressBar->setLabelText("Computing Ray casting Visibility");
    _pProgressBar->setTotalSteps(progressBarSteps);
    _pProgressBar->setProgress(0);
    progressBarDisplay = true;
  }

  unsigned counter = progressBarStep;
  FEdge *fe, *festart;
  int nSamples = 0;
  vector<Polygon3r *> aFaces;
  Polygon3r *aFace = NULL;
  unsigned tmpQI = 0;
  unsigned qiClasses[256];
  unsigned maxIndex, maxCard;
  unsigned qiMajority;
  static unsigned timestamp = 1;
  for (vector<ViewEdge *>::iterator ve = vedges.begin(), veend = vedges.end(); ve != veend; ve++) {
    if (_pRenderMonitor && _pRenderMonitor->testBreak()) {
      break;
    }
#if LOGGING
    if (_global.debug & G_DEBUG_FREESTYLE) {
      cout << "Processing ViewEdge " << (*ve)->getId() << endl;
    }
#endif
    festart = (*ve)->fedgeA();
    fe = (*ve)->fedgeA();
    qiMajority = 1;
    do {
      qiMajority++;
      fe = fe->nextEdge();
    } while (fe && fe != festart);
    qiMajority >>= 1;
#if LOGGING
    if (_global.debug & G_DEBUG_FREESTYLE) {
      cout << "\tqiMajority: " << qiMajority << endl;
    }
#endif

    tmpQI = 0;
    maxIndex = 0;
    maxCard = 0;
    nSamples = 0;
    fe = (*ve)->fedgeA();
    memset(qiClasses, 0, 256 * sizeof(*qiClasses));
    set<ViewShape *> occluders;
    do {
      if ((maxCard < qiMajority)) {
        tmpQI = ComputeRayCastingVisibility(fe, _Grid, epsilon, occluders, &aFace, timestamp++);

#if LOGGING
        if (_global.debug & G_DEBUG_FREESTYLE) {
          cout << "\tFEdge: visibility " << tmpQI << endl;
        }
#endif
        // ARB: This is an error condition, not an alert condition.
        // Some sort of recovery or abort is necessary.
        if (tmpQI >= 256) {
          cerr << "Warning: too many occluding levels" << endl;
          // ARB: Wild guess: instead of aborting or corrupting memory, treat as tmpQI == 255
          tmpQI = 255;
        }

        if (++qiClasses[tmpQI] > maxCard) {
          maxCard = qiClasses[tmpQI];
          maxIndex = tmpQI;
        }
      }
      else {
        // ARB: FindOccludee is redundant if ComputeRayCastingVisibility has been called
        FindOccludee(fe, _Grid, epsilon, &aFace, timestamp++);
#if LOGGING
        if (_global.debug & G_DEBUG_FREESTYLE) {
          cout << "\tFEdge: occludee only (" << (aFace != NULL ? "found" : "not found") << ")"
               << endl;
        }
#endif
      }

      if (aFace) {
        fe->setaFace(*aFace);
        aFaces.push_back(aFace);
        fe->setOccludeeEmpty(false);
#if LOGGING
        if (_global.debug & G_DEBUG_FREESTYLE) {
          cout << "\tFound occludee" << endl;
        }
#endif
      }
      else {
        // ARB: We are arbitrarily using the last observed value for occludee (almost always the
        // value observed
        //     for the edge before festart). Is that meaningful?
        // ...in fact, _occludeeEmpty seems to be unused.
        fe->setOccludeeEmpty(true);
      }

      ++nSamples;
      fe = fe->nextEdge();
    } while ((maxCard < qiMajority) && (fe) && (fe != festart));
#if LOGGING
    if (_global.debug & G_DEBUG_FREESTYLE) {
      cout << "\tFinished with " << nSamples << " samples, maxCard = " << maxCard << endl;
    }
#endif

    // ViewEdge
    // qi --
    (*ve)->setQI(maxIndex);
    // occluders --
    for (set<ViewShape *>::iterator o = occluders.begin(), oend = occluders.end(); o != oend;
         ++o) {
      (*ve)->AddOccluder((*o));
    }
#if LOGGING
    if (_global.debug & G_DEBUG_FREESTYLE) {
      cout << "\tConclusion: QI = " << maxIndex << ", " << (*ve)->occluders_size() << " occluders."
           << endl;
    }
#endif
    // occludee --
    if (!aFaces.empty()) {
      if (aFaces.size() <= (float)nSamples / 2.0f) {
        (*ve)->setaShape(0);
      }
      else {
        vector<Polygon3r *>::iterator p = aFaces.begin();
        WFace *wface = (WFace *)((*p)->userdata);
        ViewShape *vshape = ioViewMap->viewShape(wface->GetVertex(0)->shape()->GetId());
        ++p;
        (*ve)->setaShape(vshape);
      }
    }

    if (progressBarDisplay) {
      counter--;
      if (counter <= 0) {
        counter = progressBarStep;
        _pProgressBar->setProgress(_pProgressBar->getProgress() + 1);
      }
    }
    aFaces.clear();
  }
}

void ViewMapBuilder::ComputeFastRayCastingVisibility(ViewMap *ioViewMap, real epsilon)
{
  vector<ViewEdge *> &vedges = ioViewMap->ViewEdges();
  bool progressBarDisplay = false;
  unsigned progressBarStep = 0;
  unsigned vEdgesSize = vedges.size();
  unsigned fEdgesSize = ioViewMap->FEdges().size();

  if (_pProgressBar != NULL && fEdgesSize > gProgressBarMinSize) {
    unsigned progressBarSteps = min(gProgressBarMaxSteps, vEdgesSize);
    progressBarStep = vEdgesSize / progressBarSteps;
    _pProgressBar->reset();
    _pProgressBar->setLabelText("Computing Ray casting Visibility");
    _pProgressBar->setTotalSteps(progressBarSteps);
    _pProgressBar->setProgress(0);
    progressBarDisplay = true;
  }

  unsigned counter = progressBarStep;
  FEdge *fe, *festart;
  unsigned nSamples = 0;
  vector<Polygon3r *> aFaces;
  Polygon3r *aFace = NULL;
  unsigned tmpQI = 0;
  unsigned qiClasses[256];
  unsigned maxIndex, maxCard;
  unsigned qiMajority;
  static unsigned timestamp = 1;
  bool even_test;
  for (vector<ViewEdge *>::iterator ve = vedges.begin(), veend = vedges.end(); ve != veend; ve++) {
    if (_pRenderMonitor && _pRenderMonitor->testBreak()) {
      break;
    }

    festart = (*ve)->fedgeA();
    fe = (*ve)->fedgeA();
    qiMajority = 1;
    do {
      qiMajority++;
      fe = fe->nextEdge();
    } while (fe && fe != festart);
    if (qiMajority >= 4) {
      qiMajority >>= 2;
    }
    else {
      qiMajority = 1;
    }

    set<ViewShape *> occluders;

    even_test = true;
    maxIndex = 0;
    maxCard = 0;
    nSamples = 0;
    memset(qiClasses, 0, 256 * sizeof(*qiClasses));
    fe = (*ve)->fedgeA();
    do {
      if (even_test) {
        if ((maxCard < qiMajority)) {
          tmpQI = ComputeRayCastingVisibility(fe, _Grid, epsilon, occluders, &aFace, timestamp++);

          // ARB: This is an error condition, not an alert condition.
          // Some sort of recovery or abort is necessary.
          if (tmpQI >= 256) {
            cerr << "Warning: too many occluding levels" << endl;
            // ARB: Wild guess: instead of aborting or corrupting memory, treat as tmpQI == 255
            tmpQI = 255;
          }

          if (++qiClasses[tmpQI] > maxCard) {
            maxCard = qiClasses[tmpQI];
            maxIndex = tmpQI;
          }
        }
        else {
          // ARB: FindOccludee is redundant if ComputeRayCastingVisibility has been called
          FindOccludee(fe, _Grid, epsilon, &aFace, timestamp++);
        }

        if (aFace) {
          fe->setaFace(*aFace);
          aFaces.push_back(aFace);
        }
        ++nSamples;
        even_test = false;
      }
      else {
        even_test = true;
      }
      fe = fe->nextEdge();
    } while ((maxCard < qiMajority) && (fe) && (fe != festart));

    (*ve)->setQI(maxIndex);

    if (!aFaces.empty()) {
      if (aFaces.size() < nSamples / 2) {
        (*ve)->setaShape(0);
      }
      else {
        vector<Polygon3r *>::iterator p = aFaces.begin();
        WFace *wface = (WFace *)((*p)->userdata);
        ViewShape *vshape = ioViewMap->viewShape(wface->GetVertex(0)->shape()->GetId());
        ++p;
#if 0
        for (; p != pend; ++p) {
          WFace *f = (WFace *)((*p)->userdata);
          ViewShape *vs = ioViewMap->viewShape(f->GetVertex(0)->shape()->GetId());
          if (vs != vshape) {
            sameShape = false;
            break;
          }
        }
        if (sameShape)
#endif
        {
          (*ve)->setaShape(vshape);
        }
      }
    }

    //(*ve)->setaFace(aFace);

    if (progressBarDisplay) {
      counter--;
      if (counter <= 0) {
        counter = progressBarStep;
        _pProgressBar->setProgress(_pProgressBar->getProgress() + 1);
      }
    }
    aFaces.clear();
  }
}

void ViewMapBuilder::ComputeVeryFastRayCastingVisibility(ViewMap *ioViewMap, real epsilon)
{
  vector<ViewEdge *> &vedges = ioViewMap->ViewEdges();
  bool progressBarDisplay = false;
  unsigned progressBarStep = 0;
  unsigned vEdgesSize = vedges.size();
  unsigned fEdgesSize = ioViewMap->FEdges().size();

  if (_pProgressBar != NULL && fEdgesSize > gProgressBarMinSize) {
    unsigned progressBarSteps = min(gProgressBarMaxSteps, vEdgesSize);
    progressBarStep = vEdgesSize / progressBarSteps;
    _pProgressBar->reset();
    _pProgressBar->setLabelText("Computing Ray casting Visibility");
    _pProgressBar->setTotalSteps(progressBarSteps);
    _pProgressBar->setProgress(0);
    progressBarDisplay = true;
  }

  unsigned counter = progressBarStep;
  FEdge *fe;
  unsigned qi = 0;
  Polygon3r *aFace = NULL;
  static unsigned timestamp = 1;
  for (vector<ViewEdge *>::iterator ve = vedges.begin(), veend = vedges.end(); ve != veend; ve++) {
    if (_pRenderMonitor && _pRenderMonitor->testBreak()) {
      break;
    }

    set<ViewShape *> occluders;

    fe = (*ve)->fedgeA();
    qi = ComputeRayCastingVisibility(fe, _Grid, epsilon, occluders, &aFace, timestamp++);
    if (aFace) {
      fe->setaFace(*aFace);
      WFace *wface = (WFace *)(aFace->userdata);
      ViewShape *vshape = ioViewMap->viewShape(wface->GetVertex(0)->shape()->GetId());
      (*ve)->setaShape(vshape);
    }
    else {
      (*ve)->setaShape(0);
    }

    (*ve)->setQI(qi);

    if (progressBarDisplay) {
      counter--;
      if (counter <= 0) {
        counter = progressBarStep;
        _pProgressBar->setProgress(_pProgressBar->getProgress() + 1);
      }
    }
  }
}

void ViewMapBuilder::FindOccludee(FEdge *fe,
                                  Grid *iGrid,
                                  real epsilon,
                                  Polygon3r **oaPolygon,
                                  unsigned timestamp,
                                  Vec3r &u,
                                  Vec3r &A,
                                  Vec3r &origin,
                                  Vec3r &edgeDir,
                                  vector<WVertex *> &faceVertices)
{
  WFace *face = NULL;
  if (fe->isSmooth()) {
    FEdgeSmooth *fes = dynamic_cast<FEdgeSmooth *>(fe);
    face = (WFace *)fes->face();
  }
  OccludersSet occluders;
  WFace *oface;
  bool skipFace;

  WVertex::incoming_edge_iterator ie;
  OccludersSet::iterator p, pend;

  *oaPolygon = NULL;
  if (((fe)->getNature() & Nature::SILHOUETTE) || ((fe)->getNature() & Nature::BORDER)) {
    occluders.clear();
    // we cast a ray from A in the same direction but looking behind
    Vec3r v(-u[0], -u[1], -u[2]);
    iGrid->castInfiniteRay(A, v, occluders, timestamp);

    bool noIntersection = true;
    real mint = FLT_MAX;
    // we met some occluders, let us fill the aShape field with the first intersected occluder
    for (p = occluders.begin(), pend = occluders.end(); p != pend; p++) {
      // check whether the edge and the polygon plane are coincident:
      //-------------------------------------------------------------
      // first let us compute the plane equation.
      oface = (WFace *)(*p)->userdata;
      Vec3r v1(((*p)->getVertices())[0]);
      Vec3r normal((*p)->getNormal());
      real d = -(v1 * normal);
      real t, t_u, t_v;

      if (face) {
        skipFace = false;

        if (face == oface) {
          continue;
        }

        if (faceVertices.empty()) {
          continue;
        }

        for (vector<WVertex *>::iterator fv = faceVertices.begin(), fvend = faceVertices.end();
             fv != fvend;
             ++fv) {
          if ((*fv)->isBoundary()) {
            continue;
          }
          WVertex::incoming_edge_iterator iebegin = (*fv)->incoming_edges_begin();
          WVertex::incoming_edge_iterator ieend = (*fv)->incoming_edges_end();
          for (ie = iebegin; ie != ieend; ++ie) {
            if ((*ie) == 0) {
              continue;
            }

            WFace *sface = (*ie)->GetbFace();
            if (sface == oface) {
              skipFace = true;
              break;
            }
          }
          if (skipFace) {
            break;
          }
        }
        if (skipFace) {
          continue;
        }
      }
      else {
        if (GeomUtils::COINCIDENT ==
            GeomUtils::intersectRayPlane(origin, edgeDir, normal, d, t, epsilon)) {
          continue;
        }
      }
      if ((*p)->rayIntersect(A, v, t, t_u, t_v)) {
        if (fabs(v * normal) > 0.0001) {
          if (t > 0.0) {  // && t < 1.0) {
            if (t < mint) {
              *oaPolygon = (*p);
              mint = t;
              noIntersection = false;
              fe->setOccludeeIntersection(Vec3r(A + t * v));
            }
          }
        }
      }
    }

    if (noIntersection) {
      *oaPolygon = NULL;
    }
  }
}

void ViewMapBuilder::FindOccludee(
    FEdge *fe, Grid *iGrid, real epsilon, Polygon3r **oaPolygon, unsigned timestamp)
{
  OccludersSet occluders;

  Vec3r A;
  Vec3r edgeDir;
  Vec3r origin;
  A = Vec3r(((fe)->vertexA()->point3D() + (fe)->vertexB()->point3D()) / 2.0);
  edgeDir = Vec3r((fe)->vertexB()->point3D() - (fe)->vertexA()->point3D());
  edgeDir.normalize();
  origin = Vec3r((fe)->vertexA()->point3D());
  Vec3r u;
  if (_orthographicProjection) {
    u = Vec3r(0.0, 0.0, _viewpoint.z() - A.z());
  }
  else {
    u = Vec3r(_viewpoint - A);
  }
  u.normalize();
  if (A < iGrid->getOrigin()) {
    cerr << "Warning: point is out of the grid for fedge " << fe->getId().getFirst() << "-"
         << fe->getId().getSecond() << endl;
  }

  vector<WVertex *> faceVertices;

  WFace *face = NULL;
  if (fe->isSmooth()) {
    FEdgeSmooth *fes = dynamic_cast<FEdgeSmooth *>(fe);
    face = (WFace *)fes->face();
  }
  if (face) {
    face->RetrieveVertexList(faceVertices);
  }

  return FindOccludee(
      fe, iGrid, epsilon, oaPolygon, timestamp, u, A, origin, edgeDir, faceVertices);
}

int ViewMapBuilder::ComputeRayCastingVisibility(FEdge *fe,
                                                Grid *iGrid,
                                                real epsilon,
                                                set<ViewShape *> &oOccluders,
                                                Polygon3r **oaPolygon,
                                                unsigned timestamp)
{
  OccludersSet occluders;
  int qi = 0;

  Vec3r center;
  Vec3r edgeDir;
  Vec3r origin;

  center = fe->center3d();
  edgeDir = Vec3r(fe->vertexB()->point3D() - fe->vertexA()->point3D());
  edgeDir.normalize();
  origin = Vec3r(fe->vertexA()->point3D());
  // Is the edge outside the view frustum ?
  Vec3r gridOrigin(iGrid->getOrigin());
  Vec3r gridExtremity(iGrid->getOrigin() + iGrid->gridSize());

  if ((center.x() < gridOrigin.x()) || (center.y() < gridOrigin.y()) ||
      (center.z() < gridOrigin.z()) || (center.x() > gridExtremity.x()) ||
      (center.y() > gridExtremity.y()) || (center.z() > gridExtremity.z())) {
    cerr << "Warning: point is out of the grid for fedge " << fe->getId() << endl;
    // return 0;
  }

#if 0
  Vec3r A(fe->vertexA()->point2d());
  Vec3r B(fe->vertexB()->point2d());
  int viewport[4];
  SilhouetteGeomEngine::retrieveViewport(viewport);
  if ((A.x() < viewport[0]) || (A.x() > viewport[2]) || (A.y() < viewport[1]) ||
      (A.y() > viewport[3]) || (B.x() < viewport[0]) || (B.x() > viewport[2]) ||
      (B.y() < viewport[1]) || (B.y() > viewport[3])) {
    cerr << "Warning: point is out of the grid for fedge " << fe->getId() << endl;
    //return 0;
  }
#endif

  Vec3r vp;
  if (_orthographicProjection) {
    vp = Vec3r(center.x(), center.y(), _viewpoint.z());
  }
  else {
    vp = Vec3r(_viewpoint);
  }
  Vec3r u(vp - center);
  real raylength = u.norm();
  u.normalize();
#if 0
  if (_global.debug & G_DEBUG_FREESTYLE) {
    cout << "grid origin " << iGrid->getOrigin().x() << "," << iGrid->getOrigin().y() << ","
         << iGrid->getOrigin().z() << endl;
    cout << "center " << center.x() << "," << center.y() << "," << center.z() << endl;
  }
#endif

  iGrid->castRay(center, vp, occluders, timestamp);

  WFace *face = NULL;
  if (fe->isSmooth()) {
    FEdgeSmooth *fes = dynamic_cast<FEdgeSmooth *>(fe);
    face = (WFace *)fes->face();
  }
  vector<WVertex *> faceVertices;
  WVertex::incoming_edge_iterator ie;

  WFace *oface;
  bool skipFace;
  OccludersSet::iterator p, pend;
  if (face) {
    face->RetrieveVertexList(faceVertices);
  }

  for (p = occluders.begin(), pend = occluders.end(); p != pend; p++) {
    // If we're dealing with an exact silhouette, check whether we must take care of this occluder
    // of not. (Indeed, we don't consider the occluders that share at least one vertex with the
    // face containing this edge).
    //-----------
    oface = (WFace *)(*p)->userdata;
#if LOGGING
    if (_global.debug & G_DEBUG_FREESTYLE) {
      cout << "\t\tEvaluating intersection for occluder " << ((*p)->getVertices())[0]
           << ((*p)->getVertices())[1] << ((*p)->getVertices())[2] << endl
           << "\t\t\tand ray " << vp << " * " << u << " (center " << center << ")" << endl;
    }
#endif
    Vec3r v1(((*p)->getVertices())[0]);
    Vec3r normal((*p)->getNormal());
    real d = -(v1 * normal);
    real t, t_u, t_v;

#if LOGGING
    if (_global.debug & G_DEBUG_FREESTYLE) {
      cout << "\t\tp:  " << ((*p)->getVertices())[0] << ((*p)->getVertices())[1]
           << ((*p)->getVertices())[2] << ", norm: " << (*p)->getNormal() << endl;
    }
#endif

    if (face) {
#if LOGGING
      if (_global.debug & G_DEBUG_FREESTYLE) {
        cout << "\t\tDetermining face adjacency...";
      }
#endif
      skipFace = false;

      if (face == oface) {
#if LOGGING
        if (_global.debug & G_DEBUG_FREESTYLE) {
          cout << "  Rejecting occluder for face concurrency." << endl;
        }
#endif
        continue;
      }

      for (vector<WVertex *>::iterator fv = faceVertices.begin(), fvend = faceVertices.end();
           fv != fvend;
           ++fv) {
        if ((*fv)->isBoundary()) {
          continue;
        }

        WVertex::incoming_edge_iterator iebegin = (*fv)->incoming_edges_begin();
        WVertex::incoming_edge_iterator ieend = (*fv)->incoming_edges_end();
        for (ie = iebegin; ie != ieend; ++ie) {
          if ((*ie) == 0) {
            continue;
          }

          WFace *sface = (*ie)->GetbFace();
          // WFace *sfacea = (*ie)->GetaFace();
          // if ((sface == oface) || (sfacea == oface)) {
          if (sface == oface) {
            skipFace = true;
            break;
          }
        }
        if (skipFace) {
          break;
        }
      }
      if (skipFace) {
#if LOGGING
        if (_global.debug & G_DEBUG_FREESTYLE) {
          cout << "  Rejecting occluder for face adjacency." << endl;
        }
#endif
        continue;
      }
    }
    else {
      // check whether the edge and the polygon plane are coincident:
      //-------------------------------------------------------------
      // first let us compute the plane equation.

      if (GeomUtils::COINCIDENT ==
          GeomUtils::intersectRayPlane(origin, edgeDir, normal, d, t, epsilon)) {
#if LOGGING
        if (_global.debug & G_DEBUG_FREESTYLE) {
          cout << "\t\tRejecting occluder for target coincidence." << endl;
        }
#endif
        continue;
      }
    }

    if ((*p)->rayIntersect(center, u, t, t_u, t_v)) {
#if LOGGING
      if (_global.debug & G_DEBUG_FREESTYLE) {
        cout << "\t\tRay " << vp << " * " << u << " intersects at time " << t << " (raylength is "
             << raylength << ")" << endl;
        cout << "\t\t(u * normal) == " << (u * normal) << " for normal " << normal << endl;
      }
#endif
      if (fabs(u * normal) > 0.0001) {
        if ((t > 0.0) && (t < raylength)) {
#if LOGGING
          if (_global.debug & G_DEBUG_FREESTYLE) {
            cout << "\t\tIs occluder" << endl;
          }
#endif
          WFace *f = (WFace *)((*p)->userdata);
          ViewShape *vshape = _ViewMap->viewShape(f->GetVertex(0)->shape()->GetId());
          oOccluders.insert(vshape);
          ++qi;
          if (!_EnableQI) {
            break;
          }
        }
      }
    }
  }

  // Find occludee
  FindOccludee(fe, iGrid, epsilon, oaPolygon, timestamp, u, center, origin, edgeDir, faceVertices);

  return qi;
}

void ViewMapBuilder::ComputeIntersections(ViewMap *ioViewMap,
                                          intersection_algo iAlgo,
                                          real epsilon)
{
  switch (iAlgo) {
    case sweep_line:
      ComputeSweepLineIntersections(ioViewMap, epsilon);
      break;
    default:
      break;
  }
#if 0
  if (_global.debug & G_DEBUG_FREESTYLE) {
    ViewMap::viewvertices_container &vvertices = ioViewMap->ViewVertices();
    for (ViewMap::viewvertices_container::iterator vv = vvertices.begin(), vvend = vvertices.end();
         vv != vvend;
         ++vv) {
      if ((*vv)->getNature() == Nature::T_VERTEX) {
        TVertex *tvertex = (TVertex *)(*vv);
        cout << "TVertex " << tvertex->getId() << " has :" << endl;
        cout << "FrontEdgeA: " << tvertex->frontEdgeA().first << endl;
        cout << "FrontEdgeB: " << tvertex->frontEdgeB().first << endl;
        cout << "BackEdgeA: " << tvertex->backEdgeA().first << endl;
        cout << "BackEdgeB: " << tvertex->backEdgeB().first << endl << endl;
      }
    }
  }
#endif
}

struct less_SVertex2D : public binary_function<SVertex *, SVertex *, bool> {
  real epsilon;

  less_SVertex2D(real eps) : binary_function<SVertex *, SVertex *, bool>()
  {
    epsilon = eps;
  }

  bool operator()(SVertex *x, SVertex *y)
  {
    Vec3r A = x->point2D();
    Vec3r B = y->point2D();
    for (unsigned int i = 0; i < 3; i++) {
      if ((fabs(A[i] - B[i])) < epsilon) {
        continue;
      }
      if (A[i] < B[i]) {
        return true;
      }
      if (A[i] > B[i]) {
        return false;
      }
    }
    return false;
  }
};

typedef Segment<FEdge *, Vec3r> segment;
typedef Intersection<segment> intersection;

struct less_Intersection : public binary_function<intersection *, intersection *, bool> {
  segment *edge;

  less_Intersection(segment *iEdge) : binary_function<intersection *, intersection *, bool>()
  {
    edge = iEdge;
  }

  bool operator()(intersection *x, intersection *y)
  {
    real tx = x->getParameter(edge);
    real ty = y->getParameter(edge);
    if (tx > ty) {
      return true;
    }
    return false;
  }
};

struct silhouette_binary_rule : public binary_rule<segment, segment> {
  silhouette_binary_rule() : binary_rule<segment, segment>()
  {
  }

  virtual bool operator()(segment &s1, segment &s2)
  {
    FEdge *f1 = s1.edge();
    FEdge *f2 = s2.edge();

    if ((!(((f1)->getNature() & Nature::SILHOUETTE) || ((f1)->getNature() & Nature::BORDER))) &&
        (!(((f2)->getNature() & Nature::SILHOUETTE) || ((f2)->getNature() & Nature::BORDER)))) {
      return false;
    }

    return true;
  }
};

void ViewMapBuilder::ComputeSweepLineIntersections(ViewMap *ioViewMap, real epsilon)
{
  vector<SVertex *> &svertices = ioViewMap->SVertices();
  bool progressBarDisplay = false;
  unsigned sVerticesSize = svertices.size();
  unsigned fEdgesSize = ioViewMap->FEdges().size();
#if 0
  if (_global.debug & G_DEBUG_FREESTYLE) {
    ViewMap::fedges_container &fedges = ioViewMap->FEdges();
    for (ViewMap::fedges_container::const_iterator f = fedges.begin(), end = fedges.end();
         f != end;
         ++f) {
      cout << (*f)->aMaterialIndex() << "-" << (*f)->bMaterialIndex() << endl;
    }
  }
#endif
  unsigned progressBarStep = 0;

  if (_pProgressBar != NULL && fEdgesSize > gProgressBarMinSize) {
    unsigned progressBarSteps = min(gProgressBarMaxSteps, sVerticesSize);
    progressBarStep = sVerticesSize / progressBarSteps;
    _pProgressBar->reset();
    _pProgressBar->setLabelText("Computing Sweep Line Intersections");
    _pProgressBar->setTotalSteps(progressBarSteps);
    _pProgressBar->setProgress(0);
    progressBarDisplay = true;
  }

  unsigned counter = progressBarStep;

  sort(svertices.begin(), svertices.end(), less_SVertex2D(epsilon));

  SweepLine<FEdge *, Vec3r> SL;

  vector<FEdge *> &ioEdges = ioViewMap->FEdges();

  vector<segment *> segments;

  vector<FEdge *>::iterator fe, fend;

  for (fe = ioEdges.begin(), fend = ioEdges.end(); fe != fend; fe++) {
    segment *s = new segment((*fe), (*fe)->vertexA()->point2D(), (*fe)->vertexB()->point2D());
    (*fe)->userdata = s;
    segments.push_back(s);
  }

  vector<segment *> vsegments;
  for (vector<SVertex *>::iterator sv = svertices.begin(), svend = svertices.end(); sv != svend;
       sv++) {
    if (_pRenderMonitor && _pRenderMonitor->testBreak()) {
      break;
    }

    const vector<FEdge *> &vedges = (*sv)->fedges();

    for (vector<FEdge *>::const_iterator sve = vedges.begin(), sveend = vedges.end();
         sve != sveend;
         sve++) {
      vsegments.push_back((segment *)((*sve)->userdata));
    }

    Vec3r evt((*sv)->point2D());
    silhouette_binary_rule sbr;
    SL.process(evt, vsegments, sbr, epsilon);

    if (progressBarDisplay) {
      counter--;
      if (counter <= 0) {
        counter = progressBarStep;
        _pProgressBar->setProgress(_pProgressBar->getProgress() + 1);
      }
    }
    vsegments.clear();
  }

  if (_pRenderMonitor && _pRenderMonitor->testBreak()) {
    // delete segments
    if (!segments.empty()) {
      vector<segment *>::iterator s, send;
      for (s = segments.begin(), send = segments.end(); s != send; s++) {
        delete *s;
      }
    }
    return;
  }

  // reset userdata:
  for (fe = ioEdges.begin(), fend = ioEdges.end(); fe != fend; fe++) {
    (*fe)->userdata = NULL;
  }

  // list containing the new edges resulting from splitting operations.
  vector<FEdge *> newEdges;

  // retrieve the intersected edges:
  vector<segment *> &iedges = SL.intersectedEdges();
  // retrieve the intersections:
  vector<intersection *> &intersections = SL.intersections();

  int id = 0;
  // create a view vertex for each intersection and linked this one with the intersection object
  vector<intersection *>::iterator i, iend;
  for (i = intersections.begin(), iend = intersections.end(); i != iend; i++) {
    FEdge *fA = (*i)->EdgeA->edge();
    FEdge *fB = (*i)->EdgeB->edge();

    Vec3r A1 = fA->vertexA()->point3D();
    Vec3r A2 = fA->vertexB()->point3D();
    Vec3r B1 = fB->vertexA()->point3D();
    Vec3r B2 = fB->vertexB()->point3D();

    Vec3r a1 = fA->vertexA()->point2D();
    Vec3r a2 = fA->vertexB()->point2D();
    Vec3r b1 = fB->vertexA()->point2D();
    Vec3r b2 = fB->vertexB()->point2D();

    real ta = (*i)->tA;
    real tb = (*i)->tB;

    if ((ta < -epsilon) || (ta > 1 + epsilon)) {
      cerr << "Warning: 2D intersection out of range for edge " << fA->vertexA()->getId() << " - "
           << fA->vertexB()->getId() << endl;
    }

    if ((tb < -epsilon) || (tb > 1 + epsilon)) {
      cerr << "Warning: 2D intersection out of range for edge " << fB->vertexA()->getId() << " - "
           << fB->vertexB()->getId() << endl;
    }

    real Ta = SilhouetteGeomEngine::ImageToWorldParameter(fA, ta);
    real Tb = SilhouetteGeomEngine::ImageToWorldParameter(fB, tb);

    if ((Ta < -epsilon) || (Ta > 1 + epsilon)) {
      cerr << "Warning: 3D intersection out of range for edge " << fA->vertexA()->getId() << " - "
           << fA->vertexB()->getId() << endl;
    }

    if ((Tb < -epsilon) || (Tb > 1 + epsilon)) {
      cerr << "Warning: 3D intersection out of range for edge " << fB->vertexA()->getId() << " - "
           << fB->vertexB()->getId() << endl;
    }

#if 0
    if (_global.debug & G_DEBUG_FREESTYLE) {
      if ((Ta < -epsilon) || (Ta > 1 + epsilon) || (Tb < -epsilon) || (Tb > 1 + epsilon)) {
        printf("ta %.12e\n", ta);
        printf("tb %.12e\n", tb);
        printf("a1 %e, %e -- a2 %e, %e\n", a1[0], a1[1], a2[0], a2[1]);
        printf("b1 %e, %e -- b2 %e, %e\n", b1[0], b1[1], b2[0], b2[1]);
        //printf("line([%e, %e], [%e, %e]);\n", a1[0], a2[0], a1[1], a2[1]);
        //printf("line([%e, %e], [%e, %e]);\n", b1[0], b2[0], b1[1], b2[1]);
        if ((Ta < -epsilon) || (Ta > 1 + epsilon)) {
          printf("Ta %.12e\n", Ta);
        }
        if ((Tb < -epsilon) || (Tb > 1 + epsilon)) {
          printf("Tb %.12e\n", Tb);
        }
        printf("A1 %e, %e, %e -- A2 %e, %e, %e\n", A1[0], A1[1], A1[2], A2[0], A2[1], A2[2]);
        printf("B1 %e, %e, %e -- B2 %e, %e, %e\n", B1[0], B1[1], B1[2], B2[0], B2[1], B2[2]);
      }
    }
#endif

    TVertex *tvertex = ioViewMap->CreateTVertex(Vec3r(A1 + Ta * (A2 - A1)),
                                                Vec3r(a1 + ta * (a2 - a1)),
                                                fA,
                                                Vec3r(B1 + Tb * (B2 - B1)),
                                                Vec3r(b1 + tb * (b2 - b1)),
                                                fB,
                                                id);

    (*i)->userdata = tvertex;
    ++id;
  }

  progressBarStep = 0;

  if (progressBarDisplay) {
    unsigned iEdgesSize = iedges.size();
    unsigned progressBarSteps = min(gProgressBarMaxSteps, iEdgesSize);
    progressBarStep = iEdgesSize / progressBarSteps;
    _pProgressBar->reset();
    _pProgressBar->setLabelText("Splitting intersected edges");
    _pProgressBar->setTotalSteps(progressBarSteps);
    _pProgressBar->setProgress(0);
  }

  counter = progressBarStep;

  vector<TVertex *> edgeVVertices;
  vector<ViewEdge *> newVEdges;
  vector<segment *>::iterator s, send;
  for (s = iedges.begin(), send = iedges.end(); s != send; s++) {
    edgeVVertices.clear();
    newEdges.clear();
    newVEdges.clear();

    FEdge *fedge = (*s)->edge();
    ViewEdge *vEdge = fedge->viewedge();
    ViewShape *shape = vEdge->viewShape();

    vector<intersection *> &eIntersections = (*s)->intersections();
    // we first need to sort these intersections from farther to closer to A
    sort(eIntersections.begin(), eIntersections.end(), less_Intersection(*s));
    for (i = eIntersections.begin(), iend = eIntersections.end(); i != iend; i++) {
      edgeVVertices.push_back((TVertex *)(*i)->userdata);
    }

    shape->SplitEdge(fedge, edgeVVertices, ioViewMap->FEdges(), ioViewMap->ViewEdges());

    if (progressBarDisplay) {
      counter--;
      if (counter <= 0) {
        counter = progressBarStep;
        _pProgressBar->setProgress(_pProgressBar->getProgress() + 1);
      }
    }
  }

  // reset userdata:
  for (fe = ioEdges.begin(), fend = ioEdges.end(); fe != fend; fe++) {
    (*fe)->userdata = NULL;
  }

  // delete segments
  if (!segments.empty()) {
    for (s = segments.begin(), send = segments.end(); s != send; s++) {
      delete *s;
    }
  }
}

} /* namespace Freestyle */
