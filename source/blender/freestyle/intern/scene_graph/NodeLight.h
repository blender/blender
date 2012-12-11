//
//  Filename         : NodeLight.h
//  Author(s)        : Stephane Grabli
//  Purpose          : Class to represent a light node
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

#ifndef  NODELIGHT_H
# define NODELIGHT_H

# include "../geometry/Geom.h"
# include "../system/FreestyleConfig.h"
# include "Node.h"

using namespace Geometry;

class LIB_SCENE_GRAPH_EXPORT NodeLight : public Node
{
public:

  NodeLight(); 
  NodeLight(NodeLight& iBrother);

  virtual ~NodeLight() {}

  /*! Accept the corresponding visitor */
  virtual void accept(SceneVisitor& v);

  /*! Accessors for the light properties */
  inline const float * ambient() const {return Ambient;}
  inline const float * diffuse() const {return Diffuse;}
  inline const float * specular() const {return Specular;}
  inline const float * position() const {return Position;}
  inline bool isOn() const {return on;}
  inline int number() const {return _number;}
 
private:
  // Data members
  // ============

  /*! on=true, the light is on */
  bool on;

  /*! The color definition */
  float Ambient[4];
  float Diffuse[4];
  float Specular[4];

  /*! Light position. if w = 0, the light is 
   * placed at infinite.
   */
  float Position[4];

  /*! used to manage the number of lights */
  /*! numberOfLights
   *    the number of lights in the scene. 
   *    Initially, 0.
   */
  static int numberOfLights;
  /*! The current lignt number */
  int _number;
};

#endif // NODELIGHT_H
