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

#ifndef __FREESTYLE_SILHOUETTE_H__
#define __FREESTYLE_SILHOUETTE_H__

/** \file blender/freestyle/intern/view_map/Silhouette.h
 *  \ingroup freestyle
 *  \brief Classes to define a silhouette structure
 *  \author Stephane Grabli
 *  \date 25/03/2002
 */

#include <float.h>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "Interface0D.h"
#include "Interface1D.h"

#include "../geometry/BBox.h"
#include "../geometry/Geom.h"
#include "../geometry/Polygon.h"

#include "../scene_graph/FrsMaterial.h"

#include "../system/Exception.h"
#include "../system/FreestyleConfig.h"

#include "../winged_edge/Curvature.h"

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

using namespace std;

namespace Freestyle {

using namespace Geometry;

class ViewShape;
typedef vector<ViewShape*> occluder_container;

/**********************************/
/*                                */
/*                                */
/*             SVertex            */
/*                                */
/*                                */
/**********************************/

class FEdge;
class ViewVertex;
class SShape;

/*! Class to define a vertex of the embedding. */
class LIB_VIEW_MAP_EXPORT SVertex : public Interface0D
{
public: // Implementation of Interface0D
	/*! Returns the string "SVertex" .*/
	virtual string getExactTypeName() const
	{
		return "SVertex";
	}

	// Data access methods
	/*! Returns the 3D x coordinate of the vertex .*/
	virtual real getX() const
	{
		return _Point3D.x();
	}

	/*! Returns the 3D y coordinate of the vertex .*/
	virtual real getY() const
	{
		return _Point3D.y();
	}

	/*! Returns the 3D z coordinate of the vertex .*/
	virtual real getZ() const
	{
		return _Point3D.z();
	}

	/*!  Returns the 3D point. */ 
	virtual Vec3f getPoint3D() const
	{
		return _Point3D;
	}

	/*! Returns the projected 3D  x coordinate of the vertex .*/
	virtual real getProjectedX() const
	{
		return _Point2D.x();
	}

	/*! Returns the projected 3D  y coordinate of the vertex .*/
	virtual real getProjectedY() const
	{
		return _Point2D.y();
	}

	/*! Returns the projected 3D  z coordinate of the vertex .*/
	virtual real getProjectedZ() const
	{
		return _Point2D.z();
	}

	/*!  Returns the 2D point. */ 
	virtual Vec2f getPoint2D() const
	{
		return Vec2f((float)_Point2D.x(), (float)_Point2D.y());
	}

	/*! Returns the FEdge that lies between this Svertex and the Interface0D given as argument. */
	virtual FEdge *getFEdge(Interface0D&);

	/*! Returns the Id of the vertex .*/
	virtual Id getId() const
	{
		return _Id;
	}

	/*! Returns the nature of the vertex .*/
	virtual Nature::VertexNature getNature() const;

	/*! Cast the Interface0D in SVertex if it can be. */ 
	virtual SVertex *castToSVertex();

	/*! Cast the Interface0D in ViewVertex if it can be. */ 
	virtual ViewVertex *castToViewVertex();

	/*! Cast the Interface0D in NonTVertex if it can be. */ 
	virtual NonTVertex *castToNonTVertex();

	/*! Cast the Interface0D in TVertex if it can be. */ 
	virtual TVertex *castToTVertex();

public:
	typedef vector<FEdge*> fedges_container;

private:
	Id _Id;
	Vec3r _Point3D;
	Vec3r _Point2D;
	set<Vec3r> _Normals; 
	vector<FEdge*> _FEdges; // the edges containing this vertex
	SShape *_Shape;  // the shape to which belongs the vertex
	ViewVertex *_pViewVertex; // The associated viewvertex, in case there is one.
	real _curvatureFredo;
	Vec2r _directionFredo;
	CurvatureInfo *_curvature_info;

public:
	/*! A field that can be used by the user to store any data.
	 *  This field must be reseted afterwards using ResetUserData().
	 */
	void *userdata;

	/*! Default constructor.*/
	inline SVertex()
	{
		_Id = 0;
		userdata = NULL;
		_Shape = NULL;
		_pViewVertex = 0;
		_curvature_info = 0;
	}

	/*! Builds a SVertex from 3D coordinates and an Id. */
	inline SVertex(const Vec3r &iPoint3D, const Id& id)
	{
		_Point3D = iPoint3D;
		_Id = id;
		userdata = NULL;
		_Shape = NULL;
		_pViewVertex = 0;
		_curvature_info = 0;
	}

	/*! Copy constructor. */
	inline SVertex(SVertex& iBrother)
	{
		_Id = iBrother._Id;
		_Point3D =  iBrother.point3D();
		_Point2D = iBrother.point2D();
		_Normals = iBrother._Normals;
		_FEdges = iBrother.fedges();
		_Shape = iBrother.shape();
		_pViewVertex = iBrother._pViewVertex;
		if (!(iBrother._curvature_info))
			_curvature_info = 0;
		else
			_curvature_info = new CurvatureInfo(*(iBrother._curvature_info));
		iBrother.userdata = this;
		userdata = 0;
	}

	/*! Destructor. */
	virtual ~SVertex()
	{
		if (_curvature_info)
			delete _curvature_info;
	}

	/*! Cloning method. */
	virtual SVertex *duplicate()
	{
		SVertex *clone = new SVertex(*this);
		return clone;
	}

	/*! operator == */
	virtual bool operator==(const SVertex& iBrother)
	{
		return ((_Point2D == iBrother._Point2D) && (_Point3D == iBrother._Point3D));
	}

	/* accessors */
	inline const Vec3r& point3D() const
	{
		return _Point3D;
	}

	inline const Vec3r& point2D() const
	{
		return _Point2D;
	}

	/*! Returns the set of normals for this Vertex.
	 *  In a smooth surface, a vertex has exactly one normal.
	 *  In a sharp surface, a vertex can have any number of normals.
	 */
	inline set<Vec3r> normals()
	{
		return _Normals;
	}

	/*! Returns the number of different normals for this vertex. */
	inline unsigned normalsSize() const
	{
		return _Normals.size();
	}

	inline const vector<FEdge*>& fedges()
	{
		return _FEdges;
	}

	inline fedges_container::iterator fedges_begin()
	{
		return _FEdges.begin();
	}

	inline fedges_container::iterator fedges_end()
	{
		return _FEdges.end();
	}

	inline SShape *shape()
	{
		return _Shape;
	}

	inline real z() const
	{
		return _Point2D[2];
	}

