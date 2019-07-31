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

#ifndef __GRID_H__
#define __GRID_H__

/** \file
 * \ingroup freestyle
 * \brief Base class to define a cell grid surrounding the bounding box of the scene
 */

#include <cstring>  // for memset
#include <float.h>
#include <stdint.h>  // For POINTER_FROM_UINT, i.e. uintptr_t.
#include <vector>

#include "Geom.h"
#include "GeomUtils.h"
#include "Polygon.h"

#include "../system/FreestyleConfig.h"

extern "C" {
#include "BLI_utildefines.h"
}

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

using namespace std;

namespace Freestyle {

using namespace Geometry;

typedef vector<Polygon3r *> OccludersSet;

//
// Class to define cells used by the regular grid
//
///////////////////////////////////////////////////////////////////////////////

class Cell {
 public:
  Cell(Vec3r &orig)
  {
    _orig = orig;
  }

  virtual ~Cell()
  {
  }

  inline void addOccluder(Polygon3r *o)
  {
    if (o) {
      _occluders.push_back(o);
    }
  }

  inline const Vec3r &getOrigin()
  {
    return _orig;
  }

  inline OccludersSet &getOccluders()
  {
    return _occluders;
  }

 private:
  Vec3r _orig;
  OccludersSet _occluders;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:Cell")
#endif
};

class GridVisitor {
 public:
  virtual ~GridVisitor(){};  // soc

  virtual void discoverCell(Cell * /*cell*/)
  {
  }

  virtual void examineOccluder(Polygon3r * /*occ*/)
  {
  }

  virtual void finishCell(Cell * /*cell*/)
  {
  }

  virtual bool stop()
  {
    return false;
  }

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:GridVisitor")
#endif
};

/*! Gathers all the occluders belonging to the cells traversed by the ray */
class allOccludersGridVisitor : public GridVisitor {
 public:
  allOccludersGridVisitor(OccludersSet &occluders) : GridVisitor(), occluders_(occluders)
  {
  }

  virtual void examineOccluder(Polygon3r *occ);

  OccludersSet &occluders()
  {
    return occluders_;
  }

  void clear()
  {
    occluders_.clear();
  }

 private:
  OccludersSet &occluders_;
};

/*! Finds the first intersection and breaks.
 *  The occluder and the intersection information are stored and accessible.
 */
class firstIntersectionGridVisitor : public GridVisitor {
  // soc - changed order to remove warnings
 public:
  double u_, v_, t_;

 private:
  Polygon3r *occluder_;
  Vec3r ray_org_, ray_dir_, cell_size_;
  Cell *current_cell_;

 public:
  firstIntersectionGridVisitor(const Vec3r &ray_org, const Vec3r &ray_dir, const Vec3r &cell_size)
      : GridVisitor(),
        u_(0),
        v_(0),
        t_(DBL_MAX),
        occluder_(0),
        ray_org_(ray_org),
        ray_dir_(ray_dir),
        cell_size_(cell_size),
        current_cell_(0)
  {
  }

  virtual ~firstIntersectionGridVisitor()
  {
  }

  virtual void discoverCell(Cell *cell)
  {
    current_cell_ = cell;
  }

  virtual void examineOccluder(Polygon3r *occ);

  virtual bool stop();

  Polygon3r *occluder()
  {
    return occluder_;
  }
};

//
// Class to define a regular grid used for ray casting computations
//
///////////////////////////////////////////////////////////////////////////////

class Grid {
 public:
  /*! Builds a Grid. Must be followed by a call to configure() */
  Grid()
  {
  }

  virtual ~Grid()
  {
    clear();
  }

  /*! clears the grid
   *  Deletes all the cells, clears the hashtable, resets size, size of cell, number of cells.
   */
  virtual void clear();

  /*! Sets the different parameters of the grid
   *    orig
   *      The grid origin
   *    size
   *      The grid's dimensions
   *    nb
   *      The number of cells of the grid
   */
  virtual void configure(const Vec3r &orig, const Vec3r &size, unsigned nb);

  /*! returns a vector of integer containing the coordinates of the cell containing the point
   * passed as argument
   *    p
   *      The point for which we're looking the cell
   */
  inline void getCellCoordinates(const Vec3r &p, Vec3u &res)
  {
    int tmp;
    for (int i = 0; i < 3; i++) {
      tmp = (int)((p[i] - _orig[i]) / _cell_size[i]);
      if (tmp < 0) {
        res[i] = 0;
      }
      else if ((unsigned int)tmp >= _cells_nb[i]) {
        res[i] = _cells_nb[i] - 1;
      }
      else {
        res[i] = tmp;
      }
    }
  }

  /*! Fills the case corresponding to coord with the cell */
  virtual void fillCell(const Vec3u &coord, Cell &cell) = 0;

  /*! returns the cell whose coordinates are passed as argument */
  virtual Cell *getCell(const Vec3u &coord) = 0;

  /*! returns the cell containing the point passed as argument.
   *  If the cell is empty (contains no occluder),  NULL is returned:
   *    p
   *      The point for which we're looking the cell
   */
  inline Cell *getCell(const Vec3r &p)
  {
    Vec3u coord;
    getCellCoordinates(p, coord);
    return getCell(coord);
  }

  /*! Retrieves the x,y,z coordinates of the origin of the cell whose coordinates (i,j,k)
   *  is passed as argument:
   *    cell_coord
   *      i,j,k integer coordinates for the cell
   *    orig
   *      x,y,x vector to be filled in with the cell origin's coordinates
   */
  inline void getCellOrigin(const Vec3u &cell_coord, Vec3r &orig)
  {
    for (unsigned int i = 0; i < 3; i++) {
      orig[i] = _orig[i] + cell_coord[i] * _cell_size[i];
    }
  }

