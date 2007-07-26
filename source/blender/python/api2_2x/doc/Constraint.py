# Blender.Constraint module and the Constraint PyType object

"""
The Blender.Constraint submodule

B{New}: 
	-  provides access to Blender's constraint stack

This module provides access to the Constraint Data in Blender.

Examples::
	from Blender import *

	ob = Object.Get('Cube')
	if len(ob.constraints) > 0:
		const = ob.constraints[0]
		if const.type == Constraint.Type.FLOOR:
			offs = const[Constraint.Settings.OFFSET]
	
Or to print all the constraints attached to each bone in a pose::
	from Blender import *
	
	ob = Object.Get('Armature')
	pose = ob.getPose()
	for bonename in pose.bones.keys():
		bone = pose.bones[bonename]
		for const in bone.constraints:
			print bone.name,'=>',const

@type Type: readonly dictionary
@var Type: Constant Constraint dict used by L{Constraints.append()} and 
	for comparison with L{Constraint.type}.  Values are
	TRACKTO, IKSOLVER, FOLLOWPATH, COPYROT, COPYLOC, COPYSIZE, ACTION,
	LOCKTRACK, STRETCHTO, FLOOR, LIMITLOC, LIMITROT, LIMITSIZE, CLAMPTO, 
	PYTHON, CHILDOF, TRANSFORM, NULL

@type Settings: readonly dictionary
@var Settings: Constant dict used for changing constraint settings.
	- Used for all constraints
		- TARGET (Object) (Note: not used by Limit Location (LIMITLOC), 
			Limit Rotation (LIMITROT), Limit Scale (LIMITSIZE))
		- BONE (string): name of Bone sub-target (for armature targets) (Note: not
			used by Stretch To (STRETCHTO), Limit Location (LIMITLOC), Limit Rotation 
			(LIMITROT), Limit Scale (LIMITSIZE), Follow Path (FOLLOWPATH), Clamp To (CLAMPTO))
	- Used by some constraints:
		- OWNERSPACE (int): for TRACKTO, COPYLOC, COPYROT, COPYSIZE, LIMITLOC, LIMITROT, LIMITSIZE, PYTHON, TRANSFORM
			If the owner is an object, values are SPACE_WORLD, SPACE_LOCAL
			If the owner is a bone, values are SPACE_WORLD, SPACE_POSE, SPACE_PARLOCAL, SPACE_LOCAL
		- TARGETSPACE (int): for TRACKTO, COPYLOC, COPYROT, COPYSIZE, PYTHON, TRANSFORM, ACTION
			If the owner is an object, values are SPACE_WORLD, SPACE_LOCAL
			If the owner is a bone, values are SPACE_WORLD, SPACE_POSE, SPACE_PARLOCAL, SPACE_LOCAL
	- Used by IK Solver (IKSOLVER) constraint:
		- TOLERANCE (float): clamped to [0.0001:1.0]
		- ITERATIONS (int): clamped to [1,10000]
		- CHAINLEN (int): clamped to [0,255]
		- POSWEIGHT (float): clamped to [0.01,1.0]
		- ROTWEIGHT (float): clamped to [0.01,1.0]
		- ROTATE (bool)
		- USETIP (bool)
	- Used by Action (ACTION) constraint:
		- ACTION (Action Object)
		- START (int): clamped to [1,maxframe]
		- END (int): clamped to [1,maxframe]
		- MIN (float): clamped to [-1000.0,1000.0] for Location, [-180.0,180.0] for Rotation, [0.0001,1000.0] for Scaling
		- MAX (float): clamped to [-1000.0,1000.0] for Location, [-180.0,180.0] for Rotation, [0.0001,1000.0] for Scaling
		- KEYON (int): values are XLOC, YLOC, ZLOC, XROT, YROT, ZROT, XSIZE, YSIZE, ZSIZE
	- Used by Track To (TRACKTO) constraint:
		- TRACK (int): values are TRACKX, TRACKY, TRACKZ, TRACKNEGX,
			TRACKNEGY, TRACKNEGZ
		- UP (int): values are UPX, UPY, UPZ
	- Used by Stretch To (STRETCHTO) constraint:
		- RESTLENGTH (float): clamped to [0.0:100.0]
		- VOLVARIATION (float): clamped to [0.0:100.0]
		- VOLUMEMODE (int): values are VOLUMEXZ, VOLUMEX, VOLUMEZ,
			VOLUMENONE
		- PLANE (int): values are PLANEX, PLANEZ
	- Used by Follow Path (FOLLOWPATH) constraint:
		- FOLLOW (bool)
		- OFFSET (float): clamped to [-maxframe:maxframe]
		- FORWARD (int): values are TRACKX, TRACKY, TRACKZ, TRACKNEGX,
			TRACKNEGY, TRACKNEGZ
		- UP (int): values are UPX, UPY, UPZ
	- Used by Lock Track (FOLLOWPATH) constraint:
		- TRACK (int): values are TRACKX, TRACKY, TRACKZ, TRACKNEGX,
			TRACKNEGY, TRACKNEGZ
		- LOCK (int): values are LOCKX, LOCKY, LOCKZ
	- Used by Clamp To (CLAMPTO) constraint:
		- CLAMP (int): values are CLAMPAUTO, CLAMPX, CLAMPY, CLAMPZ
	- Used by Floor (FLOOR) constraint:
		- MINMAX (int): values are MINX, MINY, MINZ, MAXX, MAXY, MAXZ
		- OFFSET (float): clamped to [-100.0,100.0]
		- STICKY (bool)
	- Used by Copy Location (COPYLOC) and Copy Rotation (COPYROT)
		- COPY (bitfield): any combination of COPYX, COPYY and COPYZ with possible addition of COPYXINVERT, COPYYINVERT and COPYZINVERT to invert that particular input (if on).
	- Used by Copy Size (COPYSIZE) constraint:
		- COPY (bitfield): any combination of COPYX, COPYY and COPYZ
	- Used by Limit Location (LIMITLOC) constraint:
		- LIMIT (bitfield): any combination of LIMIT_XMIN, LIMIT_XMAX,
			LIMIT_YMIN, LIMIT_YMAX, LIMIT_ZMIN, LIMIT_ZMAX
		- XMIN (float): clamped to [-1000.0,1000.0]
		- XMAX (float): clamped to [-1000.0,1000.0]
		- YMIN (float): clamped to [-1000.0,1000.0]
		- YMAX (float): clamped to [-1000.0,1000.0]
		- ZMIN (float): clamped to [-1000.0,1000.0]
		- ZMAX (float): clamped to [-1000.0,1000.0]
	- Used by Limit Rotation (LIMITROT) constraint:
		- LIMIT (bitfield): any combination of LIMIT_XROT, LIMIT_YROT, 
			LIMIT_ZROT
		- XMIN (float): clamped to [-360.0,360.0]
		- XMAX (float): clamped to [-360.0,360.0]
		- YMIN (float): clamped to [-360.0,360.0]
		- YMAX (float): clamped to [-360.0,360.0]
		- ZMIN (float): clamped to [-360.0,360.0]
		- ZMAX (float): clamped to [-360.0,360.0]
	- Used by Limit Scale (LIMITSIZE) constraint:
		- LIMIT (bitfield): any combination of LIMIT_XMIN, LIMIT_XMAX,
			LIMIT_YMIN, LIMIT_YMAX, LIMIT_ZMIN, LIMIT_ZMAX
		- XMIN (float): clamped to [0.0001,1000.0]
		- XMAX (float): clamped to [0.0001,1000.0]
		- YMIN (float): clamped to [0.0001,1000.0]
		- YMAX (float): clamped to [0.0001,1000.0]
		- ZMIN (float): clamped to [0.0001,1000.0]
		- ZMAX (float): clamped to [0.0001,1000.0]
	- Used by Python Script (PYTHON) constraint:
		- SCRIPT (Text): script to use
		- PROPERTIES (IDProperties): ID-Properties of constraint
	- Used by Child Of (CHILDOF) constraint:
		- COPY (bitfield): any combination of PARLOCX, PARLOCY, PARLOCZ, 
			PARROTX, PARROTY, PARROTZ, PARSIZEX, PARSIZEY, PARSIZEZ.
	- Used by Transformation (TRANSFORM) constraint:
		- FROM (int): values are LOC, ROT, SCALE
		- TO (int): values are LOC, ROT, SCALE
		- MAPX, MAPY, MAPZ (int): values are LOC, ROT, SCALE
		- EXTRAPOLATE (bool)
		- FROM_MINX, FROM_MINY, FROM_MINZ, FROM_MAXX, 
		  FROM_MAXY, FROM_MAXZ (float):
		  If FROM==LOC, then is clamped to [-1000.0, 1000.0]
		  If FROM==ROT, then is clamped to [-360.0, 360.0]
		  If FROM==SCALE, then is clamped to [0.0001, 1000.0]
		- TO_MINX, TO_MINY, TO_MINZ, TO_MAXX, TO_MAXY, TO_MAXZ (float):
		  If TO==LOC, then is clamped to [-1000.0, 1000.0]
		  If TO==ROT, then is clamped to [-360.0, 360.0]
		  If TO==SCALE, then is clamped to [0.0001, 1000.0]

"""

