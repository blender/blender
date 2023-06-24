/* SPDX-FileCopyrightText: 2009-2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 * \brief Functions to manage I/O for the stroke
 */

#include "StrokeAdvancedIterators.h"

#include "StrokeIO.h"

namespace Freestyle {

ostream &operator<<(ostream &out, const StrokeAttribute &iStrokeAttribute)
{
  out << "    StrokeAttribute" << endl;
  out << "      color     : (" << iStrokeAttribute.getColorR() << ","
      << iStrokeAttribute.getColorG() << "," << iStrokeAttribute.getColorB() << ")" << endl;
  out << "      alpha     : " << iStrokeAttribute.getAlpha() << endl;
  out << "      thickness : " << iStrokeAttribute.getThicknessR() << ", "
      << iStrokeAttribute.getThicknessL() << endl;
  out << "      visible   : " << iStrokeAttribute.isVisible() << endl;
  return out;
}

ostream &operator<<(ostream &out, const StrokeVertex &iStrokeVertex)
{
  out << "  StrokeVertex" << endl;
  out << "    id                 : " << iStrokeVertex.getId() << endl;
  out << "    curvilinear length : " << iStrokeVertex.curvilinearAbscissa() << endl;
  out << "    2d coordinates     : (" << iStrokeVertex.getProjectedX() << ","
      << iStrokeVertex.getProjectedY() << "," << iStrokeVertex.getProjectedZ() << ")" << endl;
  out << "    3d coordinates     : (" << iStrokeVertex.getX() << "," << iStrokeVertex.getY() << ","
      << iStrokeVertex.getZ() << ")" << endl;
  out << iStrokeVertex.attribute() << endl;
  return out;
}

ostream &operator<<(ostream &out, const Stroke &iStroke)
{
  out << "Stroke" << endl;
  out << "  id          : " << iStroke.getId() << endl;
  out << "  length      : " << iStroke.getLength2D() << endl;
  out << "  medium type : " << iStroke.getMediumType() << endl;
  for (Stroke::const_vertex_iterator v = iStroke.vertices_begin(), vend = iStroke.vertices_end();
       v != vend;
       ++v)
  {
    out << *(*v) << endl;
  }
  return out;
}

} /* namespace Freestyle */
