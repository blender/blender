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

#ifndef __FREESTYLE_W_EDGE_H__
#define __FREESTYLE_W_EDGE_H__

/** \file blender/freestyle/intern/winged_edge/WEdge.h
 *  \ingroup freestyle
 *  \brief Classes to define a Winged Edge data structure.
 *  \author Stephane Grabli
 *  \date 18/02/2002
 */

#include <iterator>
#include <vector>

#include "../geometry/Geom.h"

#include "../scene_graph/FrsMaterial.h"

#include "../system/FreestyleConfig.h"

#include "BLI_math.h"

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

using namespace std;

namespace Freestyle {

using namespace Geometry;


/**********************************
 *                                *
 *                                *
 *             WVertex            *
 *                                *
 *                                *
 **********************************/


class WOEdge;
class WEdge;
class WShape;
class WFace;

class WVertex
{
protected:
	int _Id; // an identificator
	Vec3r _Vertex;
	vector<WEdge*> _EdgeList;
	WShape *_Shape; // the shape to which the vertex belongs
	bool _Smooth; // flag to indicate whether the Vertex belongs to a smooth edge or not
	int _Border; // 1 -> border, 0 -> no border, -1 -> not set

public:
	void *userdata; // designed to store specific user data
	inline WVertex(const Vec3r &v)
	{
		_Id = 0;
		_Vertex = v;
		userdata = NULL;
		_Shape = NULL;
		_Smooth = true;
		_Border = -1;
	}

	/*! Copy constructor */
	WVertex(WVertex& iBrother);
	virtual WVertex *duplicate();
	virtual ~WVertex() {}

	/*! accessors */
	inline Vec3r& GetVertex()
	{
		return _Vertex;
	}

	inline vector<WEdge*>& GetEdges()
	{
		return _EdgeList;
	}

	inline int GetId()
	{
		return _Id;
	}

	inline WShape *shape() const
	{
		return _Shape;
	}

	inline bool isSmooth() const
	{
		return _Smooth;
	}

	bool isBoundary();

	/*! modifiers */
	inline void setVertex(const Vec3r& v)
	{
		_Vertex = v;
	}

	inline void setEdges(const vector<WEdge *>& iEdgeList)
	{
		_EdgeList = iEdgeList;
	}

	inline void setId(int id)
	{
		_Id = id;
	}

	inline void setShape(WShape *iShape)
	{
		_Shape = iShape;
	}

	inline void setSmooth(bool b)
	{
		_Smooth = b;
	}

	inline void setBorder(bool b)
	{
		if (b)
			_Border = 1;
		else
			_Border = 0;
	}

	/*! Adds an edge to the edges list */
	void AddEdge(WEdge *iEdge);

	virtual void ResetUserData()
	{
		userdata = NULL;
	}

public:
	/*! Iterator to iterate over a vertex incoming edges in the CCW order*/
#if defined(__GNUC__) && (__GNUC__ < 3)
	class incoming_edge_iterator : public input_iterator<WOEdge *, ptrdiff_t>
#else
	class incoming_edge_iterator
	: public iterator<input_iterator_tag, WOEdge *, ptrdiff_t>
#endif
	{
	private:
		WVertex *_vertex;
		//
		WOEdge *_begin;
		WOEdge *_current;

	public:
#if defined(__GNUC__) && (__GNUC__ < 3)
		inline incoming_edge_iterator() : input_iterator<WOEdge *, ptrdiff_t>() {}
#else
		inline incoming_edge_iterator() : iterator<input_iterator_tag, WOEdge *, ptrdiff_t>() {}
#endif
		virtual ~incoming_edge_iterator() {}; //soc

	protected:
		friend class WVertex;
		inline incoming_edge_iterator(WVertex *iVertex, WOEdge *iBegin, WOEdge *iCurrent)
#if defined(__GNUC__) && (__GNUC__ < 3)
		: input_iterator<WOEdge *, ptrdiff_t>()
#else
		: iterator<input_iterator_tag, WOEdge *, ptrdiff_t>()
#endif
		{
			_vertex = iVertex;
			_begin = iBegin;
			_current = iCurrent;
		}

	public:
		inline incoming_edge_iterator(const incoming_edge_iterator& iBrother)
#if defined(__GNUC__) && (__GNUC__ < 3)
		: input_iterator<WOEdge *, ptrdiff_t>(iBrother)
#else
		: iterator<input_iterator_tag, WOEdge *, ptrdiff_t>(iBrother)
#endif
		{
			_vertex = iBrother._vertex;
			_begin = iBrother._begin;
			_current = iBrother._current;
		}

	public:
		// operators
		// operator corresponding to ++i
		virtual incoming_edge_iterator& operator++()
		{
			increment();
			return *this;
		}

		// operator corresponding to i++
		virtual incoming_edge_iterator operator++(int)
		{
			incoming_edge_iterator tmp = *this;
			increment();
			return tmp;
		}

		// comparibility
		virtual bool operator!=(const incoming_edge_iterator& b) const
		{
			return ((_current) != (b._current));
		}

		virtual bool operator==(const incoming_edge_iterator& b) const
		{
			return ((_current) == (b._current));
		}

		// dereferencing
		virtual WOEdge *operator*();
		//virtual WOEdge **operator->();
	protected:
		virtual void increment();

#ifdef WITH_CXX_GUARDEDALLOC
		MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:WVertex:incoming_edge_iterator")
#endif
	};