class Constraints:
	"""
	The Constraints object
	======================
	This object provides access to sequence of
	L{constraints<Constraint.Constraint>} for a particular object.
	They can be accessed from L{Object.constraints<Object.Object.constraints>}.
	or L{PoseBone.constraints<Pose.PoseBone.constraints>}.
	"""

	def __getitem__(index):
		"""
		This operator returns one of the constraints in the stack.
		@type index: int
		@return: an Constraint object
		@rtype: Constraint
		@raise KeyError: index was out of range
		"""

	def __len__():
		"""
		Returns the number of constraints in the constraint stack.
		@return: number of Constraints
		@rtype: int
		"""

	def append(type):
		"""
		Appends a new constraint to the end of the constraint stack.
		@param type: a constant specifying the type of constraint to create. as from L{Type}
		@type type: int constant
		@rtype: Constraint
		@return: the new Constraint
		"""

	def remove(con):
		"""
		Remove a constraint from this objects constraint sequence.
		@param con: a constraint from this sequence to remove.
		@type con: Constraint
		@note: Accessing attributes of the constraint after it is removed will 
		throw an exception.
		"""

	def moveUp(con):
		"""
		Moves the constraint up in the object's constraint stack.
		@param con: a constraint from this sequence to remove.
		@type con: Constraint
		@rtype: None
		"""

	def moveDown(con):
		"""
		Moves the constraint down in the object's constraint stack.
		@param con: a constraint from this sequence to remove.
		@type con: Constraint
		@rtype: None
		"""

class Constraint:
	"""
	The Constraint object
	=====================
	This object provides access to a constraint for a particular object
	accessed from L{Constraints}.
	@ivar name: The name of this constraint. 29 chars max.
	@type name: string
	@ivar type: The type of this constraint. Read-only.  The returned value
	matches the types in L{Type}.
	@type type: int
	@ivar influence: The influence value of the constraint.  Valid values
	are in the range [0.0,1.0].
	@type influence: float
	"""

	def __getitem__(key):
		"""
		This operator returns one of the constraint's data attributes.
		@param key: value from constraint's L{Constraint.Settings} constant
		@type key: int constant
		@return: the requested data
		@rtype: varies
		@raise KeyError: the key does not exist for the constraint
		"""

	def __setitem__(key):
		"""
		This operator changes one of the constraint's data attributes.
		@param key: value from constraint's L{Constraint.Settings} constant
		@type key: int constant
		@raise KeyError: the key does not exist for the constraint
		"""

	def insertKey(frame):
		"""
		Adds an influence keyframe for the constraint Ipo.  
		@rtype: None
		@param frame: the frame number at which to insert the key.
		@type frame: float
		"""