	/*! If this SVertex is also a ViewVertex, this method returns a pointer onto this ViewVertex.
	 *  0 is returned otherwise.
	 */
	inline ViewVertex *viewvertex()
	{
		return _pViewVertex;
	}

	/*! modifiers */
	/*! Sets the 3D coordinates of the SVertex. */
	inline void setPoint3D(const Vec3r &iPoint3D)
	{
		_Point3D = iPoint3D;
	}

	/*! Sets the 3D projected coordinates of the SVertex. */
	inline void setPoint2D(const Vec3r &iPoint2D)
	{
		_Point2D = iPoint2D;
	}

	/*! Adds a normal to the Svertex's set of normals. If the same normal is already in the set, nothing changes. */
	inline void AddNormal(const Vec3r& iNormal)
	{
		_Normals.insert(iNormal); // if iNormal in the set already exists, nothing is done
	}

	void setCurvatureInfo(CurvatureInfo *ci)
	{
		if (_curvature_info) // Q. is this an error condition? (T.K. 02-May-2011)
			delete _curvature_info;
		_curvature_info = ci;
	}

	const CurvatureInfo *getCurvatureInfo() const
	{
		return _curvature_info;
	}

	/* Fredo's normal and curvature*/
	void setCurvatureFredo(real c)
	{
		_curvatureFredo = c;
	}

	void setDirectionFredo(Vec2r d)
	{
		_directionFredo = d;
	}

	real curvatureFredo ()
	{
		return _curvatureFredo;
	}

	const Vec2r directionFredo ()
	{
		return _directionFredo;
	}

	/*! Sets the Id */
	inline void setId(const Id& id)
	{
		_Id = id;
	}

	inline void setFEdges(const vector<FEdge*>& iFEdges)
	{
		_FEdges = iFEdges;
	}

	inline void setShape(SShape *iShape)
	{
		_Shape = iShape;
	}

	inline void setViewVertex(ViewVertex *iViewVertex)
	{
		_pViewVertex = iViewVertex;
	}

	/*! Add an FEdge to the list of edges emanating from this SVertex. */
	inline void AddFEdge(FEdge *iFEdge)
	{
		_FEdges.push_back(iFEdge);
	}

	/* replaces edge 1 by edge 2 in the list of edges */
	inline void Replace(FEdge *e1, FEdge *e2)
	{
		vector<FEdge *>::iterator insertedfe;
		for (vector<FEdge *>::iterator fe = _FEdges.begin(), fend = _FEdges.end(); fe != fend; fe++) {
			if ((*fe) == e1) {
				insertedfe = _FEdges.insert(fe, e2);// inserts e2 before fe.
				// returns an iterator pointing toward e2. fe is invalidated.
				// we want to remove e1, but we can't use fe anymore:
				++insertedfe; // insertedfe points now to e1
				_FEdges.erase(insertedfe);
				return;
			}
		}
	}

public:
	/* Information access interface */
	FEdge *fedge(); // for non T vertex

	inline const Vec3r& point2d() const
	{
		return point2D();
	}

	inline const Vec3r& point3d() const
	{
		return point3D();
	}

	inline Vec3r normal() const
	{
		if (_Normals.size() == 1)
			return (*(_Normals.begin()));
		Exception::raiseException();
		return *(_Normals.begin());
	}

	//Material material() const ;
	Id shape_id() const;
	const SShape *shape() const;
	float shape_importance() const;

	const int qi() const;
	occluder_container::const_iterator occluders_begin() const;
	occluder_container::const_iterator occluders_end() const;
	bool occluders_empty() const;
	int occluders_size() const;
	const Polygon3r& occludee() const;
	const SShape *occluded_shape() const;
	const bool occludee_empty() const;
	real z_discontinuity() const;
#if 0
	inline float local_average_depth() const;
	inline float local_depth_variance() const;
	inline real local_average_density(float sigma = 2.3f) const;
	inline Vec3r shaded_color() const;
	inline Vec3r orientation2d() const;
	inline Vec3r orientation3d() const;
	inline Vec3r curvature2d_as_vector() const;
	/*! angle in radians */
	inline real curvature2d_as_angle() const;
#endif
};

/**********************************/
/*                                */
/*                                */
/*             FEdge              */
/*                                */
/*                                */
/**********************************/

class ViewEdge;

/*! Base Class for feature edges.
 *  This FEdge can represent a silhouette, a crease, a ridge/valley, a border or a suggestive contour.
 *  For silhouettes,  the FEdge is oriented such as, the visible face lies on the left of the edge.
 *  For borders, the FEdge is oriented such as, the face lies on the left of the edge.
 *  An FEdge can represent an initial edge of the mesh or runs accross a face of the initial mesh depending
 *  on the smoothness or sharpness of the mesh.
 *  This class is specialized into a smooth and a sharp version since their properties slightly vary from
 *  one to the other.
 */
class LIB_VIEW_MAP_EXPORT FEdge : public Interface1D
{
public: // Implementation of Interface0D
	/*! Returns the string "FEdge". */
	virtual string getExactTypeName() const
	{
		return "FEdge";
	}

	// Data access methods

	/*! Returns the 2D length of the FEdge. */
	virtual real getLength2D() const
	{
		if (!_VertexA || !_VertexB)
			return 0;
		return (_VertexB->getPoint2D() - _VertexA->getPoint2D()).norm();
	}

	/*! Returns the Id of the FEdge. */
	virtual Id getId() const
	{
		return _Id;
	}

public:
	// An edge can only be of one kind (SILHOUETTE or BORDER, etc...)
	// For an multi-nature edge there must be several different FEdge.
	//  DEBUG:
	//  Vec3r A;
	//  Vec3r u;
	//  vector<Polygon3r> _Occludees;
	//  Vec3r intersection;
	//  vector<Vec3i> _Cells;

protected:
	SVertex *_VertexA;
	SVertex *_VertexB;
	Id _Id;
	Nature::EdgeNature _Nature;
	//vector<Polygon3r> _Occluders; // visibility // NOT HANDLED BY THE COPY CONSTRUCTOR!!

	FEdge *_NextEdge; // next edge on the chain
	FEdge *_PreviousEdge;
	ViewEdge *_ViewEdge;
	// Sometimes we need to deport the visibility computation onto another edge. For example the exact edges use
	// edges of the mesh to compute their visibility

	Polygon3r _aFace; // The occluded face which lies on the right of a silhouette edge
	Vec3r _occludeeIntersection;
	bool _occludeeEmpty;

	bool _isSmooth;

	bool _isInImage;

public:
	/*! A field that can be used by the user to store any data.
	 *  This field must be reseted afterwards using ResetUserData().
	 */
	void *userdata;

