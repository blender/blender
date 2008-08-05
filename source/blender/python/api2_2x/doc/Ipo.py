# Blender.Ipo module and the Ipo PyType object

"""
The Blender.Ipo submodule

B{New}: 
	-  Ipo updates to both the program and Bpython access.
	-  access to Blender's new Ipo driver capabilities.
	-  Ipo now supports the mapping operator [] to access IpoCurves

This module provides access to the Ipo Data in Blender. An Ipo is composed of
several IpoCurves, and an IpoCurve is composed of several BezTriples.

Example::
	from Blender import Ipo

	ipo = Ipo.Get('ObIpo')				# retrieves an Ipo object
	ipo.name = 'ipo1'					# change the Ipo's name
	icu = ipo[Ipo.OB_LOCX]				# request X Location Ipo curve
	if icu != None and len(icu.bezierPoints) > 0: # if curve exists and has BezTriple points
		val = icu[2.5]					# get the curve's value at time 2.5
		ipo[Ipo.OB_LOCX] = None			# delete the Ipo curve
	
Each type of Ipo has different types Ipocurves.  With the exception of Shape
Key Ipos, constants are used to specify all Ipocurves.  There are two ways
to tell which Ipo curves go with which Ipo type:
	- all constants start with a two-character identifier for their Ipo type;
	for example, "OB_LOCX" is the LocX curve for an Object Ipo
	- each Ipo now has a read-only attribute L{Ipo.curveConsts}, which returns 
	the valid Ipo curve types for that specific Ipo

The valid IpoCurve constants are:
			1. Material Ipo: MA_R, MA_G, MA_B, MA_SPECR, MA_SPECG, MA_SPECB,
			MA_MIRR, MA_MIRG, MA_MIRB, MA_REF, MA_ALPHA, MA_EMIT, MA_AMB,
			MA_SPEC, MA_HARD, MA_SPTRA, MA_IOR, MA_MODE, MA_HASIZE, MA_TRANSLU,
			MA_RAYMIR, MA_FRESMIR, MA_FRESMIRI, MA_FRESTRA, MA_FRESTRAI,
			MA_TRAGLOW, MA_OFSX, MA_OFSY, MA_OFSZ, MA_SIZEX, MA_SIZEY, MA_SIZEZ,
			MA_TEXR, MA_TEXG, MA_TEXB, MA_DEFVAR, MA_COL, MA_NOR, MA_VAR, MA_DISP
			2. Lamp Ipo: LA_ENERG, LA_R, LA_G, LA_B, LA_DIST, LA_SPOSI, LA_SPOBL,
			LA_QUAD1, LA_QUAD2, LA_HAINT, LA_OFSX, LA_OFSY, LA_OFSZ, LA_SIZEX,
			LA_SIZEY, LA_SIZEZ, LA_TEXR, LA_TEXG, LA_TEXB, LA_DEFVAR, LA_COL
			3. World Ipo: WO_HORR, WO_HORG, WO_HORB, WO_ZENR, WO_ZENG, WO_ZENB,
			WO_EXPOS, WO_MISI, WO_MISDI, WO_MISSTA, WO_MISHI, WO_STARR,
			WO_STARB, WO_STARG, WO_STARDI, WO_STARSI, WO_OFSX, WO_OFSY,
			WO_OFSZ, WO_SIZEX, WO_SIZEY, WO_SIZEZ, WO_TEXR, WO_TEXG,
			WO_TEXB, WO_DEFVAR, WO_COL, WO_NOR, WO_VAR
			4. Camera Ipo: CA_LENS, CA_CLSTA, CA_CLEND, CA_APERT, CA_FDIST
			5. Object Ipo: OB_LOCX, OB_LOCY, OB_LOCZ, OB_DLOCX, OB_DLOCY, OB_DLOCZ,
			OB_ROTX, OB_ROTY, OB_ROTZ, OB_DROTX, OB_DROTY, OB_DROTZ,
			OB_SCALEX, OB_SCALEY, OB_SCALEZ, OB_DSCALEX, OB_DSCALEY, OB_DSCALEZ,
			OB_LAYER, OB_TIME, OB_COLR, OB_COLG, OB_COLB, OB_COLA,
			OB_FSTRENG, OB_FFALL, OB_RDAMP, OB_DAMPING, OB_PERM
			6. Curve Ipo: CU_SPEED
			7. Constraint Ipo: CO_INF
			8. Texture Ipo: TE_NSIZE, TE_NDEPTH, TE_NTYPE, TE_TURB, TE_VNW1, TE_VNW2,
			TE_VNW3, TE_VNW4, TE_MINKMEXP, TE_DISTM, TE_COLT, TE_ISCALE,
			TE_DISTA, TE_MGTYPE, TE_MGH, TE_LACU, TE_OCT, TE_MGOFF,
			TE_MGGAIN, TE_NBASE1, TE_NBASE2, TE_COLR, TE_COLG, TE_COLB,
			TE_BRIGHT, TE_CONTRAS
			9. Pose/Action Ipo: PO_LOCX, PO_LOCY, PO_LOCZ, PO_SIZEX, PO_SIZEY,
			PO_SIZEZ, PO_QUATW, PO_QUATX, PO_QUATY, PO_QUATZ
			10. Sequence Ipo: SQ_FAC

Shape Key Ipos are handled differently from other Ipos.  The user can rename
the curves, so string are used to access them instead of constants.  The
L{Ipo.curveConsts} attribute for Shape Key Ipos returns a list of all defined
key names.
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
			- (): A list with all Ipos in the current scene.
	"""