	/*! Iterator to iterate over a vertex faces in the CCW order */
#if defined(__GNUC__) && (__GNUC__ < 3)
	class face_iterator : public input_iterator<WFace *, ptrdiff_t>
#else
	class face_iterator : public iterator<input_iterator_tag, WFace *, ptrdiff_t>
#endif
	{
	private:
		incoming_edge_iterator _edge_it;

	public:
#if defined(__GNUC__) && (__GNUC__ < 3)
		inline face_iterator() : input_iterator<WFace *, ptrdiff_t>() {}
#else
		inline face_iterator() : iterator<input_iterator_tag, WFace *, ptrdiff_t>() {}
#endif
		virtual ~face_iterator() {}; //soc

	protected:
		friend class WVertex;
		inline face_iterator(incoming_edge_iterator it)
#if defined(__GNUC__) && (__GNUC__ < 3)
		: input_iterator<WFace *, ptrdiff_t>()
#else
		: iterator<input_iterator_tag, WFace *, ptrdiff_t>()
#endif
		{
			_edge_it = it;
		}

	public:
		inline face_iterator(const face_iterator& iBrother)
#if defined(__GNUC__) && (__GNUC__ < 3)
		: input_iterator<WFace *, ptrdiff_t>(iBrother)
#else
		: iterator<input_iterator_tag, WFace *, ptrdiff_t>(iBrother)
#endif
		{
			_edge_it = iBrother._edge_it;
		}

	public:
		// operators
		// operator corresponding to ++i
		virtual face_iterator& operator++()
		{
			increment();
			return *this;
		}

		// operator corresponding to i++
		virtual face_iterator operator++(int)
		{
			face_iterator tmp = *this;
			increment();
			return tmp;
		}

		// comparibility
		virtual bool operator!=(const face_iterator& b) const
		{
			return ((_edge_it) != (b._edge_it));
		}

		virtual bool operator==(const face_iterator& b) const
		{
			return ((_edge_it) == (b._edge_it));
		}

		// dereferencing
		virtual WFace *operator*();
		//virtual WOEdge **operator->();

	protected:
		inline void increment()
		{
			++_edge_it;
		}

#ifdef WITH_CXX_GUARDEDALLOC
		MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:WVertex:face_iterator")
#endif
	};

public:
	/*! iterators access */
	virtual incoming_edge_iterator incoming_edges_begin();
	virtual incoming_edge_iterator incoming_edges_end();

	virtual face_iterator faces_begin()
	{
		return face_iterator(incoming_edges_begin());
	}