	/*! Default constructor */
	inline FEdge()
	{
		userdata = NULL;
		_VertexA = NULL;
		_VertexB = NULL;
		_Nature = Nature::NO_FEATURE;
		_NextEdge = NULL;
		_PreviousEdge = NULL;
		_ViewEdge = NULL;
		//_hasVisibilityPoint = false;
		_occludeeEmpty = true;
		_isSmooth = false;
		_isInImage = true;
	}

	/*! Builds an FEdge going from vA to vB. */
	inline FEdge(SVertex *vA, SVertex *vB)
	{
		userdata = NULL;
		_VertexA = vA;
		_VertexB = vB;
		_Nature = Nature::NO_FEATURE;
		_NextEdge = NULL;
		_PreviousEdge = NULL;
		_ViewEdge = NULL;
		//_hasVisibilityPoint = false;
		_occludeeEmpty = true;
		_isSmooth = false;
		_isInImage = true;
	}

	/*! Copy constructor */
	inline FEdge(FEdge& iBrother)
	{
		_VertexA = iBrother.vertexA();
		_VertexB = iBrother.vertexB();
		_NextEdge = iBrother.nextEdge();
		_PreviousEdge = iBrother._PreviousEdge;
		_Nature = iBrother.getNature();
		_Id = iBrother._Id;
		_ViewEdge = iBrother._ViewEdge;
		//_hasVisibilityPoint = iBrother._hasVisibilityPoint;
		//_VisibilityPointA = iBrother._VisibilityPointA;
		//_VisibilityPointB = iBrother._VisibilityPointB;
		_aFace = iBrother._aFace;
		_occludeeEmpty = iBrother._occludeeEmpty;
		_isSmooth = iBrother._isSmooth;
		_isInImage = iBrother._isInImage;
		iBrother.userdata = this;
		userdata = 0;
	}

	/*! Destructor */
	virtual ~FEdge() {}

	/*! Cloning method. */
	virtual FEdge *duplicate()
	{
		FEdge *clone = new FEdge(*this);
		return clone;
	}

	/* accessors */
	/*! Returns the first SVertex. */
	inline SVertex *vertexA()
	{
		return _VertexA;
	}

	/*! Returns the second SVertex. */
	inline SVertex *vertexB()
	{
		return _VertexB;
	}

	/*! Returns the first SVertex if i=0, the seccond SVertex if i=1. */
	inline SVertex *operator[](const unsigned short int& i) const
	{
		return (i % 2 == 0) ? _VertexA : _VertexB;
	}

	/*! Returns the nature of the FEdge. */
	inline Nature::EdgeNature getNature() const
	{
		return _Nature;
	}

	/*! Returns the FEdge following this one in the ViewEdge.
	 *  If this FEdge is the last of the ViewEdge, 0 is returned.
	 */
	inline FEdge *nextEdge()
	{
		return _NextEdge;
	}

	/*! Returns the Edge preceding this one in the ViewEdge.
	 *  If this FEdge is the first one of the ViewEdge, 0 is returned.
	 */
	inline FEdge *previousEdge()
	{
		return _PreviousEdge;
	}

	inline SShape *shape()
	{
		return _VertexA->shape();
	}

#if 0
	inline int invisibility() const
	{
		return _Occluders.size();
	}
#endif

	int invisibility() const;

#if 0
	inline const vector<Polygon3r>& occluders() const
	{
		return _Occluders;
	}
#endif

	/*! Returns a pointer to the ViewEdge to which this FEdge belongs to. */
	inline ViewEdge *viewedge() const
	{
		return _ViewEdge;
	}

	inline Vec3r center3d()
	{
		return Vec3r((_VertexA->point3D() + _VertexB->point3D()) / 2.0);
	}

	inline Vec3r center2d()
	{
		return Vec3r((_VertexA->point2D() + _VertexB->point2D()) / 2.0);
	}

#if 0
	inline bool hasVisibilityPoint() const
	{
		return _hasVisibilityPoint;
	}

	inline Vec3r visibilityPointA() const
	{
		return _VisibilityPointA;
	}

	inline Vec3r visibilityPointB() const
	{
		return _VisibilityPointB;
	}
#endif

	inline const Polygon3r& aFace() const
	{
		return _aFace;
	}

	inline const Vec3r& getOccludeeIntersection()
	{
		return _occludeeIntersection;
	}

	inline bool getOccludeeEmpty()
	{
		return _occludeeEmpty;
	}

	/*! Returns true if this FEdge is a smooth FEdge. */
	inline bool isSmooth() const
	{
		return _isSmooth;
	}

	inline bool isInImage () const
	{
		return _isInImage;
	}

	/* modifiers */
	/*! Sets the first SVertex. */
	inline void setVertexA(SVertex *vA)
	{
		_VertexA = vA;
	}

	/*! Sets the second SVertex. */
	inline void setVertexB(SVertex *vB)
	{
		_VertexB = vB;
	}

	/*! Sets the FEdge Id . */
	inline void setId(const Id& id)
	{
		_Id = id;
	}

	/*! Sets the pointer to the next FEdge. */
	inline void setNextEdge(FEdge *iEdge)
	{
		_NextEdge = iEdge;
	}

	/*! Sets the pointer to the previous FEdge. */
	inline void setPreviousEdge(FEdge *iEdge)
	{
		_PreviousEdge = iEdge;
	}

	/*! Sets the nature of this FEdge. */
	inline void setNature(Nature::EdgeNature iNature)
	{
		_Nature = iNature;
	}

#if 0
	inline void AddOccluder(Polygon3r& iPolygon)
	{
		_Occluders.push_back(iPolygon);
	}
#endif

	/*! Sets the ViewEdge to which this FEdge belongs to. */
	inline void setViewEdge(ViewEdge *iViewEdge)
	{
		_ViewEdge = iViewEdge;
	}

#if 0
	inline void setHasVisibilityPoint(bool iBool)
	{
		_hasVisibilityPoint = iBool;
	}

	inline void setVisibilityPointA(const Vec3r& iPoint)
	{
		_VisibilityPointA = iPoint;
	}

	inline void setVisibilityPointB(const Vec3r& iPoint)
	{
		_VisibilityPointB = iPoint;
	}
#endif

	inline void setaFace(Polygon3r& iFace)
	{
		_aFace = iFace;
	}

	inline void setOccludeeIntersection(const Vec3r& iPoint)
	{
		_occludeeIntersection = iPoint;
	}

	inline void setOccludeeEmpty(bool iempty)
	{
		_occludeeEmpty = iempty;
	}

