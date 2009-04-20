# $Id$
# Documentation for KX_IpoActuator
from SCA_IActuator import *

class KX_IpoActuator(SCA_IActuator):
	"""
	IPO actuator activates an animation.
	
	@ivar startFrame: Start frame.
	@type startFrame: float
	@ivar endFrame: End frame.
	@type endFrame: float
	@ivar propName: Use this property to define the Ipo position
	@type propName: string
	@ivar framePropName: Assign this property this action current frame number
	@type framePropName: string
	@ivar type: Play mode for the ipo. (In GameLogic.KX_IPOACT_PLAY, KX_IPOACT_PINGPONG, KX_IPOACT_FLIPPER, KX_IPOACT_LOOPSTOP, KX_IPOACT_LOOPEND, KX_IPOACT_FROM_PROP)
	@type type: int
	@ivar useIpoAsForce: Apply Ipo as a global or local force depending on the local option (dynamic objects only)
	@type useIpoAsForce: bool
	@ivar useIpoAdd: Ipo is added to the current loc/rot/scale in global or local coordinate according to Local flag
	@type useIpoAdd: bool
	@ivar useIpoLocal: Let the ipo acts in local coordinates, used in Force and Add mode.
	@type useIpoLocal: bool
	@ivar useChildren: Update IPO on all children Objects as well
	@type useChildren: bool
	"""
	def set(mode, startframe, endframe, force):
		"""
		Sets the properties of the actuator. (B{deprecated})
		
		@param mode:       "Play", "PingPong", "Flipper", "LoopStop", "LoopEnd" or "FromProp"
		@type mode: string
		@param startframe: first frame to use
		@type startframe: integer
		@param endframe: last frame to use
		@type endframe: integer
		@param force: special mode
		@type force: integer (0=normal, 1=interpret location as force, 2=additive)
		"""
	def setProperty(property):
		"""
		Sets the name of the property to be used in FromProp mode. (B{deprecated})
		
		@type property: string
		"""
	def setStart(startframe):
		"""
		Sets the frame from which the IPO starts playing. (B{deprecated})
		
		@type startframe: integer
		"""
	def getStart():
		"""
		Returns the frame from which the IPO starts playing. (B{deprecated})
		
		@rtype: integer
		"""
	def setEnd(endframe):
		"""
		Sets the frame at which the IPO stops playing. (B{deprecated})
		
		@type endframe: integer
		"""
	def getEnd():
		"""
		Returns the frame at which the IPO stops playing. (B{deprecated})
		
		@rtype: integer
		"""
	def setIpoAsForce(force):
		"""
		Set whether to interpret the ipo as a force rather than a displacement. (B{deprecated})
		
		@type force: boolean
		@param force: KX_TRUE or KX_FALSE
		"""
	def getIpoAsForce():
		"""
		Returns whether to interpret the ipo as a force rather than a displacement. (B{deprecated})
		
		@rtype: boolean
		"""
	def setIpoAdd(add):
		"""
		Set whether to interpret the ipo as additive rather than absolute. (B{deprecated})
		
		@type add: boolean
		@param add: KX_TRUE or KX_FALSE
		"""
	def getIpoAdd():
		"""
		Returns whether to interpret the ipo as additive rather than absolute. (B{deprecated})
		
		@rtype: boolean
		"""
	def setType(mode):
		"""
		Sets the operation mode of the actuator. (B{deprecated})
		
		@param mode: KX_IPOACT_PLAY, KX_IPOACT_PINGPONG, KX_IPOACT_FLIPPER, KX_IPOACT_LOOPSTOP, KX_IPOACT_LOOPEND
		@type mode: string
		"""
	def getType():
		"""
		Returns the operation mode of the actuator. (B{deprecated})
		
		@rtype: integer
		@return: KX_IPOACT_PLAY, KX_IPOACT_PINGPONG, KX_IPOACT_FLIPPER, KX_IPOACT_LOOPSTOP, KX_IPOACT_LOOPEND
		"""
	def setForceIpoActsLocal(local):
		"""
		Set whether to apply the force in the object's local
		coordinates rather than the world global coordinates. (B{deprecated})
	
		@param local: Apply the ipo-as-force in the object's local
		              coordinates? (KX_TRUE, KX_FALSE)
		@type local: boolean
		"""
	def getForceIpoActsLocal():
		"""
		Return whether to apply the force in the object's local
		coordinates rather than the world global coordinates. (B{deprecated})
		"""
