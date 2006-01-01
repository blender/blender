# Blender.Ipo module and the Ipo PyType object

"""
The Blender.Ipo submodule

B{New}: 
  -  Ipo updates to both the program and Bpython access.
  -  access to Blender's new Ipo driver capabilities .

This module provides access to the Ipo Data in Blender. An Ipo is composed of
several IpoCurves.

A datatype is defined : IpoCurve type. The member functions of this data type
are given below.


Example::
  import Blender
  ob = Blender.Ipo.Get('ipo')    # retrieves an Ipo object
  ob.setName('ipo1')
  print ob.name
  print ipo.getRctf()
  ipo.setRctf(1,2,3,4)
  
"""

def New (type, name):
  """
  Creates a new Ipo.
  @type type: string
  @type name: string
  @param type: The Ipo's blocktype. Depends on the object the Ipo will be
      linked to. Currently supported types are Object, Camera, World,
      Material, Texture, Lamp, Action, Constraint, Sequence, Curve, Key.
  @param name: The name for this Ipo.
  @rtype: Blender Ipo
  @return: The created Ipo.
  """

def Get (name = None):
  """
  Get the Ipo from Blender.
  @type name: string
  @param name: The name of the requested Ipo, or nothing.
  @rtype: Blender Ipo or a list of Blender Ipos
  @return: It depends on the 'name' parameter:
      - (name): The Ipo with the given name;
      - ():     A list with all Ipos in the current scene.
  """

class Ipo:
  """
  The Ipo object
  ==============
  This object gives access to generic data from all objects in Blender.
  It has no attributes.
  """

  def getName():
    """
    Gets the name of the Ipo.
    @rtype: string
    @return: the name of the Ipo.
    """

  def getCurves():
    """
    Gets all the IpoCurves of the Ipo.
    @rtype: list of IpoCurves
    @return: A list (possibly void) containing all the IpoCurves associated to the Ipo object.
    """

  def getCurve(curve):
    """
    Return the specified IpoCurve.  If the curve does not exist in the Ipo,
    None is returned.  I{curve} can be either a string or an integer,
    denoting either the name of the Ipo curve or its internal adrcode.
    The possible Ipo curve names are:
    
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

    The adrcode for the Ipo curve can also be given; currently this is the
    only way to access curves for Key Ipos.  The adrcodes for Key Ipo are
    numbered consecutively starting at 0.
    @type curve : string or int
    @rtype: IpoCurve object
    @return: the corresponding IpoCurve, or None.
    @raise ValueError: I{curve} is not a valid name or adrcode for this Ipo
    type.
    """

  def addCurve(curvename):
    """
    Add a new curve to the Ipo object. The possible values for I{curvename} are:
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

    @type curvename : string
    @rtype: IpoCurve object
    @return: the corresponding IpoCurve, or None.
    @raise ValueError: I{curvename} is not valid or already exists
    """

  def delCurve(curvename):
    """
    Delete an existing curve from the Ipo object. See addCurve() for possible values for curvename.
    @type curvename : string
    @rtype: None
    @return: None.
    """

  def setName(newname):
    """
    Sets the name of the Ipo.
    @type newname: string
    @rtype: None
    @return: None
    """

  def getBlocktype():
    """
    Gets the blocktype of the Ipo.
    @rtype: int
    @return: the blocktype of the Ipo.
    """

  def setBlocktype(newblocktype):
    """
    Sets the blocktype of the Ipo.
    @type newblocktype: int 
    @rtype: None
    @return: None
    @warn: 'newblocktype' should not be changed unless you really know what
       you are doing ...
    """

  def getRctf():
    """
    Gets the rctf of the Ipo.
    Kind of bounding box...
    @rtype: list of floats
    @return: the rctf of the Ipo.
    """

  def setRctf(newrctf):
    """
    Sets the rctf of the Ipo.
    @type newrctf: four floats.
    @rtype: None
    @return: None
    @warn: rctf should not be changed unless you really know what you are
       doing ...
    """

  def getNcurves():
    """
    Gets the number of curves of the Ipo.
    @rtype: int 
    @return: the number of curve of the Ipo.
    """
    
  def getCurveBP(curvepos):
    """
    This method is unsupported.  BPoint Ipo curves are not implemented.
    Calling this method throws a NotImplementedError exception.
    @raise NotImplementedError: this method B{always} raises an exception
    """
 
  def getBeztriple(curvepos,pointpos):
    """
    Gets a beztriple of the Ipo.
    @type curvepos: int
    @param curvepos: the position of the curve in the Ipo.
    @type pointpos: int
    @param pointpos: the position of the point in the curve.
    @rtype: list of 9 floats
    @return: the beztriple of the Ipo, or an error is raised.
    """

  def setBeztriple(curvepos,pointpos,newbeztriple):
    """
    Sets the beztriple of the Ipo.
    @type curvepos: int
    @param curvepos: the position of the curve in the Ipo.
    @type pointpos: int
    @param pointpos: the position of the point in the curve.
    @type newbeztriple: list of 9 floats
    @param newbeztriple: the new value for the point
    @rtype: None
    @return: None
    """
    
  def getCurveCurval(curvepos):
    """
    Gets the current value of a curve of the Ipo (B{deprecated}). B{Note}:
    new scripts should use L{IpoCurve.evaluate()}.
    @type curvepos: int or string
    @param curvepos: the position of the curve in the Ipo or the name of the
        curve
    @rtype: float
    @return: the current value of the selected curve of the Ipo.
    """

  def EvaluateCurveOn(curvepos,time):
    """
    Gets the value at a specific time of a curve of the Ipo (B{deprecated}).
    B{Note}: new scripts should use L{IpoCurve.evaluate()}.
    @type curvepos: int
    @param curvepos: the position of the curve in the Ipo.
    @type time: float
    @param time: the desired time.
    @rtype: float
    @return: the current value of the selected curve of the Ipo at the given
        time.
    """

