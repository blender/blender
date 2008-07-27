# Blender.Armature module and the Armature PyType object

"""
The Blender.Armature submodule.

Armature
========

This module provides access to B{Armature} objects in Blender.  These are
"skeletons", used to deform and animate other objects -- meshes, for
example.

Example::
  import Blender
  from Blender import Armature
  from Blender.Mathutils import *
  #
  arms = Armature.Get()
  for arm in arms.values():
    arm.drawType = Armature.STICK #set the draw type
    arm.makeEditable() #enter editmode

    #generating new editbone
    eb = Armature.Editbone()
    eb.roll = 10
    eb.parent = arm.bones['Bone.003']
    eb.head = Vector(1,1,1)
    eb.tail = Vector(0,0,1)
    eb.options = [Armature.HINGE, Armature.CONNECTED]

    #add the bone
    arm.bones['myNewBone'] = eb
  
    #delete an old bone
    del arm.bones['Bone.002']

    arm.update()  #save changes

    for bone in arm.bones.values():
      #print bone.matrix['ARMATURESPACE']
      print bone.parent, bone.name
      print bone.children, bone.name
      print bone.options, bone.name


Example::
	# Adds empties for every bone in the selected armature, an example of getting worldspace locations for bones.
	from Blender import *
	def test_arm():
		scn= Scene.GetCurrent()
		arm_ob= scn.objects.active

		if not arm_ob or arm_ob.type != 'Armature':
			Draw.PupMenu('not an armature object')
			return

		# Deselect all
		for ob in scn.objects:
			if ob != arm_ob:
				ob.sel= 0

		arm_mat= arm_ob.matrixWorld

		arm_data= arm_ob.getData()

		bones= arm_data.bones.values()
		for bone in bones:
			bone_mat= bone.matrix['ARMATURESPACE']
			bone_mat_world= bone_mat*arm_mat

			ob_empty= scn.objects.new('Empty')
			ob_empty.setMatrix(bone_mat_world)

	test_arm()

@var CONNECTED: Connect this bone to parent
@type CONNECTED: Constant
@var HINGE: Don't inherit rotation or scale from parent
@type HINGE: Constant
@var NO_DEFORM: If bone will not deform geometry
@type NO_DEFORM: Constant
@var MULTIPLY: Multiply bone with vertex group
@type MULTIPLY: Constant
@var HIDDEN_EDIT: Bone is hidden in editmode
@type HIDDEN_EDIT: Constant
@var ROOT_SELECTED: Root of the Bone is selected
@type ROOT_SELECTED: Constant
@var BONE_SELECTED: Bone is selected
@type BONE_SELECTED: Constant
@var TIP_SELECTED: Tip of the Bone is selected
@type TIP_SELECTED: Constant
@var LOCKED_EDIT: Prevents the bone from being transformed in editmode
@type LOCKED_EDIT: Constant
@var OCTAHEDRON: Bones drawn as octahedrons
@type OCTAHEDRON: Constant
@var STICK: Bones drawn as a line
@type STICK: Constant
@var BBONE: Bones draw as a segmented B-spline
@type BBONE: Constant
@var ENVELOPE: Bones draw as a stick with envelope influence
@type ENVELOPE: Constant
"""

def Get (name = None):
  """
  Get the Armature object(s) from Blender.
  @type name: string, nothing, or list of strings
  @param name: The string name of an armature.
  @rtype: Blender Armature or a list of Blender Armatures
  @return: It depends on the I{name} parameter:
      - (name): The Armature object with the given I{name};
      - (name, name, ...): A list of Armature objects
      - ():     A list with all Armature objects in the current scene.
  @warning: In versions 2.42 and earlier, a string argument for an armature
    that doesn't exist will return None.  Later versions raise a Value error.
  """

def New (name = None):
	"""
	Return a new armature.
	@type name: string or nothing
	@param name: The string name of the new armature.
	@rtype: Blender Armature.
	@return: A new armature.
	"""

