# Blender.Curve module and the Curve PyType object

"""
The Blender.Curve submodule.

Curve Data
==========

This module provides access to B{Curve Data} objects in Blender.

A Blender Curve can consist of multiple curves. Try converting a Text object to a Curve to see an example of this.   Each curve is of
type Bezier or Nurb.  The underlying curves can be accessed with
the [] operator.  Operator [] returns an object of type CurNurb.

The Curve module also supports the Python iterator interface.  This means you can access the curves in a Curve or the control points in a CurNurb using a python for statement.


Add a Curve to a Scene Example::
  from Blender import Curve, Object, Scene
  c = Curve.New()             # create new  curve data
  cur = Scene.getCurrent()    # get current scene
  ob = Object.New('Curve')    # make curve object
  ob.link(c)                  # link curve data with this object
  cur.link(ob)                # link object into scene

Iterator Example::
  ob = Object.GetSelected()[0]
  curve = ob.getData()
  for cur in curve:
    print type( cur ), cur
    for point in cur:
      print type( point ), point


"""

def New ( name = 'CurData'):
    """
  Create a new Curve Data object.
  @type name: string
  @param name: The Curve Data name.
  @rtype: Blender Curve
  @return: The created Curve Data object.
  """

def Get (name = None):
  """
  Get the Curve Data object(s) from Blender.
  @type name: string
  @param name: The name of the Curve Data.
  @rtype: Blender Curve or a list of Blender Curves
  @return: It depends on the 'name' parameter:
        - (name): The Curve Data object with the given name;
        - ():     A list with all Curve Data objects in the current scene.
  """