	/*! Sets the flag telling whether this FEdge is smooth or sharp.
	 *  true for Smooth, false for Sharp.
	 */
	inline void setSmooth(bool iFlag)
	{
		_isSmooth = iFlag;
	}

	inline void setIsInImage (bool iFlag)
	{
		_isInImage = iFlag;
	}

	/* checks whether two FEdge have a common vertex.
	 *  Returns a pointer on the common vertex if it exists, NULL otherwise.
	 */
	static inline SVertex *CommonVertex(FEdge *iEdge1, FEdge *iEdge2)
	{
		if ((NULL == iEdge1) || (NULL == iEdge2))
			return NULL;

		SVertex *sv1 = iEdge1->vertexA();
		SVertex *sv2 = iEdge1->vertexB();
		SVertex *sv3 = iEdge2->vertexA();
		SVertex *sv4 = iEdge2->vertexB();

		if ((sv1 == sv3) || (sv1 == sv4)) {
			return sv1;
		}
		else if ((sv2 == sv3) || (sv2 == sv4)) {
			return sv2;
		}

		return NULL;
	}

	inline const SVertex *min2d() const
	{
		if (_VertexA->point2D() < _VertexB->point2D())
			return _VertexA;
		else
			return _VertexB;
	}

	inline const SVertex *max2d() const
	{
		if (_VertexA->point2D() < _VertexB->point2D())
			return _VertexB;
		else
			return _VertexA;
	}

	/* Information access interface */

	//Material material() const;
	Id shape_id() const;
	const SShape *shape() const;
	float shape_importance() const;

	inline const int qi() const
	{
		return invisibility();
	}

	occluder_container::const_iterator occluders_begin() const;
	occluder_container::const_iterator occluders_end() const;
	bool occluders_empty() const;
	int occluders_size() const;

	inline const Polygon3r& occludee() const
	{
		return aFace();
	}

	const SShape *occluded_shape() const;

#if 0
	inline const bool  occludee_empty() const
	{
		return _occludeeEmpty;
	}
#endif

	const bool  occludee_empty() const;
	real z_discontinuity() const;

#if 0
	inline float local_average_depth(int iCombination = 0) const;
	inline float local_depth_variance(int iCombination = 0) const;
	inline real local_average_density(float sigma = 2.3f, int iCombination = 0) const;
	inline Vec3r shaded_color(int iCombination = 0) const {}
#endif

	int viewedge_nature() const;

	//float viewedge_length() const;

	inline Vec3r orientation2d() const
	{
		return Vec3r(_VertexB->point2d() - _VertexA->point2d());
	}

	inline Vec3r orientation3d() const
	{
		return Vec3r(_VertexB->point3d() - _VertexA->point3d());
	}

#if 0
	inline real curvature2d() const
	{
		return viewedge()->curvature2d((_VertexA->point2d() + _VertexB->point2d()) / 2.0);
	}

	inline Vec3r curvature2d_as_vector(int iCombination = 0) const;

	/* angle in degrees*/
	inline real curvature2d_as_angle(int iCombination = 0) const;
#endif

	// Iterator access (Interface1D)
	/*! Returns an iterator over the 2 (!) SVertex pointing to the first SVertex. */
	virtual inline Interface0DIterator verticesBegin();

	/*! Returns an iterator over the 2 (!) SVertex pointing after the last SVertex. */
	virtual inline Interface0DIterator verticesEnd();

	/*! Returns an iterator over the FEdge points, pointing to the first point. The difference with verticesBegin()
	 *  is that here we can iterate over points of the FEdge at a any given sampling.
	 *  Indeed, for each iteration, a virtual point is created.
	 *  \param t
	 *    The sampling with which we want to iterate over points of this FEdge.
	 */
	virtual inline Interface0DIterator pointsBegin(float t = 0.0f);

	/*! Returns an iterator over the FEdge points, pointing after the last point. The difference with verticesEnd()
	 * is that here we can iterate over points of the FEdge at a any given sampling.
	 *  Indeed, for each iteration, a virtual point is created.
	 *  \param t
	 *    The sampling with which we want to iterate over points of this FEdge.
	 */
	virtual inline Interface0DIterator pointsEnd(float t = 0.0f);
};

//
// SVertexIterator
//
/////////////////////////////////////////////////

namespace FEdgeInternal {

class SVertexIterator : public Interface0DIteratorNested
{
public:
	SVertexIterator()
	{
		_vertex = NULL;
		_edge = NULL;
	}

	SVertexIterator(const SVertexIterator& vi)
	{
		_vertex = vi._vertex;
		_edge = vi._edge;
	}

	SVertexIterator(SVertex *v, FEdge *edge)
	{
		_vertex = v;
		_edge = edge;
	}

	SVertexIterator& operator=(const SVertexIterator& vi)
	{
		_vertex = vi._vertex;
		_edge = vi._edge;
		return *this;
	}

	virtual string getExactTypeName() const
	{
		return "SVertexIterator";
	}

	virtual SVertex& operator*()
	{
		return *_vertex;
	}

	virtual SVertex *operator->()
	{
		return &(operator*());
	}

	virtual SVertexIterator& operator++()
	{
		increment();
		return *this;
	}

	virtual SVertexIterator operator++(int)
	{
		SVertexIterator ret(*this);
		increment();
		return ret;
	}

	virtual SVertexIterator& operator--()
	{
		decrement();
		return *this;
	}

	virtual SVertexIterator operator--(int)
	{
		SVertexIterator ret(*this);
		decrement();
		return ret;
	}

	virtual int increment()
	{
		if (_vertex == _edge->vertexB()) {
			_vertex = 0;
			return 0;
		}
		_vertex = _edge->vertexB();
		return 0;
	}

	virtual int decrement()
	{
		if (_vertex == _edge->vertexA()) {
			_vertex = 0;
			return 0;
		}
		_vertex = _edge->vertexA();
		return 0;
	}

	virtual bool isBegin() const
	{
		return _vertex == _edge->vertexA();
	}

	virtual bool isEnd() const
	{
		return _vertex == _edge->vertexB();
	}

	virtual bool operator==(const Interface0DIteratorNested& it) const
	{
		const SVertexIterator *it_exact = dynamic_cast<const SVertexIterator*>(&it);
		if (!it_exact)
			return false;
		return ((_vertex == it_exact->_vertex) && (_edge == it_exact->_edge));
	}

	virtual float t() const
	{
		if (_vertex == _edge->vertexA()) {
			return 0.0f;
		}
		return ((float)_edge->getLength2D());
	}
	virtual float u() const
	{
		if (_vertex == _edge->vertexA()) {
			return 0.0f;
		}
		return 1.0f;
	}

