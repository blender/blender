/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/freestyle/intern/view_map/ViewMap.cpp
 *  \ingroup freestyle
 *  \brief Classes to define a View Map (ViewVertex, ViewEdge, etc.)
 *  \author Stephane Grabli
 *  \date 03/09/2002
 */

#include <float.h>

#include "ViewMap.h"
#include "ViewMapAdvancedIterators.h"
#include "ViewMapIterators.h"

#include "../geometry/GeomUtils.h"

namespace Freestyle {

/**********************************/
/*                                */
/*                                */
/*             ViewMap            */
/*                                */
/*                                */
/**********************************/

ViewMap *ViewMap::_pInstance = NULL;

ViewMap::~ViewMap()
{
	// The view vertices must be deleted here as some of them are shared between two shapes:
	for (vector<ViewVertex*>::iterator vv = _VVertices.begin(), vvend = _VVertices.end(); vv != vvend; vv++) {
		delete (*vv);
	}
	_VVertices.clear();

	for (vector<ViewShape*>::iterator vs = _VShapes.begin(), vsend = _VShapes.end(); vs != vsend; vs++) {
		delete (*vs);
	}
	_VShapes.clear();

	_FEdges.clear();
	_SVertices.clear();
	_VEdges.clear();
}

ViewShape *ViewMap::viewShape(unsigned id)
{
	int index = _shapeIdToIndex[id];
	return _VShapes[ index ];
}

void ViewMap::AddViewShape(ViewShape *iVShape)
{
	_shapeIdToIndex[iVShape->getId().getFirst()] = _VShapes.size(); 
	_VShapes.push_back(iVShape);
}

const FEdge *ViewMap::getClosestFEdge(real x, real y) const
{
	// find the closest of this candidates:
	real minDist = DBL_MAX;
	FEdge *winner = NULL;
	for (fedges_container::const_iterator fe = _FEdges.begin(), feend = _FEdges.end(); fe != feend; fe++) {
		Vec2d A((*fe)->vertexA()->point2D()[0], (*fe)->vertexA()->point2D()[1]);
		Vec2d B((*fe)->vertexB()->point2D()[0], (*fe)->vertexB()->point2D()[1]);
		real dist = GeomUtils::distPointSegment<Vec2r>(Vec2r(x, y), A, B);
		if (dist < minDist) {
			minDist = dist;
			winner = (*fe);
		}
	}

	return winner;
}

const ViewEdge *ViewMap::getClosestViewEdge(real x, real y) const
{
	// find the closest of this candidates:
	real minDist = DBL_MAX;
	FEdge *winner = NULL;
	for (fedges_container::const_iterator fe = _FEdges.begin(), feend = _FEdges.end(); fe != feend; fe++) {
		Vec2d A((*fe)->vertexA()->point2D()[0], (*fe)->vertexA()->point2D()[1]);
		Vec2d B((*fe)->vertexB()->point2D()[0], (*fe)->vertexB()->point2D()[1]);
		real dist = GeomUtils::distPointSegment<Vec2r>(Vec2r(x, y), A, B);
		if (dist < minDist) {
			minDist = dist;
			winner = (*fe);
		}
	}
	if (!winner)
		return NULL;

	return winner->viewedge();
}


TVertex *ViewMap::CreateTVertex(const Vec3r& iA3D, const Vec3r& iA2D, FEdge *iFEdgeA,
                                const Vec3r& iB3D, const Vec3r& iB2D, FEdge *iFEdgeB, const Id& id)
{
	ViewShape *vshapeA = iFEdgeA->viewedge()->viewShape();
	SShape *shapeA = iFEdgeA->shape();
	ViewShape *vshapeB = iFEdgeB->viewedge()->viewShape();
	SShape *shapeB = iFEdgeB->shape();

	SVertex *Ia = shapeA->CreateSVertex(iA3D, iA2D, iFEdgeA->vertexA()->getId());
	SVertex *Ib = shapeB->CreateSVertex(iB3D, iB2D, iFEdgeB->vertexA()->getId());

	// depending on which of these 2 svertices is the nearest from the viewpoint, we're going to build the TVertex
	// by giving them in an order or another (the first one must be the nearest)
	real dista = Ia->point2D()[2];
	real distb = Ib->point2D()[2];

	TVertex *tvertex;
	if (dista < distb)
		tvertex = new TVertex(Ia, Ib);
	else
		tvertex = new TVertex(Ib, Ia);

	tvertex->setId(id);

	// add these vertices to the view map
	AddViewVertex(tvertex);
	AddSVertex(Ia);
	AddSVertex(Ib);

	// and this T Vertex to the view shapes:
	vshapeA->AddVertex(tvertex);
	vshapeB->AddVertex(tvertex);

	return tvertex;
}

ViewVertex *ViewMap::InsertViewVertex(SVertex *iVertex, vector<ViewEdge*>& newViewEdges)
{
	NonTVertex *vva = dynamic_cast<NonTVertex*>(iVertex->viewvertex());
	if (vva)
		return vva;
	// because it is not already a ViewVertex, this SVertex must have only 2 FEdges. The incoming one still belongs
	// to ioEdge, the outgoing one now belongs to newVEdge
	const vector<FEdge *>& fedges = iVertex->fedges();
	if (fedges.size() != 2) {
		cerr << "ViewMap warning: Can't split the ViewEdge" << endl;
		return NULL;
	}
	FEdge *fend(NULL), *fbegin(NULL);
	for (vector<FEdge *>::const_iterator fe = fedges.begin(), feend = fedges.end(); fe != feend; ++fe) {
		if ((*fe)->vertexB() == iVertex) {
			fend = (*fe);
		}
		if ((*fe)->vertexA() == iVertex) {
			fbegin = (*fe);
		}
		if ((fbegin != NULL) && (fend != NULL))
			break;
	}
	ViewEdge *ioEdge = fbegin->viewedge();
	ViewShape *vshape = ioEdge->viewShape();
	vva = new NonTVertex(iVertex);
	// if the ViewEdge is a closed loop, we don't create a new VEdge
	if (ioEdge->A() == 0) {
		// closed loop
		ioEdge->setA(vva);
		ioEdge->setB(vva);
		// update sshape
		vshape->sshape()->RemoveEdgeFromChain(ioEdge->fedgeA());
		vshape->sshape()->RemoveEdgeFromChain(ioEdge->fedgeB());

		ioEdge->setFEdgeA(fbegin);
		ioEdge->setFEdgeB(fend);

		// Update FEdges
		fend->setNextEdge(NULL);
		fbegin->setPreviousEdge(NULL);

		// update new View Vertex:
		vva->AddOutgoingViewEdge(ioEdge);
		vva->AddIncomingViewEdge(ioEdge);

		vshape->sshape()->AddChain(ioEdge->fedgeA());
		vshape->sshape()->AddChain(ioEdge->fedgeB());
	}
	else {
		// Create new ViewEdge
		ViewEdge *newVEdge = new ViewEdge(vva, ioEdge->B(), fbegin, ioEdge->fedgeB(), vshape);
		newVEdge->setId(Id(ioEdge->getId().getFirst(), ioEdge->getId().getSecond() + 1));
		newVEdge->setNature(ioEdge->getNature());
		//newVEdge->UpdateFEdges(); // done in the ViewEdge constructor
		// Update old ViewEdge
		ioEdge->setB(vva);
		ioEdge->setFEdgeB(fend);

		// Update FEdges
		fend->setNextEdge(NULL);
		fbegin->setPreviousEdge(NULL);

		// update new View Vertex:
		vva->AddOutgoingViewEdge(newVEdge);
		vva->AddIncomingViewEdge(ioEdge);

		NonTVertex *vvb = dynamic_cast<NonTVertex*>(newVEdge->B());
		if (vvb)
			vvb->Replace(ioEdge, newVEdge);

		// update ViewShape
		//vshape->AddEdge(newVEdge);
		// update SShape
		vshape->sshape()->AddChain(fbegin);
		// update ViewMap
		//_VEdges.push_back(newVEdge);
		newViewEdges.push_back(newVEdge);
	}

	// update ViewShape
	vshape->AddVertex(vva);

	// update ViewMap
	_VVertices.push_back(vva);

	return vva;
}

#if 0
FEdge *ViewMap::Connect(FEdge *ioEdge, SVertex *ioVertex, vector<ViewEdge*>& oNewVEdges)
{
	SShape *sshape = ioEdge->shape();
	FEdge *newFEdge = sshape->SplitEdgeIn2(ioEdge, ioVertex);
	AddFEdge(newFEdge);
	InsertViewVertex(ioVertex, oNewVEdges);
	return newFEdge;
}
#endif

/**********************************/
/*                                */
/*                                */
/*             TVertex            */
/*                                */
/*                                */
/**********************************/

// is dve1 before dve2 ? (does it have a smaller angle ?)
static bool ViewEdgeComp(ViewVertex::directedViewEdge& dve1, ViewVertex::directedViewEdge& dve2)
{
	FEdge *fe1;
	if (dve1.second)
		fe1 = dve1.first->fedgeB();
	else
		fe1 = dve1.first->fedgeA();
	FEdge *fe2;
	if (dve2.second)
		fe2 = dve2.first->fedgeB();
	else
		fe2 = dve2.first->fedgeA();

	Vec3r V1 = fe1->orientation2d();
	Vec2r v1(V1.x(), V1.y());
	v1.normalize();
	Vec3r V2 = fe2->orientation2d();
	Vec2r v2(V2.x(), V2.y());
	v2.normalize();
	if (v1.y() > 0) {
		if (v2.y() < 0)
			return true;
		else 
			return (v1.x() > v2.x());
	}
	else {
		if (v2.y() > 0)
			return false;
		else 
			return (v1.x() < v2.x());
	}
	return false;
}

void TVertex::setFrontEdgeA(ViewEdge *iFrontEdgeA, bool incoming)
{
	if (!iFrontEdgeA) {
		cerr << "Warning: null pointer passed as argument of TVertex::setFrontEdgeA()" << endl;
		return;
	}
	_FrontEdgeA = directedViewEdge(iFrontEdgeA, incoming);
	if (!_sortedEdges.empty()) {
		edge_pointers_container::iterator dve = _sortedEdges.begin(), dveend = _sortedEdges.end();
		for (; (dve != dveend) && ViewEdgeComp(**dve, _FrontEdgeA); ++dve);
		_sortedEdges.insert( dve, &_FrontEdgeA);
	}
	else {
		_sortedEdges.push_back(&_FrontEdgeA);
	}
}

void TVertex::setFrontEdgeB(ViewEdge *iFrontEdgeB, bool incoming)
{
	if (!iFrontEdgeB) {
		cerr << "Warning: null pointer passed as argument of TVertex::setFrontEdgeB()" << endl;
		return;
	}
	_FrontEdgeB = directedViewEdge(iFrontEdgeB, incoming);
	if (!_sortedEdges.empty()) {
		edge_pointers_container::iterator dve = _sortedEdges.begin(), dveend = _sortedEdges.end();
		for (; (dve != dveend) && ViewEdgeComp(**dve, _FrontEdgeB); ++dve);
		_sortedEdges.insert(dve, &_FrontEdgeB);
	}
	else {
		_sortedEdges.push_back(&_FrontEdgeB);
	}
}

void TVertex::setBackEdgeA(ViewEdge *iBackEdgeA, bool incoming)
{
	if (!iBackEdgeA) {
		cerr << "Warning: null pointer passed as argument of TVertex::setBackEdgeA()" << endl;
		return;
	}
	_BackEdgeA = directedViewEdge(iBackEdgeA, incoming);
	if (!_sortedEdges.empty()) {
		edge_pointers_container::iterator dve = _sortedEdges.begin(), dveend = _sortedEdges.end();
		for (; (dve != dveend) && ViewEdgeComp(**dve, _BackEdgeA); ++dve);
		_sortedEdges.insert(dve, &_BackEdgeA);
	}
	else {
		_sortedEdges.push_back(&_BackEdgeA);
	}
}

void TVertex::setBackEdgeB(ViewEdge *iBackEdgeB, bool incoming)
{
	if (!iBackEdgeB) {
		cerr << "Warning: null pointer passed as argument of TVertex::setBackEdgeB()" << endl;
		return;
	}
	_BackEdgeB = directedViewEdge(iBackEdgeB, incoming);
	if (!_sortedEdges.empty()) {
		edge_pointers_container::iterator dve = _sortedEdges.begin(), dveend = _sortedEdges.end();
		for (; (dve != dveend) && ViewEdgeComp(**dve, _BackEdgeB); ++dve);
		_sortedEdges.insert(dve, &_BackEdgeB);
	}
	else {
		_sortedEdges.push_back(&_BackEdgeB);
	}
}

void TVertex::Replace(ViewEdge *iOld, ViewEdge *iNew)
{
	// theoritically, we only replace edges for which this 
	// view vertex is the B vertex
	if ((iOld == _FrontEdgeA.first) && (_FrontEdgeA.first->B() == this)) {
		_FrontEdgeA.first = iNew;
		return;
	}
	if ((iOld == _FrontEdgeB.first) && (_FrontEdgeB.first->B() == this)) {
		_FrontEdgeB.first = iNew;
		return;
	}
	if ((iOld == _BackEdgeA.first) && (_BackEdgeA.first->B() == this)) {
		_BackEdgeA.first = iNew;
		return;
	}
	if ((iOld == _BackEdgeB.first) && (_BackEdgeB.first->B() == this)) {
		_BackEdgeB.first = iNew;
		return;
	}
}

/*! iterators access */
ViewVertex::edge_iterator TVertex::edges_begin()
{
	//return edge_iterator(_FrontEdgeA, _FrontEdgeB, _BackEdgeA, _BackEdgeB, _FrontEdgeA);
	return edge_iterator(_sortedEdges.begin(), _sortedEdges.end(), _sortedEdges.begin());
}

ViewVertex::const_edge_iterator TVertex::edges_begin() const
{
	//return const_edge_iterator(_FrontEdgeA, _FrontEdgeB, _BackEdgeA, _BackEdgeB, _FrontEdgeA);
	return const_edge_iterator(_sortedEdges.begin(), _sortedEdges.end(), _sortedEdges.begin());
}

ViewVertex::edge_iterator TVertex::edges_end()
{
	//return edge_iterator(_FrontEdgeA, _FrontEdgeB, _BackEdgeA, _BackEdgeB, directedViewEdge(0,true));
	return edge_iterator(_sortedEdges.begin(), _sortedEdges.end(), _sortedEdges.end());
}

ViewVertex::const_edge_iterator TVertex::edges_end() const
{
	//return const_edge_iterator(_FrontEdgeA, _FrontEdgeB, _BackEdgeA, _BackEdgeB, directedViewEdge(0, true));
	return const_edge_iterator(_sortedEdges.begin(), _sortedEdges.end(), _sortedEdges.end());
}

ViewVertex::edge_iterator TVertex::edges_iterator(ViewEdge *iEdge)
{
	for (edge_pointers_container::iterator it = _sortedEdges.begin(), itend = _sortedEdges.end(); it != itend; it++) {
		if ((*it)->first == iEdge)
			return edge_iterator(_sortedEdges.begin(), _sortedEdges.end(), it);
	}
	return edge_iterator(_sortedEdges.begin(), _sortedEdges.end(), _sortedEdges.begin());

#if 0
	directedViewEdge dEdge;
	if (_FrontEdgeA.first == iEdge)
		dEdge = _FrontEdgeA;
	else if (_FrontEdgeB.first == iEdge)
		dEdge = _FrontEdgeB;
	else if (_BackEdgeA.first == iEdge)
		dEdge = _BackEdgeA;
	else if (_BackEdgeB.first == iEdge)
		dEdge = _BackEdgeB;
	return edge_iterator(_FrontEdgeA, _FrontEdgeB, _BackEdgeA, _BackEdgeB, dEdge);
#endif
}

ViewVertex::const_edge_iterator TVertex::edges_iterator(ViewEdge *iEdge) const
{
	for (edge_pointers_container::const_iterator it = _sortedEdges.begin(), itend = _sortedEdges.end();
	     it != itend;
	     it++)
	{
		if ((*it)->first == iEdge)
			return const_edge_iterator(_sortedEdges.begin(), _sortedEdges.end(), it);
	}
	return const_edge_iterator(_sortedEdges.begin(), _sortedEdges.end(), _sortedEdges.begin());

#if 0
	directedViewEdge dEdge;
	if (_FrontEdgeA.first == iEdge)
		dEdge = _FrontEdgeA;
	else if (_FrontEdgeB.first == iEdge)
		dEdge = _FrontEdgeB;
	else if (_BackEdgeA.first == iEdge)
		dEdge = _BackEdgeA;
	else if (_BackEdgeB.first == iEdge)
		dEdge = _BackEdgeB;
	return const_edge_iterator(_FrontEdgeA, _FrontEdgeB, _BackEdgeA, _BackEdgeB, dEdge);
#endif
}

ViewVertexInternal::orientedViewEdgeIterator TVertex::edgesBegin()
{
	return ViewVertexInternal::orientedViewEdgeIterator(_sortedEdges.begin(), _sortedEdges.end(), _sortedEdges.begin());
}

ViewVertexInternal::orientedViewEdgeIterator TVertex::edgesEnd()
{
	return ViewVertexInternal::orientedViewEdgeIterator(_sortedEdges.begin(), _sortedEdges.end(), _sortedEdges.end());
}

ViewVertexInternal::orientedViewEdgeIterator TVertex::edgesIterator(ViewEdge *iEdge)
{
	for (edge_pointers_container::iterator it = _sortedEdges.begin(), itend = _sortedEdges.end(); it != itend; it++) {
		if ((*it)->first == iEdge)
			return ViewVertexInternal::orientedViewEdgeIterator(_sortedEdges.begin(), _sortedEdges.end(), it);
	}
	return ViewVertexInternal::orientedViewEdgeIterator(_sortedEdges.begin(), _sortedEdges.end(), _sortedEdges.begin());
}

/**********************************/
/*                                */
/*                                */
/*             NonTVertex         */ 
/*                                */
/*                                */
/**********************************/

void NonTVertex::AddOutgoingViewEdge(ViewEdge *iVEdge)
{
	// let's keep the viewedges ordered in CCW order in the 2D image plan
	directedViewEdge idve(iVEdge, false);
	if (!_ViewEdges.empty()) {
		edges_container::iterator dve = _ViewEdges.begin(), dveend = _ViewEdges.end();
		for (; (dve != dveend) && ViewEdgeComp(*dve, idve); ++dve);
		_ViewEdges.insert(dve, idve);
	}
	else {
		_ViewEdges.push_back(idve);
	}
}

void NonTVertex::AddIncomingViewEdge(ViewEdge *iVEdge)
{
	// let's keep the viewedges ordered in CCW order in the 2D image plan
	directedViewEdge idve(iVEdge, true);
	if (!_ViewEdges.empty()) {
		edges_container::iterator dve = _ViewEdges.begin(), dveend = _ViewEdges.end();
		for (; (dve != dveend) && ViewEdgeComp(*dve, idve); ++dve);
		_ViewEdges.insert(dve, idve);
	}
	else {
		_ViewEdges.push_back(idve);
	}
}

/*! iterators access */
ViewVertex::edge_iterator NonTVertex::edges_begin()
{
	return edge_iterator(_ViewEdges.begin(), _ViewEdges.end(), _ViewEdges.begin());
}

ViewVertex::const_edge_iterator NonTVertex::edges_begin() const
{
	return const_edge_iterator(_ViewEdges.begin(), _ViewEdges.end(), _ViewEdges.begin());
}

ViewVertex::edge_iterator NonTVertex::edges_end()
{
	return edge_iterator(_ViewEdges.begin(), _ViewEdges.end(), _ViewEdges.end());
}

ViewVertex::const_edge_iterator NonTVertex::edges_end() const
{
	return const_edge_iterator(_ViewEdges.begin(), _ViewEdges.end(), _ViewEdges.end());
}

ViewVertex::edge_iterator NonTVertex::edges_iterator(ViewEdge *iEdge)
{
	for (edges_container::iterator it = _ViewEdges.begin(), itend = _ViewEdges.end(); it != itend; it++) {
		if ((it)->first == iEdge)
			return edge_iterator(_ViewEdges.begin(), _ViewEdges.end(), it);
	}
	return edge_iterator(_ViewEdges.begin(), _ViewEdges.end(), _ViewEdges.begin());
}

ViewVertex::const_edge_iterator NonTVertex::edges_iterator(ViewEdge *iEdge) const
{
	for (edges_container::const_iterator it = _ViewEdges.begin(), itend = _ViewEdges.end(); it != itend; it++) {
		if ((it)->first == iEdge)
			return const_edge_iterator(_ViewEdges.begin(), _ViewEdges.end(), it);
	}
	return const_edge_iterator(_ViewEdges.begin(), _ViewEdges.end(), _ViewEdges.begin());
}

ViewVertexInternal::orientedViewEdgeIterator NonTVertex::edgesBegin()
{
	return ViewVertexInternal::orientedViewEdgeIterator(_ViewEdges.begin(), _ViewEdges.end(), _ViewEdges.begin());
}

ViewVertexInternal::orientedViewEdgeIterator NonTVertex::edgesEnd()
{
	return ViewVertexInternal::orientedViewEdgeIterator(_ViewEdges.begin(), _ViewEdges.end(), _ViewEdges.end());
}

ViewVertexInternal::orientedViewEdgeIterator NonTVertex::edgesIterator(ViewEdge *iEdge)
{
	for (edges_container::iterator it = _ViewEdges.begin(), itend = _ViewEdges.end(); it != itend; it++) {
		if ((it)->first == iEdge)
			return ViewVertexInternal::orientedViewEdgeIterator(_ViewEdges.begin(), _ViewEdges.end(), it);
	}
	return ViewVertexInternal::orientedViewEdgeIterator(_ViewEdges.begin(), _ViewEdges.end(), _ViewEdges.begin());
}

/**********************************/
/*                                */
/*                                */
/*             ViewEdge           */
/*                                */
/*                                */
/**********************************/

real ViewEdge::getLength2D() const
{
	float length = 0.0f;
	ViewEdge::const_fedge_iterator itlast = fedge_iterator_last();
	ViewEdge::const_fedge_iterator it = fedge_iterator_begin(), itend = fedge_iterator_end();
	Vec2r seg;
	do {
		seg = Vec2r((*it)->orientation2d()[0], (*it)->orientation2d()[1]);
		length += seg.norm();
		++it;
	} while ((it != itend) && (it != itlast));
	return length;
}

//! view edge iterator
ViewEdge::edge_iterator ViewEdge::ViewEdge_iterator()
{
	return edge_iterator(this);
}

ViewEdge::const_edge_iterator ViewEdge::ViewEdge_iterator() const
{
	return const_edge_iterator((ViewEdge *)this);
}

//! feature edge iterator
ViewEdge::fedge_iterator ViewEdge::fedge_iterator_begin()
{
	return fedge_iterator(this->_FEdgeA, this->_FEdgeB);
}

ViewEdge::const_fedge_iterator ViewEdge::fedge_iterator_begin() const
{
	return const_fedge_iterator(this->_FEdgeA, this->_FEdgeB);
}

ViewEdge::fedge_iterator ViewEdge::fedge_iterator_last()
{
	return fedge_iterator(this->_FEdgeB, this->_FEdgeB);
}

ViewEdge::const_fedge_iterator ViewEdge::fedge_iterator_last() const
{
	return const_fedge_iterator(this->_FEdgeB, this->_FEdgeB);
}

ViewEdge::fedge_iterator ViewEdge::fedge_iterator_end()
{
	return fedge_iterator(0, this->_FEdgeB);
}

ViewEdge::const_fedge_iterator ViewEdge::fedge_iterator_end() const
{
	return const_fedge_iterator(0, this->_FEdgeB);
}

//! embedding vertex iterator
ViewEdge::const_vertex_iterator ViewEdge::vertices_begin() const
{
	return const_vertex_iterator(this->_FEdgeA->vertexA(), 0, _FEdgeA);
}

ViewEdge::vertex_iterator ViewEdge::vertices_begin()
{
	return vertex_iterator(this->_FEdgeA->vertexA(), 0, _FEdgeA);
}

ViewEdge::const_vertex_iterator ViewEdge::vertices_last() const
{
	return const_vertex_iterator(this->_FEdgeB->vertexB(), _FEdgeB, 0);
}

ViewEdge::vertex_iterator ViewEdge::vertices_last()
{
	return vertex_iterator(this->_FEdgeB->vertexB(), _FEdgeB, 0);
}

ViewEdge::const_vertex_iterator ViewEdge::vertices_end() const
{
	return const_vertex_iterator(0, _FEdgeB, 0);
}

ViewEdge::vertex_iterator ViewEdge::vertices_end()
{
	return vertex_iterator(0, _FEdgeB, 0);
}

Interface0DIterator ViewEdge::verticesBegin()
{
	Interface0DIterator ret(new ViewEdgeInternal::SVertexIterator(this->_FEdgeA->vertexA(),
	                                                              this->_FEdgeA->vertexA(), NULL, _FEdgeA, 0.0f));
	return ret;
}

Interface0DIterator ViewEdge::verticesEnd()
{
	Interface0DIterator ret(new ViewEdgeInternal::SVertexIterator(NULL, this->_FEdgeA->vertexA(),
	                                                              _FEdgeB, NULL, getLength2D()));
	return ret;
}

Interface0DIterator ViewEdge::pointsBegin(float t)
{
	return verticesBegin();
}

Interface0DIterator ViewEdge::pointsEnd(float t)
{
	return verticesEnd();
}

