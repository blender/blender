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

#ifndef __FREESTYLE_VIEW_MAP_BUILDER_H__
#define __FREESTYLE_VIEW_MAP_BUILDER_H__

/** \file
 * \ingroup freestyle
 * \brief Class to build silhouette edges from a Winged-Edge structure
 */

#include <vector>

#include "GridDensityProvider.h"
#include "Silhouette.h"
#include "SilhouetteGeomEngine.h"
#include "ViewEdgeXBuilder.h"
#include "ViewMap.h"

#include "../geometry/Geom.h"
#include "../geometry/GeomUtils.h"
#include "../geometry/Grid.h"
#include "../geometry/SweepLine.h"

#include "../scene_graph/NodeGroup.h"
#include "../scene_graph/TriangleRep.h"

#include "../system/FreestyleConfig.h"
#include "../system/ProgressBar.h"
#include "../system/RenderMonitor.h"
#include "../system/TimeUtils.h"

#include "../winged_edge/WEdge.h"
#include "../winged_edge/WXEdge.h"

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

namespace Freestyle {

using namespace Geometry;

class ViewMapBuilder {
 private:
  ViewMap *_ViewMap;  // result
  // SilhouetteGeomEngine _GeomEngine;
  ProgressBar *_pProgressBar;
  RenderMonitor *_pRenderMonitor;
  Vec3r _viewpoint;
  bool _orthographicProjection;
  Grid *_Grid;
  ViewEdgeXBuilder *_pViewEdgeBuilder;
  bool _EnableQI;
  double _epsilon;

  // tmp values:
  int _currentId;
  int _currentFId;
  int _currentSVertexId;

 public:
  typedef enum {
    sweep_line,
  } intersection_algo;

  typedef enum {
    ray_casting,
    ray_casting_fast,
    ray_casting_very_fast,
    ray_casting_culled_adaptive_traditional,
    ray_casting_adaptive_traditional,
    ray_casting_culled_adaptive_cumulative,
    ray_casting_adaptive_cumulative,
  } visibility_algo;

  inline ViewMapBuilder()
  {
    _pProgressBar = NULL;
    _pRenderMonitor = NULL;
    _Grid = NULL;
    _currentId = 1;
    _currentFId = 0;
    _currentSVertexId = 0;
    _pViewEdgeBuilder = new ViewEdgeXBuilder;
    _EnableQI = true;
  }

  inline ~ViewMapBuilder()
  {
    if (_pViewEdgeBuilder) {
      delete _pViewEdgeBuilder;
      _pViewEdgeBuilder = NULL;
    }
  }

  /* Build Grid for ray casting */
  /*! Build non-culled Grid in camera space for ray casting */
  void BuildGrid(WingedEdge &we, const BBox<Vec3r> &bbox, unsigned int sceneNumFaces);

  /*! Compute Shapes from a WingedEdge containing a list of WShapes */
  void computeInitialViewEdges(WingedEdge &);

  /*! Compute Cusps */
  void computeCusps(ViewMap *ioViewMap);

  /*! Detects cusps (for a single ViewEdge) among SVertices and builds a ViewVertex on top of each
   * cusp SVertex We use a hysteresis approach to avoid noise.
   */
  void DetectCusps(ViewEdge *ioEdge);

  /*! Sets the current viewpoint */
  inline void setViewpoint(const Vec3r &ivp)
  {
    _viewpoint = ivp;
    SilhouetteGeomEngine::setViewpoint(ivp);
  }

  /*! Sets the current transformation
   *    iModelViewMatrix
   *      The 4x4 model view matrix, in column major order (openGL like).
   *    iProjection matrix
   *      The 4x4 projection matrix, in column major order (openGL like).
   *    iViewport
   *      The viewport. 4 real array: origin.x, origin.y, width, length
   */
  inline void setTransform(const real iModelViewMatrix[4][4],
                           const real iProjectionMatrix[4][4],
                           const int iViewport[4],
                           real iFocalLength,
                           real /*iAspect*/,
                           real /*iFovy*/)
  {
    _orthographicProjection = (iProjectionMatrix[3][3] != 0.0);
    SilhouetteGeomEngine::setTransform(
        iModelViewMatrix, iProjectionMatrix, iViewport, iFocalLength);
  }

  inline void setFrustum(real iZnear, real iZfar)
  {
    SilhouetteGeomEngine::setFrustum(iZnear, iZfar);
  }

  /*! Builds the scene view map returns the list the view map
   *  it is up to the caller to delete this ViewMap
   *    iWRoot
   *      The root group node containing the WEdge structured scene
   */
  ViewMap *BuildViewMap(WingedEdge &we,
                        visibility_algo iAlgo,
                        real epsilon,
                        const BBox<Vec3r> &bbox,
                        unsigned int sceneNumFaces);

