# Blender.IpoCurve module and the IpoCurve PyType object

"""
The Blender.IpoCurve submodule

B{New}: 
  -  IpoCurves supports the operator [], which accesses the value of 
     curves at a given time. 

This module provides access to the IpoCurve data in Blender.  An Ipo is 
composed of several IpoCurves, and an IpoCurve are composed of several 
BezTriples.

@warning: Ipo curves store euler rotations as degrees/10.0 so 180.0 would be 18.0

Example::
  import Blender
  ipo = Blender.Ipo.Get('ObIpo')  # retrieves an Ipo object
  ipo.name = 'ipo1'				 # change the Ipo's name
  icu = ipo[Blender.Ipo.OB_LOCX] # request X Location Ipo curve object
  if icu != None and len(icu.bezierPoints) > 0: # if curve exists and has BezTriple points
     val = icu[2.5]              # get the curve's value at time 2.5
  
@type ExtendTypes: readonly dictionary
@var ExtendTypes: The available IpoCurve extend types.
    - CONST - curve is constant beyond first and last knots
    - EXTRAP - curve maintains same slope beyond first and last knots
    - CYCLIC - curve values repeat beyond first and last knots
    - CYCLIC_EXTRAP - curve values repeat beyond first and last knots,
    but while retaining continuity

@type InterpTypes: readonly dictionary
@var InterpTypes: The available IpoCurve interpolation types.
    - CONST - curve remains constant from current BezTriple knot
    - LINEAR - curve is linearly interpolated between adjacent knots
    - BEZIER - curve is interpolated by a Bezier curve between adjacent knots
"""

