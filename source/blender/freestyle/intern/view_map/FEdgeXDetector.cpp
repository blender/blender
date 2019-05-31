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
 * \brief Detects/flags/builds extended features edges on the WXEdge structure
 */

#include <float.h>

#include "FEdgeXDetector.h"

#include "../geometry/GeomUtils.h"
#include "../geometry/normal_cycle.h"

#include "BKE_global.h"

namespace Freestyle {

void FEdgeXDetector::processShapes(WingedEdge &we)
{
  bool progressBarDisplay = false;
#if 0
  Vec3r Min, Max;
#endif
  vector<WShape *> wshapes = we.getWShapes();
  WXShape *wxs;

  if (_pProgressBar != NULL) {
    _pProgressBar->reset();
    _pProgressBar->setLabelText("Detecting feature lines");
    _pProgressBar->setTotalSteps(wshapes.size() * 3);
    _pProgressBar->setProgress(0);
    progressBarDisplay = true;
  }

  for (vector<WShape *>::const_iterator it = wshapes.begin(); it != wshapes.end(); it++) {
    if (_pRenderMonitor && _pRenderMonitor->testBreak()) {
      break;
    }
    wxs = dynamic_cast<WXShape *>(*it);
#if 0
    wxs->bbox(Min, Max);
    _bbox_diagonal = (Max - Min).norm();
#endif
    if (_changes) {
      vector<WFace *> &wfaces = wxs->GetFaceList();
      for (vector<WFace *>::iterator wf = wfaces.begin(), wfend = wfaces.end(); wf != wfend;
           ++wf) {
        WXFace *wxf = dynamic_cast<WXFace *>(*wf);
        wxf->Clear();
      }
      _computeViewIndependent = true;
    }
    else if (!(wxs)->getComputeViewIndependentFlag()) {
      wxs->Reset();
      _computeViewIndependent = false;
    }
    else {
      _computeViewIndependent = true;
    }
    preProcessShape(wxs);
    if (progressBarDisplay) {
      _pProgressBar->setProgress(_pProgressBar->getProgress() + 1);
    }
    processBorderShape(wxs);
    if (_computeMaterialBoundaries) {
      processMaterialBoundaryShape(wxs);
    }
    processCreaseShape(wxs);
    if (_computeRidgesAndValleys) {
      processRidgesAndValleysShape(wxs);
    }
    if (_computeSuggestiveContours) {
      processSuggestiveContourShape(wxs);
    }
    processSilhouetteShape(wxs);
    processEdgeMarksShape(wxs);
    if (progressBarDisplay) {
      _pProgressBar->setProgress(_pProgressBar->getProgress() + 1);
    }

    // build smooth edges:
    buildSmoothEdges(wxs);

    // Post processing for suggestive contours
    if (_computeSuggestiveContours) {
      postProcessSuggestiveContourShape(wxs);
    }
    if (progressBarDisplay) {
      _pProgressBar->setProgress(_pProgressBar->getProgress() + 1);
    }

    wxs->setComputeViewIndependentFlag(false);
    _computeViewIndependent = false;
    _changes = false;

    // reset user data
    (*it)->ResetUserData();
  }
}

// GENERAL STUFF
////////////////
void FEdgeXDetector::preProcessShape(WXShape *iWShape)
{
  _meanK1 = 0;
  _meanKr = 0;
  _minK1 = FLT_MAX;
  _maxK1 = -FLT_MAX;
  _minKr = FLT_MAX;
  _maxKr = -FLT_MAX;
  _nPoints = 0;
#if 0
  _meanEdgeSize = iWShape->getMeanEdgeSize();
#else
  _meanEdgeSize = iWShape->ComputeMeanEdgeSize();
#endif

  vector<WFace *> &wfaces = iWShape->GetFaceList();
  vector<WFace *>::iterator f, fend;
  // view dependant stuff
  for (f = wfaces.begin(), fend = wfaces.end(); f != fend; ++f) {
    preProcessFace((WXFace *)(*f));
  }

  if (_computeRidgesAndValleys || _computeSuggestiveContours) {
    vector<WVertex *> &wvertices = iWShape->getVertexList();
    for (vector<WVertex *>::iterator wv = wvertices.begin(), wvend = wvertices.end(); wv != wvend;
         ++wv) {
      // Compute curvatures
      WXVertex *wxv = dynamic_cast<WXVertex *>(*wv);
      computeCurvatures(wxv);
    }
    _meanK1 /= (real)(_nPoints);
    _meanKr /= (real)(_nPoints);
  }
}

void FEdgeXDetector::preProcessFace(WXFace *iFace)
{
  Vec3f firstPoint = iFace->GetVertex(0)->GetVertex();
  Vec3f N = iFace->GetNormal();

  // Compute the dot product between V (=_Viewpoint - firstPoint) and N:
  Vec3f V;
  if (_orthographicProjection) {
    V = Vec3f(0.0f, 0.0f, _Viewpoint.z() - firstPoint.z());
  }
  else {
    V = Vec3f(_Viewpoint - firstPoint);
  }
  N.normalize();
  V.normalize();
  iFace->setDotP(N * V);

  // compute the distance between the face center and the viewpoint:
  if (_orthographicProjection) {
    iFace->setZ(iFace->center().z() - _Viewpoint.z());
  }
  else {
    Vec3f dist_vec(iFace->center() - _Viewpoint);
    iFace->setZ(dist_vec.norm());
  }
}

void FEdgeXDetector::computeCurvatures(WXVertex *vertex)
{
  // TODO: for some reason, the 'vertex' may have no associated edges
  // (i.e., WVertex::_EdgeList is empty), which causes a crash due to
  // a subsequent call of WVertex::_EdgeList.front().
  if (vertex->GetEdges().empty()) {
    if (G.debug & G_DEBUG_FREESTYLE) {
      printf("Warning: WVertex %d has no associated edges.\n", vertex->GetId());
    }
    return;
  }

  // CURVATURE LAYER
  // store all the curvature datas for each vertex

  // soc unused - real K1, K2
  real cos2theta, sin2theta;
  Vec3r e1, n, v;
  // one vertex curvature info :
  CurvatureInfo *C;
  float radius = _sphereRadius * _meanEdgeSize;

  // view independent stuff
  if (_computeViewIndependent) {
    C = new CurvatureInfo();
    vertex->setCurvatures(C);
    OGF::NormalCycle ncycle;
    ncycle.begin();
    if (radius > 0) {
      OGF::compute_curvature_tensor(vertex, radius, ncycle);
    }
    else {
      OGF::compute_curvature_tensor_one_ring(vertex, ncycle);
    }
    ncycle.end();
    C->K1 = ncycle.kmin();
    C->K2 = ncycle.kmax();
    C->e1 = ncycle.Kmax();  // ncycle.kmin() * ncycle.Kmax();
    C->e2 = ncycle.Kmin();  // ncycle.kmax() * ncycle.Kmin();

    real absK1 = fabs(C->K1);
    _meanK1 += absK1;
    if (absK1 > _maxK1) {
      _maxK1 = absK1;
    }
    if (absK1 < _minK1) {
      _minK1 = absK1;
    }
  }
  // view dependant
  C = vertex->curvatures();
  if (C == 0) {
    return;
  }

  // compute radial curvature :
  n = C->e1 ^ C->e2;
  if (_orthographicProjection) {
    v = Vec3r(0.0, 0.0, _Viewpoint.z() - vertex->GetVertex().z());
  }
  else {
    v = Vec3r(_Viewpoint - vertex->GetVertex());
  }
  C->er = v - (v * n) * n;
  C->er.normalize();
  e1 = C->e1;
  e1.normalize();
  cos2theta = C->er * e1;
  cos2theta *= cos2theta;
  sin2theta = 1 - cos2theta;
  C->Kr = C->K1 * cos2theta + C->K2 * sin2theta;
  real absKr = fabs(C->Kr);
  _meanKr += absKr;
  if (absKr > _maxKr) {
    _maxKr = absKr;
  }
  if (absKr < _minKr) {
    _minKr = absKr;
  }

  ++_nPoints;
}

// SILHOUETTE
/////////////
void FEdgeXDetector::processSilhouetteShape(WXShape *iWShape)
{
  // Make a first pass on every polygons in order to compute all their silhouette relative values:
  vector<WFace *> &wfaces = iWShape->GetFaceList();
  vector<WFace *>::iterator f, fend;
  for (f = wfaces.begin(), fend = wfaces.end(); f != fend; ++f) {
    ProcessSilhouetteFace((WXFace *)(*f));
  }

  // Make a pass on the edges to detect the silhouette edges that are not smooth
  vector<WEdge *>::iterator we, weend;
  vector<WEdge *> &wedges = iWShape->getEdgeList();
  for (we = wedges.begin(), weend = wedges.end(); we != weend; ++we) {
    ProcessSilhouetteEdge((WXEdge *)(*we));
  }
}

void FEdgeXDetector::ProcessSilhouetteFace(WXFace *iFace)
{
  // SILHOUETTE LAYER
  Vec3f normal;
  // Compute the dot products between View direction and N at each vertex of the face:
  Vec3f point;
  int closestPointId = 0;
  float dist, minDist = FLT_MAX;
  int numVertices = iFace->numberOfVertices();
  WXFaceLayer *faceLayer = new WXFaceLayer(iFace, Nature::SILHOUETTE, true);
  for (int i = 0; i < numVertices; i++) {
    point = iFace->GetVertex(i)->GetVertex();
    normal = iFace->GetVertexNormal(i);
    normal.normalize();
    Vec3f V;
    if (_orthographicProjection) {
      V = Vec3f(0.0f, 0.0f, _Viewpoint.z() - point.z());
    }
    else {
      V = Vec3f(_Viewpoint - point);
    }
    V.normalize();
    float d = normal * V;
    faceLayer->PushDotP(d);
    // Find the point the closest to the viewpoint
    if (_orthographicProjection) {
      dist = point.z() - _Viewpoint.z();
    }
    else {
      Vec3f dist_vec(point - _Viewpoint);
      dist = dist_vec.norm();
    }
    if (dist < minDist) {
      minDist = dist;
      closestPointId = i;
    }
  }
  // Set the closest point id:
  faceLayer->setClosestPointIndex(closestPointId);
  // Add this layer to the face:
  iFace->AddSmoothLayer(faceLayer);
}

void FEdgeXDetector::ProcessSilhouetteEdge(WXEdge *iEdge)
{
  if (iEdge->nature() & Nature::BORDER) {
    return;
  }
  // SILHOUETTE ?
  //-------------
  WXFace *fA = (WXFace *)iEdge->GetaOEdge()->GetaFace();
  WXFace *fB = (WXFace *)iEdge->GetaOEdge()->GetbFace();

  if ((fA->front()) ^
      (fB->front())) {  // fA->visible XOR fB->visible (true if one is 0 and the other is 1)
    // The only edges we want to set as silhouette edges in this way are the ones with 2 different
    // normals for 1 vertex for these two faces
    //--------------------
    // In reality we only test the normals for 1 of the 2 vertices.
    if (fA->GetVertexNormal(iEdge->GetaVertex()) == fB->GetVertexNormal(iEdge->GetaVertex())) {
      return;
    }
    iEdge->AddNature(Nature::SILHOUETTE);
    if (fB->front()) {
      iEdge->setOrder(1);
    }
    else {
      iEdge->setOrder(-1);
    }
  }
}

// BORDER
/////////
void FEdgeXDetector::processBorderShape(WXShape *iWShape)
{
  if (!_computeViewIndependent) {
    return;
  }
  // Make a pass on the edges to detect the BORDER
  vector<WEdge *>::iterator we, weend;
  vector<WEdge *> &wedges = iWShape->getEdgeList();
  for (we = wedges.begin(), weend = wedges.end(); we != weend; ++we) {
    ProcessBorderEdge((WXEdge *)(*we));
  }
}

void FEdgeXDetector::ProcessBorderEdge(WXEdge *iEdge)
{
  // first check whether it is a border edge: BORDER ?
  //---------
  if (iEdge->GetaFace() == 0) {
    // it is a border edge
    iEdge->AddNature(Nature::BORDER);
  }
}

// CREASE
/////////
void FEdgeXDetector::processCreaseShape(WXShape *iWShape)
{
  if (!_computeViewIndependent) {
    return;
  }

  // Make a pass on the edges to detect the CREASE
  vector<WEdge *>::iterator we, weend;
  vector<WEdge *> &wedges = iWShape->getEdgeList();
  for (we = wedges.begin(), weend = wedges.end(); we != weend; ++we) {
    ProcessCreaseEdge((WXEdge *)(*we));
  }
}

void FEdgeXDetector::ProcessCreaseEdge(WXEdge *iEdge)
{
  // CREASE ?
  //---------
  if (iEdge->nature() & Nature::BORDER) {
    return;
  }
  WXFace *fA = (WXFace *)iEdge->GetaOEdge()->GetaFace();
  WXFace *fB = (WXFace *)iEdge->GetaOEdge()->GetbFace();

  WVertex *aVertex = iEdge->GetaVertex();
  if ((fA->GetVertexNormal(aVertex) * fB->GetVertexNormal(aVertex)) <= _creaseAngle) {
    iEdge->AddNature(Nature::CREASE);
  }
}

// RIDGES AND VALLEYS
/////////////////////
void FEdgeXDetector::processRidgesAndValleysShape(WXShape *iWShape)
{
  // Don't forget to add the built layer to the face at the end of the ProcessFace:
  // iFace->AddSmoothLayer(faceLayer);

  if (!_computeViewIndependent) {
    return;
  }

  // Here the curvatures must already have been computed
  vector<WFace *> &wfaces = iWShape->GetFaceList();
  vector<WFace *>::iterator f, fend;
  for (f = wfaces.begin(), fend = wfaces.end(); f != fend; ++f) {
    ProcessRidgeFace((WXFace *)(*f));
  }
}

// RIDGES
/////////
void FEdgeXDetector::ProcessRidgeFace(WXFace *iFace)
{
  WXFaceLayer *flayer = new WXFaceLayer(iFace, Nature::RIDGE | Nature::VALLEY, false);
  iFace->AddSmoothLayer(flayer);

  unsigned int numVertices = iFace->numberOfVertices();
  for (unsigned int i = 0; i < numVertices; ++i) {
    WVertex *wv = iFace->GetVertex(i);
    WXVertex *wxv = dynamic_cast<WXVertex *>(wv);
    flayer->PushDotP(wxv->curvatures()->K1);
  }

#if 0  // XXX fabs(flayer->dotP(i)) < threshold cannot be true
  real threshold = 0;
  //real threshold = _maxK1 - (_maxK1 - _meanK1) / 20.0;

  if (flayer->nPosDotP() != numVertices) {
    if ((fabs(flayer->dotP(0)) < threshold) && (fabs(flayer->dotP(1)) < threshold) &&
        (fabs(flayer->dotP(2)) < threshold)) {
      flayer->ReplaceDotP(0, 0);
      flayer->ReplaceDotP(1, 0);
      flayer->ReplaceDotP(2, 0);
    }
  }
#endif
}

#if 0
void FEdgeXDetector::ProcessRidgeFace(WXFace *iFace)
{
  // RIDGE LAYER
  // Compute the RidgeFunction, that is the derivative of the ppal curvature along e1 at each vertex of the face
  WVertex *v;
  Vec3r v1v2;
  real t;
  vector<WXFaceLayer *> SmoothLayers;
  WXFaceLayer *faceLayer;
  Face_Curvature_Info *layer_info;
  real K1_a(0), K1_b(0);
  Vec3r Inter_a, Inter_b;

  // find the ridge layer of the face
  iFace->retrieveSmoothLayers(Nature::RIDGE, SmoothLayers);
  if (SmoothLayers.size() != 1) {
    return;
  }
  faceLayer = SmoothLayers[0];
  // retrieve the curvature info of this layer
  layer_info = (Face_Curvature_Info *)faceLayer->userdata;

  int numVertices = iFace->numberOfVertices();
  for (int i = 0; i < numVertices; i++) {
    v = iFace->GetVertex(i);
    // vec_curvature_info[i] contains the curvature info of this vertex
    Vec3r e2 = layer_info->vec_curvature_info[i]->K2 * layer_info->vec_curvature_info[i]->e2;
    Vec3r e1 = layer_info->vec_curvature_info[i]->K1 * layer_info->vec_curvature_info[i]->e1;
    e2.normalize();

    WVertex::face_iterator fit = v->faces_begin();
    WVertex::face_iterator fitend = v->faces_end();
    for (; fit != fitend; ++fit) {
      WXFace *wxf = dynamic_cast<WXFace *>(*fit);
      WOEdge *oppositeEdge;
      if (!(wxf->getOppositeEdge(v, oppositeEdge))) {
        continue;
      }
      v1v2 = oppositeEdge->GetbVertex()->GetVertex() - oppositeEdge->GetaVertex()->GetVertex();
      GeomUtils::intersection_test res;
      res = GeomUtils::intersectRayPlane(
          oppositeEdge->GetaVertex()->GetVertex(), v1v2, e2, -(v->GetVertex() * e2), t, 1.0e-06);
      if ((res == GeomUtils::DO_INTERSECT) && (t >= 0.0) && (t <= 1.0)) {
        vector<WXFaceLayer *> second_ridge_layer;
        wxf->retrieveSmoothLayers(Nature::RIDGE, second_ridge_layer);
        if (second_ridge_layer.size() != 1) {
          continue;
        }
        Face_Curvature_Info *second_layer_info =
            (Face_Curvature_Info *)second_ridge_layer[0]->userdata;

        unsigned index1 = wxf->GetIndex(oppositeEdge->GetaVertex());
        unsigned index2 = wxf->GetIndex(oppositeEdge->GetbVertex());
        real K1_1 = second_layer_info->vec_curvature_info[index1]->K1;
        real K1_2 = second_layer_info->vec_curvature_info[index2]->K1;
        real K1 = (1.0 - t) * K1_1 + t * K1_2;
        Vec3r inter((1.0 - t) * oppositeEdge->GetaVertex()->GetVertex() +
                    t * oppositeEdge->GetbVertex()->GetVertex());
        Vec3r vtmp(inter - v->GetVertex());
        // is it K1_a or K1_b ?
        if (vtmp * e1 > 0) {
          K1_b = K1;
          Inter_b = inter;
        }
        else {
          K1_a = K1;
          Inter_a = inter;
        }
      }
    }
    // Once we have K1 along the ppal direction compute the derivative : K1b - K1a put it in DotP
    //real d = fabs(K1_b) - fabs(K1_a);
    real d = 0;
    real threshold = _meanK1 + (_maxK1 - _meanK1) / 7.0;
    //real threshold = _meanK1;
    //if ((fabs(K1_b) > threshold) || ((fabs(K1_a) > threshold)))
    d = (K1_b) - (K1_a) / (Inter_b - Inter_a).norm();
    faceLayer->PushDotP(d);
    //faceLayer->PushDotP(layer_info->vec_curvature_info[i]->K1);
  }

  // Make the values relevant by checking whether all principal directions have the "same" direction:
  Vec3r e0((layer_info->vec_curvature_info[0]->K1 * layer_info->vec_curvature_info[0]->e1));
  e0.normalize();
  Vec3r e1((layer_info->vec_curvature_info[1]->K1 * layer_info->vec_curvature_info[1]->e1));
  e1.normalize();
  Vec3r e2((layer_info->vec_curvature_info[2]->K1 * layer_info->vec_curvature_info[2]->e1));
  e2.normalize();
  if (e0 * e1 < 0) {
    // invert dotP[1]
    faceLayer->ReplaceDotP(1, -faceLayer->dotP(1));
  }
  if (e0 * e2 < 0) {
    // invert dotP[2]
    faceLayer->ReplaceDotP(2, -faceLayer->dotP(2));
  }

#  if 0  // remove the weakest values;
  real minDiff = (_maxK1 - _minK1) / 10.0;
  real minDiff = _meanK1;
  if ((faceLayer->dotP(0) < minDiff) && (faceLayer->dotP(1) < minDiff) && (faceLayer->dotP(2) < minDiff)) {
    faceLayer->ReplaceDotP(0, 0);
    faceLayer->ReplaceDotP(1, 0);
    faceLayer->ReplaceDotP(2, 0);
  }
#  endif
}
#endif

// SUGGESTIVE CONTOURS
//////////////////////

void FEdgeXDetector::processSuggestiveContourShape(WXShape *iWShape)
{
  // Here the curvatures must already have been computed
  vector<WFace *> &wfaces = iWShape->GetFaceList();
  vector<WFace *>::iterator f, fend;
  for (f = wfaces.begin(), fend = wfaces.end(); f != fend; ++f) {
    ProcessSuggestiveContourFace((WXFace *)(*f));
  }
}

void FEdgeXDetector::ProcessSuggestiveContourFace(WXFace *iFace)
{
  WXFaceLayer *faceLayer = new WXFaceLayer(iFace, Nature::SUGGESTIVE_CONTOUR, true);
  iFace->AddSmoothLayer(faceLayer);

  unsigned int numVertices = iFace->numberOfVertices();
  for (unsigned int i = 0; i < numVertices; ++i) {
    WVertex *wv = iFace->GetVertex(i);
    WXVertex *wxv = dynamic_cast<WXVertex *>(wv);
    faceLayer->PushDotP(wxv->curvatures()->Kr);
  }

#if 0  // FIXME: find a more clever way to compute the threshold
  real threshold = _meanKr;
  if (faceLayer->nPosDotP() != numVertices) {
    if ((fabs(faceLayer->dotP(0)) < threshold) && (fabs(faceLayer->dotP(1)) < threshold) &&
        (fabs(faceLayer->dotP(2)) < threshold)) {
      faceLayer->ReplaceDotP(0, 0);
      faceLayer->ReplaceDotP(1, 0);
      faceLayer->ReplaceDotP(2, 0);
    }
  }
#endif
}

void FEdgeXDetector::postProcessSuggestiveContourShape(WXShape *iShape)
{
  vector<WFace *> &wfaces = iShape->GetFaceList();
  vector<WFace *>::iterator f, fend;
  for (f = wfaces.begin(), fend = wfaces.end(); f != fend; ++f) {
    postProcessSuggestiveContourFace((WXFace *)(*f));
  }
}

void FEdgeXDetector::postProcessSuggestiveContourFace(WXFace *iFace)
{
  // Compute the derivative of the radial curvature in the radial direction, at the two extremities
  // of the smooth edge.
  // If the derivative is smaller than a given threshold _kr_derivative_epsilon, discard the edge.

  // Find the suggestive contour layer of the face (zero or one edge).
  vector<WXFaceLayer *> sc_layers;
  iFace->retrieveSmoothEdgesLayers(Nature::SUGGESTIVE_CONTOUR, sc_layers);
  if (sc_layers.empty()) {
    return;
  }

  WXFaceLayer *sc_layer;
  sc_layer = sc_layers[0];

  // Compute the derivative value at each vertex of the face, and add it in a vector.
  vector<real> kr_derivatives;

  unsigned vertices_nb = iFace->numberOfVertices();
  WXVertex *v, *opposite_vertex_a, *opposite_vertex_b;
  WXFace *wxf;
  WOEdge *opposite_edge;
  Vec3r normal_vec, radial_normal_vec, er_vec, v_vec, inter, inter1, inter2, tmp_vec;
  GeomUtils::intersection_test res;
  real kr(0), kr1(0), kr2(0), t;

  for (unsigned int i = 0; i < vertices_nb; ++i) {
    v = (WXVertex *)(iFace->GetVertex(i));

    // v is a singular vertex, skip it.
    if (v->isBoundary()) {
      kr_derivatives.push_back(0);
      continue;
    }

    v_vec = v->GetVertex();
    er_vec = v->curvatures()->er;

    // For each vertex, iterate on its adjacent faces.
    for (WVertex::face_iterator fit = v->faces_begin(), fitend = v->faces_end(); fit != fitend;
         ++fit) {
      wxf = dynamic_cast<WXFace *>(*fit);
      if (!wxf->getOppositeEdge(v, opposite_edge)) {
        continue;
      }

      opposite_vertex_a = (WXVertex *)opposite_edge->GetaVertex();
      opposite_vertex_b = (WXVertex *)opposite_edge->GetbVertex();
      normal_vec = wxf->GetVertexNormal(v);  // FIXME: what about e1 ^ e2 ?
      radial_normal_vec = er_vec ^ normal_vec;

      // Test whether the radial plan intersects with the edge at the opposite of v.
      res = GeomUtils::intersectRayPlane(opposite_vertex_a->GetVertex(),
                                         opposite_edge->GetVec(),
                                         radial_normal_vec,
                                         -(v_vec * radial_normal_vec),
                                         t,
                                         1.0e-06);

      // If there is an intersection, compute the value of the derivative ath that point.
      if ((res == GeomUtils::DO_INTERSECT) && (t >= 0) && (t <= 1)) {
        kr = t * opposite_vertex_a->curvatures()->Kr +
             (1 - t) * opposite_vertex_b->curvatures()->Kr;
        inter = opposite_vertex_a->GetVertex() + t * opposite_edge->GetVec();
        tmp_vec = inter - v->GetVertex();
        // Is it kr1 or kr2?
        if (tmp_vec * er_vec > 0) {
          kr2 = kr;
          inter2 = inter;
        }
        else {
          kr1 = kr;
          inter1 = inter;
        }
      }
    }

    // Now we have kr1 and kr2 along the radial direction, for one vertex of iFace.
    // We have to compute the derivative of kr for that vertex, equal to:
    // (kr2 - kr1) / dist(inter1, inter2).
    // Then we add it to the vector of derivatives.
    v->curvatures()->dKr = (kr2 - kr1) / (inter2 - inter1).norm();
    kr_derivatives.push_back(v->curvatures()->dKr);
  }

  // At that point, we have the derivatives for each vertex of iFace.
  // All we have to do now is to use linear interpolation to compute the values at the extremities
  // of the smooth edge.
  WXSmoothEdge *sc_edge = sc_layer->getSmoothEdge();
  WOEdge *sc_oedge = sc_edge->woea();
  t = sc_edge->ta();
  if (t * kr_derivatives[iFace->GetIndex(sc_oedge->GetaVertex())] +
          (1 - t) * kr_derivatives[iFace->GetIndex(sc_oedge->GetbVertex())] <
      _kr_derivative_epsilon) {
    sc_layer->removeSmoothEdge();
    return;
  }
  sc_oedge = sc_edge->woeb();
  t = sc_edge->tb();
  if (t * kr_derivatives[iFace->GetIndex(sc_oedge->GetaVertex())] +
          (1 - t) * kr_derivatives[iFace->GetIndex(sc_oedge->GetbVertex())] <
      _kr_derivative_epsilon) {
    sc_layer->removeSmoothEdge();
  }
}

// MATERIAL_BOUNDARY
////////////////////
void FEdgeXDetector::processMaterialBoundaryShape(WXShape *iWShape)
{
  if (!_computeViewIndependent) {
    return;
  }
  // Make a pass on the edges to detect material boundaries
  vector<WEdge *>::iterator we, weend;
  vector<WEdge *> &wedges = iWShape->getEdgeList();
  for (we = wedges.begin(), weend = wedges.end(); we != weend; ++we) {
    ProcessMaterialBoundaryEdge((WXEdge *)(*we));
  }
}

void FEdgeXDetector::ProcessMaterialBoundaryEdge(WXEdge *iEdge)
{
  // check whether the edge is a material boundary?
  WFace *aFace = iEdge->GetaFace();
  WFace *bFace = iEdge->GetbFace();
  if (aFace && bFace && aFace->frs_materialIndex() != bFace->frs_materialIndex()) {
    iEdge->AddNature(Nature::MATERIAL_BOUNDARY);
  }
}

// EDGE MARKS
/////////////
void FEdgeXDetector::processEdgeMarksShape(WXShape *iShape)
{
  // Make a pass on the edges to detect material boundaries
  vector<WEdge *>::iterator we, weend;
  vector<WEdge *> &wedges = iShape->getEdgeList();
  for (we = wedges.begin(), weend = wedges.end(); we != weend; ++we) {
    ProcessEdgeMarks((WXEdge *)(*we));
  }
}

void FEdgeXDetector::ProcessEdgeMarks(WXEdge *iEdge)
{
  if (iEdge->GetMark()) {
    iEdge->AddNature(Nature::EDGE_MARK);
  }
}

// Build Smooth edges
/////////////////////
void FEdgeXDetector::buildSmoothEdges(WXShape *iShape)
{
  bool hasSmoothEdges = false;

  // Make a last pass to build smooth edges from the previous stored values:
  //--------------------------------------------------------------------------
  vector<WFace *> &wfaces = iShape->GetFaceList();
  for (vector<WFace *>::iterator f = wfaces.begin(), fend = wfaces.end(); f != fend; ++f) {
    vector<WXFaceLayer *> &faceLayers = ((WXFace *)(*f))->getSmoothLayers();
    for (vector<WXFaceLayer *>::iterator wxfl = faceLayers.begin(), wxflend = faceLayers.end();
         wxfl != wxflend;
         ++wxfl) {
      if ((*wxfl)->BuildSmoothEdge()) {
        hasSmoothEdges = true;
      }
    }
  }

  if (hasSmoothEdges && !_computeRidgesAndValleys && !_computeSuggestiveContours) {
    vector<WVertex *> &wvertices = iShape->getVertexList();
    for (vector<WVertex *>::iterator wv = wvertices.begin(), wvend = wvertices.end(); wv != wvend;
         ++wv) {
      // Compute curvatures
      WXVertex *wxv = dynamic_cast<WXVertex *>(*wv);
      computeCurvatures(wxv);
    }
    _meanK1 /= (real)(_nPoints);
    _meanKr /= (real)(_nPoints);
  }
}

} /* namespace Freestyle */
