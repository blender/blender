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

#ifndef QGLVIEWER_MANIPULATED_CAMERA_FRAME_H
#define QGLVIEWER_MANIPULATED_CAMERA_FRAME_H

#include "AppGLWidget_manipulatedFrame.h"

//namespace qglviewer {
  /*! \brief The ManipulatedCameraFrame class represents a ManipulatedFrame with Camera specific mouse bindings.
  \class ManipulatedCameraFrame manipulatedCameraFrame.h QGLViewer/manipulatedCameraFrame.h

  A ManipulatedCameraFrame is a specialization of a ManipulatedFrame, designed to be set as the
  Camera::frame(). Mouse motions are basically interpreted in a negated way: when the mouse goes to
  the right, the ManipulatedFrame translation goes to the right, while the ManipulatedCameraFrame
  has to go to the \e left, so that the \e scene seems to move to the right.

  A ManipulatedCameraFrame rotates around its revolveAroundPoint(), which corresponds to the
  associated Camera::revolveAroundPoint().

  A ManipulatedCameraFrame can also "fly" in the scene. It basically moves forward, and turns
  according to the mouse motion. See flySpeed(), flyUpVector() and the QGLViewer::MOVE_FORWARD and
  QGLViewer::MOVE_BACKWARD QGLViewer::MouseAction.

  See the <a href="../mouse.html">mouse page</a> for a description of the possible actions that can
  be performed using the mouse and their bindings.
  \nosubgrouping */
  class ManipulatedCameraFrame : public ManipulatedFrame
  {
#ifndef DOXYGEN
    friend class Camera;
    //friend class ::QGLViewer;
#endif

    //Q_OBJECT

  public:
    ManipulatedCameraFrame();
    /*! Virtual destructor. Empty. */
    virtual ~ManipulatedCameraFrame() {};

    ManipulatedCameraFrame(const ManipulatedCameraFrame& mcf);
    ManipulatedCameraFrame& operator=(const ManipulatedCameraFrame& mcf);

    /*! @name Revolve around point */
    //@{
  public:
    /*! Returns the point the ManipulatedCameraFrame revolves around when rotated.

    It is defined in the world coordinate system. Default value is (0,0,0).

    When the ManipulatedCameraFrame is associated to a Camera, Camera::revolveAroundPoint() also
    returns this value. This point can interactively be changed using the mouse (see
    QGLViewer::RAP_FROM_PIXEL and QGLViewer::RAP_IS_CENTER in the <a href="../mouse.html">mouse
    page</a>). */
    Vec revolveAroundPoint() const { return revolveAroundPoint_; }
    /*! Sets the revolveAroundPoint(), defined in the world coordinate system. */
    void setRevolveAroundPoint(const Vec& revolveAroundPoint) { revolveAroundPoint_ = revolveAroundPoint; }
    //@}

    /*! @name Fly parameters */
    //@{
  public: //slots:
    /*! Sets the flySpeed(), defined in OpenGL units.

    Default value is 0.0, but it is modified according to the QGLViewer::sceneRadius() when the
    ManipulatedCameraFrame is set as the Camera::frame(). */
    void setFlySpeed(float speed) { flySpeed_ = speed; };

    /*! Sets the flyUpVector(), defined in the world coordinate system.

    Default value is (0,1,0), but it is updated by the Camera when set as its Camera::frame(). Use
    Camera::setUpVector() instead in that case. */
    void setFlyUpVector(const Vec& up) { flyUpVector_ = up; };

  public:
    /*! Returns the fly speed, expressed in OpenGL units.

    It corresponds to the incremental displacement that is periodically applied to the
    ManipulatedCameraFrame position when a QGLViewer::MOVE_FORWARD or QGLViewer::MOVE_BACKWARD
    QGLViewer::MouseAction is proceeded.

    \attention When the ManipulatedCameraFrame is set as the Camera::frame(), this value is set
    according to the QGLViewer::sceneRadius() by QGLViewer::setSceneRadius(). */
    float flySpeed() const { return flySpeed_; };

    /*! Returns the up vector used in fly mode, expressed in the world coordinate system.

    Fly mode corresponds to the QGLViewer::MOVE_FORWARD and QGLViewer::MOVE_BACKWARD
    QGLViewer::MouseAction bindings. In these modes, horizontal displacements of the mouse rotate
    the ManipulatedCameraFrame around this vector. Vertical displacements rotate always around the
    Camera \c X axis.

    Default value is (0,1,0), but it is updated by the Camera when set as its Camera::frame().
    Camera::setOrientation() and Camera::setUpVector()) modify this value and should be used
    instead. */
    Vec flyUpVector() const { return flyUpVector_; };
    //@}

    /*! @name Mouse event handlers */
    //@{
  // protected:
  //   virtual void mouseReleaseEvent(QMouseEvent* const event, Camera* const camera);
  //   virtual void mouseMoveEvent   (QMouseEvent* const event, Camera* const camera);
  //   virtual void wheelEvent       (QWheelEvent* const event, Camera* const camera);
  //   //@}

    /*! @name Spinning */
    //@{
  // protected slots:
  //   virtual void spin();
    //@}

    /*! @name XML representation */
    //@{
  // public:
  //   virtual QDomElement domElement(const QString& name, QDomDocument& document) const;
  // public slots:
  //   virtual void initFromDOMElement(const QDomElement& element);
  //   //@}

// #ifndef DOXYGEN
//   protected:
//     virtual void startAction(int ma, bool withConstraint=true); // int is really a QGLViewer::MouseAction
// #endif

  private: //slots:
    //virtual void flyUpdate();

  private:
    void updateFlyUpVector();
    Quaternion turnQuaternion(int x, const Camera* const camera);
    Quaternion pitchYawQuaternion(int x, int y, const Camera* const camera);

  private:
    // Fly mode data
    float flySpeed_;
    float driveSpeed_;
    Vec flyUpVector_;
    //QTimer flyTimer_;

    Vec revolveAroundPoint_;
  };

//} // namespace qglviewer

#endif // QGLVIEWER_MANIPULATED_CAMERA_FRAME_H
