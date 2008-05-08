
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

#include "NodeLight.h"

int NodeLight::numberOfLights = 0;

NodeLight::NodeLight()
: Node() 
{
  if(numberOfLights > 7)
  {
    _number = 7;
  }
  else
  {
    _number = numberOfLights; 
    numberOfLights++;
  }
  
  Ambient[0] = Ambient[1] = Ambient[2] = 0.f;
  Ambient[3] = 1.f;
  
  for(int i=0; i<4; i++)
  {
    Diffuse[i] = 1.f;
    Specular[i] = 1.f;
  }

  Position[0] = Position[1] = Position[3] = 0.f;
  Position[2] = 1.f;

  on = true;
}

NodeLight::NodeLight(NodeLight& iBrother)
: Node(iBrother)
{
    if(numberOfLights > 7)
  {
    _number = 7;
  }
  else
  {
    _number = numberOfLights; 
    numberOfLights++;
  }

  for(int i=0; i<4; i++)
  {
    Ambient[i] = iBrother.ambient()[i];
    Diffuse[i] = iBrother.diffuse()[i];
    Specular[i] = iBrother.specular()[i];
    Position[i] = iBrother.position()[i];
  }

  on = iBrother.isOn();
}

void NodeLight::accept(SceneVisitor& v) {
  v.visitNodeLight(*this);
}
