# $Id$
# Documentation for KX_TouchSensor
from SCA_ISensor import *

class KX_TouchSensor(SCA_ISensor):
	"""
	Touch sensor detects collisions between objects.
	"""
	def setProperty(name):
		"""
		Set the property or material to collide with. Use
		setTouchMaterial() to switch between properties and
		materials.
		@type name: string
		"""
	def getProperty():
		"""
		Returns the property or material to collide with. Use
		getTouchMaterial() to find out whether this sensor
		looks for properties or materials.
		
		@rtype: string
		"""

	def getHitObject():
		"""
		Returns the last object hit by this touch sensor.
		
		@rtype: KX_GameObject
		"""
	def getHitObjectList():
		"""
		Returns a list of all objects hit in the last frame.
		
		Only objects that have the requisite material/property are listed.
		
		@rtype: list [KX_GameObject]
		"""
	def getTouchMaterial():
		"""
		Returns KX_TRUE if this sensor looks for a specific material,
		KX_FALSE if it looks for a specific property.
		"""
	def setTouchMaterial(flag):
		"""
		Set flag to KX_TRUE to switch on positive pulse mode,
		KX_FALSE to switch off positive pulse mode.
		
		@type flag: KX_TRUE or KX_FALSE.
		"""