	virtual SVertexIterator *copy() const
	{
		return new SVertexIterator(*this);
	}

private:
	SVertex *_vertex;
	FEdge *_edge;
};

} // end of namespace FEdgeInternal

// Iterator access (implementation)

Interface0DIterator FEdge::verticesBegin()
{
	Interface0DIterator ret(new FEdgeInternal::SVertexIterator(_VertexA, this));
	return ret;
}

Interface0DIterator FEdge::verticesEnd()
{
	Interface0DIterator ret(new FEdgeInternal::SVertexIterator(0, this));
	return ret;
}

Interface0DIterator FEdge::pointsBegin(float t)
{
	return verticesBegin();
}

Interface0DIterator FEdge::pointsEnd(float t)
{
	return verticesEnd();
}

/*! Class defining a sharp FEdge. A Sharp FEdge corresponds to an initial edge of the input mesh.
 *  It can be a silhouette, a crease or a border. If it is a crease edge, then it is borded
 *  by two faces of the mesh. Face a lies on its right whereas Face b lies on its left.
 *  If it is a border edge, then it doesn't have any face on its right, and thus Face a = 0.
 */
class LIB_VIEW_MAP_EXPORT FEdgeSharp : public FEdge
{
protected:
	Vec3r _aNormal; // When following the edge, normal of the right face
	Vec3r _bNormal; // When following the edge, normal of the left face
	unsigned _aFrsMaterialIndex;
	unsigned _bFrsMaterialIndex;
	bool _aFaceMark;
	bool _bFaceMark;

public:
	/*! Returns the string "FEdgeSharp" . */
	virtual string getExactTypeName() const
	{
		return "FEdgeSharp";
	}

	/*! Default constructor. */
	inline FEdgeSharp() : FEdge()
	{
		_aFrsMaterialIndex = _bFrsMaterialIndex = 0;
		_aFaceMark = _bFaceMark = false;
	}

	/*! Builds an FEdgeSharp going from vA to vB. */
	inline FEdgeSharp(SVertex *vA, SVertex *vB) : FEdge(vA, vB)
	{
		_aFrsMaterialIndex = _bFrsMaterialIndex = 0;
		_aFaceMark = _bFaceMark = false;
	}

	/*! Copy constructor. */
	inline FEdgeSharp(FEdgeSharp& iBrother) : FEdge(iBrother)
	{
		_aNormal = iBrother._aNormal;
		_bNormal = iBrother._bNormal;
		_aFrsMaterialIndex = iBrother._aFrsMaterialIndex;
		_bFrsMaterialIndex = iBrother._bFrsMaterialIndex;
		_aFaceMark = iBrother._aFaceMark;
		_bFaceMark = iBrother._bFaceMark;
	}

	/*! Destructor. */
	virtual ~FEdgeSharp() {}

	/*! Cloning method. */
	virtual FEdge *duplicate()
	{
		FEdge *clone = new FEdgeSharp(*this);
		return clone;
	}

	/*! Returns the normal to the face lying on the right of the FEdge. If this FEdge is a border,
	 *  it has no Face on its right and therefore, no normal.
	 */
	inline const Vec3r& normalA()
	{
		return _aNormal;
	}

	/*! Returns the normal to the face lying on the left of the FEdge. */
	inline const Vec3r& normalB()
	{
		return _bNormal;
	}

	/*! Returns the index of the material of the face lying on the
	*  right of the FEdge. If this FEdge is a border,
	*  it has no Face on its right and therefore, no material.
	*/
	inline unsigned aFrsMaterialIndex() const
	{
		return _aFrsMaterialIndex;
	}

	/*! Returns the material of the face lying on the right of the FEdge. If this FEdge is a border,
	 *  it has no Face on its right and therefore, no material.
	 */
	const FrsMaterial& aFrsMaterial() const;

	/*! Returns the index of the material of the face lying on the left of the FEdge. */
	inline unsigned bFrsMaterialIndex() const
	{
		return _bFrsMaterialIndex;
	}

	/*! Returns the  material of the face lying on the left of the FEdge. */
	const FrsMaterial& bFrsMaterial() const;

	/*! Returns the face mark of the face lying on the right of the FEdge.
	 *  If this FEdge is a border, it has no Face on its right and thus false is returned.
	 */
	inline bool aFaceMark() const
	{
		return _aFaceMark;
	}

	/*! Returns the face mark of the face lying on the left of the FEdge. */
	inline bool bFaceMark() const
	{
		return _bFaceMark;
	}

	/*! Sets the normal to the face lying on the right of the FEdge. */
	inline void setNormalA(const Vec3r& iNormal)
	{
		_aNormal = iNormal;
	}

	/*! Sets the normal to the face lying on the left of the FEdge. */
	inline void setNormalB(const Vec3r& iNormal)
	{
		_bNormal = iNormal;
	}

	/*! Sets the index of the material lying on the right of the FEdge.*/
	inline void setaFrsMaterialIndex(unsigned i)
	{
		_aFrsMaterialIndex = i;
	}

	/*! Sets the index of the material lying on the left of the FEdge.*/
	inline void setbFrsMaterialIndex(unsigned i)
	{
		_bFrsMaterialIndex = i;
	}

	/*! Sets the face mark of the face lying on the right of the FEdge. */
	inline void setaFaceMark(bool iFaceMark)
	{
		_aFaceMark = iFaceMark;
	}

	/*! Sets the face mark of the face lying on the left of the FEdge. */
	inline void setbFaceMark(bool iFaceMark)
	{
		_bFaceMark = iFaceMark;
	}
};

/*! Class defining a smooth edge. This kind of edge typically runs across a face of the input mesh. It can be
 *  a silhouette, a ridge or valley, a suggestive contour.
 */
class LIB_VIEW_MAP_EXPORT FEdgeSmooth : public FEdge
{
protected:
	Vec3r _Normal;
	unsigned _FrsMaterialIndex;
#if 0
	bool _hasVisibilityPoint;
	Vec3r _VisibilityPointA;  // The edge on which the visibility will be computed represented 
	Vec3r _VisibilityPointB;  // using its 2 extremity points A and B
#endif
	void *_Face; // In case of exact silhouette, Face is the WFace crossed by Fedge 
	              // NOT HANDLED BY THE COPY CONSTRUCTEUR
	bool _FaceMark;

public:
	/*! Returns the string "FEdgeSmooth" . */
	virtual string getExactTypeName() const
	{
		return "FEdgeSmooth";
	}

