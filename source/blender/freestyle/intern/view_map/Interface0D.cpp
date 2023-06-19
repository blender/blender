/* SPDX-FileCopyrightText: 2014-2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

extern "C" {
#include <Python.h>
}

#include "Interface0D.h"

#include "BLI_utildefines.h"

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

Geometry::Vec3r Interface0D::getPoint3D() const
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

Geometry::Vec2r Interface0D::getPoint2D() const
{
  PyErr_SetString(PyExc_TypeError, "method getPoint2D() not properly overridden");
  return 0;
}

FEdge *Interface0D::getFEdge(Interface0D & /*element*/)
{
  PyErr_SetString(PyExc_TypeError, "method getFEdge() not properly overridden");
  return nullptr;
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

SVertex *Interface0D::castToSVertex()
{
  PyErr_SetString(PyExc_TypeError, "method castToSVertex() not properly overridden");
  return nullptr;
}

ViewVertex *Interface0D::castToViewVertex()
{
  PyErr_SetString(PyExc_TypeError, "method castToViewVertex() not properly overridden");
  return nullptr;
}

NonTVertex *Interface0D::castToNonTVertex()
{
  PyErr_SetString(PyExc_TypeError, "method castToNonTVertex() not properly overridden");
  return nullptr;
}

TVertex *Interface0D::castToTVertex()
{
  PyErr_SetString(PyExc_TypeError, "method castToTVertex() not properly overridden");
  return nullptr;
}

} /* namespace Freestyle */
