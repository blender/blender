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

#ifndef __FREESTYLE_VIEW_EDGE_X_BUILDER_H__
#define __FREESTYLE_VIEW_EDGE_X_BUILDER_H__

/** \file blender/freestyle/intern/view_map/ViewEdgeXBuilder.h
 *  \ingroup freestyle
 *  \brief Class to build view edges and the underlying chains of feature edges...
 *  \author Stephane Grabli
 *  \date 27/10/2003
 */

#include <map>
#include <utility>
#include <vector>

#if 0 // soc
#if defined(__GNUC__) && (__GNUC__ >= 3)
//hash_map is not part of the C++ standard anymore; hash_map.h has been kept though for backward compatibility
#  include <hash_map.h>
#else
#  include <hash_map>
#endif
#endif

#include "Interface1D.h"

#include "../geometry/Geom.h"

#include "../system/FreestyleConfig.h"

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

using namespace std;

namespace Freestyle {

using namespace Geometry;

class SVertex;

/*! Defines a hash table used for searching the SVertex */
struct SVertexHasher
{
#define _MUL 950706376UL
#define _MOD 2147483647UL
	inline size_t operator()(const Vec3r& p) const
	{
		size_t res = ((unsigned long)(p[0] * _MUL)) % _MOD;
		res = ((res + (unsigned long)(p[1]) * _MUL)) % _MOD;
		return ((res +(unsigned long)(p[2]) * _MUL)) % _MOD;
	}
#undef _MUL
#undef _MOD
};

// Key_compare predicate for hash_map. In particular, return false if equal.
struct epsilonEquals
{
	bool operator()(const Vec3r& v1, const Vec3r& v2) const
	{
		real norm = (v1 - v2).norm();
		return (norm < 1.0e-06);
	}
};


// typedef hash_map<Vec3r, SVertex*, SVertexHasher, epsilonEquals> SVertexMap;
typedef map<Vec3r, SVertex *> SVertexMap;

class WXFaceLayer;

/*! class to describe an oriented smooth edge */
class OWXFaceLayer
{
public:
	WXFaceLayer *fl;
	bool order;

	OWXFaceLayer()
	{
		fl = NULL;
		order = true;
	}

	OWXFaceLayer(WXFaceLayer *ifl, bool iOrder = true)
	{
		fl = ifl;
		order = iOrder;
	}

	OWXFaceLayer& operator=(const OWXFaceLayer& iBrother)
	{
		fl = iBrother.fl;
		order = iBrother.order;
		return *this;
	}

	bool operator==(const OWXFaceLayer& b)
	{
		return ((fl == b.fl) && (order == b.order));
	}

	bool operator!=(const OWXFaceLayer& b)
	{
		return !(*this == b);
	}

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:OWXFaceLayer")
#endif
};

class WXEdge;

/*! class to describe an oriented sharp edge */
class OWXEdge
{
public:
	WXEdge *e;
	bool order;

	OWXEdge()
	{
		e = NULL;
		order = true;
	}

	OWXEdge(WXEdge *ie, bool iOrder = true)
	{
		e = ie;
		order = iOrder;
	}

	OWXEdge& operator=(const OWXEdge& iBrother)
	{
		e = iBrother.e;
		order = iBrother.order;
		return *this;
	}

	bool operator==(const OWXEdge& b)
	{
		return ((e == b.e) && (order == b.order));
	}

	bool operator!=(const OWXEdge& b)
	{
		return !(*this == b);
	}

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:OWXEdge")
#endif
};

class WOEdge;
class WXEdge;
class WXShape;
class SVertex;
class FEdge;
class ViewVertex;
class ViewEdge;
class ViewShape;

class ViewEdgeXBuilder
{
protected:
	int _currentViewId;    // Id for view edges
	int _currentFId;       // Id for FEdges
	int _currentSVertexId; // Id for SVertex

public:
	inline ViewEdgeXBuilder()
	{
		_currentViewId = 1;
		_currentFId = 0;
		_currentSVertexId = 0;
	}

	virtual ~ViewEdgeXBuilder() {}

	/*! Builds a view shape from a WXShape in which the feature edges are flagged
	 *  Builds chains of feature edges (so ViewEdges) from a WXShape
	 *    iWShape
	 *      The Winged Edge structure in which all silhouette edges and vertices are flagged.
	 *    oViewShape
	 *      The Silhouette Shape in which the chains must be added.
	 *    ioVEdges
	 *      The list of new ViewEdges.
	 *    ioVVertices
	 *      THe new ViewVertices
	 *    ioFEdges
	 *      A list in which all new FEdges are added
	 *    ioSVertices
	 *      A list of SVertex where all created SVertex are added.
	 */
	virtual void BuildViewEdges(WXShape *iWShape, ViewShape *oVShape, std::vector<ViewEdge*>& ioVEdges,
	                            std::vector<ViewVertex*>& ioVVertices, std::vector<FEdge*>& ioFEdges,
	                            std::vector<SVertex*>& ioSVertices);

	/*! Builds a smooth view edge, starting the face iFace. */
	ViewEdge *BuildSmoothViewEdge(const OWXFaceLayer& iFaceLayer);

	/*! Makes a sharp viewedge  */
	ViewEdge *BuildSharpViewEdge(const OWXEdge& iWEdge);

public:
	/*! accessors */
	inline int currentViewId() const
	{
		return _currentViewId;
	}

	inline int currentFId() const
	{
		return _currentFId;
	}

	inline int currentSVertexId() const
	{
		return _currentSVertexId;
	}

	/*! modifiers */
	inline void setCurrentViewId(int id)
	{
		_currentViewId = id;
	}

	inline void setCurrentFId(int id)
	{
		_currentFId = id;
	}

	inline void setCurrentSVertexId(int id)
	{
		_currentSVertexId = id;
	}

protected:
	/*! Init the view edges building */
	virtual void Init(ViewShape *oVShape);

	// SMOOTH //
	/*! checks whether a face has already been processed or not */
	bool stopSmoothViewEdge(WXFaceLayer *iFaceLayer);
	OWXFaceLayer FindNextFaceLayer(const OWXFaceLayer& iFaceLayer);
	OWXFaceLayer FindPreviousFaceLayer(const OWXFaceLayer& iFaceLayer);
	FEdge *BuildSmoothFEdge(FEdge *feprevious, const OWXFaceLayer& ifl);

	// SHARP //
	/*! checks whether a WEdge has already been processed or not */
	bool stopSharpViewEdge(WXEdge *iFace);
	int retrieveFaceMarks(WXEdge *iEdge);
	OWXEdge FindNextWEdge(const OWXEdge& iEdge);
	OWXEdge FindPreviousWEdge(const OWXEdge& iEdge);
	FEdge *BuildSharpFEdge(FEdge *feprevious, const OWXEdge& iwe);

	// GENERAL //
	/*! Instanciate a SVertex */
	SVertex *MakeSVertex(Vec3r& iPoint);
	/*! Instanciate a SVertex if it hasn't been already created */
	SVertex *MakeSVertex(Vec3r& iPoint, bool shared);
	/*! instanciate a ViewVertex from a SVertex, if it doesn't exist yet */
	ViewVertex *MakeViewVertex(SVertex *iSVertex);

	//oldtmp values
	//IdHashTable _hashtable;
	//VVIdHashTable _multivertexHashTable;
	SVertexMap _SVertexMap;
	SShape *_pCurrentSShape;
	ViewShape *_pCurrentVShape;

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:ViewEdgeXBuilder")
#endif
};

} /* namespace Freestyle */

#endif  // __FREESTYLE_VIEW_EDGE_X_BUILDER_H__
