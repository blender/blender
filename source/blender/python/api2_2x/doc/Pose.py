# Blender.Object.Pose module

"""
The Blender.Object.Pose submodule.

Pose
====

This module provides access to B{Pose} objects in Blender.  This Pose is the 
current object-level (as opposed to armature-data level) transformation.

Example::
	import Blender
	from Blender import *


	scn= Scene.GetCurrent()

	# New Armature
	arm_data= Armature.New('myArmature')
	print arm_data
	arm_ob = scn.objects.new(arm_data)
	arm_data.makeEditable()


	# Add 4 bones
	ebones = [Armature.Editbone(), Armature.Editbone(), Armature.Editbone(), Armature.Editbone()]

	# Name the editbones
	ebones[0].name = 'Bone.001'
	ebones[1].name = 'Bone.002'
	ebones[2].name = 'Bone.003'
	ebones[3].name = 'Bone.004'

	# Assign the editbones to the armature
	for eb in ebones:
		arm_data.bones[eb.name]= eb

	# Set the locations of the bones
	ebones[0].head= Mathutils.Vector(0,0,0)
	ebones[0].tail= Mathutils.Vector(0,0,1)
	ebones[1].head= Mathutils.Vector(0,0,1)
	ebones[1].tail= Mathutils.Vector(0,0,2)
	ebones[2].head= Mathutils.Vector(0,0,2)
	ebones[2].tail= Mathutils.Vector(0,0,3)
	ebones[3].head= Mathutils.Vector(0,0,3)
	ebones[3].tail= Mathutils.Vector(0,0,4)

	ebones[1].parent= ebones[0]
	ebones[2].parent= ebones[1]
	ebones[3].parent= ebones[2]

	arm_data.update()
	# Done with editing the armature


	# Assign the pose animation
	pose = arm_ob.getPose()

	act = arm_ob.getAction()
	if not act: # Add a pose action if we dont have one
		act = Armature.NLA.NewAction()
		act.setActive(arm_ob)

	xbones=arm_ob.data.bones.values()
	pbones = pose.bones.values()
	print xbones
	print pbones


	frame = 1
	for pbone in pbones: # set bones to no rotation
		pbone.quat[:] = 1.000,0.000,0.000,0.0000
		pbone.insertKey(arm_ob, frame, Object.Pose.ROT)

	# Set a different rotation at frame 25
	pbones[0].quat[:] = 1.000,0.1000,0.2000,0.20000
	pbones[1].quat[:] = 1.000,0.6000,0.5000,0.40000
	pbones[2].quat[:] = 1.000,0.1000,0.3000,0.40000
	pbones[3].quat[:] = 1.000,-0.2000,-0.3000,0.30000

	frame = 25
	for i in xrange(4):
		pbones[i].insertKey(arm_ob, frame, Object.Pose.ROT)

	pbones[0].quat[:] = 1.000,0.000,0.000,0.0000
	pbones[1].quat[:] = 1.000,0.000,0.000,0.0000
	pbones[2].quat[:] = 1.000,0.000,0.000,0.0000
	pbones[3].quat[:] = 1.000,0.000,0.000,0.0000


	frame = 50	
	for pbone in pbones: # set bones to no rotation
		pbone.quat[:] = 1.000,0.000,0.000,0.0000
		pbone.insertKey(arm_ob, frame, Object.Pose.ROT)



@var ROT: 
@type ROT: Constant
@var LOC: 
@type LOC: Constant
@var SIZE: 
@type SIZE: Constant
"""

class Pose:
	"""
	The Pose object
	===============
		This object gives access to Pose-specific data in Blender.
	@ivar bones: A Dictionary of PosePoseBones (PoseDict) that make up this Pose.
	@type bones: PoseDict Object
	"""

	def update():
		"""
		Save all changes and update the Pose.
		@rtype: None
		"""

class PoseBonesDict:
	"""
	The PoseBonesDict object
	========================
		This object gives dictionary like access to the PoseBones in a Pose. 
		It is internal to blender but is called as 'Pose.bones'
	"""

	def items():
		"""
		Return the key, value pairs in this dictionary
		@rtype: string, PosePoseBone
		@return: All strings, and PosePoseBones in the Pose (in that order)
		"""

	def keys():
		"""
		Return the keys in this dictionary
		@rtype: string
		@return: All strings representing the PosePoseBone names
		"""

	def values():
		"""
		Return the values in this dictionary
		@rtype: BPy_PoseBone
		@return: All PosePoseBones in this dictionary
		"""

