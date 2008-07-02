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

#include "AppGLWidget_manipulatedCameraFrame.h"
#include "AppGLWidget_camera.h"
//#include "qglviewer.h"

// #if QT_VERSION >= 0x040000
// # include <QMouseEvent>
// #endif
// 
// using namespace qglviewer;
using namespace std;

/*! Default constructor.

 flySpeed() is set to 0.0 and flyUpVector() is (0,1,0). The revolveAroundPoint() is set to (0,0,0).

  \attention Created object is removeFromMouseGrabberPool(). */
ManipulatedCameraFrame::ManipulatedCameraFrame()
  : driveSpeed_(0.0), flyUpVector_(0.0, 1.0, 0.0)
{
  setFlySpeed(0.0);
  //removeFromMouseGrabberPool();

  //connect(&flyTimer_, SIGNAL(timeout()), SLOT(flyUpdate()));
}

/*! Equal operator. Calls ManipulatedFrame::operator=() and then copy attributes. */
ManipulatedCameraFrame& ManipulatedCameraFrame::operator=(const ManipulatedCameraFrame& mcf)
{
  ManipulatedFrame::operator=(mcf);

  setFlySpeed(mcf.flySpeed());
  setFlyUpVector(mcf.flyUpVector());

  return *this;
}

/*! Copy constructor. Performs a deep copy of all members using operator=(). */
ManipulatedCameraFrame::ManipulatedCameraFrame(const ManipulatedCameraFrame& mcf)
  : ManipulatedFrame(mcf)
{
  //removeFromMouseGrabberPool();
  (*this)=(mcf);
}


////////////////////////////////////////////////////////////////////////////////

/*! Returns a Quaternion that is a rotation around current camera Y, proportionnal to the horizontal mouse position. */
Quaternion ManipulatedCameraFrame::turnQuaternion(int x, const AppGLWidget_Camera* const camera)
{
  return Quaternion(Vec(0.0, 1.0, 0.0), rotationSensitivity()*(prevPos_.x()-x)/camera->screenWidth());
}

/*! Returns a Quaternion that is the composition of two rotations, inferred from the
  mouse pitch (X axis) and yaw (flyUpVector() axis). */
Quaternion ManipulatedCameraFrame::pitchYawQuaternion(int x, int y, const AppGLWidget_Camera* const camera)
{
  const Quaternion rotX(Vec(1.0, 0.0, 0.0), rotationSensitivity()*(prevPos_.y()-y)/camera->screenHeight());
  const Quaternion rotY(transformOf(flyUpVector()), rotationSensitivity()*(prevPos_.x()-x)/camera->screenWidth());
  return rotY * rotX;
}
