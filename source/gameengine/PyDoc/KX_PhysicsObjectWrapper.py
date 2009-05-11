from PyObjectPlus import *

class KX_PhysicsObjectWrapper(PyObjectPlus):
	"""
	KX_PhysicsObjectWrapper
	
	"""
	def setActive(active):
		"""
		Set the object to be active.
		
		@param active: set to True to be active
		@type active: bool
		"""

	def setAngularVelocity(x, y, z, local):
		"""
		Set the angular velocity of the object.
		
		@param x: angular velocity for the x-axis
		@type x: float
		
		@param y: angular velocity for the y-axis
		@type y: float
		
		@param z: angular velocity for the z-axis
		@type z: float
		
		@param local: set to True for local axis
		@type local: bool
		"""
	def setLinearVelocity(x, y, z, local):
		"""
		Set the linear velocity of the object.
		
		@param x: linear velocity for the x-axis
		@type x: float
		
		@param y: linear velocity for the y-axis
		@type y: float
		
		@param z: linear velocity for the z-axis
		@type z: float
		
		@param local: set to True for local axis
		@type local: bool
		"""
	def setPosition(x, y, z):
		"""
		Set the position of the object
		
		@param x: x coordinate
		@type x: float
		
		@param y: y coordinate
		@type y: float
		
		@param z: z coordinate
		@type z: float
		"""
