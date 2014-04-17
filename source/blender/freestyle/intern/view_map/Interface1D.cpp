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

/** \file blender/freestyle/intern/view_map/Interface1D.cpp
 *  \ingroup freestyle
 */

extern "C" {
#include <Python.h>
}

#include "Interface1D.h"

namespace Freestyle {

Interface0DIterator Interface1D::verticesBegin()
{
	PyErr_SetString(PyExc_TypeError, "method verticesBegin() not properly overridden");
	return Interface0DIterator();
}

Interface0DIterator Interface1D::verticesEnd()
{
	PyErr_SetString(PyExc_TypeError, "method verticesEnd() not properly overridden");
	return Interface0DIterator();
}

Interface0DIterator Interface1D::pointsBegin(float t)
{
	PyErr_SetString(PyExc_TypeError, "method pointsBegin() not properly overridden");
	return Interface0DIterator();
}

Interface0DIterator Interface1D::pointsEnd(float t)
{
	PyErr_SetString(PyExc_TypeError, "method pointsEnd() not properly overridden");
	return Interface0DIterator();
}

real Interface1D::getLength2D() const
{
	PyErr_SetString(PyExc_TypeError, "method getLength2D() not properly overridden");
	return 0;
}

Id Interface1D::getId() const
{
	PyErr_SetString(PyExc_TypeError, "method getId() not properly overridden");
	return Id(0, 0);
}

Nature::EdgeNature Interface1D::getNature() const
{
	PyErr_SetString(PyExc_TypeError, "method getNature() not properly overridden");
	return Nature::NO_FEATURE;
}

} /* namespace Freestyle */