	/*! Default constructor. */
	inline FEdgeSmooth() : FEdge()
	{
		_Face = NULL;
		_FaceMark = false;
		_FrsMaterialIndex = 0;
		_isSmooth = true;
	}

	/*! Builds an FEdgeSmooth going from vA to vB. */
	inline FEdgeSmooth(SVertex *vA, SVertex *vB) : FEdge(vA, vB)
	{
		_Face = NULL;
		_FaceMark = false;
		_FrsMaterialIndex = 0;
		_isSmooth = true;
	}

	/*! Copy constructor. */
	inline FEdgeSmooth(FEdgeSmooth& iBrother) : FEdge(iBrother)
	{
		_Normal = iBrother._Normal;
		_Face = iBrother._Face;
		_FaceMark = iBrother._FaceMark;
		_FrsMaterialIndex = iBrother._FrsMaterialIndex;
		_isSmooth = true;
	}

	/*! Destructor. */
	virtual ~FEdgeSmooth() {}

	/*! Cloning method. */
	virtual FEdge *duplicate()
	{
		FEdge *clone = new FEdgeSmooth(*this);
		return clone;
	}

	inline void *face() const
	{
		return _Face;
	}

	/*! Returns the face mark of the face it is running across. */
	inline bool faceMark() const
	{
		return _FaceMark;
	}

	/*! Returns the normal to the Face it is running accross. */
	inline const Vec3r& normal()
	{
		return _Normal;
	}

	/*! Returns the index of the material of the face it is running accross. */
	inline unsigned frs_materialIndex() const
	{
		return _FrsMaterialIndex;
	}

	/*! Returns the material of the face it is running accross. */
	const FrsMaterial& frs_material() const;

	inline void setFace(void *iFace)
	{
		_Face = iFace;
	}

	/*! Sets the face mark of the face it is running across. */
	inline void setFaceMark(bool iFaceMark)
	{
		_FaceMark = iFaceMark;
	}

	/*! Sets the normal to the Face it is running accross. */
	inline void setNormal(const Vec3r& iNormal)
	{
		_Normal = iNormal;
	}

	/*! Sets the index of the material of the face it is running accross. */
	inline void setFrsMaterialIndex(unsigned i)
	{
		_FrsMaterialIndex = i;
	}
};


/**********************************/
/*                                */
/*                                */
/*             SShape             */
/*                                */
/*                                */
/**********************************/


/*! Class to define a feature shape. It is the gathering of feature elements from an identified input shape */
class LIB_VIEW_MAP_EXPORT SShape
{
private:
	vector<FEdge*> _chains;          // list of fedges that are chains starting points.
	vector<SVertex*> _verticesList;  // list of all vertices
	vector<FEdge*> _edgesList;       // list of all edges
	Id _Id;
	string _Name;
	BBox<Vec3r> _BBox;
	vector<FrsMaterial> _FrsMaterials;  

	float _importance;

	ViewShape *_ViewShape;

public:
	/*! A field that can be used by the user to store any data.
	 *  This field must be reseted afterwards using ResetUserData().
	 */
	void *userdata; // added by E.T.

	/*! Default constructor */
	inline SShape()
	{
		userdata = NULL;
		_importance = 0.0f;
		_ViewShape = NULL;
	}

	/*! Copy constructor */
	inline SShape(SShape& iBrother)
	{
		userdata = NULL;
		_Id = iBrother._Id;
		_Name = iBrother._Name;
		_BBox = iBrother.bbox();
		_FrsMaterials = iBrother._FrsMaterials;
		_importance = iBrother._importance;
		_ViewShape = iBrother._ViewShape;

		//---------
		// vertices
		//---------
		vector<SVertex*>::iterator sv, svend;
		vector<SVertex*>& verticesList = iBrother.getVertexList();
		for (sv = verticesList.begin(), svend = verticesList.end(); sv != svend; sv++) {
			SVertex *newv = new SVertex(*(*sv));
			newv->setShape(this);
			_verticesList.push_back(newv);
		}

		//------
		// edges
		//------
		vector<FEdge*>::iterator e, eend;
		vector<FEdge*>& edgesList = iBrother.getEdgeList();
		for (e = edgesList.begin(), eend = edgesList.end(); e != eend; e++) {
			FEdge *newe = (*e)->duplicate();
			_edgesList.push_back(newe);
		}

		//-------------------------
		// starting chain edges
		//-------------------------
		vector<FEdge*>::iterator fe, fend;
		vector<FEdge*>& fedges = iBrother.getChains();
		for (fe = fedges.begin(), fend = fedges.end(); fe != fend; fe++) {
			_chains.push_back((FEdge *)((*fe)->userdata));
		}

		//-------------------------
		// remap edges in vertices:
		//-------------------------
		for (sv = _verticesList.begin(), svend = _verticesList.end(); sv != svend; sv++) {
			const vector<FEdge*>& fedgeList = (*sv)->fedges();
			vector<FEdge*> newfedgelist;
			for (vector<FEdge*>::const_iterator fed = fedgeList.begin(), fedend = fedgeList.end();
			     fed != fedend;
			     fed++)
			{
				FEdge *current = *fed;
				newfedgelist.push_back((FEdge *)current->userdata);
			}
			(*sv)->setFEdges(newfedgelist);
		}

		//-------------------------------------
		// remap vertices and nextedge in edges:
		//-------------------------------------
		for (e = _edgesList.begin(), eend = _edgesList.end(); e != eend; e++) {
			(*e)->setVertexA((SVertex *)((*e)->vertexA()->userdata));
			(*e)->setVertexB((SVertex *)((*e)->vertexB()->userdata));
			(*e)->setNextEdge((FEdge *)((*e)->nextEdge()->userdata));
			(*e)->setPreviousEdge((FEdge *)((*e)->previousEdge()->userdata));
		}

		// reset all brothers userdata to NULL:
		//-------------------------------------
		//---------
		// vertices
		//---------
		for (sv = _verticesList.begin(), svend = _verticesList.end(); sv != svend; sv++) {
			(*sv)->userdata = NULL;
		}

		//------
		// edges
		//------
		for (e = _edgesList.begin(), eend = _edgesList.end(); e != eend; e++) {
			(*e)->userdata = NULL;
		}
	}

	/*! Cloning method. */
	virtual SShape *duplicate()
	{
		SShape *clone = new SShape(*this);
		return clone;
	}