	virtual face_iterator faces_end()
	{
		return face_iterator(incoming_edges_end());
	}

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:WVertex")
#endif
};


/**********************************
 *                                *
 *                                *
 *             WOEdge             *
 *                                *
 *                                *
 **********************************/

class WFace;
class WEdge;

class WOEdge
{
protected:
#if 0
	WOEdge *_paCWEdge;     // edge reached when traveling clockwise on aFace from the edge
	WOEdge *_pbCWEdge;     // edge reached when traveling clockwise on bFace from the edge
	WOEdge *_paCCWEdge;    // edge reached when traveling counterclockwise on aFace from the edge
	WOEdge *_pbCCWEdge;    // edge reached when traveling counterclockwise on bFace from the edge
#endif
	WVertex *_paVertex;    // starting vertex
	WVertex *_pbVertex;    // ending vertex
	WFace *_paFace;        // when following the edge, face on the right
	WFace *_pbFace;        // when following the edge, face on the left
	WEdge *_pOwner;        // Edge

	Vec3r _vec;
	real _angle;

public:
	void *userdata;

	inline WOEdge()
	{
#if 0
		_paCWEdge = NULL;
		_pbCWEdge = NULL;
		_paCCWEdge = NULL;
		_pbCCWEdge = NULL;
#endif
		_paVertex = NULL;
		_pbVertex = NULL;
		_paFace = NULL;
		_pbFace = NULL;
		_pOwner = NULL;
		userdata = NULL;
	}

	virtual ~WOEdge() {}; //soc

	/*! copy constructor */
	WOEdge(WOEdge& iBrother);
	virtual WOEdge *duplicate();

	/*! accessors */
#if 0
	inline WOEdge *GetaCWEdge()
	{
		return _paCWEdge;
	}

	inline WOEdge *GetbCWEdge()
	{
		return _pbCWEdge;
	}

	inline WOEdge *GetaCCWEdge()
	{
		return _paCCWEdge;
	}

	inline WOEdge *GetbCCWEdge()
	{
		return _pbCCWEdge;
	}
#endif

	inline WVertex *GetaVertex()
	{
		return _paVertex;
	}

	inline WVertex *GetbVertex()
	{
		return _pbVertex;
	}

	inline WFace *GetaFace()
	{
		return _paFace;
	}

	inline WFace *GetbFace()
	{
		return _pbFace;
	}

	inline WEdge *GetOwner()
	{
		return _pOwner;
	}

	inline const Vec3r& GetVec()
	{
		return _vec;
	}

	inline const real GetAngle()
	{
		return _angle;
	}


	/*! modifiers */
#if 0
	inline void SetaCWEdge(WOEdge *pe)
	{
		_paCWEdge = pe;
	}

	inline void SetbCWEdge(WOEdge *pe)
	{
		_pbCWEdge = pe;
	}

	inline void SetaCCWEdge(WOEdge *pe)
	{
		_paCCWEdge = pe;
	}

	inline void SetbCCCWEdge(WOEdge *pe)
	{
		_pbCCWEdge = pe;
	}
#endif

	inline void setVecAndAngle();

	inline void setaVertex(WVertex *pv)
	{
		_paVertex = pv;
		setVecAndAngle();
	}

	inline void setbVertex(WVertex *pv)
	{
		_pbVertex = pv;
		setVecAndAngle();
	}

	inline void setaFace(WFace *pf)
	{
		_paFace = pf;
		setVecAndAngle();
	}

	inline void setbFace(WFace *pf)
	{
		_pbFace = pf;
		setVecAndAngle();
	}

	inline void setOwner(WEdge *pe)
	{
		_pOwner = pe;
	}

	/*! Retrieves the list of edges in CW order */
	inline void RetrieveCWOrderedEdges(vector<WEdge*>& oEdges);

	WOEdge *twin ();
	WOEdge *getPrevOnFace();

	virtual void ResetUserData()
	{
		userdata = NULL;
	}

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:WOEdge")
#endif
};


/**********************************
 *                                *
 *                                *
 *             WEdge              *
 *                                *
 *                                *
 **********************************/

class WEdge
{
protected:
	WOEdge *_paOEdge; // first oriented edge
	WOEdge *_pbOEdge; // second oriented edge
	int _nOEdges;     // number of oriented edges associated with this edge. (1 means border edge)
	bool _Mark;       // user-specified edge mark for feature edge detection
	int _Id;          // Identifier for the edge

public:
	void *userdata; // designed to store specific user data