class PoseBone:
	"""
	The PoseBone object
	===================
		This object gives access to PoseBone-specific data in Blender. 
	@ivar name: The name of this PoseBone.
	@type name: String
	@ivar loc: The change in location for this PoseBone. this is the equivilent of bone.getLoc() in the old 2.3x python api.
	@type loc: Vector object
	@ivar size: The change in size for this PoseBone (no change is 1,1,1)
	@type size: Vector object
	@ivar quat: The change in rotation for this PoseBone.
	@type quat: Quaternion object
	@ivar head: The final head location for this PoseBone. (not settable)
	@type head: Vector object
	@ivar tail: The final tail location for this PoseBone. (not settable)
	@type tail: Vector object
	@ivar sel: The selection state of this bone
	@type sel: Boolean
	@ivar parent: The parent of this posebone (not settable)
	@type parent: posebone or None
	@ivar displayObject: The object to display in place of the bone. (custom bones)
	@type displayObject: Object or None
	@ivar localMatrix: The matrix combination of rot/size/loc.
	@type localMatrix: Matrix object
	@ivar poseMatrix: The total transformation of this PoseBone including constraints.

	This matrix is in armature space, for the current worldspace location of this pose bone, multiply
	it with its objects worldspace matrix.

	eg. pose_bone.poseMatrix * object.matrixWorld
	
	Setting the poseMatrix only sets the loc/size/rot, before constraints are applied (similar to actions).
	After setting pose matrix, run pose.update() to re-evaluate the pose and see the changes in the 3d view.
	
	@type poseMatrix: Matrix object
	@type constraints: BPy_ConstraintSeq
	@ivar constraints: a sequence of constraints for the object
	@type limitmin: 3-item sequence
	@ivar limitmin: The x,y,z minimum limits on rotation when part of an IK
	@type limitmax: 3-item sequence
	@ivar limitmax: The x,y,z maximum limits on rotation when part of an IK

	@type hasIK: bool
	@ivar hasIK: True if this pose bone is a part of an IK (readonly), when False, other IK related values have no affect.

	@type stretch: float
	@ivar stretch: The amount to stretch to the ik target when part of an IK [0.0 - 1.0]

	@type stiffX: float
	@ivar stiffX: The x stiffness when part of an IK [0.0 - 0.990]
	@type stiffY: float
	@ivar stiffY: The x stiffness when part of an IK [0.0 - 0.990]
	@type stiffZ: float
	@ivar stiffZ: The x stiffness when part of an IK [0.0 - 0.990]
	
	@type limitX: bool
	@ivar limitX: Limit rotation over X axis when part of an IK.
	@type limitY: bool
	@ivar limitY: Limit rotation over Y axis when part of an IK.
	@type limitZ: bool
	@ivar limitZ: Limit rotation over Z axis when part of an IK.
	
	@type lockXRot: bool
	@ivar lockXRot: Disable X DoF when part of an IK.
	@type lockYRot: bool
	@ivar lockYRot: Disable Y DoF when part of an IK.
	@type lockZRot: bool
	@ivar lockZRot: Disable Z DoF when part of an IK.
	@ivar layerMask: Layer bitmask
		Example::
			# set bone to layers 14 and 16
			bone.layerMask = (1<<13) + (1<<15)
	@type layerMask: Int
	"""

	def insertKey(parentObject, frameNumber, type = "[Pose.LOC, Pose.ROT, Pose.SIZE]", fast = False):
		"""
		Insert a pose key for this PoseBone at a frame.
		@type parentObject: Object object
		@param parentObject: The object the pose came from.
		@type frameNumber: integer
		@param frameNumber: The frame number to insert the pose key on.
		@type type: Constant object
		@param type: Optional argumentm.
		Can be any combination of 3 Module constants:
			- Pose.LOC
			- Pose.ROT (This adds keyframes to the quat ipo, since quaternions are used for pose bone rotation)
			- Pose.SIZE
		If this argument is omitted all keys will be added.
		@type fast: Bool
		@param fast: If enabled, the IPOs will not be recalculated, speeds up adding many keyframes at once.
		@rtype: None
		"""

