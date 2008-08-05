# $Id$
# Documentation for KX_IpoActuator
from SCA_IActuator import *

class KX_IpoActuator(SCA_IActuator):
	"""
	IPO actuator activates an animation.
	"""
	def set(mode, startframe, endframe, mode):
		"""
		Sets the properties of the actuator.
		
		@param mode:       "Play", "PingPong", "Flipper", "LoopStop", "LoopEnd" or "FromProp"
		@type mode: string
		@param startframe: first frame to use
		@type startframe: integer
		@param endframe: last frame to use
		@type endframe: integer
		@param mode: special mode
		@type mode: integer (0=normal, 1=interpret location as force, 2=additive)
		"""
	def setProperty(property):
		"""
		Sets the name of the property to be used in FromProp mode.
		
		@type property: string
		"""
	def setStart(startframe):
		"""
		Sets the frame from which the IPO starts playing.
		
		@type startframe: integer
		"""
	def getStart():
		"""
		Returns the frame from which the IPO starts playing.
		
		@rtype: integer
		"""
	def setEnd(endframe):
		"""
		Sets the frame at which the IPO stops playing.
		
		@type endframe: integer
		"""
	def getEnd():
		"""
		Returns the frame at which the IPO stops playing.
		
		@rtype: integer
		"""
	def setIpoAsForce(force):
		"""
		Set whether to interpret the ipo as a force rather than a displacement.
		
		@type force: boolean
		@param force: KX_TRUE or KX_FALSE
		"""
	def getIpoAsForce():
		"""
		Returns whether to interpret the ipo as a force rather than a displacement.
		
		@rtype: boolean
		"""
	def setIpoAdd(add):
		"""
		Set whether to interpret the ipo as additive rather than absolute.
		
		@type add: boolean
		@param add: KX_TRUE or KX_FALSE
		"""
	def getIpoAdd():
		"""
		Returns whether to interpret the ipo as additive rather than absolute.
		
		@rtype: boolean
		"""
	def setType(mode):
		"""
		Sets the operation mode of the actuator.
		
		@param mode: KX_IPOACT_PLAY, KX_IPOACT_PINGPONG, KX_IPOACT_FLIPPER, KX_IPOACT_LOOPSTOP, KX_IPOACT_LOOPEND
		@type mode: string
		"""
	def getType():
		"""
		Returns the operation mode of the actuator.
		
		@rtype: integer
		@return: KX_IPOACT_PLAY, KX_IPOACT_PINGPONG, KX_IPOACT_FLIPPER, KX_IPOACT_LOOPSTOP, KX_IPOACT_LOOPEND
		"""
	def setForceIpoActsLocal(local):
		"""
		Set whether to apply the force in the object's local
		coordinates rather than the world global coordinates.
	
		@param local: Apply the ipo-as-force in the object's local
		              coordinates? (KX_TRUE, KX_FALSE)
		@type local: boolean
		"""
	def getForceIpoActsLocal():
		"""
		Return whether to apply the force in the object's local
		coordinates rather than the world global coordinates.
		"""