	inline WEdge()
	{
		_paOEdge = NULL;
		_pbOEdge = NULL;
		_nOEdges = 0;
		userdata = NULL;
	}

	inline WEdge(WOEdge *iOEdge)
	{
		_paOEdge = iOEdge;
		_pbOEdge = NULL;
		_nOEdges = 1;
		userdata = NULL;
	}

	inline WEdge(WOEdge *iaOEdge, WOEdge *ibOEdge)
	{
		_paOEdge = iaOEdge;
		_pbOEdge = ibOEdge;
		_nOEdges = 2;
		userdata = NULL;
	}

	/*! Copy constructor */
	WEdge(WEdge& iBrother);
	virtual WEdge *duplicate();

	virtual ~WEdge()
	{
		if (_paOEdge) {
			delete _paOEdge;
			_paOEdge = NULL;
		}

		if (_pbOEdge) {
			delete _pbOEdge;
			_pbOEdge = NULL;
		}
	}

	/*! checks whether two WEdge have a common vertex.
	 *  Returns a pointer on the common vertex if it exists, NULL otherwise.
	 */
	static inline WVertex *CommonVertex(WEdge *iEdge1, WEdge *iEdge2)
	{
		if (!iEdge1 || !iEdge2)
			return NULL;

		WVertex *wv1 = iEdge1->GetaOEdge()->GetaVertex();
		WVertex *wv2 = iEdge1->GetaOEdge()->GetbVertex();
		WVertex *wv3 = iEdge2->GetaOEdge()->GetaVertex();
		WVertex *wv4 = iEdge2->GetaOEdge()->GetbVertex();

		if ((wv1 == wv3) || (wv1 == wv4)) {
			return wv1;
		}
		else if ((wv2 == wv3) || (wv2 == wv4)) {
			return wv2;
		}
		return NULL;
	}

	/*! accessors */
	inline WOEdge *GetaOEdge()
	{
		return _paOEdge;
	}

	inline WOEdge *GetbOEdge()
	{
		return _pbOEdge;
	}

	inline int GetNumberOfOEdges()
	{
		return _nOEdges;
	}

	inline bool GetMark()
	{
		return _Mark;
	}

	inline int GetId()
	{
		return _Id;
	}

	inline WVertex *GetaVertex()
	{
		return _paOEdge->GetaVertex();
	}

	inline WVertex *GetbVertex()
	{
		return _paOEdge->GetbVertex();
	}

	inline WFace *GetaFace()
	{
		return _paOEdge->GetaFace();
	}

	inline WFace *GetbFace()
	{
		return _paOEdge->GetbFace();
	}

	inline WOEdge *GetOtherOEdge(WOEdge *iOEdge) {
		if (iOEdge == _paOEdge)
			return _pbOEdge;
		else
			return _paOEdge;
	}

	/*! modifiers */
	inline void setaOEdge(WOEdge *iEdge)
	{
		_paOEdge = iEdge;
	}

	inline void setbOEdge(WOEdge *iEdge)
	{
		_pbOEdge = iEdge;
	}

	inline void AddOEdge(WOEdge *iEdge)
	{
		if (!_paOEdge) {
			_paOEdge = iEdge;
			_nOEdges++;
			return;
		}
		if (!_pbOEdge) {
			_pbOEdge = iEdge;
			_nOEdges++;
			return;
		}
	}

	inline void setNumberOfOEdges(int n)
	{
		_nOEdges = n;
	}

	inline void setMark(bool mark)
	{
		_Mark = mark;
	}

	inline void setId(int id)
	{
		_Id = id;
	}

	virtual void ResetUserData()
	{
		userdata = NULL;
	}

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:WEdge")
#endif
};


/**********************************
 *                                *
 *                                *
 *             WFace              *
 *                                *
 *                                *
 **********************************/


class WFace
{
protected:
	vector<WOEdge *> _OEdgeList; // list of oriented edges of bording the face
	Vec3r _Normal;               // normal to the face
	// in case there is a normal per vertex.
	// The normal number i corresponds to the aVertex of the oedge number i, for that face
	vector<Vec3r> _VerticesNormals;
	vector<Vec2r> _VerticesTexCoords;