class IpoCurve:
  """
  The IpoCurve object
  ===================
  This object gives access to generic data from all Ipo curves objects in Blender.

  Important Notes for Rotation Ipo Curves:\n
  For the rotation Ipo curves, the y values for points are in units of 10
  degrees.  For example, 45.0 degrees is stored as 4.50 degrees.  These are the
  same numbers you see in the Transform Properties pop-up menu ( NKey ) in
  the IPO Curve Editor window.  Positive rotations are in a counter-clockwise
  direction, following the standard convention.
  
  @ivar driver:  Status of the driver.  1= on, 0= off.
  @type driver:  int
  @ivar driverObject:  Object used to drive the Ipo curve.
  @type driverObject:  Blender Object or None
  @ivar driverChannel:  Object channel used to drive the Ipo curve.
  Use module constants: IpoCurve.LOC_X, IpoCurve.LOC_Y, IpoCurve.LOC_Z,
  IpoCurve.ROT_X, IpoCurve.ROT_Y, IpoCurve.ROT_Z, IpoCurve.SIZE_X,
  IpoCurve.SIZE_Y, IpoCurve.SIZE_Z
  @type driverChannel:  int 
  @ivar name: The IpoCurve data name.
  @type name: string
  @ivar bezierPoints : The list of the curve's bezier points.
  @type bezierPoints : list
  """

  def setExtrapolation(extendmode):
    """
    Sets the extend mode of the curve.
    @type extendmode: string
    @param extendmode: the extend mode of the curve.
        Can be Constant, Extrapolation, Cyclic or Cyclic_extrapolation.
    @rtype: None
    @return: None
    """

  def getExtrapolation():
    """
    Gets the extend mode of the curve.
    @rtype: string
    @return: the extend mode of the curve. Can be Constant, Extrapolation, Cyclic or Cyclic_extrapolation.
    """
    

  def setInterpolation(interpolationtype):
    """
    Sets the interpolation type of the curve.
    @type interpolationtype: string
    @param interpolationtype: the interpolation type of the curve. Can be Constant, Bezier, or Linear.
    @rtype: None
    @return: None
    """
  def getInterpolation():
    """
    Gets the interpolation type of the curve.
    @rtype: string
    @return: the interpolation type of the curve. Can be Constant, Bezier, or Linear.
    """
    
  def addBezier(coordlist):
    """
    Adds a Bezier point to a curve.
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
    Returns the name of the Ipo curve. This name can be:
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
    Returns all the points of the Ipo curve.
    @rtype: list of BezTriples
    @return: the points of the Ipo curve.
    """

  def evaluate( time ):
    """
    Compute the value of the Ipo curve at a particular time.
    @type time: float
    @param time: value along the X axis
    @rtype: float
    @return: the Y value of the curve at the given time
    """


class BezTriple:
  """
  The BezTriple object
  ====================
  This object gives access to generic data from all beztriple objects in Blender.  If an attribute is listed as being 'read-only' that means you cannot write to it.  Use the set*() methods instead.
  @ivar pt : the [x,y] coordinates for knot point of this BezTriple.  Read-only.
  @type pt: list of floats
  @ivar vec : a list of the 3 points [ handle, knot, handle ] that comprise a BezTriple.  See the getTriple() method for an example of the format.  Read-only.
  @type vec: list of points
  @ivar hide: the visibility status of the control point.
  @type hide: int
  """

  def __init__(coords):
    """
    Create a new BezTriple object.  

    @type coords: sequence of three or nine floats
    @param coords: the coordinate values for the new control point.  If three
    floats are given, then the handle values are automatically generated.
    @rtype: BezTriple
    @return: a new BezTriple object
    """

  def getPoints():
    """
    Returns the xy coordinates of the Bezier knot point.
    @rtype: list of floats
    @return: list of the x and y coordinates of the Bezier point.
    """

  def setPoints(newval):
    """
    Sets the point xy coordinates of the Bezier knot point.  After changing
    coordinates, it is advisable to call L{IpoCurve.recalc()} to update the
    Ipo curves.
    @type newval: tuple of 2 floats
    @param newval: the x and y coordinates of the new Bezier point.
    @rtype: None
    @return: None
    """

  def getTriple():
    """
    Returns the x,y,z coordinates for each of the three points that make up
    a BezierTriple.

    The return list looks like this [ [H1x, H1y, H1z], [Px, Py, Pz],
    [H2x, H2y, H2z] ] .

    Example::
      # where bt is of type BezierTriple
      #  and h1, p, and h2 are lists of 3 floats
      h1, p, h2 = bt.getTriple()
    
    @rtype: list consisting of 3 lists of 3 floats
    @return: handle1, knot, handle2
    """
