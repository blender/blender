//
//  Filename         : StrokeTesselator.h
//  Author(s)        : Stephane Grabli
//  Purpose          : Class to build a Node Tree designed to be displayed 
//                     from a set of strokes structure.
//  Date of creation : 26/03/2002
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

#ifndef  STROKETESSELATOR_H
# define STROKETESSELATOR_H

# include "../scene_graph/LineRep.h"
# include "Stroke.h"

class StrokeTesselator
{
public:

  inline StrokeTesselator() {_Material.SetDiffuse(0,0,0,1);_overloadMaterial=false;}
  virtual ~StrokeTesselator() {}

  /*! Builds a line rep contained from a Stroke
   */
  LineRep* Tesselate(Stroke* iStroke) ;

  /*! Builds a set of lines rep contained under a 
   *  a NodeShape, itself contained under a NodeGroup from a 
   *  set of strokes
   */
  template<class StrokeIterator>
  NodeGroup* Tesselate(StrokeIterator begin, StrokeIterator end) ;

  
  
  inline void SetMaterial(const Material& iMaterial) {_Material=iMaterial;_overloadMaterial=true;}
  inline const Material& material() const {return _Material;}

private:

  Material _Material;
  bool _overloadMaterial;
};

#endif // STROKETESSELATOR_H