	int _Id;
	unsigned _FrsMaterialIndex;
	bool _Mark; // Freestyle face mark (if true, feature edges on this face are ignored)

public:
	void *userdata;
	inline WFace()
	{
		userdata = NULL;
		_FrsMaterialIndex = 0;
	}

	/*! copy constructor */
	WFace(WFace& iBrother);
	virtual WFace *duplicate();
	virtual ~WFace() {}

	/*! accessors */
	inline const vector<WOEdge*>& getEdgeList()
	{
		return _OEdgeList;
	}

	inline WOEdge *GetOEdge(int i)
	{
		return _OEdgeList[i];
	}

	inline Vec3r& GetNormal()
	{
		return _Normal;
	}

	inline int GetId()
	{
		return _Id;
	}

	inline unsigned frs_materialIndex() const
	{
		return _FrsMaterialIndex;
	}

	inline bool GetMark() const
	{
		return _Mark;
	}

	const FrsMaterial& frs_material();

	/*! The vertex of index i corresponds to the a vertex of the edge of index i */
	inline WVertex *GetVertex(unsigned int index)
	{
#if 0
		if (index >= _OEdgeList.size())
			return NULL;
#endif
		return _OEdgeList[index]->GetaVertex();
	}

	/*! returns the index at which iVertex is stored in the array.
	 * returns -1 if iVertex doesn't belong to the face.
	 */
	inline int GetIndex(WVertex *iVertex)
	{
		int index = 0;
		for (vector<WOEdge*>::iterator woe = _OEdgeList.begin(), woend = _OEdgeList.end(); woe != woend; woe++) {
			if ((*woe)->GetaVertex() == iVertex)
				return index;
			++index;
		}
		return -1;
	}

	inline void RetrieveVertexList(vector<WVertex *>& oVertices)
	{
		for (vector<WOEdge *>::iterator woe = _OEdgeList.begin(), woend = _OEdgeList.end(); woe != woend; woe++) {
			oVertices.push_back((*woe)->GetaVertex());
		}
	}

	inline void  RetrieveBorderFaces(vector<const WFace *>& oWFaces)
	{
		for (vector<WOEdge *>::iterator woe = _OEdgeList.begin(), woend = _OEdgeList.end(); woe != woend; woe++) {
			WFace *af;
			if ((af = (*woe)->GetaFace()))
				oWFaces.push_back(af);
		}
	}

	inline WFace *GetBordingFace(int index)
	{
#if 0
		if (index >= _OEdgeList.size())
			return NULL;
#endif
		return _OEdgeList[index]->GetaFace();
	}

	inline WFace *GetBordingFace(WOEdge *iOEdge)
	{
		return iOEdge->GetaFace();
	}

	inline vector<Vec3r>& GetPerVertexNormals()
	{
		return _VerticesNormals;
	}

	inline vector<Vec2r>& GetPerVertexTexCoords()
	{
		return _VerticesTexCoords;
	}

	/*! Returns the normal of the vertex of index index */
	inline Vec3r& GetVertexNormal(int index)
	{
		return _VerticesNormals[index];
	}

	/*! Returns the tex coords of the vertex of index index */
	inline Vec2r& GetVertexTexCoords(int index)
	{
		return _VerticesTexCoords[index];
	}

	/*! Returns the normal of the vertex iVertex for that face */
	inline Vec3r& GetVertexNormal(WVertex *iVertex)
	{
		int i = 0;
		int index = 0;
		for (vector<WOEdge *>::const_iterator woe = _OEdgeList.begin(), woend = _OEdgeList.end(); woe != woend; woe++) {
			if ((*woe)->GetaVertex() == iVertex) {
				index = i;
				break;
			}
			++i;
		}

		return _VerticesNormals[index];
	}

	inline WOEdge *GetNextOEdge(WOEdge *iOEdge)
	{
		bool found = false;
		vector<WOEdge *>::iterator woe, woend, woefirst;
		woefirst = _OEdgeList.begin();
		for (woe = woefirst, woend = _OEdgeList.end(); woe != woend; ++woe) {
			if (found)
				return (*woe);

			if ((*woe) == iOEdge) {
				found = true;
			}
		}

		// We left the loop. That means that the first OEdge was the good one:
		if (found)
			return (*woefirst);

		return NULL;
	}

