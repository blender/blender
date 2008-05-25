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

#ifndef QGLVIEWER_CAMERA_H
#define QGLVIEWER_CAMERA_H

#include "manipulatedCameraFrame.h"

  /*! \brief A perspective or orthographic camera.
  \class Camera camera.h QGLViewer/camera.h

  A Camera defines some intrinsic parameters (fieldOfView(), position(), viewDirection(),
  upVector()...) and useful positioning tools that ease its placement (showEntireScene(),
  fitSphere(), lookAt()...). It exports its associated OpenGL projection and modelview matrices and
  can interactively be modified using the mouse.

  <h3>Mouse manipulation</h3>

  The position() and orientation() of the Camera are defined by a ManipulatedCameraFrame (retrieved
  using frame()). These methods are just convenient wrappers to the equivalent Frame methods. This
  also means that the Camera frame() can be attached to a Frame::referenceFrame() which enables
  complex Camera setups.

  Different displacements can be performed using the mouse. The list of possible actions is defined
  by the QGLViewer::MouseAction enum. Use QGLViewer::setMouseBinding() to attach a specific action
  to an arbitrary mouse button-state key binding. These actions are detailed in the <a
  href="../mouse.html">mouse page</a>.

  The default button binding are: QGLViewer::ROTATE (left), QGLViewer::ZOOM (middle) and
  QGLViewer::TRANSLATE (right). With this configuration, the Camera \e observes a scene and rotates
  around its revolveAroundPoint(). You can switch between this mode and a fly mode using the
  QGLViewer::CAMERA_MODE (see QGLViewer::toggleCameraMode()) keyboard shortcut (default is 'Space').

  <h3>Other functionalities</h3>

  The type() of the Camera can be Camera::ORTHOGRAPHIC or Camera::PERSPECTIVE (see Type()).
  fieldOfView() is meaningless with Camera::ORTHOGRAPHIC.

  The near and far planes of the Camera are fitted to the scene and determined from
  QGLViewer::sceneRadius(), QGLViewer::sceneCenter() and zClippingCoefficient() by the zNear() and
  zFar() methods. Reasonable values on the scene extends hence have to be provided to the QGLViewer
  in order for the Camera to correctly display the scene. High level positioning methods also use
  this information (showEntireScene(), centerScene()...).

  A Camera holds KeyFrameInterpolator that can be used to save Camera positions and paths. You can
  interactively addKeyFrameToPath() to a given path using the default \c Alt+F[1-12] shortcuts. Use
  playPath() to make the Camera follow the path (default shortcut is F[1-12]). See the <a
  href="../keyboard.html">keyboard page</a> for details on key customization.

  Use cameraCoordinatesOf() and worldCoordinatesOf() to convert to and from the Camera frame()
  coordinate system. projectedCoordinatesOf() and unprojectedCoordinatesOf() will convert from
  screen to 3D coordinates. convertClickToLine() is very useful for analytical object selection.

  Stereo display is possible on machines with quad buffer capabilities (with Camera::PERSPECTIVE
  type() only). Test the <a href="../examples/stereoViewer.html">stereoViewer example</a> to check.

  A Camera can also be used outside of a QGLViewer or even without OpenGL for its coordinate system
  conversion capabilities. Note however that some of them explicitly rely on the presence of a
  Z-buffer. \nosubgrouping */
  class Camera
  {

  public:
    Camera();
    virtual ~Camera();

    Camera(const Camera& camera);
    Camera& operator=(const Camera& camera);


    /*! Enumerates the two possible types of Camera.

    See type() and setType(). This type mainly defines different Camera projection matrix (see
    loadProjectionMatrix()). Many other methods (pointUnderPixel(), convertClickToLine(),
    projectedCoordinatesOf(), pixelGLRatio()...) take this Type into account. */
    enum Type { PERSPECTIVE, ORTHOGRAPHIC };

    /*! @name Position and orientation */
    //@{
  public:
    /*! Returns the Camera position (the eye), defined in the world coordinate system.

    Use setPosition() to set the Camera position. Other convenient methods are showEntireScene() or
    fitSphere(). Actually returns \c frame()->position().

    This position corresponds to the projection center of a Camera::PERSPECTIVE Camera. It is not
    located in the image plane, which is at a zNear() distance ahead. */
    Vec position() const { return frame()->position(); };

    /*! Returns the normalized up vector of the Camera, defined in the world coordinate system.

    Set using setUpVector() or setOrientation(). It is orthogonal to viewDirection() and to
    rightVector().

    It corresponds to the Y axis of the associated frame() (actually returns
    frame()->inverseTransformOf(Vec(0.0, 1.0, 0.0)) ). */
    Vec upVector() const
    {
      return frame()->inverseTransformOf(Vec(0.0, 1.0, 0.0));
    }
    /*! Returns the normalized view direction of the Camera, defined in the world coordinate system.

    Change this value using setViewDirection(), lookAt() or setOrientation(). It is orthogonal to
    upVector() and to rightVector().

    This corresponds to the negative Z axis of the frame() ( frame()->inverseTransformOf(Vec(0.0,
    0.0, -1.0)) ). */
    Vec viewDirection() const { return frame()->inverseTransformOf(Vec(0.0, 0.0, -1.0)); };

    /*! Returns the normalized right vector of the Camera, defined in the world coordinate system.

    This vector lies in the Camera horizontal plane, directed along the X axis (orthogonal to
    upVector() and to viewDirection()). Set using setUpVector(), lookAt() or setOrientation().

    Simply returns frame()->inverseTransformOf(Vec(1.0, 0.0, 0.0)). */
    Vec rightVector() const
    {
      return frame()->inverseTransformOf(Vec(1.0, 0.0, 0.0));
    }

    /*! Returns the Camera orientation, defined in the world coordinate system.

    Actually returns \c frame()->orientation(). Use setOrientation(), setUpVector() or lookAt() to
    set the Camera orientation. */
    Quaternion orientation() const { return frame()->orientation(); };

    void setFromModelViewMatrix(const GLdouble* const modelViewMatrix);
    void setFromProjectionMatrix(const float matrix[12]);

  public:
    /*! Sets the Camera position() (the eye), defined in the world coordinate system. */
    void setPosition(const Vec& pos) { frame()->setPosition(pos); };
    void setOrientation(const Quaternion& q);
    void setOrientation(float theta, float phi);
    void setUpVector(const Vec& up, bool noMove=true);
    void setViewDirection(const Vec& direction);
    //@}


    /*! @name Positioning tools */
    //@{
  public:
    void lookAt(const Vec& target);
    void showEntireScene();
    void fitSphere(const Vec& center, float radius);
    void fitBoundingBox(const Vec& min, const Vec& max);
    void centerScene();
    void interpolateToZoomOnPixel(const Point& pixel);
    void interpolateToFitScene();
    void interpolateTo(const Frame& fr, float duration);
    //@}


    /*! @name Frustum */
    //@{
  public:
    /*! Returns the Camera::Type of the Camera.

    Set by setType(). Mainly used by loadProjectionMatrix().

    A Camera::PERSPECTIVE Camera uses a classical projection mainly defined by its fieldOfView().

    With a Camera::ORTHOGRAPHIC type(), the fieldOfView() is meaningless and the width and height of
    the Camera frustum are inferred from the distance to the revolveAroundPoint() using
    getOrthoWidthHeight().

    Both types use zNear() and zFar() (to define their clipping planes) and aspectRatio() (for
    frustum shape). */
    Type type() const { return type_; };

    /*! Returns the vertical field of view of the Camera (in radians).

    Value is set using setFieldOfView(). Default value is pi/4 radians. This value is meaningless if
    the Camera type() is Camera::ORTHOGRAPHIC.

    The field of view corresponds the one used in \c gluPerspective (see manual). It sets the Y
    (vertical) aperture of the Camera. The X (horizontal) angle is inferred from the window aspect
    ratio (see aspectRatio() and horizontalFieldOfView()).

    Use setFOVToFitScene() to adapt the fieldOfView() to a given scene. */
    float fieldOfView() const { return fieldOfView_; };

    /*! Returns the horizontal field of view of the Camera (in radians).

    Value is set using setHorizontalFieldOfView() or setFieldOfView(). These values
    are always linked by:
    \code
    horizontalFieldOfView() = 2.0 * atan ( tan(fieldOfView()/2.0) * aspectRatio() ).
    \endcode */
    float horizontalFieldOfView() const { return 2.0 * atan ( tan(fieldOfView()/2.0) * aspectRatio() ); };

    /*! Returns the Camera aspect ratio defined by screenWidth() / screenHeight().

    When the Camera is attached to a QGLViewer, these values and hence the aspectRatio() are
    automatically fitted to the viewer's window aspect ratio using setScreenWidthAndHeight(). */
    float aspectRatio() const { return static_cast<float>(screenWidth_)/static_cast<float>(screenHeight_); };
    /*! Returns the width (in pixels) of the Camera screen.

    Set using setScreenWidthAndHeight(). This value is automatically fitted to the QGLViewer's
    window dimensions when the Camera is attached to a QGLViewer. See also QGLWidget::width() */
    int screenWidth() const { return screenWidth_; };
    /*! Returns the height (in pixels) of the Camera screen.

    Set using setScreenWidthAndHeight(). This value is automatically fitted to the QGLViewer's
    window dimensions when the Camera is attached to a QGLViewer. See also QGLWidget::height() */
    int screenHeight() const { return screenHeight_; };
    void getViewport(GLint viewport[4]) const;
    float pixelGLRatio(const Vec& position) const;

    /*! Returns the coefficient which is used to set zNear() when the Camera is inside the sphere
    defined by sceneCenter() and zClippingCoefficient() * sceneRadius().

    In that case, the zNear() value is set to zNearCoefficient() * zClippingCoefficient() *
    sceneRadius(). See the zNear() documentation for details.

    Default value is 0.005, which is appropriate for most applications. In case you need a high
    dynamic ZBuffer precision, you can increase this value (~0.1). A lower value will prevent
    clipping of very close objects at the expense of a worst Z precision.

    Only meaningful when Camera type is Camera::PERSPECTIVE. */
    float zNearCoefficient() const { return zNearCoef_; };
    /*! Returns the coefficient used to position the near and far clipping planes.

    The near (resp. far) clipping plane is positioned at a distance equal to zClippingCoefficient() *
    sceneRadius() in front of (resp. behind) the sceneCenter(). This garantees an optimal use of
    the z-buffer range and minimizes aliasing. See the zNear() and zFar() documentations.

    Default value is square root of 3.0 (so that a cube of size sceneRadius() is not clipped).

    However, since the sceneRadius() is used for other purposes (see showEntireScene(), flySpeed(),
    ...) and you may want to change this value to define more precisely the location of the clipping
    planes. See also zNearCoefficient().

    For a total control on clipping planes' positions, an other option is to overload the zNear()
    and zFar() methods. See the <a href="../examples/standardCamera.html">standardCamera example</a>.

    \attention When QGLViewer::cameraPathAreEdited(), this value is set to 5.0 so that the Camera
    paths are not clipped. The previous zClippingCoefficient() value is restored back when you leave
    this mode. */
    float zClippingCoefficient() const { return zClippingCoef_; }

    virtual float zNear() const;
    virtual float zFar()  const;
    virtual void getOrthoWidthHeight(GLdouble& halfWidth, GLdouble& halfHeight) const;
    void getFrustumPlanesCoefficients(GLdouble coef[6][4]) const;

  public:
    void setType(Type type);

    /*! Sets the vertical fieldOfView() of the Camera (in radians).

    Note that focusDistance() is set to sceneRadius() / tan(fieldOfView()/2) by this method. */
    void setFieldOfView(float fov) { fieldOfView_ = fov; setFocusDistance(sceneRadius() / tan(fov/2.0)); };

    /*! Sets the horizontalFieldOfView() of the Camera (in radians).

    horizontalFieldOfView() and fieldOfView() are linked by the aspectRatio(). This method actually
    calls setFieldOfView(( 2.0 * atan (tan(hfov / 2.0) / aspectRatio()) )) so that a call to
    horizontalFieldOfView() returns the expected value. */
    void setHorizontalFieldOfView(float hfov) { setFieldOfView( 2.0 * atan (tan(hfov / 2.0) / aspectRatio()) ); };

    void setFOVToFitScene();

    /*! Defines the Camera aspectRatio().

    This value is actually inferred from the screenWidth() / screenHeight() ratio. You should use
    setScreenWidthAndHeight() instead.

    This method might however be convenient when the Camera is not associated with a QGLViewer. It
    actually sets the screenHeight() to 100 and the screenWidth() accordingly. See also
    setFOVToFitScene().

    \note If you absolutely need an aspectRatio() that does not correspond to your viewer's window
    dimensions, overload loadProjectionMatrix() or multiply the created GL_PROJECTION matrix by a
    scaled diagonal matrix in your QGLViewer::draw() method. */
    void setAspectRatio(float aspect) { setScreenWidthAndHeight(int(100.0*aspect), 100); };

    void setScreenWidthAndHeight(int width, int height);
    /*! Sets the zNearCoefficient() value. */
    void setZNearCoefficient(float coef) { zNearCoef_ = coef; };
    /*! Sets the zClippingCoefficient() value. */
    void setZClippingCoefficient(float coef) { zClippingCoef_ = coef; }
    //@}


    /*! @name Scene radius and center */
    //@{
  public:
    /*! Returns the radius of the scene observed by the Camera.

    You need to provide such an approximation of the scene dimensions so that the Camera can adapt
    its zNear() and zFar() values. See the sceneCenter() documentation.

    See also setSceneBoundingBox().

    Note that QGLViewer::sceneRadius() (resp. QGLViewer::setSceneRadius()) simply call this method
    (resp. setSceneRadius()) on its associated QGLViewer::camera(). */
    float sceneRadius() const { return sceneRadius_; };

    /*! Returns the position of the scene center, defined in the world coordinate system.

    The scene observed by the Camera should be roughly centered on this position, and included in a
    sceneRadius() sphere. This approximate description of the scene permits a zNear() and zFar()
    clipping planes definition, and allows convenient positioning methods such as showEntireScene().

    Default value is (0,0,0) (world origin). Use setSceneCenter() to change it. See also
    setSceneBoundingBox().

    Note that QGLViewer::sceneCenter() (resp. QGLViewer::setSceneCenter()) simply call this method
    (resp. setSceneCenter()) on its associated QGLViewer::camera(). */
    Vec sceneCenter() const { return sceneCenter_; };
    float distanceToSceneCenter() const;

  public:
    void setSceneRadius(float radius);
    void setSceneCenter(const Vec& center);
    bool setSceneCenterFromPixel(const Point& pixel);
    void setSceneBoundingBox(const Vec& min, const Vec& max);
    //@}


    /*! @name Revolve Around Point */
    //@{
 public:
    void setRevolveAroundPoint(const Vec& rap);
    bool setRevolveAroundPointFromPixel(const Point& pixel);

  public:
    /*! The point the Camera revolves around with the QGLViewer::ROTATE mouse binding. Defined in world coordinate system.

    Default value is the sceneCenter().

    \attention setSceneCenter() changes this value. */
    Vec revolveAroundPoint() const { return frame()->revolveAroundPoint(); };
    //@}


    /*! @name Associated frame */
    //@{
  public:
    /*! Returns the ManipulatedCameraFrame attached to the Camera.

    This ManipulatedCameraFrame defines its position() and orientation() and can translate mouse
    events into Camera displacement. Set using setFrame(). */
    ManipulatedCameraFrame* frame() const { return frame_; };
  public:
    void setFrame(ManipulatedCameraFrame* const mcf);
    //@}


    /*! @name KeyFramed paths */
    //@{
   public:
    //KeyFrameInterpolator* keyFrameInterpolator(int i);

public:
    //void setKeyFrameInterpolator(int i, KeyFrameInterpolator* const kfi);

    //virtual void addKeyFrameToPath(int i);
    //virtual void playPath(int i);
    //virtual void deletePath(int i);
    //virtual void resetPath(int i);
    //@}


    /*! @name OpenGL matrices */
    //@{
  public:
    virtual void loadProjectionMatrix(bool reset=true) const;
    virtual void loadModelViewMatrix(bool reset=true) const;
    void computeProjectionMatrix() const;
    void computeModelViewMatrix() const;

    virtual void loadProjectionMatrixStereo(bool leftBuffer=true) const;
    virtual void loadModelViewMatrixStereo(bool leftBuffer=true) const;

    void getProjectionMatrix(GLdouble m[16]) const;
    void getModelViewMatrix(GLdouble m[16]) const;
	void getModelViewProjectionMatrix(GLdouble m[16]) const;

#ifndef DOXYGEN
    // Required for windows which otherwise silently fills
    void getProjectionMatrix(GLfloat m[16]) const;
    void getModelViewMatrix(GLfloat m[16]) const;
#endif
    //@}

    
    /*! @name Drawing */
    //@{
#ifndef DOXYGEN
    static void drawCamera(float scale=1.0, float aspectRatio=1.33, float fieldOfView=M_PI/4.0);
#endif
    virtual void draw(bool drawFarPlane=true, float scale=1.0) const;
    //@}

    
    /*! @name World to Camera coordinate systems conversions */
    //@{
  public:
    /*! Returns the Camera frame coordinates of a point \p src defined in world coordinates.

    worldCoordinatesOf() performs the inverse transformation.

    Note that the point coordinates are simply converted in a different coordinate system. They are
    not projected on screen. Use projectedCoordinatesOf() for that. */
    Vec cameraCoordinatesOf(const Vec& src) const { return frame()->coordinatesOf(src); };
    /*! Returns the world coordinates of the point whose position \p src is defined in the Camera
    coordinate system.

    cameraCoordinatesOf() performs the inverse transformation. */
    Vec worldCoordinatesOf(const Vec& src) const { return frame()->inverseCoordinatesOf(src); };
    void getCameraCoordinatesOf(const float src[3], float res[3]) const;
    void getWorldCoordinatesOf(const float src[3], float res[3]) const;
    //@}


    /*! @name 2D screen to 3D world coordinate systems conversions */
    //@{
  public:
    Vec projectedCoordinatesOf(const Vec& src, const Frame* frame=NULL) const;
    Vec unprojectedCoordinatesOf(const Vec& src, const Frame* frame=NULL) const;
    void getProjectedCoordinatesOf(const float src[3], float res[3], const Frame* frame=NULL) const;
    void getUnprojectedCoordinatesOf(const float src[3], float res[3], const Frame* frame=NULL) const;
    void convertClickToLine(const Point& pixel, Vec& orig, Vec& dir) const;
    Vec pointUnderPixel(const Point& pixel, bool& found) const;
    //@}


    /*! @name Fly speed */
    //@{
  public:
    /*! Returns the fly speed of the Camera.

    Simply returns frame()->flySpeed(). See the ManipulatedCameraFrame::flySpeed() documentation.
    This value is only meaningful when the MouseAction bindings is QGLViewer::MOVE_FORWARD or
    QGLViewer::MOVE_BACKWARD.

    Set to 0.5% of the sceneRadius() by setSceneRadius(). See also setFlySpeed(). */
    float flySpeed() const { return frame()->flySpeed(); };
  public:
    /*! Sets the Camera flySpeed().

    \attention This value is modified by setSceneRadius(). */
    void setFlySpeed(float speed) { frame()->setFlySpeed(speed); };
    //@}


    /*! @name Stereo parameters */
    //@{
  public:
    /*! Returns the user's inter-ocular distance (in meters). Default value is 0.062m, which fits most people.

    loadProjectionMatrixStereo() uses this value to define the Camera offset and frustum. See
    setIODistance(). */
    float IODistance() const { return IODistance_; };

    /*! Returns the physical distance between the user's eyes and the screen (in meters).

    Default value is 0.5m.

    Used by loadModelViewMatrixStereo() and loadProjectionMatrixStereo() for stereo display. Value
    is set using setPhysicalDistanceToScreen().

    physicalDistanceToScreen() and focusDistance() represent the same distance. The first one is
    expressed in physical real world units, while the latter is expressed in OpenGL virtual world
    units. Use their ratio to convert distances between these worlds.

    Use the following code to detect a reality center configuration (using its screen aspect ratio)
    and to automatically set physical distances accordingly:
    \code
    QDesktopWidget screen;
    if (fabs((float)screen.width() / (float)screen.height()) > 2.0)
    {
      camera()->setPhysicalDistanceToScreen(4.0);
      camera()->setPhysicalScreenWidth(10.0);
    }
    \endcode */
    float physicalDistanceToScreen() const { return physicalDistanceToScreen_; };

    /*! Returns the physical screen width, in meters. Default value is 0.4m (average monitor).

    Used for stereo display only (see loadModelViewMatrixStereo() and loadProjectionMatrixStereo()).
    Set using setPhysicalScreenWidth().

    See physicalDistanceToScreen() for reality center automatic configuration. */
    float physicalScreenWidth() const { return physicalScreenWidth_; };

    /*! Returns the focus distance used by stereo display, expressed in OpenGL units.

    This is the distance in the virtual world between the Camera and the plane where the horizontal
    stereo parallax is null (the stereo left and right images are superimposed).

    This distance is the virtual world equivalent of the real-world physicalDistanceToScreen().

    \attention This value is modified by QGLViewer::setSceneRadius(), setSceneRadius() and
    setFieldOfView(). When one of these values is modified, focusDistance() is set to sceneRadius()
    / tan(fieldOfView()/2), which provides good results. */
    float focusDistance() const { return focusDistance_; };
  public:
    /*! Sets the IODistance(). */
    void setIODistance(float distance) { IODistance_ = distance; };

    /*! Sets the physicalDistanceToScreen(). */
    void setPhysicalDistanceToScreen(float distance) { physicalDistanceToScreen_ = distance; };

    /*! Sets the physical screen (monitor or projected wall) width (in meters). */
    void setPhysicalScreenWidth(float width) { physicalScreenWidth_ = width; };

    /*! Sets the focusDistance(), in OpenGL scene units. */
    void setFocusDistance(float distance) { focusDistance_ = distance; };
    //@}


  private:
    // F r a m e
    ManipulatedCameraFrame* frame_;

    // C a m e r a   p a r a m e t e r s
    int screenWidth_, screenHeight_;  // size of the window, in pixels
    float fieldOfView_; // in radians
    Vec sceneCenter_;
    float sceneRadius_; // OpenGL units
    float zNearCoef_;
    float zClippingCoef_;
    float orthoCoef_;
    Type type_; // PERSPECTIVE or ORTHOGRAPHIC
    mutable GLdouble modelViewMatrix_[16]; // Buffered model view matrix.
    mutable GLdouble projectionMatrix_[16]; // Buffered projection matrix.

    // S t e r e o   p a r a m e t e r s
    float IODistance_;		     // inter-ocular distance, in meters
    float focusDistance_;	     // in scene units
    float physicalDistanceToScreen_; // in meters
    float physicalScreenWidth_;	     // in meters

    // P o i n t s   o f   V i e w s   a n d   K e y F r a m e s
    //map<int, KeyFrameInterpolator*> kfi_;
    //KeyFrameInterpolator* interpolationKfi_;
  };


#endif // QGLVIEWER_CAMERA_H