	/*! Destructor. */
	virtual inline ~SShape()
	{
		vector<SVertex*>::iterator sv, svend;
		vector<FEdge*>::iterator e, eend;
		if (0 != _verticesList.size()) {
			for (sv = _verticesList.begin(), svend = _verticesList.end(); sv != svend; sv++) {
				delete (*sv);
			}
			_verticesList.clear();
		}

		if (0 != _edgesList.size()) {
			for (e = _edgesList.begin(), eend = _edgesList.end(); e != eend; e++) {
				delete (*e);
			}
			_edgesList.clear();
		}

		//! Clear the chains list
		//-----------------------
		if (0 != _chains.size()) {
			_chains.clear();
		}
	}

	/*! Adds a FEdge to the list of FEdges. */
	inline void AddEdge(FEdge *iEdge)
	{
		_edgesList.push_back(iEdge);
	}

	/*! Adds a SVertex to the list of SVertex of this Shape.
	 * The SShape attribute of the SVertex is also set to 'this'.
	 */
	inline void AddNewVertex(SVertex *iv)
	{
		iv->setShape(this);
		_verticesList.push_back(iv);
	}

	inline void AddChain(FEdge *iEdge)
	{
		_chains.push_back(iEdge);
	}

	inline SVertex *CreateSVertex(const Vec3r& P3D, const Vec3r& P2D, const Id& id)
	{
		SVertex *Ia = new SVertex(P3D, id);
		Ia->setPoint2D(P2D);
		AddNewVertex(Ia);
		return Ia;
	}

	/*! Splits an edge into several edges.
	 *  The edge's vertices are passed rather than the edge itself. This way, all feature edges (SILHOUETTE,
	 *  CREASE, BORDER) are splitted in the same time.
	 *  The processed edges are flagged as done (using the userdata flag).One single new vertex is created whereas
	 *  several splitted edges might created for the different kinds of edges. These new elements are added to the lists
	 *  maintained by the shape.
	 *  New chains are also created.
	 *    ioA
	 *      The first vertex for the edge that gets splitted
	 *    ioB
	 *      The second vertex for the edge that gets splitted
	 *    iParameters
	 *      A vector containing 2D real vectors indicating the parameters giving the intersections coordinates in
	 *      3D and in 2D. These intersections points must be sorted from B to A.
	 *      Each parameter defines the intersection point I as I=A+T*AB. T<0 and T>1 are then incorrect insofar as
	 *      they give intersections points that lie outside the segment.
	 *    ioNewEdges
	 *      The edges that are newly created (the initial edges are not included) are added to this list.
	 */
	inline void SplitEdge(FEdge *fe, const vector<Vec2r>& iParameters, vector<FEdge*>& ioNewEdges)
	{
		SVertex *ioA = fe->vertexA();
		SVertex *ioB = fe->vertexB();
		Vec3r A = ioA->point3D();
		Vec3r B = ioB->point3D();
		Vec3r a = ioA->point2D();
		Vec3r b = ioB->point2D();

		Vec3r newpoint3d, newpoint2d;
		vector<SVertex*> intersections;
		real t, T;
		for (vector<Vec2r>::const_iterator p = iParameters.begin(), pend = iParameters.end(); p != pend; p++) {
			T = (*p)[0];
			t = (*p)[1];

			if ((t < 0) || (t > 1))
				cerr << "Warning: Intersection out of range for edge " << ioA->getId() << " - " << ioB->getId() << endl;

			// compute the 3D and 2D coordinates for the intersections points:
			newpoint3d = Vec3r(A + T * (B - A));
			newpoint2d = Vec3r(a + t * (b - a));

			// create new SVertex:
			// (we keep B's id)
			SVertex *newVertex = new SVertex(newpoint3d, ioB->getId());
			newVertex->setPoint2D(newpoint2d);

			// Add this vertex to the intersections list:
			intersections.push_back(newVertex);

			// Add this vertex to this sshape:
			AddNewVertex(newVertex);
		}

		for (vector<SVertex*>::iterator sv = intersections.begin(), svend = intersections.end(); sv != svend; sv++) {
			//SVertex *svA = fe->vertexA();
			SVertex *svB = fe->vertexB();

			// We split edge AB into AA' and A'B. A' and A'B are created.
			// AB becomes (address speaking) AA'. B is updated.
			//--------------------------------------------------
			// The edge AB becomes edge AA'.
			(fe)->setVertexB((*sv));
			// a new edge, A'B is created.
			FEdge *newEdge;
			if (fe->isSmooth()) {
				newEdge = new FEdgeSmooth((*sv), svB);
				FEdgeSmooth *se = dynamic_cast<FEdgeSmooth*>(newEdge);
				FEdgeSmooth *fes = dynamic_cast<FEdgeSmooth*>(fe);
				se->setFrsMaterialIndex(fes->frs_materialIndex());
			}
			else {
				newEdge = new FEdgeSharp((*sv), svB);
				FEdgeSharp *se = dynamic_cast<FEdgeSharp*>(newEdge);
				FEdgeSharp *fes = dynamic_cast<FEdgeSharp*>(fe);
				se->setaFrsMaterialIndex(fes->aFrsMaterialIndex());
				se->setbFrsMaterialIndex(fes->bFrsMaterialIndex());
			}

			newEdge->setNature((fe)->getNature());

			// to build a new chain:
			AddChain(newEdge);
			// add the new edge to the sshape edges list.
			AddEdge(newEdge);
			// add new edge to the list of new edges passed as argument:
			ioNewEdges.push_back(newEdge);

			// update edge A'B for the next pointing edge
			newEdge->setNextEdge((fe)->nextEdge());
			fe->nextEdge()->setPreviousEdge(newEdge);
			Id id(fe->getId().getFirst(), fe->getId().getSecond() + 1);
			newEdge->setId(fe->getId());
			fe->setId(id);

			// update edge AA' for the next pointing edge
			//ioEdge->setNextEdge(newEdge);
			(fe)->setNextEdge(NULL);

			// update vertex pointing edges list:
			// -- vertex B --
			svB->Replace((fe), newEdge);
			// -- vertex A' --
			(*sv)->AddFEdge((fe));
			(*sv)->AddFEdge(newEdge);
		}
	}