	WOEdge *GetPrevOEdge(WOEdge *iOEdge);

	inline int numberOfEdges() const
	{
		return _OEdgeList.size();
	}

	inline int numberOfVertices() const
	{
		return _OEdgeList.size();
	}

	/*! Returns true if the face has one ot its edge which is a border edge */
	inline bool isBorder() const
	{
		for (vector<WOEdge*>::const_iterator woe = _OEdgeList.begin(), woeend = _OEdgeList.end();
		     woe != woeend;
		     ++woe)
		{
			if ((*woe)->GetOwner()->GetbOEdge() == 0)
				return true;
		}
		return false;
	}

	/*! modifiers */
	inline void setEdgeList(const vector<WOEdge *>& iEdgeList)
	{
		_OEdgeList = iEdgeList;
	}

	inline void setNormal(const Vec3r& iNormal)
	{
		_Normal = iNormal;
	}

	inline void setNormalList(const vector<Vec3r>& iNormalsList)
	{
		_VerticesNormals = iNormalsList;
	}

	inline void setTexCoordsList(const vector<Vec2r>& iTexCoordsList)
	{
		_VerticesTexCoords = iTexCoordsList;
	}

	inline void setId(int id)
	{
		_Id = id;
	}

	inline void setFrsMaterialIndex(unsigned iMaterialIndex)
	{
		_FrsMaterialIndex = iMaterialIndex;
	}

	inline void setMark(bool iMark)
	{
		_Mark = iMark;
	}

	/*! designed to build a specialized WEdge for use in MakeEdge */
	virtual WEdge *instanciateEdge() const
	{
		return new WEdge;
	}

	/*! Builds an oriented edge
	 *  Returns the built edge.
	 *    v1, v2
	 *      Vertices at the edge's extremities
	 *      The edge is oriented from v1 to v2.
	 */
	virtual WOEdge *MakeEdge(WVertex *v1, WVertex *v2);

	/*! Adds an edge to the edges list */
	inline void AddEdge(WOEdge *iEdge)
	{
		_OEdgeList.push_back(iEdge);
	}

	/*! For triangles, returns the edge opposite to the vertex in e.
	 *  returns flase if the face is not a triangle or if the vertex is not found
	 */
	bool getOppositeEdge (const WVertex *v, WOEdge *&e);

	/*! compute the area of the face */
	real getArea ();

	WShape *getShape();
	virtual void ResetUserData()
	{
		userdata = NULL;
	}

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:WFace")
#endif
};


/**********************************
 *                                *
 *                                *
 *             WShape             *
 *                                *
 *                                *
 **********************************/


class WShape
{
protected:
	vector<WVertex *> _VertexList;
	vector<WEdge *> _EdgeList;
	vector<WFace *> _FaceList;
	int _Id;
	string _Name;
	static unsigned _SceneCurrentId;
	Vec3r _min;
	Vec3r _max;
	vector<FrsMaterial> _FrsMaterials;
	real _meanEdgeSize;

public:
	inline WShape()
	{
		_meanEdgeSize = 0;
		_Id = _SceneCurrentId;
		_SceneCurrentId++;
	}

	/*! copy constructor */
	WShape(WShape& iBrother);
	virtual WShape *duplicate();

	virtual ~WShape()
	{
		if (_EdgeList.size() != 0) {
			vector<WEdge *>::iterator e;
			for (e = _EdgeList.begin(); e != _EdgeList.end(); ++e) {
				delete (*e);
			}
			_EdgeList.clear();
		}

		if (_VertexList.size() != 0) {
			vector<WVertex *>::iterator v;
			for (v = _VertexList.begin(); v != _VertexList.end(); ++v) {
				delete (*v);
			}
			_VertexList.clear();
		}

		if (_FaceList.size() != 0) {
			vector<WFace *>::iterator f;
			for (f = _FaceList.begin(); f != _FaceList.end(); ++f) {
				delete (*f);
			}
			_FaceList.clear();
		}
	}

	/*! accessors */
	inline vector<WEdge *>& getEdgeList()
	{
		return _EdgeList;
	}

	inline vector<WVertex *>& getVertexList()
	{
		return _VertexList;
	}

