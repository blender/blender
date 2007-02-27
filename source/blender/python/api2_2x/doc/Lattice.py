# Blender.Lattice module and the Lattice PyType object

"""
The Blender.Lattice submodule.

Lattice Object
==============

This module provides access to B{Lattice} object in Blender.

Example::
	import Blender
	from Blender import Lattice, Object, Scene, Modifier

	# Make new lattice data
	lattice_data = Lattice.New()
	lattice_data.setPartitions(5,5,5)
	lattice_data.setKeyTypes(Lattice.LINEAR, Lattice.CARDINAL, Lattice.BSPLINE)
	lattice_data.setMode(Lattice.OUTSIDE)

	for y in range(125):
		vec = lattice_data.getPoint(y)
		co1 = vec[0] + vec[0] / 5
		co2 = vec[1] - vec[2] * 0.3
		co3 = vec[2] * 3
		lattice_data.setPoint(y,[co1,co2,co3])
	
	# Create a new object from the lattice in the current scene
	scn = Scene.GetCurrent()
	ob_lattice = scn.objects.new(lattice_data)
	
	# Get an object to deform with this lattice
	mySphere = Object.Get('Sphere')

	# Apply lattice modifier
	mod= mySphere.modifiers.append(Modifier.Type.LATTICE)
	mod[Modifier.Settings.OBJECT] = ob_lattice
	mySphere.makeDisplayList()

	Blender.Redraw()
"""

def New (name = None):
	"""
	Create a new Lattice object.
	Passing a name to this function will name the Lattice
	datablock, otherwise the Lattice data will be given a 
	default name.
	@type name: string
	@param name: The Lattice name.
	@rtype: Blender Lattice
	@return: The created Lattice Data object.
	"""

def Get (name = None):
	"""
	Get the Lattice object(s) from Blender.
	@type name: string
	@param name: The name of the Lattice object.
	@rtype: Blender Lattice or a list of Blender Lattices
	@return: It depends on the 'name' parameter:
			- (name): The Lattice object with the given name;
			- ():     A list with all Lattice objects in the current scene.
	"""

class Lattice:
	"""
	The Lattice object
	==================
		This object gives access to Lattices in Blender.
	@ivar width: The number of x dimension partitions.
	@ivar height: The number of y dimension partitions.
	@ivar depth: The number of z dimension partitions.
	@ivar widthType: The x dimension key type.
	@ivar heightType: The y dimension key type.
	@ivar depthType: The z dimension key type.
	@ivar mode: The current mode of the Lattice.
	@ivar latSize: The number of points in this Lattice (width*height*depth).
	@cvar key: The L{Key.Key} object associated with this Lattice or None.
	"""

	def getName():
		"""
		Get the name of this Lattice datablock.
		@rtype: string
		@return: The name of the Lattice datablock.
		"""

	def setName(name):
		"""
		Set the name of this Lattice datablock.
		@type name: string
		@param name: The new name.
		"""

	def getPartitions():
		"""
		Gets the number of 'walls' or partitions that the Lattice has 
		in the x, y, and z dimensions.
		@rtype: list of ints
		@return: A list corresponding to the number of partitions: [x,y,z]
		"""

	def setPartitions(x,y,z):
		"""
		Set the number of 'walls' or partitions that the 
		Lattice will be created with in the x, y, and z dimensions.
		@type x: int
		@param x: The number of partitions in the x dimension of the Lattice.
		@type y: int
		@param y: The number of partitions in the y dimension of the Lattice.
		@type z: int
		@param z: The number of partitions in the z dimension of the Lattice.
		"""

	def getKeyTypes():
		"""
		Returns the deformation key types for the x, y, and z dimensions of the
		Lattice.
		@rtype: list of strings
		@return: A list corresponding to the key types will be returned: [x,y,z]
		"""

	def setKeyTypes(xType,yType,zType):
		"""
		Sets the deformation key types for the x, y, and z dimensions of the
		Lattice.
		There are three key types possible:
			-  Lattice.CARDINAL
			-  Lattice.LINEAR
			-  Lattice.BSPLINE
		@type xType: enum constant
		@param xType: the deformation key type for the x dimension of the Lattice
		@type yType: enum constant
		@param yType: the deformation key type for the y dimension of the Lattice
		@type zType: enum constant
		@param zType: the deformation key type for the z dimension of the Lattice
		"""

	def getMode():
		"""
		Returns the current Lattice mode
		@rtype: string
		@return: A string representing the current Lattice mode
		"""

	def setMode(modeType):
		"""
		Sets the current Lattice mode
		There are two Lattice modes possible:
			-  Lattice.GRID
			-  Lattice.OUTSIDE
		@type modeType: enum constant
		@param modeType: the Lattice mode
		"""

	def getPoint(index):
		"""
		Returns the coordinates of a point in the Lattice by index.
		@type index: int
		@param index: The index of the point on the Lattice you want returned
		@rtype: list of floats
		@return: The x,y,z coordiates of the Lattice point : [x,y,z]
		"""

	def setPoint(index, position):
		"""
		Sets the coordinates of a point in the Lattice by index.
		@type index: int
		@param index: The index of the point on the Lattice you want set
		@type position: list of floats
		@param position: The x,y,z coordinates that you want the point to be: [x,y,z]
		"""

	def getKey():
		"""
		Returns the L{Key.Key} object associated with this Lattice.
		@rtype: L{Key.Key}
		@return: A key object representing the keyframes of the lattice or None.
		"""

	def insertKey(frame):
		"""
		Inserts the current state of the Lattice as a new absolute keyframe

		B{Example}::
			for z in range(5):
				for y in range(125):
					vec = myLat.getPoint(y)
					co1 = vec[0] + vec[2]
					co2 = vec[1] - vec[2]
					co3 = vec[2] + vec[1]
					myLat.setPoint(y,[co1,co2,co3])
				w = (z + 1) * 10
				myLat.insertKey(w)

		@type frame: int
		@param frame: the frame at which the Lattice will be set as a keyframe
		"""

	def __copy__ ():
		"""
		Make a copy of this lattice
		@rtype: Lattice
		@return:  a copy of this lattice
		"""

import id_generics
Lattice.__doc__ += id_generics.attributes
