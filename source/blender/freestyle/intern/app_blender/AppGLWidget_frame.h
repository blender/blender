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

#ifndef QGLVIEWER_FRAME_H
#define QGLVIEWER_FRAME_H

#include "AppGLWidget_constraint.h"
// #include "GL/gl.h" is now included in config.h for ease of configuration

//namespace qglviewer {
  /*! \brief The Frame class represents a coordinate system, defined by a position and an
  orientation. \class Frame frame.h QGLViewer/frame.h

  A Frame is a 3D coordinate system, represented by a position() and an orientation(). The order of
  these transformations is important: the Frame is first translated \e and \e then rotated around
  the new translated origin.

  A Frame is useful to define the position and orientation of a 3D rigid object, using its matrix()
  method, as shown below:
  \code
  // Builds a Frame at position (0.5,0,0) and oriented such that its Y axis is along the (1,1,1)
  // direction. One could also have used setPosition() and setOrientation().
  Frame fr(Vec(0.5,0,0), Quaternion(Vec(0,1,0), Vec(1,1,1)));
  glPushMatrix();
  glMultMatrixd(fr.matrix());
  // Draw your object here, in the local fr coordinate system.
  glPopMatrix();
  \endcode

  Many functions are provided to transform a 3D point from one coordinate system (Frame) to an
  other: see coordinatesOf(), inverseCoordinatesOf(), coordinatesOfIn(), coordinatesOfFrom()...

  You may also want to transform a 3D vector (such as a normal), which corresponds to applying only
  the rotational part of the frame transformation: see transformOf() and inverseTransformOf(). See
  the <a href="../examples/frameTransform.html">frameTransform example</a> for an illustration.

  The translation() and the rotation() that are encapsulated in a Frame can also be used to
  represent a \e rigid \e transformation of space. Such a transformation can also be interpreted as
  a change of coordinate system, and the coordinate system conversion functions actually allow you
  to use a Frame as a rigid transformation. Use inverseCoordinatesOf() (resp. coordinatesOf()) to
  apply the transformation (resp. its inverse). Note the inversion.

  <h3>Hierarchy of Frames</h3>

  The position and the orientation of a Frame are actually defined with respect to a
  referenceFrame(). The default referenceFrame() is the world coordinate system (represented by a \c
  NULL referenceFrame()). If you setReferenceFrame() to a different Frame, you must then
  differentiate:

  \arg the \e local translation() and rotation(), defined with respect to the referenceFrame(),

  \arg the \e global position() and orientation(), always defined with respect to the world
  coordinate system.

  A Frame is actually defined by its translation() with respect to its referenceFrame(), and then by
  a rotation() of the coordinate system around the new translated origin.

  This terminology for \e local (translation() and rotation()) and \e global (position() and
  orientation()) definitions is used in all the methods' names and should be sufficient to prevent
  ambiguities. These notions are obviously identical when the referenceFrame() is \c NULL, i.e. when
  the Frame is defined in the world coordinate system (the one you are in at the beginning of the
  QGLViewer::draw() method, see the <a href="../introduction.html">introduction page</a>).

  Frames can hence easily be organized in a tree hierarchy, which root is the world coordinate
  system. A loop in the hierarchy would result in an inconsistent (multiple) Frame definition.
  settingAsReferenceFrameWillCreateALoop() checks this and prevents setReferenceFrame() from
  creating such a loop.

  This frame hierarchy is used in methods like coordinatesOfIn(), coordinatesOfFrom()... which allow
  coordinates (or vector) conversions from a Frame to any other one (including the world coordinate
  system).

  However, one must note that this hierarchical representation is internal to the Frame classes.
  When the Frames represent OpenGL coordinates system, one should map this hierarchical
  representation to the OpenGL GL_MODELVIEW matrix stack. See the matrix() documentation for
  details.

  <h3>Constraints</h3>

  An interesting feature of Frames is that their displacements can be constrained. When a Constraint
  is attached to a Frame, it filters the input of translate() and rotate(), and only the resulting
  filtered motion is applied to the Frame. The default constraint() is \c NULL resulting in no
  filtering. Use setConstraint() to attach a Constraint to a frame.

  Constraints are especially usefull for the ManipulatedFrame instances, in order to forbid some
  mouse motions. See the <a href="../examples/constrainedFrame.html">constrainedFrame</a>, <a
  href="../examples/constrainedCamera.html">constrainedCamera</a> and <a
  href="../examples/luxo.html">luxo</a> examples for an illustration.

  Classical constraints are provided for convenience (see LocalConstraint, WorldConstraint and
  CameraConstraint) and new constraints can very easily be implemented.

  <h3>Derived classes</h3>

  The ManipulatedFrame class inherits Frame and implements a mouse motion convertion, so that a
  Frame (and hence an object) can be manipulated in the scene with the mouse.

  \nosubgrouping */
  class Frame
  {

  public:
    Frame();

    /*! Virtual destructor. Empty. */
    virtual ~Frame() {};

    Frame(const Frame& frame);
    Frame& operator=(const Frame& frame);

    /*! This signal is emitted whenever the position() or the orientation() of the Frame is modified.

    Connect this signal to any object that must be notified:
    \code
    QObject::connect(myFrame, SIGNAL(modified()), myObject, SLOT(update()));
    \endcode
    Use the QGLViewer::QGLViewerPool() to connect the signal to all the viewers.

    \note If your Frame is part of a Frame hierarchy (see referenceFrame()), a modification of one
    of the parents of this Frame will \e not emit this signal. Use code like this to change this
    behavior (you can do this recursively for all the referenceFrame() until the \c NULL world root
    frame is encountered):
    \code
    // Emits the Frame modified() signal when its referenceFrame() is modified().
    connect(myFrame->referenceFrame(), SIGNAL(modified()), myFrame, SIGNAL(modified()));
    \endcode

    \attention Connecting this signal to a QGLWidget::updateGL() slot (or a method that calls it)
    will prevent you from modifying the Frame \e inside your QGLViewer::draw() method as it would
    result in an infinite loop. However, QGLViewer::draw() should not modify the scene.

    \note For efficiency reasons, this signal is emitted even if the Frame is not actually modified, for
    instance with translate(Vec(0,0,0)) or setPosition(position()). */
    void modified();

    /*! This signal is emitted when the Frame is interpolated by a KeyFrameInterpolator.

    See the KeyFrameInterpolator documentation for details.

    If a KeyFrameInterpolator is used to successively interpolate several Frames in your scene,
    connect the KeyFrameInterpolator::interpolated() signal instead (identical, but independent of
    the interpolated Frame). */
    void interpolated();

  public:
    /*! @name World coordinates position and orientation */
    //@{
    Frame(const Vec& position, const Quaternion& orientation);

    void setPosition(const Vec& position);
    void setPosition(float x, float y, float z);
    void setPositionWithConstraint(Vec& position);

    void setOrientation(const Quaternion& orientation);
    void setOrientation(double q0, double q1, double q2, double q3);
    void setOrientationWithConstraint(Quaternion& orientation);

    void setPositionAndOrientation(const Vec& position, const Quaternion& orientation);
    void setPositionAndOrientationWithConstraint(Vec& position, Quaternion& orientation);

    /*! Returns the position of the Frame, defined in the world coordinate system. See also
      orientation(), setPosition() and translation(). */
    Vec position() const { return inverseCoordinatesOf(Vec(0.0,0.0,0.0)); };
    Quaternion orientation() const;

    void getPosition(float& x, float& y, float& z) const;
    void getOrientation(double& q0, double& q1, double& q2, double& q3) const;
  //@}


    public:
    /*! @name Local translation and rotation w/r reference Frame */
    //@{
    /*! Sets the translation() of the frame, locally defined with respect to the referenceFrame().
      Emits the modified() signal.

    Use setPosition() to define the world coordinates position(). Use
    setTranslationWithConstraint() to take into account the potential constraint() of the Frame. */
    void setTranslation(const Vec& translation) { t_ = translation; };
    void setTranslation(float x, float y, float z);
    void setTranslationWithConstraint(Vec& translation);

    /*! Set the current rotation Quaternion. See rotation() and the different Quaternion
    constructors. Emits the modified() signal. See also setTranslation() and
    setRotationWithConstraint(). */

    /*! Sets the rotation() of the Frame, locally defined with respect to the referenceFrame().
      Emits the modified() signal.

     Use setOrientation() to define the world coordinates orientation(). The potential
     constraint() of the Frame is not taken into account, use setRotationWithConstraint()
     instead. */
    void setRotation(const Quaternion& rotation) { q_ = rotation; };
    void setRotation(double q0, double q1, double q2, double q3);
    void setRotationWithConstraint(Quaternion& rotation);

    void setTranslationAndRotation(const Vec& translation, const Quaternion& rotation);
    void setTranslationAndRotationWithConstraint(Vec& translation, Quaternion& rotation);

    /*! Returns the Frame translation, defined with respect to the referenceFrame().

    Use position() to get the result in the world coordinates. These two values are identical
    when the referenceFrame() is \c NULL (default).

    See also setTranslation() and setTranslationWithConstraint(). */
    Vec translation() const { return t_; };
    /*! Returns the Frame rotation, defined with respect to the referenceFrame().

    Use orientation() to get the result in the world coordinates. These two values are identical
    when the referenceFrame() is \c NULL (default).

    See also setRotation() and setRotationWithConstraint(). */

    /*! Returns the current Quaternion orientation. See setRotation(). */
    Quaternion rotation() const { return q_; };

    void getTranslation(float& x, float& y, float& z) const;
    void getRotation(double& q0, double& q1, double& q2, double& q3) const;
    //@}

    public:
    /*! @name Frame hierarchy */
    //@{
    /*! Returns the reference Frame, in which coordinates system the Frame is defined.

    The translation() and rotation() of the Frame are defined with respect to the referenceFrame()
    coordinate system. A \c NULL referenceFrame() (default value) means that the Frame is defined in
    the world coordinate system.

    Use position() and orientation() to recursively convert values along the referenceFrame() chain
    and to get values expressed in the world coordinate system. The values match when the
    referenceFrame() is \c NULL.

    Use setReferenceFrame() to set this value and create a Frame hierarchy. Convenient functions
    allow you to convert 3D coordinates from one Frame to an other: see coordinatesOf(),
    localCoordinatesOf(), coordinatesOfIn() and their inverse functions.

    Vectors can also be converted using transformOf(), transformOfIn, localTransformOf() and their
    inverse functions. */
    const Frame* referenceFrame() const { return referenceFrame_; };
    void setReferenceFrame(const Frame* const refFrame);
    bool settingAsReferenceFrameWillCreateALoop(const Frame* const frame);
    //@}


    /*! @name Frame modification */
    //@{
    void translate(Vec& t);
    void translate(const Vec& t);
    // Some compilers complain about "overloading cannot distinguish from previous declaration"
    // Simply comment out the following method and its associated implementation
    void translate(float x, float y, float z);
    void translate(float& x, float& y, float& z);

    void rotate(Quaternion& q);
    void rotate(const Quaternion& q);
    // Some compilers complain about "overloading cannot distinguish from previous declaration"
    // Simply comment out the following method and its associated implementation
    void rotate(double q0, double q1, double q2, double q3);
    void rotate(double& q0, double& q1, double& q2, double& q3);

    void rotateAroundPoint(Quaternion& rotation, const Vec& point);
    void rotateAroundPoint(const Quaternion& rotation, const Vec& point);

    void alignWithFrame(const Frame* const frame, bool move=false, float threshold=0.85f);
    void projectOnLine(const Vec& origin, const Vec& direction);
    //@}


    /*! @name Coordinate system transformation of 3D coordinates */
    //@{
    Vec coordinatesOf(const Vec& src) const;
    Vec inverseCoordinatesOf(const Vec& src) const;
    Vec localCoordinatesOf(const Vec& src) const;
    Vec localInverseCoordinatesOf(const Vec& src) const;
    Vec coordinatesOfIn(const Vec& src, const Frame* const in) const;
    Vec coordinatesOfFrom(const Vec& src, const Frame* const from) const;

    void getCoordinatesOf(const float src[3], float res[3]) const;
    void getInverseCoordinatesOf(const float src[3], float res[3]) const;
    void getLocalCoordinatesOf(const float src[3], float res[3]) const;
    void getLocalInverseCoordinatesOf(const float src[3], float res[3]) const;
    void getCoordinatesOfIn(const float src[3], float res[3], const Frame* const in) const;
    void getCoordinatesOfFrom(const float src[3], float res[3], const Frame* const from) const;
    //@}

    /*! @name Coordinate system transformation of vectors */
    // A frame is as a new coordinate system, defined with respect to a reference frame (the world
    // coordinate system by default, see the "Composition of frame" section).

    // The transformOf() (resp. inverseTransformOf()) functions transform a 3D vector from (resp.
    // to) the world coordinates system. This section defines the 3D vector transformation
    // functions. See the Coordinate system transformation of 3D points above for the transformation
    // of 3D points. The difference between the two sets of functions is simple: for vectors, only
    // the rotational part of the transformations is taken into account, while translation is also
    // considered for 3D points.

    // The length of the resulting transformed vector is identical to the one of the source vector
    // for all the described functions.

    // When local is prepended to the names of the functions, the functions simply transform from
    // (and to) the reference frame.

    // When In (resp. From) is appended to the names, the functions transform from (resp. To) the
    // frame that is given as an argument. The frame does not need to be in the same branch or the
    // hierarchical tree, and can be \c NULL (the world coordinates system).

    // Combining any of these functions with its inverse (in any order) leads to the identity.
    //@{
    Vec transformOf(const Vec& src) const;
    Vec inverseTransformOf(const Vec& src) const;
    Vec localTransformOf(const Vec& src) const;
    Vec localInverseTransformOf(const Vec& src) const;
    Vec transformOfIn(const Vec& src, const Frame* const in) const;
    Vec transformOfFrom(const Vec& src, const Frame* const from) const;

    void getTransformOf(const float src[3], float res[3]) const;
    void getInverseTransformOf(const float src[3], float res[3]) const;
    void getLocalTransformOf(const float src[3], float res[3]) const;
    void getLocalInverseTransformOf(const float src[3], float res[3]) const;
    void getTransformOfIn(const float src[3], float res[3], const Frame* const in) const;
    void getTransformOfFrom(const float src[3], float res[3], const Frame* const from) const;
    //@}


    /*! @name Constraint on the displacement */
    //@{
    /*! Returns the current constraint applied to the Frame.

    A \c NULL value (default) means that no Constraint is used to filter Frame translation and
    rotation. See the Constraint class documentation for details.

    You may have to use a \c dynamic_cast to convert the result to a Constraint derived class. */
    Constraint* constraint() const { return constraint_; }
    /*! Sets the constraint() attached to the Frame.

    A \c NULL value means no constraint. The previous constraint() should be deleted by the calling
    method if needed. */
    void setConstraint(Constraint* const constraint) { constraint_ = constraint; }
    //@}

    /*! @name Associated matrices */
    //@{
  public:
    const GLdouble* matrix() const;
    void getMatrix(GLdouble m[4][4]) const;
    void getMatrix(GLdouble m[16]) const;

    const GLdouble* worldMatrix() const;
    void getWorldMatrix(GLdouble m[4][4]) const;
    void getWorldMatrix(GLdouble m[16]) const;

    void setFromMatrix(const GLdouble m[4][4]);
    void setFromMatrix(const GLdouble m[16]);
    //@}

    /*! @name Inversion of the transformation */
    //@{
    Frame inverse() const;
    /*! Returns the inverse() of the Frame world transformation.

    The orientation() of the new Frame is the Quaternion::inverse() of the original orientation.
    Its position() is the negated and inverse rotated image of the original position.

    The result Frame has a \c NULL referenceFrame() and a \c NULL constraint().

    Use inverse() for a local (i.e. with respect to referenceFrame()) transformation inverse. */
    Frame worldInverse() const { return Frame(-(orientation().inverseRotate(position())), orientation().inverse()); }
    //@}


  private:
    // P o s i t i o n   a n d   o r i e n t a t i o n
    Vec t_;
    Quaternion q_;

    // C o n s t r a i n t s
    Constraint* constraint_;

    // F r a m e   c o m p o s i t i o n
    const Frame* referenceFrame_;
  };

//} // namespace qglviewer

#endif // QGLVIEWER_FRAME_H
