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

#ifndef __FREESTYLE_SPHERICAL_GRID_H__
#define __FREESTYLE_SPHERICAL_GRID_H__

/** \file blender/freestyle/intern/view_map/SphericalGrid.h
 *  \ingroup freestyle
 *  \brief Class to define a cell grid surrounding the projected image of a scene
 *  \author Alexander Beels
 *  \date 2010-12-19
 */

#define SPHERICAL_GRID_LOGGING 0

// I would like to avoid using deque because including ViewMap.h and <deque> or <vector> separately results in
// redefinitions of identifiers. ViewMap.h already includes <vector> so it should be a safe fall-back.
//#include <vector>
//#include <deque>

#include "GridDensityProvider.h"
#include "OccluderSource.h"
#include "ViewMap.h"

#include "../geometry/Polygon.h"
#include "../geometry/BBox.h"
#include "../geometry/GridHelpers.h"

#include "../system/PointerSequence.h"

#include "../winged_edge/WEdge.h"

#include "BKE_global.h"

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

namespace Freestyle {

class SphericalGrid
{
public:
	// Helper classes
	struct OccluderData
	{
		explicit OccluderData (OccluderSource& source, Polygon3r& p);
		Polygon3r poly;
		Polygon3r cameraSpacePolygon;
		real shallowest, deepest;
		// N.B. We could, of course, store face in poly's userdata member, like the old ViewMapBuilder code does.
		// However, code comments make it clear that userdata is deprecated, so we avoid the temptation to save
		// 4 or 8 bytes.
		WFace *face;

#ifdef WITH_CXX_GUARDEDALLOC
		MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:SphericalGrid:OccluderData")
#endif
	};

private:
	struct Cell
	{
		// Can't store Cell in a vector without copy and assign
		//Cell(const Cell& other);
		//Cell& operator=(const Cell& other);

		explicit Cell();
		~Cell();

		static bool compareOccludersByShallowestPoint(const OccluderData *a, const OccluderData *b);

		void setDimensions(real x, real y, real sizeX, real sizeY);
		void checkAndInsert(OccluderSource& source, Polygon3r& poly, OccluderData*& occluder);
		void indexPolygons();

		real boundary[4];
		//deque<OccluderData*> faces;
		vector<OccluderData*> faces;
	};

public:
	/*! Iterator needs to allow the user to avoid full 3D comparison in two cases:
	 *
	 *  (1) Where (*current)->deepest < target[2], where the occluder is unambiguously in front of the target point.
	 *
	 *  (2) Where (*current)->shallowest > target[2], where the occluder is unambiguously in back of the target point.
	 *
	 *  In addition, when used by OptimizedFindOccludee, Iterator should stop iterating as soon as it has an occludee
	 *  candidate and (*current)->shallowest > candidate[2], because at that point forward no new occluder could
	 *  possibly be a better occludee.
	 */

	class Iterator
	{
	public:
		// epsilon is not used in this class, but other grids with the same interface may need an epsilon
		explicit Iterator(SphericalGrid& grid, Vec3r& center, real epsilon = 1.0e-06);
		~Iterator();
		void initBeforeTarget();
		void initAfterTarget();
		void nextOccluder();
		void nextOccludee();
		bool validBeforeTarget();
		bool validAfterTarget();
		WFace *getWFace() const;
		Polygon3r *getCameraSpacePolygon();
		void reportDepth(Vec3r origin, Vec3r u, real t);
	private:
		bool testOccluder(bool wantOccludee);
		void markCurrentOccludeeCandidate(real depth);

		Cell *_cell;
		Vec3r _target;
		bool _foundOccludee;
		real _occludeeDepth;
		//deque<OccluderData*>::iterator _current, _occludeeCandidate;
		vector<OccluderData*>::iterator _current, _occludeeCandidate;

#ifdef WITH_CXX_GUARDEDALLOC
		MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:SphericalGrid:Iterator")
#endif

	};

	class Transform : public GridHelpers::Transform
	{
	public:
		explicit Transform();
		explicit Transform(Transform& other);
		Vec3r operator()(const Vec3r& point) const;
		static Vec3r sphericalProjection(const Vec3r& M);
	};

private:
	// Prevent implicit copies and assignments.
	SphericalGrid(const SphericalGrid& other);
	SphericalGrid& operator=(const SphericalGrid& other);

public:
	explicit SphericalGrid(OccluderSource& source, GridDensityProvider& density, ViewMap *viewMap,
	                       Vec3r& viewpoint, bool enableQI);
	virtual ~SphericalGrid();

