# $Id$
# Documentation for KX_TouchSensor
from SCA_ISensor import *
from KX_GameObject import *

class KX_TouchSensor(SCA_ISensor):
	"""
	Touch sensor detects collisions between objects.
	
	@ivar property: The property or material to collide with.
	@type property: string
	@ivar useMaterial: Determines if the sensor is looking for a property or material.
						KX_True = Find material; KX_False = Find property
	@type useMaterial: boolean
	@ivar pulseCollisions: The last collided object.
	@type pulseCollisions: bool
	@ivar objectHit: The last collided object. (Read Only)
	@type objectHit: L{KX_GameObject} or None
	@ivar objectHitList: A list of colliding objects. (Read Only)
	@type objectHitList: L{CListValue<CListValue.CListValue>} of L{KX_GameObject<KX_GameObject.KX_GameObject>}
	"""
		
	#--The following methods are deprecated, please use properties instead.
	def setProperty(name):
		"""
		DEPRECATED: use the property property
		Set the property or material to collide with. Use
		setTouchMaterial() to switch between properties and
		materials.
		@type name: string
		"""
		
	def getProperty():
		"""
		DEPRECATED: use the property property
		Returns the property or material to collide with. Use
		getTouchMaterial() to find out whether this sensor
		looks for properties or materials. (B{deprecated})
		
		@rtype: string
		"""
	def getHitObject():
		"""
		DEPRECATED: use the objectHit property
		Returns the last object hit by this touch sensor. (B{deprecated})
		
		@rtype: L{KX_GameObject}
		"""
	def getHitObjectList():
		"""
		DEPRECATED: use the objectHitList property
		Returns a list of all objects hit in the last frame. (B{deprecated})
		
		Only objects that have the requisite material/property are listed.
		
		@rtype: L{CListValue<CListValue.CListValue>} of L{KX_GameObject<KX_GameObject.KX_GameObject>}
		"""
	def getTouchMaterial():
		"""
		DEPRECATED: use the useMaterial property
		Returns KX_TRUE if this sensor looks for a specific material,
		KX_FALSE if it looks for a specific property. (B{deprecated})
		"""
