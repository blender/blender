# Blender.Curve module and the Curve PyType object

"""
The Blender.Curve submodule.

Curve Data
==========

This module provides access to B{Curve Data} objects in Blender.

A Blender Curve Data consists of multiple L{CurNurb}(s). Try converting a Text object to a Curve to see an example of this.   Each curve is of
type Bezier or Nurb.  The underlying L{CurNurb}(s) can be accessed with
the [] operator.  Operator [] returns an object of type L{CurNurb}. Removing a L{CurNurb} can be done this way too. del curve[0] removes the first curve.

Note that L{CurNurb} can be used to acces a curve of any type (Poly, Bezier or Nurb)

The Curve module also supports the Python iterator interface.  This means you
can access the L{CurNurb}(s) in a Curve and the control points in a L{CurNurb} using a
Python B{for} statement.


Add a Curve to a Scene Example::
	from Blender import Curve, Object, Scene
	cu = Curve.New()             # create new  curve data
	scn = Scene.GetCurrent()    # get current scene
	ob = scn.objects.new(cu)     # make a new curve from the curve data

Iterator Example::
	from Blender import Curve, Object, Scene
	scn = Scene.GetCurrent()    # get current scene
	ob = scn.objects.active
	curvedata = ob.data
	for curnurb in curvedata:
		print type( curnurb ), curnurb
		for point in curnurb:
			print type( point ), point

Creating a Curve from a list of Vec triples Examples::
	from Blender import *
	def bezList2Curve(bezier_vecs):
		'''
		Take a list or vector triples and converts them into a bezier curve object
		'''
		
		def bezFromVecs(vecs):
			'''
			Bezier triple from 3 vecs, shortcut functon
			'''
			bt= BezTriple.New(\
			vecs[0].x, vecs[0].y, vecs[0].z,\
			vecs[1].x, vecs[1].y, vecs[1].z,\
			vecs[2].x, vecs[2].y, vecs[2].z)
			
			bt.handleTypes= (BezTriple.HandleTypes.FREE, BezTriple.HandleTypes.FREE)
			
			return bt
		
		# Create the curve data with one point
		cu= Curve.New()
		cu.appendNurb(bezFromVecs(bezier_vecs[0])) # We must add with a point to start with
		cu_nurb= cu[0] # Get the first curve just added in the CurveData
		
		
		i= 1 # skip first vec triple because it was used to init the curve
		while i<len(bezier_vecs):
			bt_vec_triple= bezier_vecs[i]
			bt= bezFromVecs(bt_vec_triple)
			cu_nurb.append(bt)
			i+=1
		
		# Add the Curve into the scene
		scn= Scene.GetCurrent()
		ob = scn.objects.new(cu)
		return ob
"""