class Armature:
	"""
	The Armature object
	===================
		This object gives access to Armature-specific data in Blender.
	@ivar bones: A Dictionary of Bones (BonesDict) that make up this armature.
	@type bones: BonesDict Object
	@ivar vertexGroups: Whether vertex groups define deformation
	@type vertexGroups: Bool
	@ivar envelopes: Whether bone envelopes define deformation
	@type envelopes: Bool
	@ivar restPosition: Show rest position (no posing possible)
	@type restPosition: Bool
	@ivar delayDeform: Don't deform children when manipulating bones
	@type delayDeform: Bool
	@ivar drawAxes: Draw bone axes
	@type drawAxes: Bool
	@ivar drawNames: Draw bone names
	@type drawNames: Bool
	@ivar ghost: Draw ghosts around frame for current Action
	@type ghost: Bool
	@ivar ghostStep: Number of frames between ghosts
	@type ghostStep: Int
	@ivar drawType: The drawing type that is used to display the armature
	Acceptable values are:
			- Armature.OCTAHEDRON: bones drawn as octahedrons
			- Armature.STICK: bones drawn as sticks
			- Armature.BBONE: bones drawn as b-bones
			- Armature.ENVELOPE: bones drawn as sticks with envelopes
	@type drawType: Constant Object
	@ivar mirrorEdit: X-axis mirrored editing
	@type mirrorEdit: Bool
	@ivar autoIK: Adds temporary IK chains while grabbing bones
	@type autoIK: Bool
	@ivar layerMask: Layer bitmask
		Example::
			# set armature to layers 14 and 16
			armature.layerMask = (1<<13) + (1<<15)
	@type layerMask: Int
	"""

	def __init__(name = 'myArmature'):
		"""
		Initializer for the Armature TypeObject.
		Example::
			  myNewArmature = Blender.Armature.Armature('AR_1')
		@param name: The name for the new armature
		@type name: string
		@return: New Armature Object
		@rtype: Armature Object
		"""
	
	def makeEditable():
		"""
		Put the armature into EditMode for editing purposes. (Enters Editmode)
		@warning: Using Window.Editmode() to switch the editmode manually will cause problems and possibly even crash Blender.
		@warning: This is only needed for operations such as adding and removing bones.
		@warning: Do access pose data until you have called update() or settings will be lost. 
		@warning: The armature should not be in manual editmode
		prior to calling this method. The armature must be parented
		to an object prior to editing.
		@rtype: None
		"""

	def update():
		"""
		Save all changes and update the armature. (Leaves Editmode)
		@note: Must have called makeEditable() first.
		@rtype: None
		"""
	def copy():
		"""
		Return a copy of this armature.
		@rtype: Armature
		"""
	def __copy__():
		"""
		Return a copy of this armature.
		@rtype: Armature
		"""

import id_generics
Armature.__doc__ += id_generics.attributes 

class BonesDict:
	"""
	The BonesDict object
	====================
		This object gives gives dictionary like access to the bones in an armature. 
		It is internal to blender but is called as 'Armature.bones'

		Removing a bone: 
		Example::
			del myArmature.bones['bone_name']
		Adding a bone:
		Example::
			myEditBone = Armature.Editbone()
			myArmature.bones['bone_name'] = myEditBone
	"""

	def items():
		"""
		Return the key, value pairs in this dictionary
		@rtype: string, BPy_bone
		@return: All strings, and py_bones in the armature (in that order)
		"""

	def keys():
		"""
		Return the keys in this dictionary
		@rtype: string
		@return: All strings representing the bone names
		"""

	def values():
		"""
		Return the values in this dictionary
		@rtype: BPy_bone
		@return: All BPy_bones in this dictionary
		"""

