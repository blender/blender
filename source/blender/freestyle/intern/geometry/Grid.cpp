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
 * \brief Base class to define a cell grid surrounding the bounding box of the scene
 */

#include <stdexcept>

#include "BBox.h"
#include "Grid.h"

#include "BLI_utildefines.h"

namespace Freestyle {

// Grid Visitors
/////////////////
void allOccludersGridVisitor::examineOccluder(Polygon3r *occ)
{
  occluders_.push_back(occ);
}

static bool inBox(const Vec3r &inter, const Vec3r &box_min, const Vec3r &box_max)
{
  if (((inter.x() >= box_min.x()) && (inter.x() < box_max.x())) &&
      ((inter.y() >= box_min.y()) && (inter.y() < box_max.y())) &&
      ((inter.z() >= box_min.z()) && (inter.z() < box_max.z()))) {
    return true;
  }
  return false;
}

void firstIntersectionGridVisitor::examineOccluder(Polygon3r *occ)
{
  // check whether the edge and the polygon plane are coincident:
  //-------------------------------------------------------------
  // first let us compute the plane equation.
  Vec3r v1(((occ)->getVertices())[0]);
  Vec3d normal((occ)->getNormal());
  // soc unused - double d = -(v1 * normal);

  double tmp_u, tmp_v, tmp_t;
  if ((occ)->rayIntersect(ray_org_, ray_dir_, tmp_t, tmp_u, tmp_v)) {
    if (fabs(ray_dir_ * normal) > 0.0001) {
      // Check whether the intersection is in the cell:
      if (inBox(ray_org_ + tmp_t * ray_dir_ / ray_dir_.norm(),
                current_cell_->getOrigin(),
                current_cell_->getOrigin() + cell_size_)) {
#if 0
        Vec3d bboxdiag(_scene3d->bbox().getMax() - _scene3d->bbox().getMin());
        if ((t > 1.0e-06 * (min(min(bboxdiag.x(), bboxdiag.y()), bboxdiag.z()))) &&
            (t < raylength)) {
#else
        if (tmp_t < t_) {
#endif
        occluder_ = occ;
        u_ = tmp_u;
        v_ = tmp_v;
        t_ = tmp_t;
      }
    }
    else {
      occ->userdata2 = 0;
    }
  }
}
}  // namespace Freestyle

bool firstIntersectionGridVisitor::stop()
{
  if (occluder_) {
    return true;
  }
  return false;
}

// Grid
/////////////////
void Grid::clear()
{
  if (_occluders.size() != 0) {
    for (OccludersSet::iterator it = _occluders.begin(); it != _occluders.end(); it++) {
      delete (*it);
    }
    _occluders.clear();
  }

  _size = Vec3r(0, 0, 0);
  _cell_size = Vec3r(0, 0, 0);
  _orig = Vec3r(0, 0, 0);
  _cells_nb = Vec3u(0, 0, 0);
  //_ray_occluders.clear();
}

void Grid::configure(const Vec3r &orig, const Vec3r &size, unsigned nb)
{
  _orig = orig;
  Vec3r tmpSize = size;
  // Compute the volume of the desired grid
  real grid_vol = size[0] * size[1] * size[2];

  if (grid_vol == 0) {
    double min = DBL_MAX;
    int index = 0;
    int nzeros = 0;
    for (int i = 0; i < 3; ++i) {
      if (size[i] == 0) {
        ++nzeros;
        index = i;
      }
      if ((size[i] != 0) && (min > size[i])) {
        min = size[i];
      }
    }
    if (nzeros > 1) {
      throw std::runtime_error("Warning: the 3D grid has more than one null dimension");
    }
    tmpSize[index] = min;
    _orig[index] = _orig[index] - min / 2;
  }
  // Compute the desired volume of a single cell
  real cell_vol = grid_vol / nb;
  // The edge of such a cubic cell is cubic root of cellVolume
  real edge = pow(cell_vol, 1.0 / 3.0);

  // We compute the number of cells par edge such as we cover at least the whole box.
  unsigned i;
  for (i = 0; i < 3; i++) {
    _cells_nb[i] = (unsigned)floor(tmpSize[i] / edge) + 1;
  }

  _size = tmpSize;

  for (i = 0; i < 3; i++) {
    _cell_size[i] = _size[i] / _cells_nb[i];
  }
}

void Grid::insertOccluder(Polygon3r *occluder)
{
  const vector<Vec3r> vertices = occluder->getVertices();
  if (vertices.size() == 0) {
    return;
  }

  // add this occluder to the grid's occluders list
  addOccluder(occluder);

  // find the bbox associated to this polygon
  Vec3r min, max;
  occluder->getBBox(min, max);

  // Retrieve the cell x, y, z cordinates associated with these min and max
  Vec3u imax, imin;
  getCellCoordinates(max, imax);
  getCellCoordinates(min, imin);

  // We are now going to fill in the cells overlapping with the polygon bbox.
  // If the polygon is a triangle (most of cases), we also check for each of these cells if it is
  // overlapping with the triangle in order to only fill in the ones really overlapping the
  // triangle.

  unsigned i, x, y, z;
  vector<Vec3r>::const_iterator it;
  Vec3u coord;

  if (vertices.size() == 3) {  // Triangle case
    Vec3r triverts[3];
    i = 0;
    for (it = vertices.begin(); it != vertices.end(); it++) {
      triverts[i] = Vec3r(*it);
      i++;
    }

    Vec3r boxmin, boxmax;

    for (z = imin[2]; z <= imax[2]; z++) {
      for (y = imin[1]; y <= imax[1]; y++) {
        for (x = imin[0]; x <= imax[0]; x++) {
          coord[0] = x;
          coord[1] = y;
          coord[2] = z;
          // We retrieve the box coordinates of the current cell
          getCellBox(coord, boxmin, boxmax);
          // We check whether the triangle and the box ovewrlap:
          Vec3r boxcenter((boxmin + boxmax) / 2.0);
          Vec3r boxhalfsize(_cell_size / 2.0);
          if (GeomUtils::overlapTriangleBox(boxcenter, boxhalfsize, triverts)) {
            // We must then create the Cell and add it to the cells list if it does not exist yet.
            // We must then add the occluder to the occluders list of this cell.
            Cell *cell = getCell(coord);
            if (!cell) {
              cell = new Cell(boxmin);
              fillCell(coord, *cell);
            }
            cell->addOccluder(occluder);
          }
        }
      }
    }
  }
  else {  // The polygon is not a triangle, we add all the cells overlapping the polygon bbox.
    for (z = imin[2]; z <= imax[2]; z++) {
      for (y = imin[1]; y <= imax[1]; y++) {
        for (x = imin[0]; x <= imax[0]; x++) {
          coord[0] = x;
          coord[1] = y;
          coord[2] = z;
          Cell *cell = getCell(coord);
          if (!cell) {
            Vec3r orig;
            getCellOrigin(coord, orig);
            cell = new Cell(orig);
            fillCell(coord, *cell);
          }
          cell->addOccluder(occluder);
        }
      }
    }
  }
}

bool Grid::nextRayCell(Vec3u &current_cell, Vec3u &next_cell)
{
  next_cell = current_cell;
  real t_min, t;
  unsigned i;

  t_min = FLT_MAX;     // init tmin with handle of the case where one or 2 _u[i] = 0.
  unsigned coord = 0;  // predominant coord(0=x, 1=y, 2=z)

  // using a parametric equation of a line : B = A + t u, we find the tx, ty and tz respectively
  // coresponding to the intersections with the plans:
  //     x = _cell_size[0], y = _cell_size[1], z = _cell_size[2]
  for (i = 0; i < 3; i++) {
    if (_ray_dir[i] == 0) {
      continue;
    }
    if (_ray_dir[i] > 0) {
      t = (_cell_size[i] - _pt[i]) / _ray_dir[i];
    }
    else {
      t = -_pt[i] / _ray_dir[i];
    }
    if (t < t_min) {
      t_min = t;
      coord = i;
    }
  }

  // We use the parametric line equation and the found t (tamx) to compute the B coordinates:
  Vec3r pt_tmp(_pt);
  _pt = pt_tmp + t_min * _ray_dir;

  // We express B coordinates in the next cell coordinates system. We just have to
  // set the coordinate coord of B to 0 of _CellSize[coord] depending on the sign of _u[coord]
  if (_ray_dir[coord] > 0) {
    next_cell[coord]++;
    _pt[coord] -= _cell_size[coord];
    // if we are out of the grid, we must stop
    if (next_cell[coord] >= _cells_nb[coord]) {
      return false;
    }
  }
  else {
    int tmp = next_cell[coord] - 1;
    _pt[coord] = _cell_size[coord];
    if (tmp < 0) {
      return false;
    }
    next_cell[coord]--;
  }

  _t += t_min;
  if (_t >= _t_end) {
    return false;
  }

  return true;
}

void Grid::castRay(const Vec3r &orig,
                   const Vec3r &end,
                   OccludersSet &occluders,
                   unsigned timestamp)
{
  initRay(orig, end, timestamp);
  allOccludersGridVisitor visitor(occluders);
  castRayInternal(visitor);
}

void Grid::castInfiniteRay(const Vec3r &orig,
                           const Vec3r &dir,
                           OccludersSet &occluders,
                           unsigned timestamp)
{
  Vec3r end = Vec3r(orig + FLT_MAX * dir / dir.norm());
  bool inter = initInfiniteRay(orig, dir, timestamp);
  if (!inter) {
    return;
  }
  allOccludersGridVisitor visitor(occluders);
  castRayInternal(visitor);
}

Polygon3r *Grid::castRayToFindFirstIntersection(
    const Vec3r &orig, const Vec3r &dir, double &t, double &u, double &v, unsigned timestamp)
{
  Polygon3r *occluder = 0;
  Vec3r end = Vec3r(orig + FLT_MAX * dir / dir.norm());
  bool inter = initInfiniteRay(orig, dir, timestamp);
  if (!inter) {
    return 0;
  }
  firstIntersectionGridVisitor visitor(orig, dir, _cell_size);
  castRayInternal(visitor);
  // ARB: This doesn't work, because occluders are unordered within any cell
  // visitor.occluder() will be an occluder, but we have no guarantee it will be the *first*
  // occluder. I assume that is the reason this code is not actually used for FindOccludee.
  occluder = visitor.occluder();
  t = visitor.t_;
  u = visitor.u_;
  v = visitor.v_;
  return occluder;
}

void Grid::initRay(const Vec3r &orig, const Vec3r &end, unsigned timestamp)
{
  _ray_dir = end - orig;
  _t_end = _ray_dir.norm();
  _t = 0;
  _ray_dir.normalize();
  _timestamp = timestamp;

  for (unsigned i = 0; i < 3; i++) {
    _current_cell[i] = (unsigned)floor((orig[i] - _orig[i]) / _cell_size[i]);
    // soc unused - unsigned u = _current_cell[i];
    _pt[i] = orig[i] - _orig[i] - _current_cell[i] * _cell_size[i];
  }
  //_ray_occluders.clear();
}

bool Grid::initInfiniteRay(const Vec3r &orig, const Vec3r &dir, unsigned timestamp)
{
  _ray_dir = dir;
  _t_end = FLT_MAX;
  _t = 0;
  _ray_dir.normalize();
  _timestamp = timestamp;

  // check whether the origin is in or out the box:
  Vec3r boxMin(_orig);
  Vec3r boxMax(_orig + _size);
  BBox<Vec3r> box(boxMin, boxMax);
  if (box.inside(orig)) {
    for (unsigned int i = 0; i < 3; i++) {
      _current_cell[i] = (unsigned int)floor((orig[i] - _orig[i]) / _cell_size[i]);
      // soc unused - unsigned u = _current_cell[i];
      _pt[i] = orig[i] - _orig[i] - _current_cell[i] * _cell_size[i];
    }
  }
  else {
    // is the ray intersecting the box?
    real tmin(-1.0), tmax(-1.0);
    if (GeomUtils::intersectRayBBox(orig, _ray_dir, boxMin, boxMax, 0, _t_end, tmin, tmax)) {
      BLI_assert(tmin != -1.0);
      Vec3r newOrig = orig + tmin * _ray_dir;
      for (unsigned int i = 0; i < 3; i++) {
        _current_cell[i] = (unsigned)floor((newOrig[i] - _orig[i]) / _cell_size[i]);
        if (_current_cell[i] == _cells_nb[i]) {
          _current_cell[i] = _cells_nb[i] - 1;
        }
        // soc unused - unsigned u = _current_cell[i];
        _pt[i] = newOrig[i] - _orig[i] - _current_cell[i] * _cell_size[i];
      }
    }
    else {
      return false;
    }
  }
  //_ray_occluders.clear();

  return true;
}

} /* namespace Freestyle */