def New ( name):
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
	This object gives access to Curve and Surface data linked from Blender Objects.
	
	@ivar name: The Curve Data name.
	@type name: string
	@ivar pathlen: The Curve Data path length, used to set the number of frames for an animation (not the physical length).
	@type pathlen: int
	@ivar totcol: The Curve Data maximal number of linked materials. Read-only.
	@type totcol: int
	@ivar flag: The Curve Data flag value; see L{getFlag()} for the semantics.
	@ivar bevresol: The Curve Data bevel resolution. [0 - 32]
	@type bevresol: int
	@ivar resolu: The Curve Data U-resolution (used for curve and surface resolution) [0 - 1024].
	@type resolu: int
	@ivar resolv: The Curve Data V-resolution (used for surface resolution) [0 - 1024].
	@type resolv: int
	@ivar width: The Curve Data width [0 - 2].
	@type width: float
	@ivar ext1: The Curve Data extent1 Called "Extrude" in the user interface (for bevels only).
	@type ext1: float
	@ivar ext2: The Curve Data extent2 - Called "Bevel Depth" in the user interface (for bevels only).
	@type ext2: float
	@ivar loc: The Curve Data location(from the center).
	@type loc: list of 3 floats
	@ivar rot: The Curve Data rotation(from the center).
	@type rot: list of 3 floats
	@ivar size: The Curve Data size(from the center).
	@type size: list of 3 floats
	@ivar bevob: The Curve Bevel Object
	@type bevob: Blender L{Object<Object.Object>} or None
	@ivar taperob: The Curve Taper Object
	@type taperob: Blender L{Object<Object.Object>} or None
	@ivar key: The Key object associated with this Curve, if any.
	@type key: Blender L{Key<Key.Key>}
	@ivar materials: The curves's materials.  Each curve can reference up to
		16 materials.  Empty slots in the curve's list are represented by B{None}.
		B{Note}: L{Object.colbits<Object.Object.colbits>} needs to be set correctly
		for each object in order for these materials to be used instead of
		the object's materials.
		B{Note}: The list that's returned is I{not} linked to the original curve.
		curve.materials.append(material) won't do anything.
		Use curve.materials += [material] instead.
	@type materials: list of L{Material}s
	"""

	def getName():
		"""
		Get the name of this Curve Data object.
		@rtype: string
		"""

	def setName(name):
		"""
		Set the name of this Curve Data object.
		@rtype: None
		@type name: string
		@param name: The new name.
		"""

	def getPathLen():
		"""
		Get this Curve's path frame length, used for an animated path.
		@rtype: int
		@return: the path length.
		"""

	def setPathLen(len):
		"""
		Set this Curve's path length.
		@rtype: None
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
		Set the number of materials linked to the Curve.  B{Note}: this method
		will probably be deprecated in the future.
		@rtype: None
		@type totcol: int
		@param totcol: number of materials linked.
		@warn: It is not advisable to use this method unless you know what you
		are doing; it's possible to
		corrupt a .blend file if you don't know what you're doing.  If you want
		to change the number of materials, use the L{materials} attribute.
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
		@rtype: None
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
		@rtype: None
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
		Set the Curve's U-resolution value. [0 - 1024]
		This is used for surfaces and curves.
		@rtype: None
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
		Set the Curve's V-resolution value. [0 - 1024].
		This is used for surfaces only.
		@rtype: None
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
		@rtype: None
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
		@rtype: None
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
		@rtype: None 
		@type ext2: float
		@param ext2: The new Curve's ext2 value. 
		"""

	def getControlPoint(numcurve,numpoint):
		"""
		Get the curve's control point value (B{deprecated}).  The numpoint arg
		is an index into the list of points and starts with 0.  B{Note}: new
		scripts should use the [] operator on Curves and CurNurbs.  Example::
			curve = Blender.Curve.Get('Curve')
			p0 = curve[0][0]    # get first point from first nurb
						# -- OR --
			nurb = curve[0]     # get first nurb
			p0 = nurb[0]        # get nurb's first point

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
		@rtype: None
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
		Add a new control point to the indicated curve (B{deprecated}).
		New scripts should use L{CurNurb.append()}.
		@rtype: None
		@type numcurve: int
		@type new_control_point: list of floats or BezTriple
		@param numcurve:  index for spline in Curve, starting from 0
		@param new_control_point: depends on curve's type.
			- type Bezier: a BezTriple 
			- type Nurb: a list of four or five floats for the xyzw values
		@raise AttributeError:  throws exception if numcurve is out of range.
		"""

	def appendNurb( new_point ):
		"""
		add a new curve to this Curve.  The new point is added to the new curve.  Blender does not support a curve with zero points.  The new curve is added to the end of the list of curves in the Curve.
		@rtype: CurNurb
		@return: the newly added spline
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
		@rtype: None 
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
		@rtype: None
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
		@rtype: None 
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
		@rtype: Blender Object or None
		@return: Bevel Object (BevOb) assigned to the Curve.
		"""

	def setBevOb( object ):
		"""
		Assign a Bevel Object (BevOb) to the Curve.  Passing None as the object parameter removes the bevel.
		@rtype: None
		@return: None
		@type object: Curve type Blender Object
		@param object: Blender Object to assign as Bevel Object (BevOb)
		@raise TypeError: throws exception if the parameter is not a Curve type Blender Object or None
		"""

	def getTaperOb():
		"""
		Returns the Taper Object (TaperOb) assigned to the Curve.
		@rtype: Blender Object or None
		@return: Taper Object (TaperOb) assigned to the Curve.
		"""

	def setTaperOb( object ):
		"""
		Assign a Taper Object (TaperOb) to the Curve.  Passing None as the object parameter removes the taper.
		@rtype: None
		@return: None
		@type object: Curve type Blender Object
		@param object: Blender Object to assign as Taper Object (TaperOb)
		@raise TypeError: throws exception if the parameter is not a Curve type Blender Object or None
		"""

	def update():
		"""
		Updates display list for a Curve.
		Used after making changes to control points.
		You B{must} use this if you want to see your changes!
		@rtype: None
		@return: None
		"""

	def isNurb( curve_num ):
		"""
		Tells type of a CurNurb (B{deprecated}).
		New scripts should use L{CurNurb.isNurb()}.

		@rtype: integer
		@return:  Zero if curve is type Bezier, one if curve is of type Nurb.
		@type curve_num: integer
		@param curve_num: zero-based index into list of curves in this Curve.
		@raise AttributeError:  throws exception if curve_num is out of range.
		"""

	def isCyclic( curve_num ):
		"""
		Tells whether or not a CurNurb is cyclic (closed) (B{deprecated}).
		New scripts should use L{CurNurb.isCyclic()}.

		@rtype: boolean
		@return: True if is cyclic, False if not
		@type curve_num: integer
		@param curve_num: zero-based index into list of curves in this Curve
		@raise AttributeError:  throws exception if curve_num is out of range.
		"""

	def switchDirection( ):
		"""
		Reverse the direction of a curve.
		@return: None

		I{B{Example:}}
		# This example switches the direction of all curves on the active object.
		from Blender import *
		scn = Scene.GetCurrent()
		ob = scn.objects.active # must be a curve
		data = ob.data
		for cu in data: cu.switchDirection()
		"""

	def getNumCurves():
		"""
		Get the number of curves in this Curve Data object.
		@rtype: integer
		"""

	def getNumPoints( curve_num ):
		"""
		Get the number of control points in the curve (B{deprecated}).
		New scripts should use the len operator (I{len(curve)}).
		@type curve_num: integer
		@param curve_num: zero-based index into list of curves in this Curve
		@rtype: integer
		"""

	def getKey():
		"""
		Return the L{Key<Key.Key>} object containing the keyframes for this
		curve, if any.
		@rtype: L{Key<Key.Key>} object or None
		"""

	def recalc():
		"""
		Recalculate control point handles after a curve has been changed.
		@rtype: None
		"""

	def __copy__ ():
		"""
		Make a copy of this curve
		@rtype: Curve
		@return:  a copy of this curve
		"""

class CurNurb:
	"""
	The CurNurb Object
	==================
	This object provides access to the control points of the curves that make up a Blender Curve ObData.

	The CurNurb supports the python iterator protocol which means you can use a python for statement to access the points in a curve.

	The CurNurb also supports the sequence protocol which means you can access the control points of a CurNurb using the [] operator.

	Note that CurNurb is used for accesing poly, bezier and nurbs type curves.

	@ivar flagU: The CurNurb knot flag U.  See L{setFlagU} for description.
	@type flagU: int
	@ivar flagV: The CurNurb knot flag V.  See L{setFlagU} for description.
	@type flagV: int
	@ivar orderU: The CurNurb knot order U, for nurbs curves only, this is clamped by the number of points, so the orderU will never be greater.
	@type orderU: int
	@ivar type: The type of the curve (Poly: 0, Bezier: 1, NURBS: 4)
	@type type: int
	@ivar knotsU: The knot vector in the U direction. The tuple will be empty
	if the curve isn't a NURB or doesn't have knots in this direction.
	@type knotsU: tuple of floats
	@ivar knotsV: The knot vector in the V direction. The tuple will be empty
	if the curve isn't a NURB or doesn't have knots in this direction.
	@type knotsV: tuple of floats
	@ivar smooth: Set the smoothing for this curve (applies to cuve objects that have a bevel)
	@type smooth: bool
	"""

	def __setitem__( n, point ):
		"""
		Replace the Nth point in the curve. The type of the argument must match the type of the curve. List of 4 floats (optional 5th float is the tilt value in radians) for Nurbs or BezTriple for Bezier.
		@rtype: None
		@return: None
		@type n: integer
		@param n: the index of the element to replace
		@type point: BezTriple or list of 4 floats (optional 5th float is the tilt value in radians)
		@param point: the point that will replace the one in the curve.  The point can be either a BezTriple type or a list of 4 floats in x,y,z,w (optionally tilt in radians as 5th value) format for a Nurb curve.
		"""

	def __getitem__( n ):
		"""
		Get the Nth element in the curve. For Bezier curves, that element is a BezTriple. For the rest (Poly and Nurbs), it is a list of 5 floats: x, y, z, weight, tilt (in radians). NOTE 1: This element is independent on the curve, modifying it will not affect the curve. NOTE 2: Each successive call returns a new object.
		@rtype: BezTriple (Bezier Curve) or List of 5 floats [x, y, z, w, t] for Poly or Nurbs
		@return: The Nth element in the curve
		@type n: integer
		@param n: the index of the element to return
		"""

	def append( new_point ):
		"""
		Appends a new point to a curve.  This method appends points to both Bezier and Nurb curves. The type of the argument must match the type of the curve. List of 4 floats (optional 5th float is the tilt value in radians) for Nurbs or BezTriple for Bezier.
		@rtype: None
		@return: None
		@type new_point: BezTriple or list of 4 floats (optional 5th float is the tilt value in radians)
		@param new_point: the new point to be appended to the curve.  The new point can be either a BezTriple type or a list of 4 floats in x,y,z,w (optionally tilt in radians as 5th value) format for a Nurb curve.
		"""

	def setMatIndex( index ):
		"""
		Sets the Material index for this CurNurb.
		@rtype: None
		@return: None
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
		Get the CurNurb knot flag U.  
		@rtype: integer
		@return: See L{setFlagU} for description of return value.
		"""

	def setFlagU( flag ):
		"""
		Set the entire CurNurb knot flag U (knots are recalculated automatically).
		The flag can be one of six values:
				 - 0 or 1: uniform knots
				 - 2 or 3: endpoints knots
				 - 4 or 5: bezier knots
		Bit 0 controls whether or not the curve is cyclic (1 = cyclic).
		@type flag: integer
		@param flag: CurNurb knot flag
		@rtype: None
		@return: None
		"""

	def getFlagV():
		"""
		Get the CurNurb knot flag V.
		@rtype: integer
		@return: See L{setFlagU} for description of return value.
		"""

	def setFlagV( value ):
		"""
		Set the CurNurb knot flag V (knots are recalculated automatically).
		@type value: integer
		@param value: See L{setFlagU} for description of return.
		@rtype: None
		@return: None
		"""

	def getType():
		"""
		Get the type of the curve.
		@rtype: integer
		@return:  0 - Poly, 1 - Bezier, 4 - NURBS
		"""

	def setType( value ):
		"""
		Set the type of the curve and converts the curve to its new type if needed
		@type value: integer
		@param value: CurNurb type flag (0 - Poly, 1 - Bezier, 4 - NURBS)
		@rtype: None
		@return: None
		"""

class SurfNurb:
	"""
	The SurfNurb Object
	===================
	This object provides access to the control points of the surfaces that make
	up a Blender Curve.

	The SurfNurb supports the Python iterator and sequence protocols which
	means you can use a python B{for} statement or [] operator to access the
	points in a surface.  Points are accessed linearly; for a N-by-M UV surface,
	the first N control points correspond to V=0, then second N to V=1, and so
	on.

	@ivar flagU: The knot flag U.  Changing the knot type automatically
	recalculates the knots.  The flag can be one of three values:
					 - 0 : uniform knots
					 - 1 : endpoints knots
					 - 2 : bezier knots
	@type flagU: int
	@ivar flagV: The knot flag V.  See L{flagU} for description.
	@type flagV: int
	@ivar pointsU: The number of control points in the U direction (read only).
	@type pointsU: int
	@ivar pointsV: The number of control points in the V direction (read only).
	@type pointsV: int
	@ivar cyclicU: The cyclic setting for the U direction (True = cyclic).
	@type cyclicU: boolean
	@ivar cyclicV: The cyclic setting for the V direction (True = cyclic).
	@type cyclicV: boolean
	@ivar orderU: The order setting for the U direction.  Values are clamped
	to the range [2:6] and not greater than the U dimension.
	@type orderU: int
	@ivar orderV: The order setting for the V direction.  Values are clamped
	to the range [2:6] and not greater than the V dimension.
	@type orderV: int
	@ivar knotsU: The The knot vector in the U direction
	@type knotsU: tuple
	@ivar knotsV: The The knot vector in the V direction
	@type knotsV: tuple
	"""

	def __setitem__( n, point ):
		"""
		Set the Nth control point in the surface. 
		@rtype: None
		@return: None
		@type n: integer
		@param n: the index of the point to replace
		@type point: list of 4 floats (optional 5th float is the tilt value
		in radians)
		@param point: the point that will replace the one in the curve.  The
		point is  list of 4 floats in x,y,z,w (optionally tilt in radians as
		5th value) format.
		"""

	def __getitem__( n ):
		"""
		Get the Nth control point in the surface. 
		@rtype: List of 5 floats [x, y, z, w, t] for Poly or Nurbs
		@return: The Nth point in the curve
		@type n: integer
		@param n: the index of the point to return
		@note: This returned value is independent on the curve; modifying it will not affect the curve. 
		@note: Each successive call returns a new object.
		"""

