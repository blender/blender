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

/** \file blender/freestyle/intern/stroke/Stroke.cpp
 *  \ingroup freestyle
 *  \brief Classes to define a stroke
 *  \author Stephane Grabli
 *  \date 09/09/2002
 */

#include "Stroke.h"
#include "StrokeIterators.h"
#include "StrokeAdvancedIterators.h"
#include "StrokeRenderer.h"

#include "BKE_global.h"
#include "BKE_node.h"

namespace Freestyle {

/**********************************/
/*                                */
/*                                */
/*         StrokeAttribute        */
/*                                */
/*                                */
/**********************************/

StrokeAttribute::StrokeAttribute()
{
	int i;
	_alpha = 1.0f;
	_thickness[0] = 1.0f;
	_thickness[1] = 1.0f;
	for (i = 0; i < 3; ++i)
		_color[i] = 0.2f;
	_color[0] = 0.8f;
	_userAttributesReal = NULL;
	_userAttributesVec2f = NULL;
	_userAttributesVec3f = NULL;
	_visible = true;
}

StrokeAttribute::StrokeAttribute(const StrokeAttribute& iBrother)
{
	_alpha = iBrother._alpha;
	_thickness[0] = iBrother._thickness[0];
	_thickness[1] = iBrother._thickness[1];
	for (int i = 0; i < 3; ++i)
		_color[i] = iBrother._color[i];
	_visible = iBrother._visible;
	if (iBrother._userAttributesReal)
		_userAttributesReal = new realMap(*iBrother._userAttributesReal);
	else
		_userAttributesReal = NULL;
	if (iBrother._userAttributesVec2f)
		_userAttributesVec2f = new Vec2fMap(*iBrother._userAttributesVec2f);
	else
		_userAttributesVec2f = NULL;
	if (iBrother._userAttributesVec3f)
		_userAttributesVec3f = new Vec3fMap(*iBrother._userAttributesVec3f);
	else
		_userAttributesVec3f = NULL;
}

StrokeAttribute::StrokeAttribute(float iRColor, float iGColor, float iBColor, float iAlpha,
                                 float iRThickness, float iLThickness)
{
	_color[0] = iRColor;
	_color[1] = iGColor;
	_color[2] = iBColor;

	_alpha = iAlpha;

	_thickness[0] = iRThickness;
	_thickness[1] = iLThickness;

	_visible = true;

	_userAttributesReal = NULL;
	_userAttributesVec2f = NULL;
	_userAttributesVec3f = NULL;
}

StrokeAttribute::StrokeAttribute(const StrokeAttribute& a1, const StrokeAttribute& a2, float t)
{
	_alpha = (1 - t) * a1._alpha + t * a2._alpha;
	_thickness[0] = (1 - t) * a1._thickness[0] + t * a2._thickness[0];
	_thickness[1] = (1 - t) * a1._thickness[1] + t * a2._thickness[1];
	for (int i = 0; i < 3; ++i)
		_color[i] = (1 - t) * a1._color[i] + t * a2._color[i];

	_visible = a1.isVisible();

	// FIXME: to be checked (and enhanced)
	if ((a1._userAttributesReal) && (a2._userAttributesReal)) {
		if (a1._userAttributesReal->size() == a2._userAttributesReal->size()) {
			_userAttributesReal = new realMap;
			realMap::iterator it1 = a1._userAttributesReal->begin(), it1end = a1._userAttributesReal->end();
			realMap::iterator it2 = a2._userAttributesReal->begin();
			for (; it1 != it1end; ++it1, ++it2) {
				(*_userAttributesReal)[(*it1).first] = ((1 - t) * (*it1).second + t * (*it2).second);
			}
		}
	}
	else {
		_userAttributesReal = NULL;
	}
	if ((a1._userAttributesVec2f) && (a2._userAttributesVec2f)) {
		if (a1._userAttributesVec2f->size() == a2._userAttributesVec2f->size()) {
			_userAttributesVec2f = new Vec2fMap;
			Vec2fMap::iterator it1 = a1._userAttributesVec2f->begin(), it1end = a1._userAttributesVec2f->end();
			Vec2fMap::iterator it2 = a2._userAttributesVec2f->begin();
			for (; it1 != it1end; ++it1, ++it2) {
				(*_userAttributesVec2f)[(*it1).first] = ((1 - t) * (*it1).second + t * (*it2).second);
			}
		}
	}
	else {
		_userAttributesVec2f = NULL;
	}
	if ((a1._userAttributesVec3f) && (a2._userAttributesVec3f)) {
		if (a1._userAttributesVec3f->size() == a2._userAttributesVec3f->size()) {
			_userAttributesVec3f = new Vec3fMap;
			Vec3fMap::iterator it1 = a1._userAttributesVec3f->begin(), it1end = a1._userAttributesVec3f->end();
			Vec3fMap::iterator it2 = a2._userAttributesVec3f->begin();
			for (; it1 != it1end; ++it1, ++it2) {
				(*_userAttributesVec3f)[(*it1).first] = ((1 - t) * (*it1).second + t * (*it2).second);
			}
		}
	}
	else {
		_userAttributesVec3f = NULL;
	}
}

StrokeAttribute::~StrokeAttribute()
{
	if (_userAttributesReal) {
		_userAttributesReal->clear();
		delete _userAttributesReal;
	}
	if (_userAttributesVec2f) {
		_userAttributesVec2f->clear();
		delete _userAttributesVec2f;
	}
	if (_userAttributesVec3f) {
		_userAttributesVec3f->clear();
		delete _userAttributesVec3f;
	}
}

StrokeAttribute& StrokeAttribute::operator=(const StrokeAttribute& iBrother)
{
	int i;
	_alpha = iBrother._alpha;
	_thickness[0] = iBrother._thickness[0];
	_thickness[1] = iBrother._thickness[1];
	for (i = 0; i < 3; ++i)
		_color[i] = iBrother._color[i];
	_visible = iBrother._visible;
	if (iBrother._userAttributesReal) {
		if (!_userAttributesReal)
			_userAttributesReal = new realMap;
		_userAttributesReal = new realMap(*(iBrother._userAttributesReal));
	}
	else {
		_userAttributesReal = NULL;
	}
	if (iBrother._userAttributesVec2f) {
		if (!_userAttributesVec2f)
			_userAttributesVec2f = new Vec2fMap;
		_userAttributesVec2f = new Vec2fMap(*(iBrother._userAttributesVec2f));
	}
	else {
		_userAttributesVec2f = NULL;
	}
	if (iBrother._userAttributesVec3f) {
		if (!_userAttributesVec3f)
			_userAttributesVec3f = new Vec3fMap;
		_userAttributesVec3f = new Vec3fMap(*(iBrother._userAttributesVec3f));
	}
	else {
		_userAttributesVec3f = NULL;
	}
	return *this;
}

float StrokeAttribute::getAttributeReal(const char *iName) const
{
	if (!_userAttributesReal) {
		if (G.debug & G_DEBUG_FREESTYLE) {
			cout << "StrokeAttribute warning: no real attribute was defined" << endl;
		}
		return 0.0f;
	}
	realMap::iterator a = _userAttributesReal->find(iName);
	if (a == _userAttributesReal->end()) {
		if (G.debug & G_DEBUG_FREESTYLE) {
			cout << "StrokeAttribute warning: no real attribute was added with the name " << iName << endl;
		}
		return 0.0f;
	}
	return (*a).second;
}

Vec2f StrokeAttribute::getAttributeVec2f(const char *iName) const
{
	if (!_userAttributesVec2f) {
		if (G.debug & G_DEBUG_FREESTYLE) {
			cout << "StrokeAttribute warning: no Vec2f attribute was defined" << endl;
		}
		return 0;
	}
	Vec2fMap::iterator a = _userAttributesVec2f->find(iName);
	if (a == _userAttributesVec2f->end()) {
		if (G.debug & G_DEBUG_FREESTYLE) {
			cout << "StrokeAttribute warning: no Vec2f attribute was added with the name " << iName << endl;
		}
		return 0;
	}
	return (*a).second;
}

Vec3f StrokeAttribute::getAttributeVec3f(const char *iName) const
{
	if (!_userAttributesVec3f) {
		if (G.debug & G_DEBUG_FREESTYLE) {
			cout << "StrokeAttribute warning: no Vec3f attribute was defined" << endl;
		}
		return 0;
	}
	Vec3fMap::iterator a = _userAttributesVec3f->find(iName);
	if (a == _userAttributesVec3f->end()) {
		if (G.debug & G_DEBUG_FREESTYLE) {
			cout << "StrokeAttribute warning: no Vec3f attribute was added with the name " << iName << endl;
		}
		return 0;
	}
	return (*a).second;
}

bool StrokeAttribute::isAttributeAvailableReal(const char *iName) const
{
	if (!_userAttributesReal) {
		return false;
	}
	realMap::iterator a = _userAttributesReal->find(iName);
	if (a == _userAttributesReal->end()) {
		return false;
	}
	return true;
}

bool StrokeAttribute::isAttributeAvailableVec2f(const char *iName) const
{
	if (!_userAttributesVec2f) {
		return false;
	}
	Vec2fMap::iterator a = _userAttributesVec2f->find(iName);
	if (a == _userAttributesVec2f->end()) {
		return false;
	}
	return true;
}

bool StrokeAttribute::isAttributeAvailableVec3f(const char *iName) const
{
	if (!_userAttributesVec3f) {
		return false;
	}
	Vec3fMap::iterator a = _userAttributesVec3f->find(iName);
	if (a == _userAttributesVec3f->end()) {
		return false;
	}
	return true;
}

void StrokeAttribute::setAttributeReal(const char *iName, float att)
{
	if (!_userAttributesReal)
		_userAttributesReal = new realMap;
	(*_userAttributesReal)[iName] = att;
}

void StrokeAttribute::setAttributeVec2f(const char *iName, const Vec2f& att)
{
	if (!_userAttributesVec2f)
		_userAttributesVec2f = new Vec2fMap;
	(*_userAttributesVec2f)[iName] = att;
}

void StrokeAttribute::setAttributeVec3f(const char *iName, const Vec3f& att)
{
	if (!_userAttributesVec3f)
		_userAttributesVec3f = new Vec3fMap;
	(*_userAttributesVec3f)[iName] = att;
}


/**********************************/
/*                                */
/*                                */
/*           StrokeVertex         */
/*                                */
/*                                */
/**********************************/

StrokeVertex::StrokeVertex() : CurvePoint()
{
	_CurvilignAbscissa = 0.0f;
	_StrokeLength = 0.0f;
}

StrokeVertex::StrokeVertex(const StrokeVertex& iBrother) : CurvePoint(iBrother)
{
	_Attribute = iBrother._Attribute;
	_CurvilignAbscissa = 0.0f;
	_StrokeLength = 0.0f;
}

StrokeVertex::StrokeVertex(SVertex *iSVertex) : CurvePoint(iSVertex, 0, 0.0f)
{
	_CurvilignAbscissa = 0.0f;
	_StrokeLength = 0.0f;
}

StrokeVertex::StrokeVertex(CurvePoint *iPoint) : CurvePoint(*iPoint)
{
	_CurvilignAbscissa = 0.0f;
	_StrokeLength = 0.0f;
}

StrokeVertex::StrokeVertex(StrokeVertex *iA, StrokeVertex *iB, float t3) : CurvePoint(iA, iB, t3)
{
	// interpolate attributes:
	_Attribute = StrokeAttribute(iA->attribute(), iB->attribute(), t3);
	_CurvilignAbscissa = (1 - t3) * iA->curvilinearAbscissa() + t3 * iB->curvilinearAbscissa();
	_StrokeLength = iA->strokeLength();
}

StrokeVertex::StrokeVertex(SVertex *iSVertex, const StrokeAttribute& iAttribute) : CurvePoint(iSVertex, 0, 0.0f)
{
	_Attribute = iAttribute;
	_CurvilignAbscissa = 0.0f;
	_StrokeLength = 0.0f;
}

StrokeVertex::~StrokeVertex() {}

StrokeVertex& StrokeVertex::operator=(const StrokeVertex& iBrother)
{
	((CurvePoint *)this)->operator=(iBrother);
	_Attribute = iBrother._Attribute;

	_CurvilignAbscissa = 0.0f;

	_StrokeLength = 0.0f;
	return *this;
}



/**********************************/
/*                                */
/*                                */
/*             Stroke             */
/*                                */
/*                                */
/**********************************/

Stroke::Stroke()
{
	_Length = 0;
	_id = 0;
	_sampling = FLT_MAX;
	//_mediumType = DEFAULT_STROKE;
	_mediumType = OPAQUE_MEDIUM;
	_textureId = 0;
	_textureStep = 1.0;
	for (int a = 0; a < MAX_MTEX; a++) {
		_mtex[a] = NULL;
	}
	_nodeTree = NULL;
	_tips = false;
}

Stroke::Stroke(const Stroke& iBrother)
{
	for (vertex_container::const_iterator v = iBrother._Vertices.begin(), vend = iBrother._Vertices.end();
	     v != vend;
	     v++)
	{
		_Vertices.push_back(*v);
	}
	_Length = 0;
	_id = iBrother._id;
	_ViewEdges = iBrother._ViewEdges;
	_sampling = iBrother._sampling;
	_mediumType = iBrother._mediumType;
	_textureId = iBrother._textureId;
	_textureStep = iBrother._textureStep;
	for (int a = 0; a < MAX_MTEX; a++) {
		if (iBrother._mtex) {
			_mtex[a] = iBrother._mtex[a];
		}
		else {
			_mtex[a] = NULL;
		}
	}
	_nodeTree = iBrother._nodeTree;
	_tips = iBrother._tips;
}

Stroke::~Stroke()
{
	if (!_Vertices.empty()) {
		for (vertex_container::iterator v = _Vertices.begin(), vend = _Vertices.end(); v != vend; v++) {
			delete (*v);
		}
		_Vertices.clear();
	}

	_ViewEdges.clear();
}

Stroke& Stroke::operator=(const Stroke& iBrother)
{
	if (!_Vertices.empty())
		_Vertices.clear();

	for (vertex_container::const_iterator v = iBrother._Vertices.begin(), vend = iBrother._Vertices.end();
	     v != vend;
	     v++)
	{
		_Vertices.push_back(*v);
	}
	_Length = iBrother._Length;
	_id = iBrother._id;
	_ViewEdges = iBrother._ViewEdges;
	_sampling = iBrother._sampling;
	return *this;
}

void Stroke::setLength(float iLength)
{
	_Length = iLength;
	for (vertex_container::iterator v = _Vertices.begin(), vend = _Vertices.end(); v != vend; ++v) {
		(*v)->setStrokeLength(iLength);
	}
}

float Stroke::ComputeSampling(int iNVertices)
{
	if (iNVertices <= (int)_Vertices.size()) //soc
		return _sampling;

	float sampling = _Length / (float)(iNVertices - _Vertices.size() + 1);
	return sampling;
}

class StrokeSegment
{
public:
	StrokeInternal::StrokeVertexIterator _begin;
	StrokeInternal::StrokeVertexIterator _end;
	float _length;
	int _n;
	float _sampling;
	bool _resampled;