class Curve:
  """
  The Curve Data object
  =====================
  This object gives access to Curve-specific data in Blender.
  
  @cvar name: The Curve Data name.
  @cvar pathlen: The Curve Data path length.
  @cvar totcol: The Curve Data maximal number of linked materials.
  @cvar flag: The Curve Data flag value; see function getFlag for the semantics.
  @cvar bevresol: The Curve Data bevel resolution.
  @cvar resolu: The Curve Data U-resolution.
  @cvar resolv: The Curve Data V-resolution.
  @cvar width: The Curve Data width.
  @cvar ext1: The Curve Data extent 1(for bevels).
  @cvar ext2: The Curve Data extent2 (for bevels).
  @cvar loc: The Curve Data location(from the center).
  @cvar rot: The Curve Data rotation(from the center).
  @cvar size: The Curve Data size(from the center).
  @cvar bevob: The Curve Bevel Object
  """

  def getName():
    """
    Get the name of this Curve Data object.
    @rtype: string
    """

  def setName(name):
    """
    Set the name of this Curve Data object.
    @rtype: PyNone
    @type name: string
    @param name: The new name.
    """

  def getPathLen():
    """
    Get this Curve's path length.
    @rtype: int
    @return: the path length.
    """

  def setPathLen(len):
    """
    Set this Curve's path length.
    @rtype: PyNone
    @type len: int
    @param len: the new curve's length.
    """

  def getTotcol():
    """
    Get the number of materials linked to the Curve.
    @rtype: int
    @return: number of materials linked.
    """

  def setTotcol(totcol):
    """
    Set the number of materials linked to the Curve.
    @rtype: PyNone
    @type totcol: int
    @param totcol: number of materials linked.
    """

  def getFlag():
    """
    Get the Curve flag value.   
    This item is a bitfield whose value is a combination of the following parameters.
       - Bit 0 :  "3D" is set
       - Bit 1 :  "Front" is set
       - Bit 2 :  "Back" is set
       - Bit 3 :  "CurvePath" is set.
       - Bit 4 :  "CurveFollow" is set.
      
    @rtype: integer bitfield
    """

  def setFlag(val):
    """
    Set the Curve flag value.  The flag corresponds to the Blender settings for 3D, Front, Back, CurvePath and CurveFollow.  This parameter is a bitfield.
    @rtype: PyNone
    @type val: integer bitfield
    @param val : The Curve's flag bits.  See L{getFlag} for the meaning of the individual bits.
    """

  def getBevresol():
    """
    Get the Curve's bevel resolution value.
    @rtype: float
    """

  def setBevresol(bevelresol):
    """
    Set the Curve's bevel resolution value.
    @rtype: PyNone
    @type bevelresol: float
    @param bevelresol: The new Curve's bevel resolution value.
    """

  def getResolu():
    """
    Get the Curve's U-resolution value.
    @rtype: float
    """

  def setResolu(resolu):
    """
    Set the Curve's U-resolution value.
    @rtype: PyNone
    @type resolu: float
    @param resolu: The new Curve's U-resolution value.
    """

  def getResolv():
    """
    Get the Curve's V-resolution value.
    @rtype: float
    """

  def setResolv(resolv):
    """
    Set the Curve's V-resolution value.
    @rtype: PyNone
    @type resolv: float
    @param resolv: The new Curve's V-resolution value.
    """

  def getWidth():
    """
    Get the Curve's width value.
    @rtype: float
    """

  def setWidth(width):
    """
    Set the Curve's width value. 
    @rtype: PyNone
    @type width: float
    @param width: The new Curve's width value. 
    """

  def getExt1():
    """
    Get the Curve's ext1 value.
    @rtype: float
    """

  def setExt1(ext1):
    """
    Set the Curve's ext1 value. 
    @rtype: PyNone
    @type ext1: float
    @param ext1: The new Curve's ext1 value. 
    """

  def getExt2():
    """
    Get the Curve's ext2 value.
    @rtype: float
    """

  def setExt2(ext2):
    """
    Set the Curve's ext2 value.
    @rtype: PyNone 
    @type ext2: float
    @param ext2: The new Curve's ext2 value. 
    """

  def getControlPoint(numcurve,numpoint):
    """
    Get the curve's control point value. The numpoint arg is an index into the list of points and starts with 0.
    @type numcurve: int
    @type numpoint: int
    @rtype: list of floats
    @return: depends upon the curve's type.
      - type Bezier : a list of nine floats.  Values are x, y, z for handle-1, vertex and handle-2 
      - type Nurb : a list of 4 floats.  Values are x, y, z, w.
    """

  def setControlPoint( numcurve, numpoint, controlpoint):
    """
    Set the Curve's controlpoint value.   The numpoint arg is an index into the list of points and starts with 0.
    @rtype: PyNone
    @type numcurve: int
    @type numpoint: int
    @type controlpoint: list
    @param numcurve: index for spline in Curve, starting from 0
    @param numpoint: index for point in spline, starting from 0
    @param controlpoint: The new controlpoint value.
    See L{getControlPoint} for the length of the list.
    """

  def appendPoint( numcurve, new_control_point ):
    """
      add a new control point to the indicated curve.
      @rtype: PyNone
      @type numcurve: int
      @type new_control_point: list xyzw or BezTriple
      @param numcurve:  index for spline in Curve, starting from 0
      @param new_control_point: depends on curve's type.
        - type Bezier: a BezTriple 
	- type Nurb: a list of four floats for the xyzw values
      @raise AttributeError:  throws exception if numcurve is out of range.
    """

  def appendNurb( new_point ):
      """
      add a new curve to this Curve.  The new point is added to the new curve.  Blender does not support a curve with zero points.  The new curve is added to the end of the list of curves in the Curve.
      @rtype: PyNone
      @return: PyNone
      @type new_point: BezTriple or list of xyzw coordinates for a Nurb curve.
      @param new_point: see L{CurNurb.append} for description of parameter.
      """

  def getLoc():
    """
    Get the curve's location value.
    @rtype: a list of 3 floats.
    """

  def setLoc(location):
    """
    Set the curve's location value.
    @rtype: PyNone 
    @type location: list[3]
    @param location: The new Curve's location values. 
    """

  def getRot():
    """
    Get the curve's rotation value.
    @rtype: a list of 3 floats.
    """

  def setRot(rotation):
    """
    Set the Curve's rotation value. 
    @rtype: PyNone
    @type rotation: list[3]
    @param rotation: The new Curve's rotation values. 
    """

  def getSize():
    """
    Get the curve's size value.
    @rtype: a list of 3 floats.
    """

  def setSize(size):
    """
    Set the curve size value.
    @rtype: PyNone 
    @type size: list[3]
    @param size: The new Curve's size values. 
    """

  def getMaterials():
    """
    Returns a list of materials assigned to the Curve.
    @rtype: list of Material Objects
    @return: list of Material Objects assigned to the Curve.
    """

  def getBevOb():
    """
    Returns the Bevel Object (BevOb) assigned to the Curve.
    @rtype: Blender Object or PyNone
    @return: Bevel Object (BevOb) assigned to the Curve.
    """

  def setBevOb( object ):
    """
    Assign a Bevel Object (BevOb) to the Curve.  Passing None as the object parameter removes the bevel.
    @rtype: PyNone
    @return: PyNone
    @type object: Curve type Blender Object
    @param object: Blender Object to assign as Bevel Object (BevOb)
    @raise TypeError: throws exception if the parameter is not a Curve type Blender Object or PyNone
    """

  def update():
    """
    Updates display list for a Curve.
    Used after making changes to control points.
    
    You B{must} use this if you want to see your changes!
    @rtype: PyNone
    @return: PyNone
    """

  def isNurb( curve_num):
      """
      method used to determine whether a CurNurb is of type Bezier or of type Nurb.
      @rtype: integer
      @return:  Zero if curve is type Bezier, One if curve is of type Nurb.
      @type curve_num: integer
      @param curve_num: zero-based index into list of curves in this Curve.
      @raise AttributeError:  throws exception if curve_num is out of range.
      """

  def isCyclic( curve_num ):
      """
      Boolean method checks whether the curve is cyclic (closed) or not.

      @rtype: boolean
      @return: True if is cyclic, False if not
      @type curve_num: integer
      @param curve_num: zero-based index into list of curves in this Curve
      @raise AttributeError:  throws exception if curve_num is out of range.
      """

