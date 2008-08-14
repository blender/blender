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
	def setObject(object):
		"""
		Sets the game object to add.
		
		A copy of the object will be added to the scene when the actuator is activated.
		
		If the object does not exist, this function is ignored.
		
		object can either be a L{KX_GameObject} or the name of an object or None.
		
		@type object: L{KX_GameObject}, string or None
		"""
	def getObject(name_only = 0):
		"""
		Returns the name of the game object to be added.
		
		Returns None if no game object has been assigned to be added.
		@type name_only: bool
		@param name_only: optional argument, when 0 return a KX_GameObject
		@rtype: string, KX_GameObject or None if no object is set
		"""
	def setTime(time):
		"""
		Sets the lifetime of added objects, in frames.
		
		If time == 0, the object will last forever.
		
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
