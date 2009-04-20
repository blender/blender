# Blender.BezTriple module and the BezTriple PyType object

"""
The Blender.BezTriple submodule

B{New}: 
  -  new attributes L{handleTypes<BezTriple.handleTypes>},
     L{selects<BezTriple.selects>} and L{weight<BezTriple.weight>} 

This module provides access to the BezTriple Data in Blender.  It is used by
CurNurb and IpoCurve objects.

@type HandleTypes: readonly dictionary
@var HandleTypes: The available BezTriple handle types.
    - FREE - handle has no constraints
    - AUTO - completely constrain handle based on knot position
    - VECT - constraint handle to line between current and neighboring knot
    - ALIGN - constrain handle to lie in a straight line with knot's other
    handle
    - AUTOANIM - constrain IPO handles to be horizontal on extremes
"""

def New (coords):
  """
  Create a new BezTriple object.  

  @type coords: sequence of three or nine floats
  @param coords: the coordinate values for the new control point.  If three
  floats are given, then the handle values are automatically generated.
  @rtype: BezTriple
  @return: a new BezTriple object
  """

class BezTriple:
  """
  The BezTriple object
  ====================
  This object gives access to generic data from all BezTriple objects in
  Blender.  
  @ivar pt : the [x,y] coordinates for knot point of this BezTriple.  After
  changing coordinates of a Ipo curve, it is advisable to call 
  L{IpoCurve.recalc()<IpoCurve.IpoCurve.recalc>} to update the curve.
  @type pt: list of two floats
  @ivar vec : a list of the 3 points [ handle, knot, handle ] that comprise a
  BezTriple, with each point composed of a list [x,y,z] of floats.  The list 
  looks like [ [H1x, H1y, H1z], [Px, Py, Pz], [H2x, H2y, H2z] ].
  Example::
      # where bt is of type BezTriple
      #  and h1, p, and h2 are lists of 3 floats
      h1, p, h2 = bt.vec
  @type vec: list of points
  @ivar tilt: the tilt/alpha value for the point
  @type tilt: float
  @ivar radius: the radius of this point (used for tapering bevels)
  @type radius: float
  @ivar hide: the visibility status of the knot.  B{Note}: true/nonzero means
  I{not} hidden.  B{Note}: primarily intended for curves; not a good idea to 
  hide IPO control points.
  @type hide: int
  @ivar handleTypes: the types of the point's two handles.  See 
  L{HandleTypes} for a complete description.
  @type handleTypes list of two ints
  @ivar selects: the select status for [handle, knot, handle].  True/nonzero
  if the point is selected.
  @type selects: list of three ints
  @ivar weight: the weight assigned to the control point.  Useful for
  softbodies and possibly others.
  @type weight: float
  """

  def getPoints():
    """
    Returns the x,y coordinates of the Bezier knot point (B{deprecated}).
    See the L{BezTriple.pt} attribute.
    @rtype: list of floats
    @return: list of the x and y coordinates of the Bezier point.
    """

  def setPoints(newval):
    """
    Sets the x,y coordinates of the Bezier knot point (B{deprecated}).
    See the L{BezTriple.pt} attribute.
    @type newval: tuple of 2 floats
    @param newval: the x and y coordinates of the new Bezier point.
    @rtype: None
    @return: None
    """

  def getTriple():
    """
    Returns the x,y,z coordinates for each of the three points that make up
    a BezierTriple (B{deprecated}).  See the L{BezTriple.vec} attribute.
    @rtype: list consisting of 3 lists of 3 floats
    @return: handle1, knot, handle2
    """