class CurNurb:
    """
    The CurNurb Object
    ==================
    This object provides access to the control points of the curves that make up a Blender Curve.

    The CurNurb supports the python iterator protocol which means you can use a python for statement to access the points in a curve.

    The CurNurb also supports the sequence protocol which means you can access the control points of a CurNurb using the [] operator.

    @cvar flagU: The CurNurb knot flag U (0: uniform, 1: endpoints, 2: bezier)
    @cvar flagV: The CurNurb knot flag V (0: uniform, 1: endpoints, 2: bezier)
    """


    def append( new_point ):
      """
      Appends a new point to a curve.  This method appends points to both Bezier and Nurb curves.  The type of the argument must match the type of the curve.  An empty curve will assume the type of the first appended point.
      @rtype: PyNone
      @return: PyNone
      @type new_point: BezTriple or list of 4 floats
      @param new_point: the new point to be appended to the curve.  The new point can be either a BezTriple type or a list of 4 floats in x,y,z,w format for a Nurb curve.
      """

    def setMatIndex( index ):
      """
      Sets the Material index for this CurNurb.
      @rtype: PyNone
      @return: PyNone
      @type index:  integer
      @param index: the new value for the Material number of this CurNurb.  No range checking is done.
      """

    def getMatIndex():
      """
      Returns the Material index for this CurNurb.
      @rtype: integer
      @return: integer
      """

    def isNurb():
      """
      Boolean method used to determine whether a CurNurb is of type Bezier or of type Nurb.
      @rtype: boolean
      @return:  True or False
      """

    def isCyclic():
      """
      Boolean method checks whether a CurNurb is cyclic (a closed curve) or not.
      @rtype: boolean
      @return: True or False
	  """

    def getFlagU():
      """
      Get the CurNurb knot flag U 
      @rtype: integer
      @return: 0 - uniform, 1 - endpoints, 2 - bezier
      """

    def setFlagU( value ):
      """
      Set the CurNurb knot flag U (knots are recalculated automatically)
      @type value: integer
      @param value: CurNurb knot flag (0 - uniform, 1 - endpoints, 2 - bezier)
      @rtype: PyNone
      @return: PyNone
      """

    def getFlagV():
      """
      Get the CurNurb knot flag V 
      @rtype: integer
      @return: 0 - uniform, 1 - endpoints, 2 - bezier
      """

    def setFlagV( value ):
      """
      Set the CurNurb knot flag V (knots are recalculated automatically)
      @type value: integer
      @param value: CurNurb knot flag (0 - uniform, 1 - endpoints, 2 - bezier)
      @rtype: PyNone
      @return: PyNone
      """

    
