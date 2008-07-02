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

#include "AppGLWidget_manipulatedFrame.h"
//#include "qglviewer.h"
#include "AppGLWidget_camera.h"

//using namespace qglviewer;
using namespace std;

/*! Default constructor.

  The translation is set to (0,0,0), with an identity rotation (0,0,0,1) (see Frame constructor
  for details).

  The different sensitivities are set to their default values (see rotationSensitivity(),
  translationSensitivity(), spinningSensitivity() and wheelSensitivity()). */
ManipulatedFrame::ManipulatedFrame()
{
  // #CONNECTION# initFromDOMElement and accessor docs
  setRotationSensitivity(1.0f);
  setTranslationSensitivity(1.0f);
  setSpinningSensitivity(0.3f);
  setWheelSensitivity(1.0f);

  isSpinning_ = false;
  previousConstraint_ = false;

  //connect(&spinningTimer_, SIGNAL(timeout()), SLOT(spinUpdate()));
}

/*! Equal operator. Calls Frame::operator=() and then copy attributes. */
ManipulatedFrame& ManipulatedFrame::operator=(const ManipulatedFrame& mf)
{
  Frame::operator=(mf);

  setRotationSensitivity(mf.rotationSensitivity());
  setTranslationSensitivity(mf.translationSensitivity());
  setSpinningSensitivity(mf.spinningSensitivity());
  setWheelSensitivity(mf.wheelSensitivity());

  mouseSpeed_ = 0.0;
  dirIsFixed_ = false;
  keepsGrabbingMouse_ = false;

  return *this;
}

/*! Copy constructor. Performs a deep copy of all attributes using operator=(). */
ManipulatedFrame::ManipulatedFrame(const ManipulatedFrame& mf)
  : Frame(mf)
{
  (*this)=mf;
}



////////////////////////////////////////////////////////////////////////////////

/*! Returns "pseudo-distance" from (x,y) to ball of radius size.
\arg for a point inside the ball, it is proportional to the euclidean distance to the ball
\arg for a point outside the ball, it is proportional to the inverse of this distance (tends to
zero) on the ball, the function is continuous. */
static float projectOnBall(float x, float y)
{
  // If you change the size value, change angle computation in deformedBallQuaternion().
  const float size       = 1.0f;
  const float size2      = size*size;
  const float size_limit = size2*0.5;

  const float d = x*x + y*y;
  return d < size_limit ? sqrt(size2 - d) : size_limit/sqrt(d);
}

#ifndef DOXYGEN
/*! Returns a quaternion computed according to the mouse motion. Mouse positions are projected on a
deformed ball, centered on (\p cx,\p cy). */
Quaternion ManipulatedFrame::deformedBallQuaternion(int x, int y, float cx, float cy, const AppGLWidget_Camera* const camera)
{
  // Points on the deformed ball
  float px = rotationSensitivity() * (prevPos_.x()  - cx) / camera->screenWidth();
  float py = rotationSensitivity() * (cy - prevPos_.y())  / camera->screenHeight();
  float dx = rotationSensitivity() * (x - cx)	    / camera->screenWidth();
  float dy = rotationSensitivity() * (cy - y)	    / camera->screenHeight();

  const Vec p1(px, py, projectOnBall(px, py));
  const Vec p2(dx, dy, projectOnBall(dx, dy));
  // Approximation of rotation angle
  // Should be divided by the projectOnBall size, but it is 1.0
  const Vec axis = cross(p2,p1);
  const float angle = 2.0 * asin(sqrt(axis.squaredNorm() / p1.squaredNorm() / p2.squaredNorm()));
  return Quaternion(axis, angle);
}
#endif // DOXYGEN
