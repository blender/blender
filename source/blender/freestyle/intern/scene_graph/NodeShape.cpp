
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

#include "NodeShape.h"

NodeShape::~NodeShape()
{
  vector<Rep *>::iterator rep;
 
  if(0 != _Shapes.size())
  {
    for(rep=_Shapes.begin(); rep!=_Shapes.end(); rep++)
    {
      int refCount = (*rep)->destroy();
      if(0 == refCount)
        delete (*rep);
    }

    _Shapes.clear();
  }
}

void NodeShape::accept(SceneVisitor& v) {
  v.visitNodeShape(*this);
  
  v.visitFrsMaterial(_FrsMaterial);
 
  v.visitNodeShapeBefore(*this);
  vector<Rep *>::iterator rep;
  for(rep = _Shapes.begin(); rep != _Shapes.end(); rep++)
    (*rep)->accept(v);
  v.visitNodeShapeAfter(*this);
}
