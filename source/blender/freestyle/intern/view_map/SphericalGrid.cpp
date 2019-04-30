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
 * \brief Class to define a cell grid surrounding the projected image of a scene
 */

#include <algorithm>
#include <stdexcept>

#include "SphericalGrid.h"

#include "BKE_global.h"

using namespace std;

namespace Freestyle {

// Helper Classes

// OccluderData
///////////////

// Cell
/////////

SphericalGrid::Cell::Cell()
{
}

SphericalGrid::Cell::~Cell()
{
}

void SphericalGrid::Cell::setDimensions(real x, real y, real sizeX, real sizeY)
{
  const real epsilon = 1.0e-06;
  boundary[0] = x - epsilon;
  boundary[1] = x + sizeX + epsilon;
  boundary[2] = y - epsilon;
  boundary[3] = y + sizeY + epsilon;
}

bool SphericalGrid::Cell::compareOccludersByShallowestPoint(const SphericalGrid::OccluderData *a,
                                                            const SphericalGrid::OccluderData *b)
{
  return a->shallowest < b->shallowest;
}

void SphericalGrid::Cell::indexPolygons()
{
  // Sort occluders by their shallowest points.
  sort(faces.begin(), faces.end(), compareOccludersByShallowestPoint);
}

// Iterator
//////////////////

SphericalGrid::Iterator::Iterator(SphericalGrid &grid, Vec3r &center, real /*epsilon*/)
    : _target(SphericalGrid::Transform::sphericalProjection(center)), _foundOccludee(false)
{
  // Find target cell
  _cell = grid.findCell(_target);
#if SPHERICAL_GRID_LOGGING
  if (G.debug & G_DEBUG_FREESTYLE) {
    cout << "Searching for occluders of edge centered at " << _target << " in cell ["
         << _cell->boundary[0] << ", " << _cell->boundary[1] << ", " << _cell->boundary[2] << ", "
         << _cell->boundary[3] << "] (" << _cell->faces.size() << " occluders)" << endl;
  }
#endif

  // Set iterator
  _current = _cell->faces.begin();
}

SphericalGrid::Iterator::~Iterator()
{
}

// SphericalGrid
/////////////////

SphericalGrid::SphericalGrid(OccluderSource &source,
                             GridDensityProvider &density,
                             ViewMap *viewMap,
                             Vec3r &viewpoint,
                             bool enableQI)
    : _viewpoint(viewpoint), _enableQI(enableQI)
{
  if (G.debug & G_DEBUG_FREESTYLE) {
    cout << "Generate Cell structure" << endl;
  }
  // Generate Cell structure
  assignCells(source, density, viewMap);
  if (G.debug & G_DEBUG_FREESTYLE) {
    cout << "Distribute occluders" << endl;
  }
  // Fill Cells
  distributePolygons(source);
  if (G.debug & G_DEBUG_FREESTYLE) {
    cout << "Reorganize cells" << endl;
  }
  // Reorganize Cells
  reorganizeCells();
  if (G.debug & G_DEBUG_FREESTYLE) {
    cout << "Ready to use SphericalGrid" << endl;
  }
}

SphericalGrid::~SphericalGrid()
{
}

void SphericalGrid::assignCells(OccluderSource & /*source*/,
                                GridDensityProvider &density,
                                ViewMap *viewMap)
{
  _cellSize = density.cellSize();
  _cellsX = density.cellsX();
  _cellsY = density.cellsY();
  _cellOrigin[0] = density.cellOrigin(0);
  _cellOrigin[1] = density.cellOrigin(1);
  if (G.debug & G_DEBUG_FREESTYLE) {
    cout << "Using " << _cellsX << "x" << _cellsY << " cells of size " << _cellSize << " square."
         << endl;
    cout << "Cell origin: " << _cellOrigin[0] << ", " << _cellOrigin[1] << endl;
  }

  // Now allocate the cell table and fill it with default (empty) cells
  _cells.resize(_cellsX * _cellsY);
  for (cellContainer::iterator i = _cells.begin(), end = _cells.end(); i != end; ++i) {
    (*i) = NULL;
  }

  // Identify cells that will be used, and set the dimensions for each
  ViewMap::fedges_container &fedges = viewMap->FEdges();
  for (ViewMap::fedges_container::iterator f = fedges.begin(), fend = fedges.end(); f != fend;
       ++f) {
    if ((*f)->isInImage()) {
      Vec3r point = SphericalGrid::Transform::sphericalProjection((*f)->center3d());
      unsigned i, j;
      getCellCoordinates(point, i, j);
      if (_cells[i * _cellsY + j] == NULL) {
        // This is an uninitialized cell
        real x, y, width, height;

        x = _cellOrigin[0] + _cellSize * i;
        width = _cellSize;

        y = _cellOrigin[1] + _cellSize * j;
        height = _cellSize;

        // Initialize cell
        Cell *b = _cells[i * _cellsY + j] = new Cell();
        b->setDimensions(x, y, width, height);
      }
    }
  }
}

void SphericalGrid::distributePolygons(OccluderSource &source)
{
  unsigned long nFaces = 0;
  unsigned long nKeptFaces = 0;

  for (source.begin(); source.isValid(); source.next()) {
    OccluderData *occluder = NULL;

    try {
      if (insertOccluder(source, occluder)) {
        _faces.push_back(occluder);
        ++nKeptFaces;
      }
    }
    catch (...) {
      // If an exception was thrown, _faces.push_back() cannot have succeeded. Occluder is not
      // owned by anyone, and must be deleted. If the exception was thrown before or during new
      // OccluderData(), then occluder is NULL, and this delete is harmless.
      delete occluder;
      throw;
    }
    ++nFaces;
  }
  if (G.debug & G_DEBUG_FREESTYLE) {
    cout << "Distributed " << nFaces << " occluders.  Retained " << nKeptFaces << "." << endl;
  }
}

void SphericalGrid::reorganizeCells()
{
  // Sort the occluders by shallowest point
  for (vector<Cell *>::iterator i = _cells.begin(), end = _cells.end(); i != end; ++i) {
    if (*i != NULL) {
      (*i)->indexPolygons();
    }
  }
}

void SphericalGrid::getCellCoordinates(const Vec3r &point, unsigned &x, unsigned &y)
{
  x = min(_cellsX - 1, (unsigned)floor(max((double)0.0f, point[0] - _cellOrigin[0]) / _cellSize));
  y = min(_cellsY - 1, (unsigned)floor(max((double)0.0f, point[1] - _cellOrigin[1]) / _cellSize));
}

SphericalGrid::Cell *SphericalGrid::findCell(const Vec3r &point)
{
  unsigned x, y;
  getCellCoordinates(point, x, y);
  return _cells[x * _cellsY + y];
}

bool SphericalGrid::orthographicProjection() const
{
  return false;
}

const Vec3r &SphericalGrid::viewpoint() const
{
  return _viewpoint;
}

bool SphericalGrid::enableQI() const
{
  return _enableQI;
}

SphericalGrid::Transform::Transform() : GridHelpers::Transform()
{
}

Vec3r SphericalGrid::Transform::operator()(const Vec3r &point) const
{
  return sphericalProjection(point);
}

Vec3r SphericalGrid::Transform::sphericalProjection(const Vec3r &M)
{
  Vec3r newPoint;

  newPoint[0] = ::atan(M[0] / M[2]);
  newPoint[1] = ::atan(M[1] / M[2]);
  newPoint[2] = ::sqrt(M[0] * M[0] + M[1] * M[1] + M[2] * M[2]);

  return newPoint;
}

} /* namespace Freestyle */
