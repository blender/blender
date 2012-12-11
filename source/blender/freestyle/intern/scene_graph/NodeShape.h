//
//  Filename         : NodeShape.h
//  Author(s)        : Stephane Grabli
//  Purpose          : Class to build a shape node. It contains a Rep, 
//                     which is the shape geometry
//  Date of creation : 25/01/2002
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

#ifndef  NODESHAPE_H
# define NODESHAPE_H

# include <vector>
# include "../system/FreestyleConfig.h"
# include "Node.h"
# include "Rep.h"
# include "../geometry/BBox.h"
# include "../geometry/Geom.h"
# include "FrsMaterial.h"

using namespace std;
using namespace Geometry;

class LIB_SCENE_GRAPH_EXPORT NodeShape : public Node
{
public:
  
  inline NodeShape() : Node() {}
  
  virtual ~NodeShape();

  /*! Adds a Rep to the _Shapes list
   *  The delete of the rep is done 
   *  when it is not used any more by 
   *  the Scene Manager. So, it must not 
   *  be deleted by the caller
   */
  virtual void AddRep(Rep *iRep)
  {
    if(NULL == iRep)
      return;
    _Shapes.push_back(iRep);
    iRep->addRef();
    
    // updates bbox:
    AddBBox(iRep->bbox());
  }

  /*! Accept the corresponding visitor */
  virtual void accept(SceneVisitor& v);

  /*! Sets the shape material */
  inline void setFrsMaterial(const FrsMaterial& iMaterial) { _FrsMaterial = iMaterial; }

  /*! accessors */
  /*! returns the shape's material */
  inline FrsMaterial& frs_material() { return _FrsMaterial; }
  inline const vector<Rep*>& shapes() {return _Shapes;}

private:
  /*! list of shapes */
  vector<Rep*> _Shapes;

  /*! Shape Material */
  FrsMaterial _FrsMaterial;
};

#endif // NODESHAPE_H