                  /**********************************/
                  /*                                */
                  /*                                */
                  /*             ViewShape          */
                  /*                                */
                  /*                                */
                  /**********************************/

ViewShape::~ViewShape()
{
	_Vertices.clear();

	if (!(_Edges.empty())) {
		for (vector<ViewEdge*>::iterator e = _Edges.begin(), eend = _Edges.end(); e != eend; e++) {
			delete (*e);
		}
		_Edges.clear();
	}

	if (_SShape) {
		delete _SShape;
		_SShape = NULL;
	}
}

void ViewShape::RemoveEdge(ViewEdge *iViewEdge)
{
	FEdge *fedge = iViewEdge->fedgeA();
	for (vector<ViewEdge*>::iterator ve = _Edges.begin(), veend = _Edges.end(); ve != veend; ve++) {
		if (iViewEdge == (*ve)) {
			_Edges.erase(ve);
			_SShape->RemoveEdge(fedge);
			break;
		}
	}
}

void ViewShape::RemoveVertex(ViewVertex *iViewVertex)
{
	for (vector<ViewVertex*>::iterator vv = _Vertices.begin(), vvend = _Vertices.end(); vv != vvend; vv++) {
		if (iViewVertex == (*vv)) {
			_Vertices.erase(vv);
			break;
		}
	}
}

/**********************************/
/*                                */
/*                                */
/*             ViewEdge           */
/*                                */
/*                                */
/**********************************/

void ViewEdge::UpdateFEdges()
{
	FEdge *currentEdge = _FEdgeA;
	do {
		currentEdge->setViewEdge(this);
		currentEdge = currentEdge->nextEdge();
	} while ((currentEdge != NULL) && (currentEdge != _FEdgeB));
	// last one
	_FEdgeB->setViewEdge(this);
}

} /* namespace Freestyle */
