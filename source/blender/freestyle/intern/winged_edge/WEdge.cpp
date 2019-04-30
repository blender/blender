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
 * \brief Classes to define a Winged Edge data structure.
 */

#include <iostream>

#include "WEdge.h"

namespace Freestyle {

/*! Temporary structures */
class vertexdata {
 public:
  WVertex *_copy;
};

class oedgedata {
 public:
  WOEdge *_copy;
};

class edgedata {
 public:
  WEdge *_copy;
};

class facedata {
 public:
  WFace *_copy;
};

/**********************************
 *                                *
 *                                *
 *             WVertex            *
 *                                *
 *                                *
 **********************************/

WVertex::WVertex(WVertex &iBrother)
{
  _Id = iBrother._Id;
  _Vertex = iBrother._Vertex;
  _EdgeList = iBrother._EdgeList;

  _Shape = iBrother._Shape;
  _Smooth = iBrother._Smooth;
  _Border = iBrother._Border;
  userdata = NULL;
  iBrother.userdata = new vertexdata;
  ((vertexdata *)(iBrother.userdata))->_copy = this;
}

WVertex *WVertex::duplicate()
{
  WVertex *clone = new WVertex(*this);
  return clone;
}

WOEdge *WVertex::incoming_edge_iterator::operator*()
{
  return _current;
}

void WVertex::incoming_edge_iterator::increment()
{
  WOEdge *twin = _current->twin();
  if (!twin) {
    // we reached a hole
    _current = 0;
    return;
  }
  WOEdge *next = twin->getPrevOnFace();
  if (next == _begin) {
    next = NULL;
  }
  _current = next;
}

WFace *WVertex::face_iterator::operator*()
{
  WOEdge *woedge = *_edge_it;
  if (!woedge)
    return NULL;
  return (woedge)->GetbFace();
}

#if 0
bool WVertex::isBoundary() const
{
  return _Border;
}
#endif
bool WVertex::isBoundary()
{
  if (_Border == 1)
    return true;
  else if (_Border == 0)
    return false;

  vector<WEdge *>::const_iterator it;
  for (it = _EdgeList.begin(); it != _EdgeList.end(); it++) {
    if ((*it)->GetNumberOfOEdges() == 1) {
      _Border = 1;
      return true;
    }
  }
#if 0
  if (!(*it)->GetaOEdge()->GetaFace())
    return true;
#endif
  _Border = 0;
  return false;
}

void WVertex::AddEdge(WEdge *iEdge)
{
  _EdgeList.push_back(iEdge);
}

WVertex::incoming_edge_iterator WVertex::incoming_edges_begin()
{
  WOEdge *begin;
  WEdge *wedge = _EdgeList.front();
  WOEdge *aOEdge = wedge->GetaOEdge();
  if (aOEdge->GetbVertex() == this)
    begin = aOEdge;
  else
    begin = _EdgeList.front()->GetbOEdge();
  return incoming_edge_iterator(this, begin, begin);
}

WVertex::incoming_edge_iterator WVertex::incoming_edges_end()
{
  WOEdge *begin;
  WOEdge *aOEdge = _EdgeList.front()->GetaOEdge();
  if (aOEdge->GetbVertex() == this)
    begin = aOEdge;
  else
    begin = _EdgeList.front()->GetbOEdge();
  return incoming_edge_iterator(this, begin, 0);
}
#if 0
WOEdge **WVertex::incoming_edge_iterator::operator->()
{
  WOEdge **ppaOEdge = (*_iter)->GetaOEdge();
  if (aOEdge->GetbVertex() == _vertex) {
    return ppaOEdge;
  }
  else {
    WOEdge *bOEdge = (*_iter)->GetbOEdge();
    return &bOEdge;
  }
}
#endif

/**********************************
 *                                *
 *                                *
 *             WOEdge             *
 *                                *
 *                                *
 **********************************/

WOEdge::WOEdge(WOEdge &iBrother)
{
  _paVertex = iBrother.GetaVertex();
  _pbVertex = iBrother.GetbVertex();
  _paFace = iBrother.GetaFace();
  _pbFace = iBrother.GetbFace();
  _pOwner = iBrother.GetOwner();
  userdata = NULL;
  iBrother.userdata = new oedgedata;
  ((oedgedata *)(iBrother.userdata))->_copy = this;

  _vec = iBrother._vec;
  _angle = iBrother._angle;
}

WOEdge *WOEdge::duplicate()
{
  WOEdge *clone = new WOEdge(*this);
  return clone;
}

WOEdge *WOEdge::twin()
{
  return GetOwner()->GetOtherOEdge(this);
}

WOEdge *WOEdge::getPrevOnFace()
{
  return _pbFace->GetPrevOEdge(this);
}

/**********************************
 *                                *
 *                                *
 *             WEdge              *
 *                                *
 *                                *
 **********************************/

WEdge::WEdge(WEdge &iBrother)
{
  _paOEdge = NULL;
  _pbOEdge = NULL;
  WOEdge *aoedge = iBrother.GetaOEdge();
  WOEdge *boedge = iBrother.GetbOEdge();
  userdata = NULL;

  if (aoedge)
    //_paOEdge = new WOEdge(*aoedge);
    _paOEdge = aoedge->duplicate();
  if (boedge)
    //_pbOEdge = new WOEdge(*boedge);
    _pbOEdge = boedge->duplicate();

  _nOEdges = iBrother.GetNumberOfOEdges();
  _Id = iBrother.GetId();
  iBrother.userdata = new edgedata;
  ((edgedata *)(iBrother.userdata))->_copy = this;
}

WEdge *WEdge::duplicate()
{
  WEdge *clone = new WEdge(*this);
  return clone;
}

/**********************************
 *                                *
 *                                *
 *             WFace              *
 *                                *
 *                                *
 **********************************/

WFace::WFace(WFace &iBrother)
{
  _OEdgeList = iBrother.getEdgeList();
  _Normal = iBrother.GetNormal();
  _VerticesNormals = iBrother._VerticesNormals;
  _VerticesTexCoords = iBrother._VerticesTexCoords;
  _Id = iBrother.GetId();
  _FrsMaterialIndex = iBrother._FrsMaterialIndex;
  _Mark = iBrother._Mark;
  userdata = NULL;
  iBrother.userdata = new facedata;
  ((facedata *)(iBrother.userdata))->_copy = this;
}

WFace *WFace::duplicate()
{
  WFace *clone = new WFace(*this);
  return clone;
}

const FrsMaterial &WFace::frs_material()
{
  return getShape()->frs_material(_FrsMaterialIndex);
}

WOEdge *WFace::MakeEdge(WVertex *v1, WVertex *v2)
{
  // First check whether the same oriented edge already exists or not:
  vector<WEdge *> &v1Edges = v1->GetEdges();
  for (vector<WEdge *>::iterator it1 = v1Edges.begin(), end = v1Edges.end(); it1 != end; it1++) {
    WEdge *we = (*it1);
    WOEdge *woea = we->GetaOEdge();

    if ((woea->GetaVertex() == v1) && (woea->GetbVertex() == v2)) {
      // The oriented edge already exists
      cerr << "Warning: edge " << v1->GetId() << " - " << v2->GetId()
           << " appears twice, correcting" << endl;
      // Adds the edge to the face
      AddEdge(woea);
      (*it1)->setNumberOfOEdges((*it1)->GetNumberOfOEdges() + 1);
      // sets these vertices as border:
      v1->setBorder(true);
      v2->setBorder(true);
      return woea;
    }

    WOEdge *woeb = we->GetbOEdge();
    if (woeb && (woeb->GetaVertex() == v1) && (woeb->GetbVertex() == v2)) {
      // The oriented edge already exists
      cerr << "Warning: edge " << v1->GetId() << " - " << v2->GetId()
           << " appears twice, correcting" << endl;
      // Adds the edge to the face
      AddEdge(woeb);
      (*it1)->setNumberOfOEdges((*it1)->GetNumberOfOEdges() + 1);
      // sets these vertices as border:
      v1->setBorder(true);
      v2->setBorder(true);
      return woeb;
    }
  }

  // the oriented edge we're about to build
  WOEdge *pOEdge = new WOEdge;
  // The edge containing the oriented edge.
  WEdge *edge;

  // checks whether this edge already exists or not
  // If it exists, it points outward v2
  bool exist = false;
  WOEdge *pInvertEdge = NULL;  // The inverted edge if it exists
  vector<WEdge *> &v2Edges = v2->GetEdges();
  vector<WEdge *>::iterator it;
  for (it = v2Edges.begin(); it != v2Edges.end(); it++) {
    if ((*it)->GetbVertex() == v1) {
      // The invert edge already exists
      exist = true;
      pInvertEdge = (*it)->GetaOEdge();
      break;
    }
  }

  // DEBUG:
  if (true == exist) {  // The invert edge already exists
    // Retrieves the corresponding edge
    edge = pInvertEdge->GetOwner();

    // Sets the a Face (retrieved from pInvertEdge
    pOEdge->setaFace(pInvertEdge->GetbFace());

    // Updates the invert edge:
    pInvertEdge->setaFace(this);
  }
  else {  // The invert edge does not exist yet
    // we must create a new edge
    // edge = new WEdge;
    edge = instanciateEdge();

    // updates the a,b vertex edges list:
    v1->AddEdge(edge);
    v2->AddEdge(edge);
  }

  pOEdge->setOwner(edge);
  // Add the vertices:
  pOEdge->setaVertex(v1);
  pOEdge->setbVertex(v2);

  // Debug:
  if (v1->GetId() == v2->GetId())
    cerr << "Warning: edge " << this << " null with vertex " << v1->GetId() << endl;

  edge->AddOEdge(pOEdge);
  // edge->setNumberOfOEdges(edge->GetNumberOfOEdges() + 1);

  // Add this face (the b face)
  pOEdge->setbFace(this);

  // Adds the edge to the face
  AddEdge(pOEdge);

  return pOEdge;
}

bool WFace::getOppositeEdge(const WVertex *v, WOEdge *&e)
{
  if (_OEdgeList.size() != 3)
    return false;

  vector<WOEdge *>::iterator it;
  e = NULL;
  for (it = _OEdgeList.begin(); it != _OEdgeList.end(); it++) {
    if ((*it)->GetaVertex() == v)
      e = *it;
  }
  if (!e)
    return false;
  e = NULL;
  for (it = _OEdgeList.begin(); it != _OEdgeList.end(); it++) {
    if (((*it)->GetaVertex() != v) && ((*it)->GetbVertex() != v))
      e = *it;
  }
  if (!e)
    return false;
  else
    return true;
}

float WFace::getArea()
{
  vector<WOEdge *>::iterator it;
  Vec3f origin = (*(_OEdgeList.begin()))->GetaVertex()->GetVertex();
  it = _OEdgeList.begin();
  float a = 0;
  for (it = it++; it != _OEdgeList.end(); it++) {
    Vec3f v1 = Vec3f((*it)->GetaVertex()->GetVertex() - origin);
    Vec3f v2 = Vec3f((*it)->GetbVertex()->GetVertex() - origin);
    a += (v1 ^ v2).norm() / 2.0f;
  }
  return a;
}

WOEdge *WFace::GetPrevOEdge(WOEdge *iOEdge)
{
  vector<WOEdge *>::iterator woe, woend, woefirst;
  woefirst = _OEdgeList.begin();
  woend = _OEdgeList.end();
  WOEdge *prev = *woefirst;
  woe = woefirst;
  ++woe;
  for (; woe != woend; woe++) {
    if ((*woe) == iOEdge)
      return prev;
    prev = *woe;
  }
  // We left the loop. That means that the first OEdge was the good one:
  if ((*woefirst) == iOEdge)
    return prev;

  return NULL;
}

WShape *WFace::getShape()
{
  return GetVertex(0)->shape();
}

/**********************************
 *                                *
 *                                *
 *             WShape             *
 *                                *
 *                                *
 **********************************/

unsigned WShape::_SceneCurrentId = 0;

WShape *WShape::duplicate()
{
  WShape *clone = new WShape(*this);
  return clone;
}

WShape::WShape(WShape &iBrother)
{
  _Id = iBrother.GetId();
  _Name = iBrother._Name;
  _LibraryPath = iBrother._LibraryPath;
  _FrsMaterials = iBrother._FrsMaterials;
#if 0
  _meanEdgeSize = iBrother._meanEdgeSize;
  iBrother.bbox(_min, _max);
#endif
  vector<WVertex *> &vertexList = iBrother.getVertexList();
  vector<WVertex *>::iterator v = vertexList.begin(), vend = vertexList.end();
  for (; v != vend; ++v) {
    // WVertex *newVertex = new WVertex(*(*v));
    WVertex *newVertex = (*v)->duplicate();

    newVertex->setShape(this);
    AddVertex(newVertex);
  }

  vector<WEdge *> &edgeList = iBrother.getEdgeList();
  vector<WEdge *>::iterator e = edgeList.begin(), eend = edgeList.end();
  for (; e != eend; ++e) {
    // WEdge *newEdge = new WEdge(*(*e));
    WEdge *newEdge = (*e)->duplicate();
    AddEdge(newEdge);
  }

  vector<WFace *> &faceList = iBrother.GetFaceList();
  vector<WFace *>::iterator f = faceList.begin(), fend = faceList.end();
  for (; f != fend; ++f) {
    // WFace *newFace = new WFace(*(*f));
    WFace *newFace = (*f)->duplicate();
    AddFace(newFace);
  }

  // update all pointed addresses thanks to the newly created objects:
  vend = _VertexList.end();
  for (v = _VertexList.begin(); v != vend; ++v) {
    const vector<WEdge *> &vedgeList = (*v)->GetEdges();
    vector<WEdge *> newvedgelist;
    unsigned int i;
    for (i = 0; i < vedgeList.size(); i++) {
      WEdge *current = vedgeList[i];
      edgedata *currentvedata = (edgedata *)current->userdata;
      newvedgelist.push_back(currentvedata->_copy);
    }
    (*v)->setEdges(newvedgelist);
  }

  eend = _EdgeList.end();
  for (e = _EdgeList.begin(); e != eend; ++e) {
    // update aOedge:
    WOEdge *aoEdge = (*e)->GetaOEdge();
    aoEdge->setaVertex(((vertexdata *)(aoEdge->GetaVertex()->userdata))->_copy);
    aoEdge->setbVertex(((vertexdata *)(aoEdge->GetbVertex()->userdata))->_copy);
    if (aoEdge->GetaFace())
      aoEdge->setaFace(((facedata *)(aoEdge->GetaFace()->userdata))->_copy);
    aoEdge->setbFace(((facedata *)(aoEdge->GetbFace()->userdata))->_copy);
    aoEdge->setOwner(((edgedata *)(aoEdge->GetOwner()->userdata))->_copy);

    // update bOedge:
    WOEdge *boEdge = (*e)->GetbOEdge();
    if (boEdge) {
      boEdge->setaVertex(((vertexdata *)(boEdge->GetaVertex()->userdata))->_copy);
      boEdge->setbVertex(((vertexdata *)(boEdge->GetbVertex()->userdata))->_copy);
      if (boEdge->GetaFace())
        boEdge->setaFace(((facedata *)(boEdge->GetaFace()->userdata))->_copy);
      boEdge->setbFace(((facedata *)(boEdge->GetbFace()->userdata))->_copy);
      boEdge->setOwner(((edgedata *)(boEdge->GetOwner()->userdata))->_copy);
    }
  }

  fend = _FaceList.end();
  for (f = _FaceList.begin(); f != fend; ++f) {
    unsigned int i;
    const vector<WOEdge *> &oedgeList = (*f)->getEdgeList();
    vector<WOEdge *> newoedgelist;

    unsigned int n = oedgeList.size();
    for (i = 0; i < n; i++) {
      WOEdge *current = oedgeList[i];
      oedgedata *currentoedata = (oedgedata *)current->userdata;
      newoedgelist.push_back(currentoedata->_copy);
      // oedgeList[i] = currentoedata->_copy;
      // oedgeList[i] = ((oedgedata *)(oedgeList[i]->userdata))->_copy;
    }
    (*f)->setEdgeList(newoedgelist);
  }

  // Free all memory (arghh!)
  // Vertex
  vend = iBrother.getVertexList().end();
  for (v = iBrother.getVertexList().begin(); v != vend; ++v) {
    delete (vertexdata *)((*v)->userdata);
    (*v)->userdata = NULL;
  }

  // Edges and OEdges:
  eend = iBrother.getEdgeList().end();
  for (e = iBrother.getEdgeList().begin(); e != eend; ++e) {
    delete (edgedata *)((*e)->userdata);
    (*e)->userdata = NULL;
    // OEdge a:
    delete (oedgedata *)((*e)->GetaOEdge()->userdata);
    (*e)->GetaOEdge()->userdata = NULL;
    // OEdge b:
    WOEdge *oedgeb = (*e)->GetbOEdge();
    if (oedgeb) {
      delete (oedgedata *)(oedgeb->userdata);
      oedgeb->userdata = NULL;
    }
  }

  // Faces
  fend = iBrother.GetFaceList().end();
  for (f = iBrother.GetFaceList().begin(); f != fend; ++f) {
    delete (facedata *)((*f)->userdata);
    (*f)->userdata = NULL;
  }
}

WFace *WShape::MakeFace(vector<WVertex *> &iVertexList,
                        vector<bool> &iFaceEdgeMarksList,
                        unsigned iMaterial)
{
  // allocate the new face
  WFace *face = instanciateFace();

  WFace *result = MakeFace(iVertexList, iFaceEdgeMarksList, iMaterial, face);
  if (!result)
    delete face;
  return result;
}

WFace *WShape::MakeFace(vector<WVertex *> &iVertexList,
                        vector<Vec3f> &iNormalsList,
                        vector<Vec2f> &iTexCoordsList,
                        vector<bool> &iFaceEdgeMarksList,
                        unsigned iMaterial)
{
  // allocate the new face
  WFace *face = MakeFace(iVertexList, iFaceEdgeMarksList, iMaterial);

  if (!face)
    return NULL;

  // set the list of per-vertex normals
  face->setNormalList(iNormalsList);
  // set the list of per-vertex tex coords
  face->setTexCoordsList(iTexCoordsList);

  return face;
}

WFace *WShape::MakeFace(vector<WVertex *> &iVertexList,
                        vector<bool> &iFaceEdgeMarksList,
                        unsigned iMaterial,
                        WFace *face)
{
  int id = _FaceList.size();

  face->setFrsMaterialIndex(iMaterial);

  // Check whether we have a degenerated face:

  // LET'S HACK IT FOR THE TRIANGLE CASE:

  if (3 == iVertexList.size()) {
    if ((iVertexList[0] == iVertexList[1]) || (iVertexList[0] == iVertexList[2]) ||
        (iVertexList[2] == iVertexList[1])) {
      cerr << "Warning: degenerated triangle detected, correcting" << endl;
      return NULL;
    }
  }

  vector<WVertex *>::iterator it;

  // compute the face normal (v1v2 ^ v1v3)
  // Double precision numbers are used here to avoid truncation errors [T47705]
  Vec3r v1, v2, v3;
  it = iVertexList.begin();
  v1 = (*it)->GetVertex();
  it++;
  v2 = (*it)->GetVertex();
  it++;
  v3 = (*it)->GetVertex();

  Vec3r vector1(v2 - v1);
  Vec3r vector2(v3 - v1);

  Vec3r normal(vector1 ^ vector2);
  normal.normalize();
  face->setNormal(normal);

  vector<bool>::iterator mit = iFaceEdgeMarksList.begin();
  face->setMark(*mit);
  mit++;

  // vertex pointers used to build each edge
  vector<WVertex *>::iterator va, vb;

  va = iVertexList.begin();
  vb = va;
  for (; va != iVertexList.end(); va = vb) {
    ++vb;
    // Adds va to the vertex list:
    // face->AddVertex(*va);

    WOEdge *oedge;
    if (*va == iVertexList.back())
      oedge = face->MakeEdge(*va, iVertexList.front());  // for the last (closing) edge
    else
      oedge = face->MakeEdge(*va, *vb);

    if (!oedge)
      return NULL;

    WEdge *edge = oedge->GetOwner();
    if (1 == edge->GetNumberOfOEdges()) {
      // means that we just created a new edge and that we must add it to the shape's edges list
      edge->setId(_EdgeList.size());
      AddEdge(edge);
#if 0
      // compute the mean edge value:
      _meanEdgeSize += edge->GetaOEdge()->GetVec().norm();
#endif
    }

    edge->setMark(*mit);
    ++mit;
  }

  // Add the face to the shape's faces list:
  face->setId(id);
  AddFace(face);

  return face;
}

real WShape::ComputeMeanEdgeSize() const
{
  real meanEdgeSize = 0.0;
  for (vector<WEdge *>::const_iterator it = _EdgeList.begin(), itend = _EdgeList.end();
       it != itend;
       it++) {
    meanEdgeSize += (*it)->GetaOEdge()->GetVec().norm();
  }
  return meanEdgeSize / (real)_EdgeList.size();
}

} /* namespace Freestyle */