	StrokeSegment(StrokeInternal::StrokeVertexIterator ibegin, StrokeInternal::StrokeVertexIterator iend,
	              float ilength, int in, float isampling)
	{
		_begin = ibegin;
		_end = iend;
		_length = ilength;
		_n = in;
		_sampling = isampling;
		_resampled = false;
	}
};

int Stroke::Resample(int iNPoints)
{
	int NPointsToAdd = iNPoints - strokeVerticesSize();
	if (NPointsToAdd <= 0)
		return 0;

	StrokeInternal::StrokeVertexIterator it = strokeVerticesBegin();
	StrokeInternal::StrokeVertexIterator next = it;
	++next;
	StrokeInternal::StrokeVertexIterator itend = strokeVerticesEnd();

	vertex_container newVertices;
	real t = 0.0f;
	StrokeVertex *newVertex = NULL;
	vector<StrokeSegment> strokeSegments;
	int N = 0;
	float meanlength = 0;
	int nsegments = 0;
	while ((it != itend) && (next != itend)) {
		Vec2r a((it)->getPoint());
		Vec2r b((next)->getPoint());
		Vec2r vec_tmp(b - a);
		real norm_var = vec_tmp.norm();
		int numberOfPointsToAdd = (int)floor(NPointsToAdd * norm_var / _Length);
		float csampling = norm_var / (float)(numberOfPointsToAdd + 1);
		strokeSegments.push_back(StrokeSegment(it, next, norm_var, numberOfPointsToAdd, csampling));
		N += numberOfPointsToAdd;
		meanlength += norm_var;
		++nsegments;
		++it;
		++next;
	}
	meanlength /= (float)nsegments;

	// if we don't have enough points let's resample finer some segments
	bool checkEveryone = false;
	bool resampled;
	while (N < NPointsToAdd) {
		resampled = false;
		for (vector<StrokeSegment>::iterator s = strokeSegments.begin(), send = strokeSegments.end(); s != send; ++s) {
			if (s->_sampling == 0.0f)
				continue;

			if (s->_resampled == false) {
				if ((!checkEveryone) && (s->_length < meanlength))
					continue;
				//resample
				s->_n = s->_n + 1;
				s->_sampling = s->_length / (float)(s->_n + 1);
				s->_resampled = resampled = true;
				N++;
				if (N == NPointsToAdd)
					break;
			}
		}
		if (checkEveryone && !resampled)
			break;
		checkEveryone = true;
	}
	if (N < NPointsToAdd) {
		// fatal error, likely because _Length is inconsistent with the stroke length computed with the vertices
		return -1;
	}
	//actually resample:
	for (vector<StrokeSegment>::iterator s = strokeSegments.begin(), send = strokeSegments.end(); s != send; ++s) {
		newVertices.push_back(&(*(s->_begin)));
		if (s->_sampling < _sampling)
			_sampling = s->_sampling;

		t = s->_sampling / s->_length;
		for (int i = 0; i < s->_n; ++i) {
			newVertex = new StrokeVertex(&(*(s->_begin)), &(*(s->_end)), t);
			newVertices.push_back(newVertex);
			t += s->_sampling / s->_length;
		}
		it = s->_begin;
		next = s->_end;
	}

	// add last:
	++it;
	++next;
	if ((it != itend) && (next == itend)/* && (t == 0.0f)*/)
		newVertices.push_back(&(*it));

	int newsize = newVertices.size();
	if (newsize != iNPoints)
		cerr << "Warning: incorrect points number" << endl;

	_Vertices.clear();
	_Vertices = newVertices;
	newVertices.clear();

	return 0;
}

int Stroke::Resample(float iSampling)
{
	//cerr << "old size :" << strokeVerticesSize() << endl;
	if (iSampling == 0)
		return 0;
	if (iSampling >= _sampling)
		return 0;

	_sampling = iSampling;
	// Resample...
	//real curvilinearLength = 0.0f;
	vertex_container newVertices;
	real t = 0.0f;
	const real limit = 0.99;
	StrokeVertex *newVertex = NULL;
	StrokeInternal::StrokeVertexIterator it = strokeVerticesBegin();
	StrokeInternal::StrokeVertexIterator next = it;
	++next;
	StrokeInternal::StrokeVertexIterator itend = strokeVerticesEnd();
	while ((it != itend) && (next != itend)) {
		newVertices.push_back(&(*it));
		Vec2r a((it)->getPoint());
		Vec2r b((next)->getPoint());
		Vec2r vec_tmp(b - a);
		real norm_var = vec_tmp.norm();
		if (norm_var <= _sampling) {
			//curvilinearLength += norm_var;
			++it;
			++next;
			continue;
		}

		//curvilinearLength += _sampling;
		t = _sampling / norm_var;
		while (t < limit) {
			newVertex = new StrokeVertex(&(*it), &(*next), t);
			//newVertex->setCurvilinearAbscissa(curvilinearLength);
			newVertices.push_back(newVertex);
			t = t + _sampling / norm_var;
		}
		++it;
		++next;
	}
	// add last:
	if ((it != itend) && (next == itend)/* && (t == 0.f)*/)
		newVertices.push_back(&(*it));

	_Vertices.clear();
	_Vertices = newVertices;
	newVertices.clear();

	return 0;
}

void Stroke::RemoveAllVertices()
{
	vertex_container::iterator it = _Vertices.begin(), itend = _Vertices.end();
	for (; it != itend; ++it)
		delete (*it);
	_Vertices.clear();
	UpdateLength();
}

void Stroke::RemoveVertex(StrokeVertex *iVertex)
{
	vertex_container::iterator it = _Vertices.begin(), itend = _Vertices.end();
	for (; it != itend; ++it) {
		if ((*it) == iVertex) {
			delete iVertex;
			it = _Vertices.erase(it); // it is now the element just after the erased element
			break;
		}
	}
	UpdateLength();
}

void Stroke::InsertVertex(StrokeVertex *iVertex, StrokeInternal::StrokeVertexIterator next)
{
	vertex_container::iterator itnext = next.getIt();
	_Vertices.insert(itnext, iVertex);
	UpdateLength();
}

void Stroke::UpdateLength()
{
	// recompute curvilinear abscissa and stroke length
	float curvabsc = 0.0f;
	vertex_container::iterator it = _Vertices.begin(), itend = _Vertices.end();
	vertex_container::iterator previous = it;
	for (; it != itend; ++it) {
		curvabsc += ((*it)->getPoint() - (*previous)->getPoint()).norm();
		(*it)->setCurvilinearAbscissa(curvabsc);
		previous = it;
	}
	_Length = curvabsc;
	for (it = _Vertices.begin(); it != itend; ++it) {
		(*it)->setStrokeLength(_Length);
	}
}

//! embedding vertex iterator
Stroke::const_vertex_iterator Stroke::vertices_begin() const
{
	return const_vertex_iterator(_Vertices.begin(), _Vertices.begin(), _Vertices.end());
}

Stroke::const_vertex_iterator Stroke::vertices_end() const
{
	return const_vertex_iterator(_Vertices.end(), _Vertices.begin(), _Vertices.end());
}

Stroke::vertex_iterator Stroke::vertices_end()
{
	return vertex_iterator(_Vertices.end(), _Vertices.begin(), _Vertices.end());
}

StrokeInternal::StrokeVertexIterator Stroke::strokeVerticesBegin(float t)
{
	if ((t != 0) && (t < _sampling))
		Resample(t);
	return StrokeInternal::StrokeVertexIterator(this->_Vertices.begin(), this->_Vertices.begin(),
	                                            this->_Vertices.end());
}

StrokeInternal::StrokeVertexIterator Stroke::strokeVerticesEnd()
{
	return StrokeInternal::StrokeVertexIterator(this->_Vertices.end(), this->_Vertices.begin(), this->_Vertices.end());
}

Interface0DIterator Stroke::verticesBegin()
{
Interface0DIterator ret(new StrokeInternal::StrokeVertexIterator(this->_Vertices.begin(), this->_Vertices.begin(),
                                                                 this->_Vertices.end()));
return ret;
}

Interface0DIterator Stroke::verticesEnd()
{
	Interface0DIterator ret(new StrokeInternal::StrokeVertexIterator(this->_Vertices.end(), this->_Vertices.begin(),
	                                                                 this->_Vertices.end()));
	return ret;
}

Interface0DIterator Stroke::pointsBegin(float t)
{
	return verticesBegin(); // FIXME
}

Interface0DIterator Stroke::pointsEnd(float t)
{
	return verticesEnd();
}

void Stroke::ScaleThickness(float iFactor)
{
	for (vertex_container::iterator it = _Vertices.begin(), itend = _Vertices.end(); it != itend; ++it) {
		StrokeAttribute& attr = (*it)->attribute();
		attr.setThickness(iFactor * attr.getThicknessR(), iFactor * attr.getThicknessL());
	}
}

void Stroke::Render(const StrokeRenderer *iRenderer)
{
	StrokeRep rep(this);
	iRenderer->RenderStrokeRep(&rep);
}

void Stroke::RenderBasic(const StrokeRenderer *iRenderer)
{
	StrokeRep rep(this);
	iRenderer->RenderStrokeRepBasic(&rep);
}

Stroke::vertex_iterator Stroke::vertices_begin(float sampling)
{
	// Resample if necessary
	if ((sampling != 0) && (sampling < _sampling))
		Resample(sampling);
	return vertex_iterator(_Vertices.begin(), _Vertices.begin(), _Vertices.end());
	//return _Vertices.begin();
}

#if 0
Stroke::vertex_iterator Stroke::vertices_last()
{
	vertex_iterator res = vertices_begin();
	vertex_iterator next = res;
	++next;
	while (!next.end()) {
		++next;
		++res;
	}
	return res;
}

Stroke::const_vertex_iterator Stroke::vertices_last() const
{
	const_vertex_iterator res = vertices_begin();
	const_vertex_iterator next = res;
	++next;
	while (!next.end()) {
		++next;
		++res;
	}
	return res;
}

Stroke::vertex_container::reverse_iterator Stroke::vertices_last(float sampling)
{
	// Resample if necessary
	if (sampling < _sampling)
		Resample(sampling);
	return _Vertices.rbegin();
}


inline Vec3r shaded_color(int iCombination = 0) const;

inline Vec<3, real> Stroke::orientation2d(const_vertex_iterator it) const
{
	return iterator_edge_orientation2d_function<Stroke, const_vertex_iterator>(this, it);
}

Vec3r Stroke::orientation2d(int iCombination) const
{
	return edge_orientation2d_function<Stroke>(*this, iCombination);
}

inline Vec3r Stroke::orientation3d(const_vertex_iterator it) const
{
	return iterator_edge_orientation3d_function<Stroke, const_vertex_iterator>(*this, it);
}

Vec3r Stroke::orientation3d(int iCombination) const
{
	return edge_orientation3d_function<Stroke>(*this, iCombination);
}

Material Stroke::material() const
{
	const_vertex_iterator v = vertices_begin(), vend = strokeVerticesEnd();
	Material mat = (*v)->material();
	for (; v != vend; ++v) {
		if (mat != (*v)->material())
			Exception::raiseException();
	}
	return mat;
}

int Stroke::qi() const
{
	const_vertex_iterator v = vertices_begin(), vend = vertices_end();
	int qi_= (*v)->qi();
	for (; v != vend; ++v) {
		if ((*v)->qi() != qi_)
			Exception::raiseException();
	}
	return qi_;
}

inline occluder_container::const_iterator occluders_begin() const
{
	return _FEdgeA->occluders().begin();
}

inline occluder_container::const_iterator occluders_end() const
{
	return _FEdgeA->occluders().end();
}

int Stroke::occluders_size() const
{
	return qi();
}

bool Stroke::occluders_empty() const
{
	const_vertex_iterator v = vertices_begin(), vend = vertices_end();
	bool empty = (*v)->occluders_empty();
	for (; v != vend; ++v) {
		if ((*v)->occluders_empty() != empty)
			Exception::raiseException();
	}
	return empty;
}

#if 0
inline const polygon3d& occludee() const
{
	return *(_FEdgeA->aFace());
}
#endif

const SShape *Stroke::occluded_shape() const
{
	const_vertex_iterator v = vertices_begin(), vend = vertices_end();
	const SShape *sshape = (*v)->occluded_shape();
	for (; v != vend; ++v) {
		if ((*v)->occluded_shape() != sshape)
			Exception::raiseException();
	}
	return sshape;
}

const bool Stroke::occludee_empty() const
{
	const_vertex_iterator v = vertices_begin(), vend = vertices_end();
	bool empty = (*v)->occludee_empty();
	for (; v != vend; ++v) {
		if ((*v)->occludee_empty() != empty)
			Exception::raiseException();
	}
	return empty;
}

const SShape *Stroke::shape() const
{
	const_vertex_iterator v = vertices_begin(), vend = vertices_end();
	const SShape *sshape = (*v)->shape();
	for (; v != vend; ++v) {
		if ((*v)->shape() != sshape)
			Exception::raiseException();
	}
	return sshape;
}

real Stroke::z_discontinuity(int iCombination) const
{
	return z_discontinuity_edge_function<Stroke>(*this, iCombination);
}

Vec3r Stroke::curvature2d_as_vector(int iCombination) const
{
	return curvature2d_as_vector_edge_function<Stroke>(*this, iCombination);
}

real Stroke::curvature2d_as_angle(int iCombination) const
{
	return curvature2d_as_angle_edge_function<Stroke>(*this, iCombination);
}

float Stroke::shape_importance(int iCombination) const
{
	return shape_importance_edge_function<Stroke>(*this, iCombination);
}


float Stroke::local_average_depth(int iCombination ) const
{
	return local_average_depth_edge_function<Stroke >(*this, iCombination);
}

float Stroke::local_depth_variance(int iCombination) const
{
	return local_depth_variance_edge_function<Stroke>(*this, iCombination);
}

real Stroke::local_average_density(float sigma , int iCombination ) const
{
	return density_edge_function<Stroke>(*this, iCombination);
}
#endif

} /* namespace Freestyle */