	inline vector<WFace *>& GetFaceList()
	{
		return _FaceList;
	}

	inline unsigned GetId()
	{
		return _Id;
	}

	inline void bbox(Vec3r& min, Vec3r& max)
	{
		min = _min;
		max = _max;
	}

	inline const FrsMaterial& frs_material(unsigned i) const
	{
		return _FrsMaterials[i];
	}

	inline const vector<FrsMaterial>& frs_materials() const
	{
		return _FrsMaterials;
	}

	inline const real getMeanEdgeSize() const
	{
		return _meanEdgeSize;
	}

	inline const string& getName() const
	{
		return _Name;
	}

	/*! modifiers */
	static inline void setCurrentId(const unsigned id)
	{
		_SceneCurrentId = id;
	}

	inline void setEdgeList(const vector<WEdge *>& iEdgeList)
	{
		_EdgeList = iEdgeList;
	}

	inline void setVertexList(const vector<WVertex *>& iVertexList)
	{
		_VertexList = iVertexList;
	}

	inline void setFaceList(const vector<WFace *>& iFaceList)
	{
		_FaceList = iFaceList;
	}

	inline void setId(int id)
	{
		_Id = id;
	}

	inline void setBBox(const Vec3r& min, const Vec3r& max)
	{
		_min = min;
		_max = max;
	}

	inline void setFrsMaterial(const FrsMaterial& frs_material, unsigned i)
	{
		_FrsMaterials[i] = frs_material;
	}

	inline void setFrsMaterials(const vector<FrsMaterial>& iMaterials)
	{
		_FrsMaterials = iMaterials;
	}

	inline void setName(const string& name)
	{
		_Name = name;
	}

	/*! designed to build a specialized WFace for use in MakeFace */
	virtual WFace *instanciateFace() const
	{
		return new WFace;
	}

	/*! adds a new face to the shape
	 *  returns the built face.
	 *   iVertexList
	 *      List of face's vertices. These vertices are not added to the WShape vertex list; they are supposed to be
	 *      already stored when calling MakeFace.
	 *      The order in which the vertices are stored in the list determines the face's edges orientation and (so) the
	 *      face orientation.
	 *   iMaterialIndex
	 *      The material index for this face
	 */
	virtual WFace *MakeFace(vector<WVertex *>& iVertexList, vector<bool>& iFaceEdgeMarksList, unsigned iMaterialIndex);

	/*! adds a new face to the shape. The difference with the previous method is that this one is designed
	 *  to build a WingedEdge structure for which there are per vertex normals, opposed to per face normals.
	 *  returns the built face.
	 *   iVertexList
	 *      List of face's vertices. These vertices are not added to the WShape vertex list; they are supposed to be
	 *      already stored when calling MakeFace.
	 *      The order in which the vertices are stored in the list determines the face's edges orientation and (so) the
	 *      face orientation.
	 *   iMaterialIndex
	 *      The materialIndex for this face
	 *   iNormalsList
	 *     The list of normals, iNormalsList[i] corresponding to the normal of the vertex iVertexList[i] for that face.
	 *   iTexCoordsList
	 *     The list of tex coords, iTexCoordsList[i] corresponding to the normal of the vertex iVertexList[i] for
	 *     that face.
	 */
	virtual WFace *MakeFace(vector<WVertex *>& iVertexList, vector<Vec3r>& iNormalsList, vector<Vec2r>& iTexCoordsList,
	                        vector<bool>& iFaceEdgeMarksList, unsigned iMaterialIndex);

	inline void AddEdge(WEdge *iEdge)
	{
		_EdgeList.push_back(iEdge);
	}

	inline void AddFace(WFace *iFace)
	{
		_FaceList.push_back(iFace);
	}

	inline void AddVertex(WVertex *iVertex)
	{
		iVertex->setShape(this);
		_VertexList.push_back(iVertex);
	}