	// Generate Cell structure
	void assignCells(OccluderSource& source, GridDensityProvider& density, ViewMap *viewMap);
	// Fill Cells
	void distributePolygons(OccluderSource& source);
	// Insert one polygon into each matching cell, return true if any cell consumes the polygon
	bool insertOccluder(OccluderSource& source, OccluderData*& occluder);
	// Sort occluders in each cell
	void reorganizeCells();

	Cell *findCell(const Vec3r& point);

	// Accessors:
	bool orthographicProjection() const;
	const Vec3r& viewpoint() const;
	bool enableQI() const; 

private:
	void getCellCoordinates(const Vec3r& point, unsigned& x, unsigned& y);

	typedef PointerSequence<vector<Cell*>, Cell*> cellContainer;
	//typedef PointerSequence<deque<OccluderData*>, OccluderData*> occluderContainer;
	typedef PointerSequence<vector<OccluderData*>, OccluderData*> occluderContainer;
	unsigned _cellsX, _cellsY;
	float _cellSize;
	float _cellOrigin[2];
	cellContainer _cells;
	occluderContainer _faces;
	Vec3r _viewpoint;
	bool _enableQI;

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:SphericalGrid")
#endif
};

inline void SphericalGrid::Iterator::initBeforeTarget()
{
	_current = _cell->faces.begin();
	while (_current != _cell->faces.end() && !testOccluder(false)) {
		++_current;
	}
}

inline void SphericalGrid::Iterator::initAfterTarget()
{
	if (_foundOccludee) {
#if SPHERICAL_GRID_LOGGING
		if (G.debug & G_DEBUG_FREESTYLE) {
			std::cout << "\tStarting occludee search from occludeeCandidate at depth " <<
			             _occludeeDepth << std::endl;
		}
#endif
		_current = _occludeeCandidate;
		return;
	}

#if SPHERICAL_GRID_LOGGING
	if (G.debug & G_DEBUG_FREESTYLE) {
		std::cout << "\tStarting occludee search from current position" << std::endl;
	}
#endif

	while (_current != _cell->faces.end() && !testOccluder(true)) {
		++_current;
	}
}

inline bool SphericalGrid::Iterator::testOccluder(bool wantOccludee)
{
	// End-of-list is not even a valid iterator position
	if (_current == _cell->faces.end()) {
		// Returning true seems strange, but it will break us out of whatever loop is calling testOccluder, and
		// _current=_cell->face.end() will make the calling routine give up.
		return true;
	}
#if SPHERICAL_GRID_LOGGING
	if (G.debug & G_DEBUG_FREESTYLE) {
		std::cout << "\tTesting occluder " << (*_current)->poly.getVertices()[0];
		for (unsigned int i = 1; i < (*_current)->poly.getVertices().size(); ++i) {
			std::cout << ", " << (*_current)->poly.getVertices()[i];
		}
		std::cout << " from shape " << (*_current)->face->GetVertex(0)->shape()->GetId() << std::endl;
	}
#endif

	// If we have an occluder candidate and we are unambiguously after it, abort
	if (_foundOccludee && (*_current)->shallowest > _occludeeDepth) {
#if SPHERICAL_GRID_LOGGING
		if (G.debug & G_DEBUG_FREESTYLE) {
			std::cout << "\t\tAborting: shallowest > occludeeCandidate->deepest" << std::endl;
		}
#endif
		_current = _cell->faces.end();

		// See note above
		return true;
	}

	// Specific continue or stop conditions when searching for each type
	if (wantOccludee) {
		if ((*_current)->deepest < _target[2]) {
#if SPHERICAL_GRID_LOGGING
			if (G.debug & G_DEBUG_FREESTYLE) {
				std::cout << "\t\tSkipping: shallower than target while looking for occludee" << std::endl;
			}
#endif
			return false;
		}
	}
	else {
		if ((*_current)->shallowest > _target[2]) {
#if SPHERICAL_GRID_LOGGING
			if (G.debug & G_DEBUG_FREESTYLE) {
				std::cout << "\t\tStopping: deeper than target while looking for occluder" << std::endl;
			}
#endif
			return true;
		}
	}

	// Depthwise, this is a valid occluder.

	// Check to see if target is in the 2D bounding box
	Vec3r bbMin, bbMax;
	(*_current)->poly.getBBox(bbMin, bbMax);
	if (_target[0] < bbMin[0] || _target[0] > bbMax[0] || _target[1] < bbMin[1] || _target[1] > bbMax[1]) {
#if SPHERICAL_GRID_LOGGING
		if (G.debug & G_DEBUG_FREESTYLE) {
			std::cout << "\t\tSkipping: bounding box violation" << std::endl;
		}
#endif
		return false;
	}

	// We've done all the corner cutting we can. Let the caller work out whether or not the geometry is correct.
	return true;
}

inline void SphericalGrid::Iterator::reportDepth(Vec3r origin, Vec3r u, real t)
{
	// The reported depth is the length of a ray in camera space. We need to convert it into the distance from viewpoint
	// If origin is the viewpoint, depth == t. A future optimization could allow the caller to tell us if origin is
	// viewponit or target, at the cost of changing the OptimizedGrid API.
	real depth = (origin + u * t).norm();
#if SPHERICAL_GRID_LOGGING
	if (G.debug & G_DEBUG_FREESTYLE) {
		std::cout << "\t\tReporting depth of occluder/ee: " << depth;
	}
#endif
	if (depth > _target[2]) {
#if SPHERICAL_GRID_LOGGING
		if (G.debug & G_DEBUG_FREESTYLE) {
			std::cout << " is deeper than target" << std::endl;
		}
#endif
		// If the current occluder is the best occludee so far, save it.
		if (! _foundOccludee || _occludeeDepth > depth) {
			markCurrentOccludeeCandidate(depth);
		} 
	}
	else {
#if SPHERICAL_GRID_LOGGING
		if (G.debug & G_DEBUG_FREESTYLE) {
			std::cout << std::endl;
		}
#endif
	}
}

inline void SphericalGrid::Iterator::nextOccluder()
{
	if (_current != _cell->faces.end()) {
		do {
			++_current;
		} while (_current != _cell->faces.end() && !testOccluder(false));
	}
}

inline void SphericalGrid::Iterator::nextOccludee()
{
	if (_current != _cell->faces.end()) {
		do {
			++_current;
		} while (_current != _cell->faces.end() && !testOccluder(true));
	}
}

inline bool SphericalGrid::Iterator::validBeforeTarget()
{
	return _current != _cell->faces.end() && (*_current)->shallowest <= _target[2];
}

inline bool SphericalGrid::Iterator::validAfterTarget()
{
	return _current != _cell->faces.end();
}

inline void SphericalGrid::Iterator::markCurrentOccludeeCandidate(real depth)
{
#if SPHERICAL_GRID_LOGGING
	if (G.debug & G_DEBUG_FREESTYLE) {
		std::cout << "\t\tFound occludeeCandidate at depth " << depth << std::endl;
	}
#endif
	_occludeeCandidate = _current;
	_occludeeDepth = depth;
	_foundOccludee = true;
}

inline WFace *SphericalGrid::Iterator::getWFace() const
{
	return (*_current)->face;
}

inline Polygon3r *SphericalGrid::Iterator::getCameraSpacePolygon()
{
	return &((*_current)->cameraSpacePolygon);
}

inline SphericalGrid::OccluderData::OccluderData (OccluderSource& source, Polygon3r& p)
: poly(p), cameraSpacePolygon(source.getCameraSpacePolygon()), face(source.getWFace())
{
	const Vec3r viewpoint(0, 0, 0);
	// Get the point on the camera-space polygon that is closest to the viewpoint
	// shallowest is the distance from the viewpoint to that point
	shallowest = GridHelpers::distancePointToPolygon(viewpoint, cameraSpacePolygon);

	// Get the point on the camera-space polygon that is furthest from the viewpoint
	// deepest is the distance from the viewpoint to that point
	deepest = cameraSpacePolygon.getVertices()[2].norm();
	for (unsigned int i = 0; i < 2; ++i) {
		real t = cameraSpacePolygon.getVertices()[i].norm();
		if (t > deepest) {
			deepest = t;
		}
	}
}

inline void SphericalGrid::Cell::checkAndInsert(OccluderSource& source, Polygon3r& poly, OccluderData*& occluder)
{
	if (GridHelpers::insideProscenium (boundary, poly)) {
		if (occluder == NULL) {
			// Disposal of occluder will be handled in SphericalGrid::distributePolygons(),
			// or automatically by SphericalGrid::_faces;
			occluder = new OccluderData(source, poly);
		}
		faces.push_back(occluder);
	}
}

inline bool SphericalGrid::insertOccluder(OccluderSource& source, OccluderData*& occluder)
{
	Polygon3r& poly(source.getGridSpacePolygon());
	occluder = NULL;

	Vec3r bbMin, bbMax;
	poly.getBBox(bbMin, bbMax);
	// Check overlapping cells
	unsigned startX, startY, endX, endY;
	getCellCoordinates(bbMin, startX, startY);
	getCellCoordinates(bbMax, endX, endY);

	for (unsigned int i = startX; i <= endX; ++i) {
		for (unsigned int j = startY; j <= endY; ++j) {
			if (_cells[i * _cellsY + j] != NULL) {
				_cells[i * _cellsY + j]->checkAndInsert(source, poly, occluder);
			}
		}
	}

	return occluder != NULL;
}

} /* namespace Freestyle */

#endif // __FREESTYLE_SPHERICAL_GRID_H__
