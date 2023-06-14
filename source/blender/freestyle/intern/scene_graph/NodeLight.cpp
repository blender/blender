/* SPDX-FileCopyrightText: 2012-2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 * \brief Class to represent a light node
 */

#include "NodeLight.h"

namespace Freestyle {

int NodeLight::numberOfLights = 0;

NodeLight::NodeLight()
{
  if (numberOfLights > 7) {
    _number = 7;
  }
  else {
    _number = numberOfLights;
    numberOfLights++;
  }

  Ambient[0] = Ambient[1] = Ambient[2] = 0.0f;
  Ambient[3] = 1.0f;

  for (int i = 0; i < 4; i++) {
    Diffuse[i] = 1.0f;
    Specular[i] = 1.0f;
  }

  Position[0] = Position[1] = Position[3] = 0.0f;
  Position[2] = 1.0f;

  on = true;
}

NodeLight::NodeLight(NodeLight &iBrother) : Node(iBrother)
{
  if (numberOfLights > 7) {
    _number = 7;
  }
  else {
    _number = numberOfLights;
    numberOfLights++;
  }

  for (int i = 0; i < 4; i++) {
    Ambient[i] = iBrother.ambient()[i];
    Diffuse[i] = iBrother.diffuse()[i];
    Specular[i] = iBrother.specular()[i];
    Position[i] = iBrother.position()[i];
  }

  on = iBrother.isOn();
}

void NodeLight::accept(SceneVisitor &v)
{
  v.visitNodeLight(*this);
}

} /* namespace Freestyle */