	inline void ResetUserData()
	{
		for (vector<WVertex *>::iterator v = _VertexList.begin(), vend = _VertexList.end(); v != vend; v++) {
			(*v)->ResetUserData();
		}

		for (vector<WEdge *>::iterator e = _EdgeList.begin(), eend = _EdgeList.end(); e != eend; e++) {
			(*e)->ResetUserData();
			// manages WOEdge:
			WOEdge *oe = (*e)->GetaOEdge();
			if (oe)
				oe->ResetUserData();
			oe = (*e)->GetbOEdge();
			if (oe)
				oe->ResetUserData();
		}

		for (vector<WFace *>::iterator f = _FaceList.begin(), fend = _FaceList.end(); f != fend; f++) {
			(*f)->ResetUserData();
		}
	}

	inline void ComputeBBox()
	{
		_min = _VertexList[0]->GetVertex();
		_max = _VertexList[0]->GetVertex();

		Vec3r v;
		for (vector<WVertex *>::iterator wv = _VertexList.begin(), wvend = _VertexList.end(); wv != wvend; wv++) {
			for (unsigned int i = 0; i < 3; i++) {
				v = (*wv)->GetVertex();
				if (v[i] < _min[i])
					_min[i] = v[i];
				if (v[i] > _max[i])
					_max[i] = v[i];
			}
		}
	}

	inline real ComputeMeanEdgeSize()
	{
		_meanEdgeSize = _meanEdgeSize / _EdgeList.size();
		return _meanEdgeSize;
	}

protected:
	/*! Builds the face passed as argument (which as already been allocated)
	 *    iVertexList
	 *      List of face's vertices. These vertices are not added to the WShape vertex list; they are supposed to be
	 *      already stored when calling MakeFace.
	 *      The order in which the vertices are stored in the list determines the face's edges orientation and (so) the
	 *      face orientation.
	 *    iMaterialIndex
	 *      The material index for this face
	 *    face
	 *      The Face that is filled in
	 */
	virtual WFace *MakeFace(vector<WVertex *>& iVertexList, vector<bool>& iFaceEdgeMarksList, unsigned iMaterialIndex,
	                        WFace *face);

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:WShape")
#endif
};


/**********************************
 *                                *
 *                                *
 *          WingedEdge            *
 *                                *
 *                                *
 **********************************/

class WingedEdge
{
public:
	WingedEdge() {
		_numFaces = 0;
	}

	~WingedEdge()
	{
		clear();
	}

	void clear()
	{
		for (vector<WShape *>::iterator it = _wshapes.begin(); it != _wshapes.end(); it++)
			delete *it;
		_wshapes.clear();
		_numFaces = 0;
	}

	void addWShape(WShape *wshape)
	{
		_wshapes.push_back(wshape);
		_numFaces += wshape->GetFaceList().size();
	}

	vector<WShape *>& getWShapes()
	{
		return _wshapes;
	}

	unsigned getNumFaces()
	{
		return _numFaces;
	}

private:
	vector<WShape *> _wshapes;
	unsigned _numFaces;

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:WingedEdge")
#endif
};



/*

#############################################
#############################################
#############################################
######                                 ######
######   I M P L E M E N T A T I O N   ######
######                                 ######
#############################################
#############################################
#############################################

*/
/* for inline functions */
void WOEdge::RetrieveCWOrderedEdges(vector<WEdge *>& oEdges)
{
	WOEdge *currentOEdge = this;
	do {
		WOEdge *nextOEdge = currentOEdge->GetbFace()->GetNextOEdge(currentOEdge);
		oEdges.push_back(nextOEdge->GetOwner());
		currentOEdge = nextOEdge->GetOwner()->GetOtherOEdge(nextOEdge);
	} while (currentOEdge && (currentOEdge->GetOwner() != GetOwner()));
}

inline void WOEdge::setVecAndAngle()
{
	if (_paVertex && _pbVertex) {
		_vec = _pbVertex->GetVertex() - _paVertex->GetVertex();
		if (_paFace && _pbFace) {
			real sine = (_pbFace->GetNormal() ^ _paFace->GetNormal()) * _vec / _vec.norm();
			if (sine >= 1.0) {
				_angle = M_PI / 2.0;
				return;
			}
			if (sine <= -1.0) {
				_angle = -M_PI / 2.0;
				return;
			}
			_angle = ::asin(sine);
		}
	}
}

} /* namespace Freestyle */

#endif // __FREESTYLE_W_EDGE_H__
