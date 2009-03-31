# $Id$
# Documentation for KX_TrackToActuator
from SCA_IActuator import *

class KX_TrackToActuator(SCA_IActuator):
	"""
	Edit Object actuator in Track To mode.
	
	@warning: Track To Actuators will be ignored if at game start, the
		object to track to is invalid.
		
		This will generate a warning in the console:
		
		C{ERROR: GameObject I{OBName} no object in EditObjectActuator I{ActuatorName}}

	@ivar object: the object this actuator tracks.
	@type object: KX_GameObject or None
	@ivar time: the time in frames with which to delay the tracking motion
	@type time: integer
	@ivar use3D: the tracking motion to use 3D
	@type use3D: boolean
	
	"""
	def setObject(object):
		"""
        DEPRECATED: Use the object property.
		Sets the object to track.
		
		@type object: L{KX_GameObject}, string or None
		@param object: Either a reference to a game object or the name of the object to track.
		"""
	def getObject(name_only):
		"""
        DEPRECATED: Use the object property.
		Returns the name of the object to track.
		
		@type name_only: bool
		@param name_only: optional argument, when 0 return a KX_GameObject
		@rtype: string, KX_GameObject or None if no object is set
		"""
	def setTime(time):
		"""
        DEPRECATED: Use the time property.
		Sets the time in frames with which to delay the tracking motion.
		
		@type time: integer
		"""
	def getTime():
		"""
        DEPRECATED: Use the time property.
		Returns the time in frames with which the tracking motion is delayed.
		
		@rtype: integer
		"""
	def setUse3D(use3d):
		"""
        DEPRECATED: Use the use3D property.
		Sets the tracking motion to use 3D.
		
		@type use3d: boolean
		@param use3d: - True: allow the tracking motion to extend in the z-direction.
		              - False: lock the tracking motion to the x-y plane.
		"""
	def getUse3D():
		"""
        DEPRECATED: Use the use3D property.
		Returns True if the tracking motion will track in the z direction.
		
		@rtype: boolean
		"""
