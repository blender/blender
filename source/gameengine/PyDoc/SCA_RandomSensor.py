# $Id$
# Documentation for SCA_RandomSensor
from SCA_ISensor import *

class SCA_RandomSensor(SCA_ISensor):
	"""
	This sensor activates randomly.
	"""
	
	def setSeed(seed):
		"""
		Sets the seed of the random number generator.
		
		If the seed is 0, the generator will produce the same value on every call.
		
		@type seed: integer.
		"""
	def getSeed():
		"""
		Returns the initial seed of the generator.  Equal seeds produce equal random
		series.
		
		@rtype: integer
		"""
	def getLastDraw():
		"""
		Returns the last random number generated.
		"""