class Bone:
	"""
	The Bone object
	===============
		This object gives access to Bone-specific data in Blender. This object
		cannot be instantiated but is returned by BonesDict when the armature is not in editmode.
	@ivar name: The name of this Bone.
	@type name: String
	@ivar roll: This Bone's roll value.
		Keys are:
			 - 'ARMATURESPACE' - this roll in relation to the armature
			 - 'BONESPACE' - the roll in relation to itself 
	@type roll: Dictionary
	@ivar head: This Bone's "head" ending position when in rest state.
		Keys are:
			 - 'ARMATURESPACE' - this head position in relation to the armature
			 - 'BONESPACE' - the head position in relation to itself.
	@type head: Dictionary
	@ivar tail: This Bone's "tail" ending position when in rest state.
		Keys are:
			 - 'ARMATURESPACE' - this tail position in relation to the armature
			 - 'BONESPACE' - the tail position in relation to itself 
	@type tail: Dictionary
	@ivar matrix: This Bone's matrix. This cannot be set.
		Keys are:
			 - 'ARMATURESPACE' - this matrix of the bone in relation to the armature
			 - 'BONESPACE' - the matrix of the bone in relation to itself 
	@type matrix: Matrix Object
	@ivar parent: The parent Bone.
	@type parent: Bone Object
	@ivar children: The children directly attached to this bone.
	@type children: List of Bone Objects
	@ivar weight: The bone's weight.
	@type weight: Float
	@ivar options: Various bone options which can be:
			 - Armature.CONNECTED: IK to parent
			 - Armature.HINGE: No parent rotation or scaling
			 - Armature.NO_DEFORM: The bone does not deform geometry
			 - Armature.MULTIPLY: Multiply vgroups by envelope
			 - Armature.HIDDEN_EDIT: Hide bones in editmode
			 - Armature.ROOT_SELECTED: Selection of root ball of bone
			 - Armature.BONE_SELECTED: Selection of bone
			 - Armature.TIP_SELECTED: Selection of tip ball of bone
			 - Armature.LOCKED_EDIT: Prevents the bone from being transformed in editmode
	@type options: List of Constants
	@ivar subdivision: The number of bone subdivisions.
	@type subdivision: Int
	@ivar deformDist: The deform distance of the bone
	@type deformDist: Float
	@ivar length: The length of the bone. This cannot be set.
	@type length: Float
	@ivar headRadius: The radius of this bones head (used for envalope bones)
	@type headRadius: Float
	@ivar tailRadius: The radius of this bones head (used for envalope bones)
	@type tailRadius: Float
	@ivar layerMask: Layer bitmask
		Example::
			# set bone to layers 14 and 16
			bone.layerMask = (1<<13) + (1<<15)
	@type layerMask: Int
	"""

	def hasParent():
		"""
		Whether or not this bone has a parent
		@rtype: Bool
		"""

	def hasChildren():
		"""
		Whether or not this bone has children
		@rtype: Bool
		"""

	def getAllChildren():
		"""
		Gets all the children under this bone including the children's children.
		@rtype: List of Bone object
		@return: all bones under this one
		"""

class Editbone:
	"""
	The Editbone Object
	===================
		This object is a wrapper for editbone data and is used only in the manipulation
		of the armature in editmode.
	@ivar name: The name of this Bone.
	@type name: String
	@ivar roll: This Bone's roll value (armaturespace).
	@type roll: Float
	@ivar head: This Bone's "head" ending position when in rest state (armaturespace).
	@type head: Vector Object
	@ivar tail: This Bone's "tail" ending position when in rest state (armaturespace).
	@type tail: Vector Object
	@ivar matrix: This Bone's matrix. (armaturespace)
	@type matrix: Matrix Object
	@ivar parent: The parent Bone.
	@type parent: Editbone Object
	@ivar weight: The bone's weight.
	@type weight: Float
	@ivar options: Various bone options which can be:
			 - Armature.CONNECTED: IK to parent
			 - Armature.HINGE: No parent rotation or scaling
			 - Armature.NO_DEFORM: The bone does not deform geometry
			 - Armature.MULTIPLY: Multiply vgroups by envelope
			 - Armature.HIDDEN_EDIT: Hide bones in editmode
			 - Armature.ROOT_SELECTED: Selection of root ball of bone
			 - Armature.BONE_SELECTED: Selection of bone
			 - Armature.TIP_SELECTED: Selection of tip ball of bone
	@type options: List of Constants
	@ivar subdivision: The number of bone subdivisions.
	@type subdivision: Int
	@ivar deformDist: The deform distance of the bone
	@type deformDist: Float
	@ivar length: The length of the bone. This cannot be set.
	@type length: Float
	@ivar headRadius: The radius of this bones head (used for envalope bones)
	@type headRadius: Float
	@ivar tailRadius: The radius of this bones head (used for envalope bones)
	@type tailRadius: Float
	"""

	def hasParent():
		"""
		Whether or not this bone has a parent
		@rtype: Bool
		"""

	def clearParent():
		"""
		Set the parent to None
		@rtype: None
		"""