class IpoCurve:
  """
  The IpoCurve object
  ===================
  This object gives access to generic data from all Ipo curves objects
  in Blender.

  Important Notes for Rotation Ipo Curves:\n
  For the rotation Ipo curves, the y values for points are in units of 10
  degrees.  For example, 45.0 degrees is stored as 4.50 degrees.  These are the
  same numbers you see in the Transform Properties pop-up menu ( NKey ) in
  the IPO Curve Editor window.  Positive rotations are in a counter-clockwise
  direction, following the standard convention.
  
  @ivar driver:  Status of the driver.  1= on, 0= object, 2= python expression.
  @type driver:  int
  @ivar driverObject:  Object used to drive the Ipo curve.
  @type driverObject:  Blender Object or None
  @ivar driverExpression:  Python expression used to drive the Ipo curve. [0 - 127 chars]
  @type driverExpression:  string
  @ivar sel:  The selection state of this curve.
  @type sel:  bool
  @ivar driverChannel:  Object channel used to drive the Ipo curve.
  Use module constants: IpoCurve.LOC_X, IpoCurve.LOC_Y, IpoCurve.LOC_Z,
  IpoCurve.ROT_X, IpoCurve.ROT_Y, IpoCurve.ROT_Z, IpoCurve.SIZE_X,
  IpoCurve.SIZE_Y, IpoCurve.SIZE_Z
  @type driverChannel:  int 
  @ivar name: The IpoCurve data name.
  @type name: string
  @ivar bezierPoints: The list of the curve's bezier points.
  @type bezierPoints: list of BezTriples.
  @ivar interpolation: The curve's interpolation mode.  See L{InterpTypes} for
  values.
  @type interpolation: int
  @ivar extend: The curve's extend mode. See L{ExtendTypes} for values.

  B{Note}: Cyclic Ipo curves never reach the end value.  If the first and
  last bezier points do not have the same y coordinate, the value of the
  curve when it "cycles" is that of the first point.  If a user wants to
  get the value of the final curve point, read the final point from the
  curve::

		ipo = Blender.Object.Get('Cube').ipo
		icu = ipo['LocX']
		endtime,endvalue = icu.bezierPoints[-1].pt
  @type extend: int
  """

  def __getitem__ (time):
	"""
	Returns the value of the curve at a particular time.
    @type time: float
    @param time: time (Vertex X) on the curve
    @rtype: float
    @return: value (Vertex Y) corresponding to the given time
	"""

  def __setitem__ (time):
	"""
	Sets the value (Vertex Y) of the curve at a particular time.
    @type time: float
    @param time: time (Vertex X) on the curve
	"""

  def setExtrapolation(extendmode):
    """
    Sets the extend mode of the curve (B{deprecated}).  B{Note}: new scripts
    should use the L{extend} attribute instead.
    @type extendmode: string
    @param extendmode: the extend mode of the curve.
        Can be Constant, Extrapolation, Cyclic or Cyclic_extrapolation.
    @rtype: None
    @return: None
    """

  def getExtrapolation():
    """
    Gets the extend mode of the curve (B{deprecated}).  B{Note}: new scripts
    should use the L{extend} attribute instead.
    @rtype: string
    @return: the extend mode of the curve. Can be Constant, Extrapolation, Cyclic or Cyclic_extrapolation.
    """

  def setInterpolation(interpolationtype):
    """
    Sets the interpolation type of the curve (B{deprecated}).  B{Note}: 
    new scripts should use the L{interpolation} attribute instead.
    @type interpolationtype: string
    @param interpolationtype: the interpolation type of the curve. Can be Constant, Bezier, or Linear.
    @rtype: None
    @return: None
    """

  def getInterpolation():
    """
    Gets the interpolation type of the curve (B{deprecated}).  B{Note}:
    new scripts should use the L{interpolation} attribute instead.
    @rtype: string
    @return: the interpolation type of the curve. Can be Constant, Bezier, or Linear.
    """
    
  def append(point):
    """
    Adds a Bezier point to a IpoCurve.
    @type point: BezTriple or tuple of 2 floats 
    @param point: Can either be a BezTriple, or the x and y coordinates of
    the Bezier knot point.
    @rtype: None
    @return: None
    """
 
  def addBezier(coordlist):
    """
    Adds a Bezier point to a curve B{deprecated}). B{Note}: new scripts
    should use L{append} instead.
    @type coordlist: tuple of (at least) 2 floats
    @param coordlist: the x and y coordinates of the new Bezier point.
    @rtype: None
    @return: None
    """

  def delBezier(index):
    """
    Deletes a Bezier point from a curve.
    @type index: integer
    @param index: the index of the Bezier point.  Negative values index from the end of the list.
    @rtype: None
    @return: None
    """

  def recalc():
    """
    Recomputes the curve after changes to control points.
    @rtype: None
    @return: None
    """

  def getName():
    """
    Returns the name of the Ipo curve (B{deprecated}). B{Note}:
    new scripts should use the L{name} attribute instead.
    The name can be:
      1. Camera Ipo:  Lens, ClSta, ClEnd, Apert, FDist.
      2. Material Ipo: R, G, B, SpecR, SpecG, SpecB, MirR, MirG, MirB, Ref,
      Alpha, Emit, Amb, Spec, Hard, SpTra, Ior, Mode, HaSize, Translu,
      RayMir, FresMir, FresMirI, FresTra, FresTraI, TraGlow, OfsX, OfsY,
      OfsZ, SizeX, SizeY, SizeZ, texR, texG, texB, DefVar, Col, Nor, Var,
      Disp.
      3. Object Ipo: LocX, LocY, LocZ, dLocX, dLocY, dLocZ, RotX, RotY, RotZ,
      dRotX, dRotY, dRotZ, SizeX, SizeY, SizeZ, dSizeX, dSizeY, dSizeZ,
      Layer, Time, ColR, ColG, ColB, ColA, FStreng, FFall, Damping,
      RDamp, Perm.
      4. Lamp Ipo: Energ, R, G, B, Dist, SpoSi, SpoBl, Quad1, Quad2, HaInt.
      5. World Ipo: HorR, HorG, HorB, ZenR, ZenG, ZenB, Expos, Misi, MisDi,
      MisSta, MisHi, StaR, StaG, StaB, StarDi, StarSi, OfsX, OfsY, OfsZ,
      SizeX, SizeY, SizeZ, TexR, TexG, TexB, DefVar, Col, Nor, Var.
      5. World Ipo: HorR, HorG, HorB, ZenR, ZenG, ZenB, Expos, Misi, MisDi,
      MisSta, MisHi, StarR, StarB, StarG, StarDi, StarSi, OfsX, OfsY, OfsZ,i
      SizeX, SizeY, SizeZ, texR, texG, texB, DefVar, Col, Nor, Var.
      6. Texture Ipo: NSize, NDepth, NType, Turb, Vnw1, Vnw2, Vnw3, Vnw4,
      MinkMExp, DistM, ColT, iScale, DistA, MgType, MgH, Lacu, Oct,
      MgOff, MgGain, NBase1, NBase2.
      7. Curve Ipo: Speed.
      8. Action Ipo: LocX, LocY, LocZ, SizeX, SizeY, SizeZ, QuatX, QuatY,
      QuatZ, QuatW.
      9. Sequence Ipo: Fac.
      10. Constraint Ipo: Inf.

    @rtype: string
    @return: the name of the Ipo curve.
    """

  def getPoints():
    """
    Returns all the points of the IpoCurve (B{deprecated}).
    B{Note}: new scripts should use the L{bezierPoints} attribute instead.
    @rtype: list of BezTriples
    @return: the points of the Ipo curve.
    """

  def evaluate( time ):
    """
    Compute the value of the Ipo curve at a particular time (B{deprecated}).
    B{Note}: new scripts should use L{icu[time]<__getitem__>} instead.
    @type time: float
    @param time: value along the X axis
    @rtype: float
    @return: the Y value of the curve at the given time
    """

