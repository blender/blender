
//
//  Copyright (C) : Please refer to the COPYRIGHT file distributed 
//   with this source distribution. 
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
///////////////////////////////////////////////////////////////////////////////

#include "FastGrid.h"

void FastGrid::clear() {
  if(!_cells)
    return;

  for(unsigned i = 0; i < _cells_size; i++)
    if (_cells[i])
      delete _cells[i];
  delete[] _cells;
  _cells = NULL;
  _cells_size = 0;

  Grid::clear();
}

void FastGrid::configure(const Vec3r& orig, const Vec3r& size, unsigned nb) {
  Grid::configure(orig, size, nb);
  _cells_size = _cells_nb[0] * _cells_nb[1] * _cells_nb[2];
  _cells = new Cell*[_cells_size];
  memset(_cells, 0, _cells_size * sizeof(*_cells));
}

Cell* FastGrid::getCell(const Vec3u& p) {
    //cout << _cells<< " "<< p << " " <<_cells_nb[0]<<"-"<< _cells_nb[1]<<"-"<< _cells_nb[2]<< " "<<_cells_size<< endl;
    assert(_cells||("_cells is a null pointer"));
    assert((_cells_nb[0] * (p[2] * _cells_nb[1] + p[1]) + p[0])<_cells_size);
    assert(p[0]<_cells_nb[0]);
    assert(p[1]<_cells_nb[1]);
    assert(p[2]<_cells_nb[2]);
    return _cells[_cells_nb[0] * (p[2] * _cells_nb[1] + p[1]) + p[0]];
}

void FastGrid::fillCell(const Vec3u& p, Cell& cell) {
    assert(_cells||("_cells is a null pointer"));
    assert((_cells_nb[0] * (p[2] * _cells_nb[1] + p[1]) + p[0])<_cells_size);
    assert(p[0]<_cells_nb[0]);
    assert(p[1]<_cells_nb[1]);
    assert(p[2]<_cells_nb[2]);
    _cells[_cells_nb[0] * (p[2] * _cells_nb[1] + p[1]) + p[0]] = &cell;
}
