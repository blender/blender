# $Id$
# Documentation for KX_SCA_AddObjectActuator
from SCA_IActuator import *

class KX_SCA_AddObjectActuator(SCA_IActuator):
	"""
	Edit Object Actuator (in Add Object Mode)
	
	@warning: An Add Object actuator will be ignored if at game start, the linked object doesn't exist
		  (or is empty) or the linked object is in an active layer.
		  
		  This will genereate a warning in the console:
		  
		  C{ERROR: GameObject I{OBName} has a AddObjectActuator I{ActuatorName} without object (in 'nonactive' layer)}
	"""
	def setObject(obj):
		"""
		Sets the game object to add.
		
		A copy of the object will be added to the scene.
		
		If the object does not exist, this function is ignored.
		
		obj can either be a L{KX_GameObject} or the name of an object.
		
		@type obj: L{KX_GameObject} or string
		"""
	def getObject():
		"""
		Returns the name of the game object to be added.
		
		Returns None if no game object has been assigned to be added.
		
		@rtype: string
		"""
	def setTime(time):
		"""
		Sets the lifetime of added objects, in frames.
		
		@type time: integer
		@param time: The minimum value for time is 0.
		"""
	def getTime():
		"""
		Returns the lifetime of the added object, in frames.
		
		@rtype: integer
		"""
	def setLinearVelocity(vx, vy, vz):
		"""
		Sets the initial linear velocity of added objects.
		
		@type vx: float
		@param vx: the x component of the initial linear velocity.
		@type vy: float
		@param vy: the y component of the initial linear velocity.
		@type vz: float
		@param vz: the z component of the initial linear velocity.
		"""
	def getLinearVelocity():
		"""
		Returns the initial linear velocity of added objects.
		
		@rtype: list [vx, vy, vz]
		"""
	def getLastCreatedObject():
		"""
		Returns the last object created by this actuator.
		
		@rtype: L{KX_GameObject}
		@return: A L{KX_GameObject} or None if no object has been created.
		"""
