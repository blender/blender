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
 * \brief Class to build view edges and the underlying chains of feature edges...
 */

#include <list>

#include "SilhouetteGeomEngine.h"
#include "ViewEdgeXBuilder.h"
#include "ViewMap.h"

#include "../winged_edge/WXEdge.h"

using namespace std;

namespace Freestyle {

void ViewEdgeXBuilder::Init(ViewShape *oVShape)
{
  if (NULL == oVShape) {
    return;
  }

  // for design conveniance, we store the current SShape.
  _pCurrentSShape = oVShape->sshape();
  if (0 == _pCurrentSShape) {
    return;
  }

  _pCurrentVShape = oVShape;

  // Reset previous data
  //--------------------
  if (!_SVertexMap.empty()) {
    _SVertexMap.clear();
  }
}

void ViewEdgeXBuilder::BuildViewEdges(WXShape *iWShape,
                                      ViewShape *oVShape,
                                      vector<ViewEdge *> &ioVEdges,
                                      vector<ViewVertex *> &ioVVertices,
                                      vector<FEdge *> &ioFEdges,
                                      vector<SVertex *> &ioSVertices)
{
  // Reinit structures
  Init(oVShape);

  /* ViewEdge *vedge; */ /* UNUSED */
  // Let us build the smooth stuff
  //----------------------------------------
  // We parse all faces to find the ones that contain smooth edges
  vector<WFace *> &wfaces = iWShape->GetFaceList();
  vector<WFace *>::iterator wf, wfend;
  WXFace *wxf;
  for (wf = wfaces.begin(), wfend = wfaces.end(); wf != wfend; wf++) {
    wxf = dynamic_cast<WXFace *>(*wf);
    if (false == ((wxf))->hasSmoothEdges()) {  // does it contain at least one smooth edge ?
      continue;
    }
    // parse all smooth layers:
    vector<WXFaceLayer *> &smoothLayers = wxf->getSmoothLayers();
    for (vector<WXFaceLayer *>::iterator sl = smoothLayers.begin(), slend = smoothLayers.end();
         sl != slend;
         ++sl) {
      if (!(*sl)->hasSmoothEdge()) {
        continue;
      }
      if (stopSmoothViewEdge((*sl))) {  // has it been parsed already ?
        continue;
      }
      // here we know that we're dealing with a face layer that has not been processed yet and that
      // contains a smooth edge.
      /* vedge =*//* UNUSED */ BuildSmoothViewEdge(OWXFaceLayer(*sl, true));
    }
  }

  // Now let's build sharp view edges:
  //----------------------------------
  // Reset all userdata for WXEdge structure
  //----------------------------------------
  // iWShape->ResetUserData();

  WXEdge *wxe;
  vector<WEdge *> &wedges = iWShape->getEdgeList();
  //------------------------------
  for (vector<WEdge *>::iterator we = wedges.begin(), weend = wedges.end(); we != weend; we++) {
    wxe = dynamic_cast<WXEdge *>(*we);
    if (Nature::NO_FEATURE == wxe->nature()) {
      continue;
    }

    if (!stopSharpViewEdge(wxe)) {
      bool b = true;
      if (wxe->order() == -1) {
        b = false;
      }
      BuildSharpViewEdge(OWXEdge(wxe, b));
    }
  }

  // Reset all userdata for WXEdge structure
  //----------------------------------------
  iWShape->ResetUserData();

  // Add all these new edges to the scene's feature edges list:
  //-----------------------------------------------------------
  vector<FEdge *> &newedges = _pCurrentSShape->getEdgeList();
  vector<SVertex *> &newVertices = _pCurrentSShape->getVertexList();
  vector<ViewVertex *> &newVVertices = _pCurrentVShape->vertices();
  vector<ViewEdge *> &newVEdges = _pCurrentVShape->edges();

  // inserts in ioFEdges, at its end, all the edges of newedges
  ioFEdges.insert(ioFEdges.end(), newedges.begin(), newedges.end());
  ioSVertices.insert(ioSVertices.end(), newVertices.begin(), newVertices.end());
  ioVVertices.insert(ioVVertices.end(), newVVertices.begin(), newVVertices.end());
  ioVEdges.insert(ioVEdges.end(), newVEdges.begin(), newVEdges.end());
}

ViewEdge *ViewEdgeXBuilder::BuildSmoothViewEdge(const OWXFaceLayer &iFaceLayer)
{
  // Find first edge:
  OWXFaceLayer first = iFaceLayer;
  OWXFaceLayer currentFace = first;

  // bidirectional chaining.
  // first direction
  list<OWXFaceLayer> facesChain;
  unsigned size = 0;
  while (!stopSmoothViewEdge(currentFace.fl)) {
    facesChain.push_back(currentFace);
    ++size;
    currentFace.fl->userdata = (void *)1;  // processed
    // Find the next edge!
    currentFace = FindNextFaceLayer(currentFace);
  }
  OWXFaceLayer end = facesChain.back();
  // second direction
  currentFace = FindPreviousFaceLayer(first);
  while (!stopSmoothViewEdge(currentFace.fl)) {
    facesChain.push_front(currentFace);
    ++size;
    currentFace.fl->userdata = (void *)1;  // processed
    // Find the previous edge!
    currentFace = FindPreviousFaceLayer(currentFace);
  }
  first = facesChain.front();

  if (iFaceLayer.fl->nature() & Nature::RIDGE) {
    if (size < 4) {
      return 0;
    }
  }

  // Start a new chain edges
  ViewEdge *newVEdge = new ViewEdge;
  newVEdge->setId(_currentViewId);
  ++_currentViewId;

  _pCurrentVShape->AddEdge(newVEdge);

  // build FEdges
  FEdge *feprevious = NULL;
  FEdge *fefirst = NULL;
  FEdge *fe = NULL;
  for (list<OWXFaceLayer>::iterator fl = facesChain.begin(), flend = facesChain.end(); fl != flend;
       ++fl) {
    fe = BuildSmoothFEdge(feprevious, (*fl));
    if (feprevious && fe == feprevious) {
      continue;
    }
    fe->setViewEdge(newVEdge);
    if (!fefirst) {
      fefirst = fe;
    }
    feprevious = fe;
  }
  // Store the chain starting edge:
  _pCurrentSShape->AddChain(fefirst);
  newVEdge->setNature(iFaceLayer.fl->nature());
  newVEdge->setFEdgeA(fefirst);
  newVEdge->setFEdgeB(fe);

  // is it a closed loop ?
  if ((first == end) && (size != 1)) {
    fefirst->setPreviousEdge(fe);
    fe->setNextEdge(fefirst);
    newVEdge->setA(0);
    newVEdge->setB(0);
  }
  else {
    ViewVertex *vva = MakeViewVertex(fefirst->vertexA());
    ViewVertex *vvb = MakeViewVertex(fe->vertexB());

    ((NonTVertex *)vva)->AddOutgoingViewEdge(newVEdge);
    ((NonTVertex *)vvb)->AddIncomingViewEdge(newVEdge);

    newVEdge->setA(vva);
    newVEdge->setB(vvb);
  }

  return newVEdge;
}

ViewEdge *ViewEdgeXBuilder::BuildSharpViewEdge(const OWXEdge &iWEdge)
{
  // Start a new sharp chain edges
  ViewEdge *newVEdge = new ViewEdge;
  newVEdge->setId(_currentViewId);
  ++_currentViewId;
  unsigned size = 0;

  _pCurrentVShape->AddEdge(newVEdge);

  // Find first edge:
  OWXEdge firstWEdge = iWEdge;
  /* OWXEdge previousWEdge = firstWEdge; */ /* UNUSED */
  OWXEdge currentWEdge = firstWEdge;
  list<OWXEdge> edgesChain;
#if 0 /* TK 02-Sep-2012 Experimental fix for incorrect view edge visibility. */
  // bidirectional chaining
  // first direction:
  while (!stopSharpViewEdge(currentWEdge.e)) {
    edgesChain.push_back(currentWEdge);
    ++size;
    currentWEdge.e->userdata = (void *)1;  // processed
    // Find the next edge!
    currentWEdge = FindNextWEdge(currentWEdge);
  }
  OWXEdge endWEdge = edgesChain.back();
  // second direction
  currentWEdge = FindPreviousWEdge(firstWEdge);
  while (!stopSharpViewEdge(currentWEdge.e)) {
    edgesChain.push_front(currentWEdge);
    ++size;
    currentWEdge.e->userdata = (void *)1;  // processed
    // Find the previous edge!
    currentWEdge = FindPreviousWEdge(currentWEdge);
  }
#else
  edgesChain.push_back(currentWEdge);
  ++size;
  currentWEdge.e->userdata = (void *)1;  // processed
  OWXEdge endWEdge = edgesChain.back();
#endif
  firstWEdge = edgesChain.front();

  // build FEdges
  FEdge *feprevious = NULL;
  FEdge *fefirst = NULL;
  FEdge *fe = NULL;
  for (list<OWXEdge>::iterator we = edgesChain.begin(), weend = edgesChain.end(); we != weend;
       ++we) {
    fe = BuildSharpFEdge(feprevious, (*we));
    fe->setViewEdge(newVEdge);
    if (!fefirst) {
      fefirst = fe;
    }
    feprevious = fe;
  }
  // Store the chain starting edge:
  _pCurrentSShape->AddChain(fefirst);
  newVEdge->setNature(iWEdge.e->nature());
  newVEdge->setFEdgeA(fefirst);
  newVEdge->setFEdgeB(fe);

  // is it a closed loop ?
  if ((firstWEdge == endWEdge) && (size != 1)) {
    fefirst->setPreviousEdge(fe);
    fe->setNextEdge(fefirst);
    newVEdge->setA(0);
    newVEdge->setB(0);
  }
  else {
    ViewVertex *vva = MakeViewVertex(fefirst->vertexA());
    ViewVertex *vvb = MakeViewVertex(fe->vertexB());

    ((NonTVertex *)vva)->AddOutgoingViewEdge(newVEdge);
    ((NonTVertex *)vvb)->AddIncomingViewEdge(newVEdge);

    newVEdge->setA(vva);
    newVEdge->setB(vvb);
  }

  return newVEdge;
}

OWXFaceLayer ViewEdgeXBuilder::FindNextFaceLayer(const OWXFaceLayer &iFaceLayer)
{
  WXFace *nextFace = NULL;
  WOEdge *woeend;
  real tend;
  if (iFaceLayer.order) {
    woeend = iFaceLayer.fl->getSmoothEdge()->woeb();
    tend = iFaceLayer.fl->getSmoothEdge()->tb();
  }
  else {
    woeend = iFaceLayer.fl->getSmoothEdge()->woea();
    tend = iFaceLayer.fl->getSmoothEdge()->ta();
  }
  // special case of EDGE_VERTEX config:
  if ((tend == 0.0) || (tend == 1.0)) {
    WVertex *nextVertex;
    if (tend == 0.0) {
      nextVertex = woeend->GetaVertex();
    }
    else {
      nextVertex = woeend->GetbVertex();
    }
    if (nextVertex->isBoundary()) {  // if it's a non-manifold vertex -> ignore
      return OWXFaceLayer(0, true);
    }
    bool found = false;
    WVertex::face_iterator f = nextVertex->faces_begin();
    WVertex::face_iterator fend = nextVertex->faces_end();
    while ((!found) && (f != fend)) {
      nextFace = dynamic_cast<WXFace *>(*f);
      if ((0 != nextFace) && (nextFace != iFaceLayer.fl->getFace())) {
        vector<WXFaceLayer *> sameNatureLayers;
        nextFace->retrieveSmoothEdgesLayers(iFaceLayer.fl->nature(), sameNatureLayers);
        // don't know... Maybe should test whether this face has also a vertex_edge configuration.
        if (sameNatureLayers.size() == 1) {
          WXFaceLayer *winner = sameNatureLayers[0];
          // check face mark continuity
          if (winner->getFace()->GetMark() != iFaceLayer.fl->getFace()->GetMark()) {
            return OWXFaceLayer(NULL, true);
          }
          if (woeend == winner->getSmoothEdge()->woea()->twin()) {
            return OWXFaceLayer(winner, true);
          }
          else {
            return OWXFaceLayer(winner, false);
          }
        }
      }
      ++f;
    }
  }
  else {
    nextFace = dynamic_cast<WXFace *>(iFaceLayer.fl->getFace()->GetBordingFace(woeend));
    if (!nextFace) {
      return OWXFaceLayer(NULL, true);
    }
    // if the next face layer has either no smooth edge or no smooth edge of same nature, no next
    // face
    if (!nextFace->hasSmoothEdges()) {
      return OWXFaceLayer(NULL, true);
    }
    vector<WXFaceLayer *> sameNatureLayers;
    nextFace->retrieveSmoothEdgesLayers(iFaceLayer.fl->nature(), sameNatureLayers);
    // don't know how to deal with several edges of same nature on a single face
    if ((sameNatureLayers.empty()) || (sameNatureLayers.size() != 1)) {
      return OWXFaceLayer(NULL, true);
    }
    else {
      WXFaceLayer *winner = sameNatureLayers[0];
      // check face mark continuity
      if (winner->getFace()->GetMark() != iFaceLayer.fl->getFace()->GetMark()) {
        return OWXFaceLayer(NULL, true);
      }
      if (woeend == winner->getSmoothEdge()->woea()->twin()) {
        return OWXFaceLayer(winner, true);
      }
      else {
        return OWXFaceLayer(winner, false);
      }
    }
  }
  return OWXFaceLayer(NULL, true);
}

OWXFaceLayer ViewEdgeXBuilder::FindPreviousFaceLayer(const OWXFaceLayer &iFaceLayer)
{
  WXFace *previousFace = NULL;
  WOEdge *woebegin;
  real tend;
  if (iFaceLayer.order) {
    woebegin = iFaceLayer.fl->getSmoothEdge()->woea();
    tend = iFaceLayer.fl->getSmoothEdge()->ta();
  }
  else {
    woebegin = iFaceLayer.fl->getSmoothEdge()->woeb();
    tend = iFaceLayer.fl->getSmoothEdge()->tb();
  }

  // special case of EDGE_VERTEX config:
  if ((tend == 0.0) || (tend == 1.0)) {
    WVertex *previousVertex;
    if (tend == 0.0) {
      previousVertex = woebegin->GetaVertex();
    }
    else {
      previousVertex = woebegin->GetbVertex();
    }
    if (previousVertex->isBoundary()) {  // if it's a non-manifold vertex -> ignore
      return OWXFaceLayer(NULL, true);
    }
    bool found = false;
    WVertex::face_iterator f = previousVertex->faces_begin();
    WVertex::face_iterator fend = previousVertex->faces_end();
    for (; (!found) && (f != fend); ++f) {
      previousFace = dynamic_cast<WXFace *>(*f);
      if ((0 != previousFace) && (previousFace != iFaceLayer.fl->getFace())) {
        vector<WXFaceLayer *> sameNatureLayers;
        previousFace->retrieveSmoothEdgesLayers(iFaceLayer.fl->nature(), sameNatureLayers);
        // don't know... Maybe should test whether this face has also a vertex_edge configuration
        if (sameNatureLayers.size() == 1) {
          WXFaceLayer *winner = sameNatureLayers[0];
          // check face mark continuity
          if (winner->getFace()->GetMark() != iFaceLayer.fl->getFace()->GetMark()) {
            return OWXFaceLayer(NULL, true);
          }
          if (woebegin == winner->getSmoothEdge()->woeb()->twin()) {
            return OWXFaceLayer(winner, true);
          }
          else {
            return OWXFaceLayer(winner, false);
          }
        }
      }
    }
  }
  else {
    previousFace = dynamic_cast<WXFace *>(iFaceLayer.fl->getFace()->GetBordingFace(woebegin));
    if (0 == previousFace) {
      return OWXFaceLayer(NULL, true);
    }
    // if the next face layer has either no smooth edge or no smooth edge of same nature, no next
    // face
    if (!previousFace->hasSmoothEdges()) {
      return OWXFaceLayer(NULL, true);
    }
    vector<WXFaceLayer *> sameNatureLayers;
    previousFace->retrieveSmoothEdgesLayers(iFaceLayer.fl->nature(), sameNatureLayers);
    // don't know how to deal with several edges of same nature on a single face
    if ((sameNatureLayers.empty()) || (sameNatureLayers.size() != 1)) {
      return OWXFaceLayer(NULL, true);
    }
    else {
      WXFaceLayer *winner = sameNatureLayers[0];
      // check face mark continuity
      if (winner->getFace()->GetMark() != iFaceLayer.fl->getFace()->GetMark()) {
        return OWXFaceLayer(NULL, true);
      }
      if (woebegin == winner->getSmoothEdge()->woeb()->twin()) {
        return OWXFaceLayer(winner, true);
      }
      else {
        return OWXFaceLayer(winner, false);
      }
    }
  }
  return OWXFaceLayer(NULL, true);
}

FEdge *ViewEdgeXBuilder::BuildSmoothFEdge(FEdge *feprevious, const OWXFaceLayer &ifl)
{
  WOEdge *woea, *woeb;
  real ta, tb;
  SVertex *va, *vb;
  FEdgeSmooth *fe;
  // retrieve exact silhouette data
  WXSmoothEdge *se = ifl.fl->getSmoothEdge();

  if (ifl.order) {
    woea = se->woea();
    woeb = se->woeb();
    ta = se->ta();
    tb = se->tb();
  }
  else {
    woea = se->woeb();
    woeb = se->woea();
    ta = se->tb();
    tb = se->ta();
  }

  Vec3r normal;
  // Make the 2 Svertices
  if (feprevious == 0) {  // that means that we don't have any vertex already built for that face
    Vec3r A1(woea->GetaVertex()->GetVertex());
    Vec3r A2(woea->GetbVertex()->GetVertex());
    Vec3r A(A1 + ta * (A2 - A1));

    va = MakeSVertex(A, false);
    // Set normal:
    Vec3r NA1(ifl.fl->getFace()->GetVertexNormal(woea->GetaVertex()));
    Vec3r NA2(ifl.fl->getFace()->GetVertexNormal(woea->GetbVertex()));
    Vec3r na((1 - ta) * NA1 + ta * NA2);
    na.normalize();
    va->AddNormal(na);
    normal = na;

    // Set CurvatureInfo
    CurvatureInfo *curvature_info_a = new CurvatureInfo(
        *(dynamic_cast<WXVertex *>(woea->GetaVertex())->curvatures()),
        *(dynamic_cast<WXVertex *>(woea->GetbVertex())->curvatures()),
        ta);
    va->setCurvatureInfo(curvature_info_a);
  }
  else {
    va = feprevious->vertexB();
  }

  Vec3r B1(woeb->GetaVertex()->GetVertex());
  Vec3r B2(woeb->GetbVertex()->GetVertex());
  Vec3r B(B1 + tb * (B2 - B1));

  if (feprevious && (B - va->point3D()).norm() < 1.0e-6) {
    return feprevious;
  }

  vb = MakeSVertex(B, false);
  // Set normal:
  Vec3r NB1(ifl.fl->getFace()->GetVertexNormal(woeb->GetaVertex()));
  Vec3r NB2(ifl.fl->getFace()->GetVertexNormal(woeb->GetbVertex()));
  Vec3r nb((1 - tb) * NB1 + tb * NB2);
  nb.normalize();
  normal += nb;
  vb->AddNormal(nb);

  // Set CurvatureInfo
  CurvatureInfo *curvature_info_b = new CurvatureInfo(
      *(dynamic_cast<WXVertex *>(woeb->GetaVertex())->curvatures()),
      *(dynamic_cast<WXVertex *>(woeb->GetbVertex())->curvatures()),
      tb);
  vb->setCurvatureInfo(curvature_info_b);

  // Creates the corresponding feature edge
  fe = new FEdgeSmooth(va, vb);
  fe->setNature(ifl.fl->nature());
  fe->setId(_currentFId);
  fe->setFrsMaterialIndex(ifl.fl->getFace()->frs_materialIndex());
  fe->setFace(ifl.fl->getFace());
  fe->setFaceMark(ifl.fl->getFace()->GetMark());
  if (feprevious == 0) {
    normal.normalize();
  }
  fe->setNormal(normal);
  fe->setPreviousEdge(feprevious);
  if (feprevious) {
    feprevious->setNextEdge(fe);
  }
  _pCurrentSShape->AddEdge(fe);
  va->AddFEdge(fe);
  vb->AddFEdge(fe);

  ++_currentFId;
  ifl.fl->userdata = fe;
  return fe;
}

bool ViewEdgeXBuilder::stopSmoothViewEdge(WXFaceLayer *iFaceLayer)
{
  if (NULL == iFaceLayer) {
    return true;
  }
  if (iFaceLayer->userdata == 0) {
    return false;
  }
  return true;
}

int ViewEdgeXBuilder::retrieveFaceMarks(WXEdge *iEdge)
{
  WFace *aFace = iEdge->GetaFace();
  WFace *bFace = iEdge->GetbFace();
  int result = 0;
  if (aFace && aFace->GetMark()) {
    result += 1;
  }
  if (bFace && bFace->GetMark()) {
    result += 2;
  }
  return result;
}

OWXEdge ViewEdgeXBuilder::FindNextWEdge(const OWXEdge &iEdge)
{
  if (Nature::NO_FEATURE == iEdge.e->nature()) {
    return OWXEdge(NULL, true);
  }

  WVertex *v;
  if (true == iEdge.order) {
    v = iEdge.e->GetbVertex();
  }
  else {
    v = iEdge.e->GetaVertex();
  }

  if (((WXVertex *)v)->isFeature()) {
    return 0; /* XXX eeek? NULL? OWXEdge(NULL, true/false)?*/
  }

  int faceMarks = retrieveFaceMarks(iEdge.e);
  vector<WEdge *> &vEdges = (v)->GetEdges();
  for (vector<WEdge *>::iterator ve = vEdges.begin(), veend = vEdges.end(); ve != veend; ve++) {
    WXEdge *wxe = dynamic_cast<WXEdge *>(*ve);
    if (wxe == iEdge.e) {
      continue;  // same edge as the one processed
    }

    if (wxe->nature() != iEdge.e->nature()) {
      continue;
    }

    // check face mark continuity
    if (retrieveFaceMarks(wxe) != faceMarks) {
      continue;
    }

    if (wxe->GetaVertex() == v) {
      // That means that the face necesarily lies on the edge left.
      // So the vertex order is OK.
      return OWXEdge(wxe, true);
    }
    else {
      // That means that the face necesarily lies on the edge left.
      // So the vertex order is OK.
      return OWXEdge(wxe, false);
    }
  }
  // we did not find:
  return OWXEdge(NULL, true);
}

OWXEdge ViewEdgeXBuilder::FindPreviousWEdge(const OWXEdge &iEdge)
{
  if (Nature::NO_FEATURE == iEdge.e->nature()) {
    return OWXEdge(NULL, true);
  }

  WVertex *v;
  if (true == iEdge.order) {
    v = iEdge.e->GetaVertex();
  }
  else {
    v = iEdge.e->GetbVertex();
  }

  if (((WXVertex *)v)->isFeature()) {
    return 0;
  }

  int faceMarks = retrieveFaceMarks(iEdge.e);
  vector<WEdge *> &vEdges = (v)->GetEdges();
  for (vector<WEdge *>::iterator ve = vEdges.begin(), veend = vEdges.end(); ve != veend; ve++) {
    WXEdge *wxe = dynamic_cast<WXEdge *>(*ve);
    if (wxe == iEdge.e) {
      continue;  // same edge as the one processed
    }

    if (wxe->nature() != iEdge.e->nature()) {
      continue;
    }

    // check face mark continuity
    if (retrieveFaceMarks(wxe) != faceMarks) {
      continue;
    }

    if (wxe->GetbVertex() == v) {
      return OWXEdge(wxe, true);
    }
    else {
      return OWXEdge(wxe, false);
    }
  }
  // we did not find:
  return OWXEdge(NULL, true);
}

FEdge *ViewEdgeXBuilder::BuildSharpFEdge(FEdge *feprevious, const OWXEdge &iwe)
{
  SVertex *va, *vb;
  FEdgeSharp *fe;
  Vec3r vA, vB;
  if (iwe.order) {
    vA = iwe.e->GetaVertex()->GetVertex();
    vB = iwe.e->GetbVertex()->GetVertex();
  }
  else {
    vA = iwe.e->GetbVertex()->GetVertex();
    vB = iwe.e->GetaVertex()->GetVertex();
  }
  // Make the 2 SVertex
  va = MakeSVertex(vA, true);
  vb = MakeSVertex(vB, true);

  // get the faces normals and the material indices
  Vec3r normalA, normalB;
  unsigned matA(0), matB(0);
  bool faceMarkA = false, faceMarkB = false;
  if (iwe.order) {
    normalB = (iwe.e->GetbFace()->GetNormal());
    matB = (iwe.e->GetbFace()->frs_materialIndex());
    faceMarkB = (iwe.e->GetbFace()->GetMark());
    if (!(iwe.e->nature() & Nature::BORDER)) {
      normalA = (iwe.e->GetaFace()->GetNormal());
      matA = (iwe.e->GetaFace()->frs_materialIndex());
      faceMarkA = (iwe.e->GetaFace()->GetMark());
    }
  }
  else {
    normalA = (iwe.e->GetbFace()->GetNormal());
    matA = (iwe.e->GetbFace()->frs_materialIndex());
    faceMarkA = (iwe.e->GetbFace()->GetMark());
    if (!(iwe.e->nature() & Nature::BORDER)) {
      normalB = (iwe.e->GetaFace()->GetNormal());
      matB = (iwe.e->GetaFace()->frs_materialIndex());
      faceMarkB = (iwe.e->GetaFace()->GetMark());
    }
  }
  // Creates the corresponding feature edge
  fe = new FEdgeSharp(va, vb);
  fe->setNature(iwe.e->nature());
  fe->setId(_currentFId);
  fe->setaFrsMaterialIndex(matA);
  fe->setbFrsMaterialIndex(matB);
  fe->setaFaceMark(faceMarkA);
  fe->setbFaceMark(faceMarkB);
  fe->setNormalA(normalA);
  fe->setNormalB(normalB);
  fe->setPreviousEdge(feprevious);
  if (feprevious) {
    feprevious->setNextEdge(fe);
  }
  _pCurrentSShape->AddEdge(fe);
  va->AddFEdge(fe);
  vb->AddFEdge(fe);
  // Add normals:
  va->AddNormal(normalA);
  va->AddNormal(normalB);
  vb->AddNormal(normalA);
  vb->AddNormal(normalB);

  ++_currentFId;
  iwe.e->userdata = fe;
  return fe;
}

bool ViewEdgeXBuilder::stopSharpViewEdge(WXEdge *iEdge)
{
  if (NULL == iEdge) {
    return true;
  }
  if (iEdge->userdata == 0) {
    return false;
  }
  return true;
}

SVertex *ViewEdgeXBuilder::MakeSVertex(Vec3r &iPoint)
{
  SVertex *va = new SVertex(iPoint, _currentSVertexId);
  SilhouetteGeomEngine::ProjectSilhouette(va);
  ++_currentSVertexId;
  // Add the svertex to the SShape svertex list:
  _pCurrentSShape->AddNewVertex(va);
  return va;
}

SVertex *ViewEdgeXBuilder::MakeSVertex(Vec3r &iPoint, bool shared)
{
  SVertex *va;
  if (!shared) {
    va = MakeSVertex(iPoint);
  }
  else {
    // Check whether the iPoint is already in the table
    SVertexMap::const_iterator found = _SVertexMap.find(iPoint);
    if (shared && found != _SVertexMap.end()) {
      va = (*found).second;
    }
    else {
      va = MakeSVertex(iPoint);
      // Add the svertex into the table using iPoint as the key
      _SVertexMap[iPoint] = va;
    }
  }
  return va;
}

ViewVertex *ViewEdgeXBuilder::MakeViewVertex(SVertex *iSVertex)
{
  ViewVertex *vva = iSVertex->viewvertex();
  if (vva) {
    return vva;
  }
  vva = new NonTVertex(iSVertex);
  // Add the view vertex to the ViewShape svertex list:
  _pCurrentVShape->AddVertex(vva);
  return vva;
}

} /* namespace Freestyle */