class Ipo:
	"""
	The Ipo object
	==============
	This object gives access to Ipo data from all objects in Blender.
	@Note: Blender Materials, Lamps and Worlds have I{texture channels} which
	allow the user to assign textures to them.  The Blender Ipo Window allows
	the user to access the IpoCurves for these channels by specifying a number
	between 0 and 9 (the number appears next to the Ipo type in the window
	header).  Prior to Version 2.42, the BPy API did not allow users to access
	these texture channels in a predictable manner.  A new attribute named
	L{channel} was added to the API in Version 2.42 to correct this problem.

	The current channel setting has an effect on the operators B{[]}, B{len()} 
	and others.  For example, suppose a Material has three IpoCurves 
	(R, G, and B), and two texture channels (numbered 0 and 1), and furthermore
	channel 0 has one Ipocurve (Col).  The IpoCurve Col can only be
	"seen" through the API when B{ipo.channel} is 0.  Setting B{ipo.channel} to
	1 will cause this curve to be ignored by B{len(ipo)}::

		from Blender import Ipo

		ipo = Ipo.Get('MatIpo')
		for channel in xrange(2):
				ipo.channel = channel
				print 'channel is',channel
				print ' len is',len(ipo)
				names = dict([(x[1],x[0]) for x in ipo.curveConsts.items()])
				for curve in [Ipo.MA_R,Ipo.MA_COL]:
						print ' ',names[curve],'is',curve in ipo

	will output::
		channel is 0
		len is 4
			MA_R is True
			MA_COL is True
		channel is 1
		len is 3
			MA_R is True
			MA_COL is False

	@ivar curves: Ipo curves currently defined for the Ipo.
	@type curves: list of Ipocurves.
	@ivar curveConsts: The valid Ipo curves for this Ipo.  These can be used
	by the [] mapping operator.  The value 
	depends on the Ipo curve type.  If the Ipo is any type other than a Key or
	Shape Ipo, this attribute returns a set of constants that can be
	used to specify a particular curve.  For Key or Shape Ipos, the attribute
	returns a list of all defined keys by name.  
	@type curveConsts: constant or list of strings. Read-only.
	@ivar channel: the current texture channel for Blender object which support
	textures (materials, lamps and worlds).  Returns None if the Ipo does
	not support texture channels. Value must be in the range [0,9].
	@type channel: int or None
	"""

	def __contains__():
		"""
		The "in" operator for Ipos. It returns B{True} if the specified 
		IpoCurve exists for the Ipo.  This operator B{should not} be used to 
		test for whether a curve constant is valid for a particular Ipo type.
		Many constants for different Ipo types have the same value, and it is
		the constant's value used internally.
		No exceptions are raised if the argument is not a valid curve constant or
		or string, nor does the operator return B{True} when the curve
		constant is valid but does not currently exist.  As such, it should only be
		used to test for specific curves when the Ipo type is known::
			ipo = Object.Get('Cube').ipo # get Object-type Ipo 
			if ipo:
				print Ipo.OB_LOCX in ipo # prints "True" if 'LocX' curve exists
				print Ipo.MA_R in ipo    # also prints "True" since MA_R and OB_LOCX are have the same value
				print 'hiccup' in ipo    # always prints "False" since argument is not a constant

		@return: see above.
		@rtype: Boolean
		"""

	def __getitem__():
		"""
		This operator is similar to the Python dictionary mapping operator [],
		except that the user cannot assign arbitrary keys.  Each Ipo type has
		a pre-defined set of IpoCurves which may or may not exist at a given time.      This operator
		will either return an IpoCurve object if the specified curve exists,
		return None if the curve does not exists, or throws a KeyError exception
		if the curve is not valid for this Ipo type.
		@return: an IpoCurve object if it exists
		@rtype: IpoCurve or None
		@raise KeyError: an undefined IpoCurve was specified for the Ipo
		"""

	def __iter__():
		"""
		Iterator for Ipos.  It returns all the defined IpoCurve objects associated 
		with the Ipo.  For example::
			from Blender import Ipo

			ipo = Ipo.Get()
			if len(ipo) > 0:
				ipo = ipo[0]
				print 'ipo name is',ipo.name
				for icu in ipo:
					print ' curve name is',icu.name
		might result in::
			ipo name is ObIpo
				curve name is LocX
				curve name is LocY
				curve name is LocZ

		@return: an IpoCurve object
		@rtype: IpoCurve
		"""

	def __len__():
		"""
		Returns the number of curves defined for the Ipo.
		@return: number of defined IpoCurves
		@rtype: int
		"""

	def getName():
		"""
		Gets the name of the Ipo (B{deprecated}).  See the L{name} attribute.
		@rtype: string
		@return: the name of the Ipo.
		"""

	def setName(newname):
		"""
		Sets the name of the Ipo (B{deprecated}).  See the L{name} attribute.
		@type newname: string
		@rtype: None
		@return: None
		"""

	def getCurves():
		"""
		Gets all the IpoCurves of the Ipo (B{deprecated}).  Use the
		L{iterator operator []<__iter__>} instead.
		@rtype: list of IpoCurves
		@return: A list (possibly empty) containing all the IpoCurves associated
		to the Ipo object.
		"""

	def getCurve(curve):
		"""
		Return the specified IpoCurve (B{deprecated}).  Use the L{mapping
		operator B{[]}<__getitem__>} instead.
		If the curve does not exist in the Ipo,
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
			dRotX, dRotY, dRotZ, ScaleX, ScaleY, ScaleZ, dScaleX, dScaleY, dScaleZ,
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

		The adrcode for the Ipo curve can also be given; this is useful for
		accessing curves for Shape Key Ipos.  The adrcodes for Shape Key Ipo are
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
			dRotX, dRotY, dRotZ, ScaleX, ScaleY, ScaleZ, dScaleX, dScaleY, dScaleZ,
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

		For Key IPOs, the name must be an existing KeyBlock name.  Use
		L{curveConsts} to determine the set of valid names.

		@type curvename : string
		@rtype: IpoCurve object
		@return: the corresponding IpoCurve, or None.
		@raise ValueError: I{curvename} is not valid or already exists
		"""

	def delCurve(curvename):
		"""
		Delete an existing curve from the Ipo object (B{deprecated}).
		Use the L{mapping operator B{[]}<__getitem__>} instead::
			 from Blender import Ipo

			 ipo = Ipo.Get('ObIpo')
			 ipo[Ipo.LOCX] = None

		@type curvename : string
		@rtype: None
		@return: None.
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
		Gets the number of curves of the Ipo (B{deprecated}).  Use
		L{len(ipo)<__len__>} instead.
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
		Gets a beztriple of the Ipo (B{deprecated}).  B{Note}:
		Use L{IpoCurve.bezierPoints<IpoCurve.IpoCurve.bezierPoints>} instead.
		@type curvepos: int
		@param curvepos: the position of the curve in the Ipo.
		@type pointpos: int
		@param pointpos: the position of the point in the curve.
		@rtype: list of 9 floats
		@return: the beztriple of the Ipo, or an error is raised.
		"""

	def setBeztriple(curvepos,pointpos,newbeztriple):
		"""
		Sets the beztriple of the Ipo (B{deprecated}).  B{Note}: use 
		L{IpoCurve.bezierPoints<IpoCurve.IpoCurve.bezierPoints>} to get a
		BezTriple point, then use the
		L{BezTriple} API to set the point's attributes.
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
		new scripts should use L{IpoCurve.evaluate()<IpoCurve.IpoCurve.evaluate>}.
		@type curvepos: int or string
		@param curvepos: the position of the curve in the Ipo or the name of the
				curve
		@rtype: float
		@return: the current value of the selected curve of the Ipo.
		"""

	def EvaluateCurveOn(curvepos,time):
		"""
		Gets the value at a specific time of a curve of the Ipo (B{deprecated}).
		B{Note}: new scripts should use 
		L{IpoCurve.evaluate()<IpoCurve.IpoCurve.evaluate>}.
		@type curvepos: int
		@param curvepos: the position of the curve in the Ipo.
		@type time: float
		@param time: the desired time.
		@rtype: float
		@return: the current value of the selected curve of the Ipo at the given
		time.
		"""
import id_generics
Ipo.__doc__ += id_generics.attributes
