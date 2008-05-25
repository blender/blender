/****************************************************************************

 Copyright (C) 2002-2007 Gilles Debunne (Gilles.Debunne@imag.fr)

 This file is part of the QGLViewer library.
 Version 2.2.6-3, released on August 28, 2007.

 http://artis.imag.fr/Members/Gilles.Debunne/QGLViewer

 libQGLViewer is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 libQGLViewer is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with libQGLViewer; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*****************************************************************************/

#include "vec.h"

// Most of the methods are declared inline in vec.h
using namespace std;

/*! Projects the Vec on the axis of direction \p direction that passes through the origin.

\p direction does not need to be normalized (but must be non null). */
void Vec::projectOnAxis(const Vec& direction)
{
#ifndef QT_NO_DEBUG
  if (direction.squaredNorm() < 1.0E-10)
    cout << "Vec::projectOnAxis: axis direction is not normalized (norm=" << direction.norm() << ")." << endl;
#endif

  *this = (((*this)*direction) / direction.squaredNorm()) * direction;
}

/*! Projects the Vec on the plane whose normal is \p normal that passes through the origin.

\p normal does not need to be normalized (but must be non null). */
void Vec::projectOnPlane(const Vec& normal)
{
#ifndef QT_NO_DEBUG
  if (normal.squaredNorm() < 1.0E-10)
    cout << "Vec::projectOnPlane: plane normal is not normalized (norm=" << normal.norm() << ")." << endl;
#endif

  *this -= (((*this)*normal) / normal.squaredNorm()) * normal;
}

/*! Returns a Vec orthogonal to the Vec. Its norm() depends on the Vec, but is zero only for a
 null Vec. Note that the function that associates an orthogonalVec() to a Vec is not continous. */
Vec Vec::orthogonalVec() const
{
  // Find smallest component. Keep equal case for null values.
  if ((fabs(y) >= 0.9*fabs(x)) && (fabs(z) >= 0.9*fabs(x)))
    return Vec(0.0, -z, y);
  else
    if ((fabs(x) >= 0.9*fabs(y)) && (fabs(z) >= 0.9*fabs(y)))
      return Vec(-z, 0.0, x);
    else
      return Vec(-y, x, 0.0);
}

ostream& operator<<(ostream& o, const Vec& v)
{
  return o << v.x << '\t' << v.y << '\t' << v.z;
}

