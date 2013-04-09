/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/freestyle/intern/stroke/StrokeIO.cpp
 *  \ingroup freestyle
 *  \brief Functions to manage I/O for the stroke
 *  \author Stephane Grabli
 *  \date 03/02/2004
 */

#include "StrokeAdvancedIterators.h"

#include "StrokeIO.h"

namespace Freestyle {

ostream& operator<<(ostream& out, const StrokeAttribute& iStrokeAttribute)
{
	out << "    StrokeAttribute" << endl;
	out << "      color     : (" << iStrokeAttribute.getColorR() << "," << iStrokeAttribute.getColorG() <<
	       "," << iStrokeAttribute.getColorB() << ")" << endl;
	out << "      alpha     : " << iStrokeAttribute.getAlpha() << endl;
	out << "      thickness : " << iStrokeAttribute.getThicknessR() << ", " << iStrokeAttribute.getThicknessL() <<
	       endl;
	out << "      visible   : " << iStrokeAttribute.isVisible() << endl;
	return out;
}

ostream& operator<<(ostream& out, const StrokeVertex& iStrokeVertex)
{
	out << "  StrokeVertex" << endl;
	out << "    id                 : " << iStrokeVertex.getId() << endl;
	out << "    curvilinear length : " << iStrokeVertex.curvilinearAbscissa() << endl;
	out << "    2d coordinates     : (" << iStrokeVertex.getProjectedX() << "," << iStrokeVertex.getProjectedY() <<
	       "," << iStrokeVertex.getProjectedZ() << ")" << endl;
	out << "    3d coordinates     : (" << iStrokeVertex.getX() << "," << iStrokeVertex.getY() <<
	       "," << iStrokeVertex.getZ() << ")" << endl;
	out << iStrokeVertex.attribute() << endl;
	return out;
}

ostream& operator<<(ostream& out, const Stroke& iStroke)
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
