# Blender.Metaball module and the Metaball PyType metaball

"""
The Blender.Metaball submodule

This module provides access to B{Metaball} data in Blender and the elements they contain.


Example::
 import Blender
 mb = Blender.Metaball.New()
 for i in xrange(20):
   element= mb.elements.add()
   element.co = Blender.Mathutils.Vector(i, 0, 0) 
 sce = Blender.Scene.GetCurrent()
 sce.objects.new(mb)



Example::
    # Converts the active armature into metaballs
    from Blender import *
    def main():

            scn= Scene.GetCurrent()
            ob_arm= scn.objects.active
            if not ob_arm or ob_arm.type!='Armature':
                    Draw.PupMenu('No Armature Selected')
                    return
            arm= ob_arm.data

            res= Draw.PupFloatInput('res:', 0.2, 0.05, 2.0)
            if not res:
                    return

            # Make a metaball
            mb= Metaball.New()
            mb.wiresize= res

            # Link to the Scene
            ob_mb = scn.objects.new(ob_mb)
            ob_arm.sel= 0
            ob_mb.setMatrix(ob_arm.matrixWorld)


            meta_type= 0 # all elemts are ball type
            meta_stiffness= 2.0 # Volume

            for bone in arm.bones.values():
                    print bone

                    # Find out how many metaballs to add based on bone length, 4 min
                    length= bone.length
                    if length < res:
                            mballs= 4
                    else:
                            mballs= int(length/res)
                            if mballs < 4:
                                    mballs = 4

                    print 'metaball count', mballs

                    # get the bone properties
                    head_rad= bone.headRadius
                    tail_rad= bone.tailRadius

                    head_loc= bone.head['ARMATURESPACE']
                    tail_loc= bone.tail['ARMATURESPACE']


                    for i in range(mballs):
                            f= float(i)

                            w1= f/mballs # weighting of this position on the bone for rad and loc
                            w2= 1-w1

                            loc= head_loc*w1 + tail_loc*w2
                            rad= (head_rad*w1 + tail_rad*w2) * 1.3

                            # Add the metaball
                            ml= mb.elements.add()
                            ml.co= loc
                            ml.radius= rad
                            ml.stiffness= meta_stiffness


            Window.RedrawAll()

    main()

@type Types: readonly dictionary
@var Types: MeteElement types.
    - BALL
    - TUBE
    - PLANE
    - ELIPSOID
    - CUBE

@type Update: readonly dictionary
@var Update: MeteElement types.
    - ALWAYS
    - HALFRES
    - FAST
    - NEVER

"""


def New (name):
	"""
	Creates a new Metaball.
	@type name: string
	@param name: The name of the metaball. If this parameter is not given (or not valid) blender will assign a name to the metaball.
	@rtype: Blender Metaball
	@return: The created Metaball.
	"""

def Get (name):
	"""
	Get the Metaball from Blender.
	@type name: string
	@param name: The name of the requested Metaball.
	@rtype: Blender Metaball or a list of Blender Metaballs
	@return: It depends on the 'name' parameter:
			- (name): The Metaball with the given name;
			- ():     A list with all Metaballs in the current scene.
	"""

class Metaball:
	"""
	The Metaball object
	===================
	This metaball gives access to generic data from all metaballs in Blender.
	@ivar elements: Element iterator of MetaElemSeq type.
	@type elements: MetaElemSeq
	@ivar wiresize: display resolution.
		Value clamped between 0.05 and 1.0.

		A lower value results in more polygons.
	@type wiresize: float
	@ivar rendersize: render resolution.
		Value clamped between 0.05 and 1.0.

		A lower value results in more polygons.
	@type rendersize: float
	@ivar thresh: Threshold setting for this metaball.
		Value clamped between 0.0 and 5.0.
	@type thresh: float
	@ivar materials: List of up to 16 Materials or None types
		Only the first material of the mother-ball used at the moment.
	@type materials: list
	@ivar update: The update method to use for this metaball.
	@type update: int
	"""
	
	def __copy__():
		"""
		Return a copy of this metaball object data.
		@rtype: Metaball
		@return: Metaball
		"""

import id_generics
Metaball.__doc__ += id_generics.attributes 


class MetaElemSeq:
	"""
	The MetaElemSeq object
	======================
		This object provides sequence and iterator access to the metaballs elements.
		The elements accessed within this iterator "wraps" the actual metaball elements; changing any
		of the elements's attributes will immediately change the data in the metaball.

		This iterator is most like pythons 'set' type.
	"""

	def add():
		"""
		Append a new element to the metaball.
		no arguments are taken, instead a new metaelement is
		added to the metaball data and returned.
		This new element can then be modified.

		@return: a new meta element.
		@rtype: Metaelement
		"""

	def remove(element):
		"""
		remove an element from the metaball data.
		
		if the element is not a part of the metaball data, an error will be raised.

		@return: None
		@rtype: None
		"""

	def __iter__():
		"""
		Iterate over elements in this metaball.

		@return: One of the metaelem in this metaball.
		@rtype: Metaelem
		"""

	def __len__():
		"""
		Iterate over elements in this metaball.

		@return: The number of elements in this metaball
		@rtype: int
		"""

class Metaelem:
	"""
	The Metaelem object
	===================
	This gives direct access to meta element data within a metaball.
	@ivar type: The type of the metaball.
		Values must be from L{Types}

		Example::
			from Blender import Metaball
			mb= Metaball.Get('mb')
			for el in mb.elements:
			  el.type= Metaball.Types.CUBE
	@type type: int
	@ivar co: The location of this element.
	@type co: Vector
	@ivar dims: Element dimensions.
		Values clamped between 0 and 20 on all axies.
	@type dims: Vector
	@ivar quat: Element rotation.
	@type quat: Quaternion
	@ivar stiffness: Element stiffness.
		Value clamped between 0 and 10.
	@type stiffness: float
	@ivar radius: Element radius.
		Value clamped between 0 and 5000.
	@type radius: float
	@ivar negative: Element negative volume status.
	@type negative: bool
	@ivar hide: Element hidden status.
	@type hide: bool
	"""
