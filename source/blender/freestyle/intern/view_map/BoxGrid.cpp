/* SPDX-FileCopyrightText: 2011-2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 * \brief Class to define a cell grid surrounding the projected image of a scene
 */

#include <algorithm>
#include <stdexcept>

#include "BoxGrid.h"

#include "BLI_sys_types.h"

#include "BKE_global.h"

using namespace std;

namespace Freestyle {

// Helper Classes

// OccluderData
///////////////

// Cell
/////////

void BoxGrid::Cell::setDimensions(real x, real y, real sizeX, real sizeY)
{
  const real epsilon = 1.0e-06;
  boundary[0] = x - epsilon;
  boundary[1] = x + sizeX + epsilon;
  boundary[2] = y - epsilon;
  boundary[3] = y + sizeY + epsilon;
}

bool BoxGrid::Cell::compareOccludersByShallowestPoint(const BoxGrid::OccluderData *a,
                                                      const BoxGrid::OccluderData *b)
{
  return a->shallowest < b->shallowest;
}

void BoxGrid::Cell::indexPolygons()
{
  // Sort occluders by their shallowest points.
  sort(faces.begin(), faces.end(), compareOccludersByShallowestPoint);
}

// Iterator
//////////////////

BoxGrid::Iterator::Iterator(BoxGrid &grid, Vec3r &center, real /*epsilon*/)
    : _target(grid.transform(center)), _foundOccludee(false)
{
  // Find target cell
  _cell = grid.findCell(_target);
#if BOX_GRID_LOGGING
  if (G.debug & G_DEBUG_FREESTYLE) {
    cout << "Searching for occluders of edge centered at " << _target << " in cell ["
         << 1_cell->boundary[0] << ", " << _cell->boundary[1] << ", " << _cell->boundary[2] << ", "
         << _cell->boundary[3] << "] (" << _cell->faces.size() << " occluders)" << endl;
  }
#endif

  // Set iterator
  _current = _cell->faces.begin();
}

// BoxGrid
/////////////////

BoxGrid::BoxGrid(OccluderSource &source,
                 GridDensityProvider &density,
                 ViewMap *viewMap,
                 Vec3r &viewpoint,
                 bool enableQI)
    : _viewpoint(viewpoint), _enableQI(enableQI)
{
  // Generate Cell structure
  if (G.debug & G_DEBUG_FREESTYLE) {
    cout << "Generate Cell structure" << endl;
  }
  assignCells(source, density, viewMap);

  // Fill Cells
  if (G.debug & G_DEBUG_FREESTYLE) {
    cout << "Distribute occluders" << endl;
  }
  distributePolygons(source);

  // Reorganize Cells
  if (G.debug & G_DEBUG_FREESTYLE) {
    cout << "Reorganize cells" << endl;
  }
  reorganizeCells();

  if (G.debug & G_DEBUG_FREESTYLE) {
    cout << "Ready to use BoxGrid" << endl;
  }
}

BoxGrid::~BoxGrid() = default;

void BoxGrid::assignCells(OccluderSource & /*source*/,
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
    (*i) = nullptr;
  }

  // Identify cells that will be used, and set the dimensions for each
  ViewMap::fedges_container &fedges = viewMap->FEdges();
  for (ViewMap::fedges_container::iterator f = fedges.begin(), fend = fedges.end(); f != fend; ++f)
  {
    if ((*f)->isInImage()) {
      Vec3r point = transform((*f)->center3d());
      uint i, j;
      getCellCoordinates(point, i, j);
      if (_cells[i * _cellsY + j] == nullptr) {
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

void BoxGrid::distributePolygons(OccluderSource &source)
{
  ulong nFaces = 0;
  ulong nKeptFaces = 0;

  for (source.begin(); source.isValid(); source.next()) {
    OccluderData *occluder = nullptr;

    try {
      if (insertOccluder(source, occluder)) {
        _faces.push_back(occluder);
        ++nKeptFaces;
      }
    }
    catch (...) {
      // If an exception was thrown, _faces.push_back() cannot have succeeded.
      // occluder is not owned by anyone, and must be deleted.
      // If the exception was thrown before or during new OccluderData(), then
      // occluder is NULL, and this delete is harmless.
      delete occluder;
      throw;
    }
    ++nFaces;
  }
  if (G.debug & G_DEBUG_FREESTYLE) {
    cout << "Distributed " << nFaces << " occluders. Retained " << nKeptFaces << "." << endl;
  }
}

void BoxGrid::reorganizeCells()
{
  // Sort the occluders by shallowest point
  for (vector<Cell *>::iterator i = _cells.begin(), end = _cells.end(); i != end; ++i) {
    if (*i != nullptr) {
      (*i)->indexPolygons();
    }
  }
}

void BoxGrid::getCellCoordinates(const Vec3r &point, uint &x, uint &y)
{
  x = min(_cellsX - 1, uint(floor(max(double(0.0f), point[0] - _cellOrigin[0]) / _cellSize)));
  y = min(_cellsY - 1, uint(floor(max(double(0.0f), point[1] - _cellOrigin[1]) / _cellSize)));
}

BoxGrid::Cell *BoxGrid::findCell(const Vec3r &point)
{
  uint x, y;
  getCellCoordinates(point, x, y);
  return _cells[x * _cellsY + y];
}

bool BoxGrid::orthographicProjection() const
{
  return true;
}

const Vec3r &BoxGrid::viewpoint() const
{
  return _viewpoint;
}

bool BoxGrid::enableQI() const
{
  return _enableQI;
}

Vec3r BoxGrid::Transform::operator()(const Vec3r &point) const
{
  return Vec3r(point[0], point[1], -point[2]);
}

} /* namespace Freestyle */
