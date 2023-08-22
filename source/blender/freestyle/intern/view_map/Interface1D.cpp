/* SPDX-FileCopyrightText: 2014-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
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

Interface0DIterator Interface1D::pointsBegin(float /*t*/)
{
  PyErr_SetString(PyExc_TypeError, "method pointsBegin() not properly overridden");
  return Interface0DIterator();
}

Interface0DIterator Interface1D::pointsEnd(float /*t*/)
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
