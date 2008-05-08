//
//  Filename         : OrientedLineRep.h
//  Author(s)        : Stephane Grabli
//  Purpose          : Class to display an oriented line representation.
//  Date of creation : 24/10/2002
//
///////////////////////////////////////////////////////////////////////////////


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

#ifndef  VIEWEDGEREP_H
# define VIEWEDGEREP_H

# include "../system/FreestyleConfig.h"
# include "LineRep.h"

class LIB_SCENE_GRAPH_EXPORT OrientedLineRep : public LineRep
{
public:

  OrientedLineRep() : LineRep() {}
  /*! Builds a single line from 2 vertices
   *  v1
   *    first vertex
   *  v2
   *    second vertex
   */
  inline OrientedLineRep(const Vec3r& v1, const Vec3r& v2)
    : LineRep(v1,v2)
  {}

  /*! Builds a line rep from a vertex chain */
  inline OrientedLineRep(const vector<Vec3r>& vertices)
    : LineRep(vertices)
  {}

  /*! Builds a line rep from a vertex chain */
  inline OrientedLineRep(const list<Vec3r>& vertices)
    : LineRep(vertices)
  {}

  virtual ~OrientedLineRep() {}

  /*! Accept the corresponding visitor */
  virtual void accept(SceneVisitor& v);
};

#endif // VIEWEDGEREP_H