  void CullViewEdges(ViewMap *ioViewMap,
                     real viewProscenium[4],
                     real occluderProscenium[4],
                     bool extensiveFEdgeSearch = true);

  /*! computes the intersection between all 2D feature edges of the scene.
   *    ioViewMap
   *      The view map. It is modified by the method.
   *      The list of all features edges of the scene.
   *      Each time an intersection is found, the 2 intersecting edges are splitted (creating 2 new
   * vertices) At the end, this list is updated with the adding of all new created edges (resulting
   * from splitting). iAlgo The algo to use for computing the intersections
   */
  void ComputeIntersections(ViewMap *ioViewMap,
                            intersection_algo iAlgo = sweep_line,
                            real epsilon = 1.0e-06);

  /*! Computes the 2D scene silhouette edges visibility
   *    iGrid
   *      For the Ray Casting algorithm.
   */
  void ComputeEdgesVisibility(ViewMap *ioViewMap,
                              WingedEdge &we,
                              const BBox<Vec3r> &bbox,
                              unsigned int sceneNumFaces,
                              visibility_algo iAlgo = ray_casting,
                              real epsilon = 1.0e-6);

  void setGrid(Grid *iGrid)
  {
    _Grid = iGrid;
  }

  /*! accessors */

  /*! Modifiers */
  inline void setProgressBar(ProgressBar *iProgressBar)
  {
    _pProgressBar = iProgressBar;
  }

  inline void setRenderMonitor(RenderMonitor *iRenderMonitor)
  {
    _pRenderMonitor = iRenderMonitor;
  }

  inline void setEnableQI(bool iBool)
  {
    _EnableQI = iBool;
  }

 protected:
  /*! Computes intersections on all edges of the scene using a sweep line algorithm */
  void ComputeSweepLineIntersections(ViewMap *ioViewMap, real epsilon = 1.0e-6);

  /*! Computes the 2D scene silhouette edges visibility using a ray casting. On each edge, a ray is
   * cast to check its quantitative invisibility. The list of occluders are each time stored in the
   * tested edge. ioViewMap The view map. The 2D scene silhouette edges as FEdges. These edges have
   * already been splitted at their intersections points. Thus, these edges do not intersect
   * anymore. The visibility corresponding to each edge of ioScene is set is this edge.
   */
  void ComputeRayCastingVisibility(ViewMap *ioViewMap, real epsilon = 1.0e-6);
  void ComputeFastRayCastingVisibility(ViewMap *ioViewMap, real epsilon = 1.0e-6);
  void ComputeVeryFastRayCastingVisibility(ViewMap *ioViewMap, real epsilon = 1.0e-6);

  void ComputeCumulativeVisibility(ViewMap *ioViewMap,
                                   WingedEdge &we,
                                   const BBox<Vec3r> &bbox,
                                   real epsilon,
                                   bool cull,
                                   GridDensityProviderFactory &factory);
  void ComputeDetailedVisibility(ViewMap *ioViewMap,
                                 WingedEdge &we,
                                 const BBox<Vec3r> &bbox,
                                 real epsilon,
                                 bool cull,
                                 GridDensityProviderFactory &factory);

  /*! Compute the visibility for the FEdge fe.
   *  The occluders are added to fe occluders list.
   *    fe
   *      The FEdge
   *    iGrid
   *      The grid used to compute the ray casting visibility
   *    epsilon
   *      The epsilon used for computation
   *    oShapeId
   *      fe is the border (in 2D) between 2 2D spaces.
   *      if fe is a silhouette, One of these 2D spaces is occupied by the shape to which fe
   * belongs (on its left) and the other one is either occupied by another shape or empty or
   * occupied by the same shape. We use this ray csating operation to determine which shape lies on
   * fe's right. The result is the shape id stored in oShapeId
   */
  int ComputeRayCastingVisibility(FEdge *fe,
                                  Grid *iGrid,
                                  real epsilon,
                                  set<ViewShape *> &oOccluders,
                                  Polygon3r **oaPolygon,
                                  unsigned timestamp);
  // FIXME
  void FindOccludee(
      FEdge *fe, Grid *iGrid, real epsilon, Polygon3r **oaPolygon, unsigned timestamp);
  void FindOccludee(FEdge *fe,
                    Grid *iGrid,
                    real epsilon,
                    Polygon3r **oaPolygon,
                    unsigned timestamp,
                    Vec3r &u,
                    Vec3r &A,
                    Vec3r &origin,
                    Vec3r &edgeDir,
                    vector<WVertex *> &faceVertices);

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:ViewMapBuilder")
#endif
};

} /* namespace Freestyle */

#endif  // __FREESTYLE_VIEW_MAP_BUILDER_H__
