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

/** \file blender/freestyle/intern/view_map/Interface0D.cpp
 *  \ingroup freestyle
 */

extern "C" {
#include <Python.h>
}

#include "Interface0D.h"

namespace Freestyle {

real Interface0D::getX() const
{
	PyErr_SetString(PyExc_TypeError, "method getX() not properly overridden");
	return 0;
}

real Interface0D::getY() const
{
	PyErr_SetString(PyExc_TypeError, "method getY() not properly overridden");
	return 0;
}

real Interface0D::getZ() const
{
	PyErr_SetString(PyExc_TypeError, "method getZ() not properly overridden");
	return 0;
}

Geometry::Vec3f Interface0D::getPoint3D() const
{
	PyErr_SetString(PyExc_TypeError, "method getPoint3D() not properly overridden");
	return 0;
}

real Interface0D::getProjectedX() const
{
	PyErr_SetString(PyExc_TypeError, "method getProjectedX() not properly overridden");
	return 0;
}

real Interface0D::getProjectedY() const
{
	PyErr_SetString(PyExc_TypeError, "method getProjectedY() not properly overridden");
	return 0;
}

real Interface0D::getProjectedZ() const
{
	PyErr_SetString(PyExc_TypeError, "method getProjectedZ() not properly overridden");
	return 0;
}

Geometry::Vec2f Interface0D::getPoint2D() const
{
	PyErr_SetString(PyExc_TypeError, "method getPoint2D() not properly overridden");
	return 0;
}

FEdge * Interface0D::getFEdge(Interface0D&)
{
	PyErr_SetString(PyExc_TypeError, "method getFEdge() not properly overridden");
	return 0;
}

Id Interface0D::getId() const
{
	PyErr_SetString(PyExc_TypeError, "method getId() not properly overridden");
	return 0;
}

Nature::VertexNature Interface0D::getNature() const
{
	PyErr_SetString(PyExc_TypeError, "method getNature() not properly overridden");
	return Nature::POINT;
}

SVertex * Interface0D::castToSVertex()
{
	PyErr_SetString(PyExc_TypeError, "method castToSVertex() not properly overridden");
	return 0;
}

ViewVertex * Interface0D::castToViewVertex()
{
	PyErr_SetString(PyExc_TypeError, "method castToViewVertex() not properly overridden");
	return 0;
}

NonTVertex * Interface0D::castToNonTVertex()
{
	PyErr_SetString(PyExc_TypeError, "method castToNonTVertex() not properly overridden");
	return 0;
}

TVertex * Interface0D::castToTVertex()
{
	PyErr_SetString(PyExc_TypeError, "method castToTVertex() not properly overridden");
	return 0;
}

} /* namespace Freestyle */