  /*! Retrieves the box corresponding to the cell whose coordinates are passed as argument:
   *    cell_coord
   *      i,j,k integer coordinates for the cell
   *    min_out
   *      The min x,y,x vector of the box. Filled in by the method.
   *    max_out
   *      The max x,y,z coordinates of the box. Filled in by the method.
   */
  inline void getCellBox(const Vec3u &cell_coord, Vec3r &min_out, Vec3r &max_out)
  {
    getCellOrigin(cell_coord, min_out);
    max_out = min_out + _cell_size;
  }

  /*! inserts a convex polygon occluder
   *  This method is quite coarse insofar as it adds all cells intersecting the polygon bounding
   * box convex_poly The list of 3D points constituting a convex polygon
   */
  void insertOccluder(Polygon3r *convex_poly);

  /*! Adds an occluder to the list of occluders */
  void addOccluder(Polygon3r *occluder)
  {
    _occluders.push_back(occluder);
  }

  /*! Casts a ray between a starting point and an ending point
   *  Returns the list of occluders contained in the cells intersected by this ray
   *  Starts with a call to InitRay.
   */
  void castRay(const Vec3r &orig, const Vec3r &end, OccludersSet &occluders, unsigned timestamp);

  // Prepares to cast ray without generating OccludersSet
  void initAcceleratedRay(const Vec3r &orig, const Vec3r &end, unsigned timestamp);

  /*! Casts an infinite ray (still finishing at the end of the grid) from a starting point and in a
   * given direction. Returns the list of occluders contained in the cells intersected by this ray
   *  Starts with a call to InitRay.
   */
  void castInfiniteRay(const Vec3r &orig,
                       const Vec3r &dir,
                       OccludersSet &occluders,
                       unsigned timestamp);

  // Prepares to cast ray without generating OccludersSet.
  bool initAcceleratedInfiniteRay(const Vec3r &orig, const Vec3r &dir, unsigned timestamp);

  /*! Casts an infinite ray (still finishing at the end of the grid) from a starting point and in a
   * given direction. Returns the first intersection (occluder,t,u,v) or null. Starts with a call
   * to InitRay.
   */
  Polygon3r *castRayToFindFirstIntersection(
      const Vec3r &orig, const Vec3r &dir, double &t, double &u, double &v, unsigned timestamp);

  /*! Init all structures and values for computing the cells intersected by this new ray */
  void initRay(const Vec3r &orig, const Vec3r &end, unsigned timestamp);

  /*! Init all structures and values for computing the cells intersected by this infinite ray.
   * Returns false if the ray doesn't intersect the grid.
   */
  bool initInfiniteRay(const Vec3r &orig, const Vec3r &dir, unsigned timestamp);

  /*! Accessors */
  inline const Vec3r &getOrigin() const
  {
    return _orig;
  }

  inline Vec3r gridSize() const
  {
    return _size;
  }

  inline Vec3r getCellSize() const
  {
    return _cell_size;
  }

  // ARB profiling only:
  inline OccludersSet *getOccluders()
  {
    return &_occluders;
  }

  void displayDebug()
  {
    cerr << "Cells nb     : " << _cells_nb << endl;
    cerr << "Cell size    : " << _cell_size << endl;
    cerr << "Origin       : " << _orig << endl;
    cerr << "Occluders nb : " << _occluders.size() << endl;
  }

 protected:
  /*! Core of castRay and castInfiniteRay, find occluders along the given ray */
  inline void castRayInternal(GridVisitor &visitor)
  {
    Cell *current_cell = NULL;
    do {
      current_cell = getCell(_current_cell);
      if (current_cell) {
        visitor.discoverCell(current_cell);
        OccludersSet &occluders =
            current_cell->getOccluders();  // FIXME: I had forgotten the ref &
        for (OccludersSet::iterator it = occluders.begin(); it != occluders.end(); it++) {
          if (POINTER_AS_UINT((*it)->userdata2) != _timestamp) {
            (*it)->userdata2 = POINTER_FROM_UINT(_timestamp);
            visitor.examineOccluder(*it);
          }
        }
        visitor.finishCell(current_cell);
      }
    } while ((!visitor.stop()) && (nextRayCell(_current_cell, _current_cell)));
  }

  /*! returns the  cell next to the cell passed as argument. */
  bool nextRayCell(Vec3u &current_cell, Vec3u &next_cell);

  unsigned int _timestamp;

  Vec3u _cells_nb;   // number of cells for x,y,z axis
  Vec3r _cell_size;  // cell x,y,z dimensions
  Vec3r _size;       // grid x,y,x dimensions
  Vec3r _orig;       // grid origin

  Vec3r _ray_dir;       // direction vector for the ray
  Vec3u _current_cell;  // The current cell being processed (designated by its 3 coordinates)
  Vec3r _pt;    // Points corresponding to the incoming and outgoing intersections of one cell with
                // the ray
  real _t_end;  // To know when we are at the end of the ray
  real _t;

  // OccludersSet _ray_occluders; // Set storing the occluders contained in the cells traversed by
  // a ray
  OccludersSet _occluders;  // List of all occluders inserted in the grid

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:Grid")
#endif
};

//
// Class to walk through occluders in grid without building intermediate data structures
//
///////////////////////////////////////////////////////////////////////////////

class VirtualOccludersSet {
 public:
  VirtualOccludersSet(Grid &_grid) : grid(_grid){};
  Polygon3r *begin();
  Polygon3r *next();
  Polygon3r *next(bool stopOnNewCell);

 private:
  Polygon3r *firstOccluderFromNextCell();
  Grid &grid;
  OccludersSet::iterator it, end;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:VirtualOccludersSet")
#endif
};

} /* namespace Freestyle */

#endif  // __GRID_H__