	/* splits an edge into 2 edges. The new vertex and edge are added to the sshape list of vertices and edges
	 *  a new chain is also created.
	 *  returns the new edge.
	 *    ioEdge
	 *      The edge that gets splitted
	 *    newpoint
	 *      x,y,z coordinates of the new point.
	 */
	inline FEdge *SplitEdgeIn2(FEdge *ioEdge, SVertex *ioNewVertex)
	{
		//soc unused - SVertex *A = ioEdge->vertexA();
		SVertex *B = ioEdge->vertexB();

		// We split edge AB into AA' and A'B. A' and A'B are created.
		// AB becomes (address speaking) AA'. B is updated.
		//--------------------------------------------------
		// a new edge, A'B is created.
		FEdge *newEdge;
		if (ioEdge->isSmooth()) {
			newEdge = new FEdgeSmooth(ioNewVertex, B);
			FEdgeSmooth *se = dynamic_cast<FEdgeSmooth*>(newEdge);
			FEdgeSmooth *fes = dynamic_cast<FEdgeSmooth*>(ioEdge);
			se->setNormal(fes->normal());
			se->setFrsMaterialIndex(fes->frs_materialIndex());
			se->setFaceMark(fes->faceMark());
		}
		else {
			newEdge = new FEdgeSharp(ioNewVertex, B);
			FEdgeSharp *se = dynamic_cast<FEdgeSharp*>(newEdge);
			FEdgeSharp *fes = dynamic_cast<FEdgeSharp*>(ioEdge);
			se->setNormalA(fes->normalA());
			se->setNormalB(fes->normalB());
			se->setaFrsMaterialIndex(fes->aFrsMaterialIndex());
			se->setbFrsMaterialIndex(fes->bFrsMaterialIndex());
			se->setaFaceMark(fes->aFaceMark());
			se->setbFaceMark(fes->bFaceMark());
		}
		newEdge->setNature(ioEdge->getNature());

		if (ioEdge->nextEdge() != 0)
			ioEdge->nextEdge()->setPreviousEdge(newEdge);

		// update edge A'B for the next pointing edge
		newEdge->setNextEdge(ioEdge->nextEdge());
		// update edge A'B for the previous pointing edge
		newEdge->setPreviousEdge(0); // because it is now a TVertex
		Id id(ioEdge->getId().getFirst(), ioEdge->getId().getSecond() + 1);
		newEdge->setId(ioEdge->getId());
		ioEdge->setId(id);

		// update edge AA' for the next pointing edge
		ioEdge->setNextEdge(0); // because it is now a TVertex

		// update vertex pointing edges list:
		// -- vertex B --
		B->Replace(ioEdge, newEdge);
		// -- vertex A' --
		ioNewVertex->AddFEdge(ioEdge);
		ioNewVertex->AddFEdge(newEdge);

		// to build a new chain:
		AddChain(newEdge);
		AddEdge(newEdge); // FIXME ??

		// The edge AB becomes edge AA'.
		ioEdge->setVertexB(ioNewVertex);

		if (ioEdge->isSmooth()) {
			((FEdgeSmooth *)newEdge)->setFace(((FEdgeSmooth *)ioEdge)->face());
		}

		return newEdge;
	}

	/*! Sets the Bounding Box of the Shape */
	inline void setBBox(const BBox<Vec3r>& iBBox)
	{
		_BBox = iBBox;
	}

	/*! Compute the bbox of the sshape */
	inline void ComputeBBox()
	{
		if (0 == _verticesList.size())
			return;

		Vec3r firstVertex = _verticesList[0]->point3D();
		real XMax = firstVertex[0];
		real YMax = firstVertex[1];
		real ZMax = firstVertex[2];

		real XMin = firstVertex[0];
		real YMin = firstVertex[1];
		real ZMin = firstVertex[2];

		vector<SVertex*>::iterator v, vend;
		// parse all the coordinates to find the Xmax, YMax, ZMax
		for (v = _verticesList.begin(), vend = _verticesList.end(); v != vend; v++) {
			Vec3r vertex = (*v)->point3D();
			// X
			real x = vertex[0];
			if (x > XMax)
				XMax = x;
			else if (x < XMin)
				XMin = x;

			// Y
			real y = vertex[1];
			if (y > YMax)
				YMax = y;
			else if (y < YMin)
				YMin = y;

			// Z
			real z = vertex[2];
			if (z > ZMax)
				ZMax = z;
			else if (z < ZMin)
				ZMin = z;
		}

		setBBox(BBox<Vec3r>(Vec3r(XMin, YMin, ZMin), Vec3r(XMax, YMax, ZMax)));
	}

	inline void RemoveEdgeFromChain(FEdge *iEdge)
	{
		for (vector<FEdge*>::iterator fe = _chains.begin(), feend = _chains.end(); fe != feend; fe++) {
			if (iEdge == (*fe)) {
				_chains.erase(fe);
				break;
			}
		}
	}

	inline void RemoveEdge(FEdge *iEdge)
	{
		for (vector<FEdge*>::iterator fe = _edgesList.begin(), feend = _edgesList.end(); fe != feend; fe++) {
			if (iEdge == (*fe)) {
				_edgesList.erase(fe);
				break;
			}
		}
	}

	/* accessors */
	/*! Returns the list of SVertex of the Shape. */
	inline vector<SVertex*>& getVertexList()
	{
		return _verticesList;
	}

	/*! Returns the list of FEdges of the Shape. */
	inline vector<FEdge*>& getEdgeList()
	{
		return _edgesList;
	}

	inline vector<FEdge*>& getChains()
	{
		return _chains;
	}

	/*! Returns the bounding box of the shape. */
	inline const BBox<Vec3r>& bbox()
	{
		return _BBox;
	}

	/*! Returns the ith material of the shape. */
	inline const FrsMaterial& frs_material(unsigned i) const
	{
		return _FrsMaterials[i];
	}

	/*! Returns the list of materials of the Shape. */
	inline const vector<FrsMaterial>& frs_materials() const
	{
		return _FrsMaterials;
	}

	inline ViewShape *viewShape()
	{
		return _ViewShape;
	}

	inline float importance() const
	{
		return _importance;
	}

	/*! Returns the Id of the Shape. */
	inline Id getId() const
	{
		return _Id;
	}

	/*! Returns the name of the Shape. */
	inline const string& getName() const
	{
		return _Name;
	}

	/* Modififers */
	/*! Sets the Id of the shape.*/
	inline void setId(Id id)
	{
		_Id = id;
	}

	/*! Sets the name of the shape.*/
	inline void setName(const string& name)
	{
		_Name = name;
	}

	/*! Sets the list of materials for the shape */
	inline void setFrsMaterials(const vector<FrsMaterial>& iMaterials)
	{
		_FrsMaterials = iMaterials;
	}

	inline void setViewShape(ViewShape *iShape)
	{
		_ViewShape = iShape;
	}

	inline void setImportance(float importance)
	{
		_importance = importance;
	}

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:SShape")
#endif
};

} /* namespace Freestyle */

#endif // __FREESTYLE_SILHOUETTE_H__
