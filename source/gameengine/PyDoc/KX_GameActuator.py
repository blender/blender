# $Id$
# Documentation for KX_GameActuator
from SCA_IActuator import *

class KX_GameActuator(SCA_IActuator):
	"""
	The game actuator loads a new .blend file, restarts the current .blend file or quits the game.
	"""
	def getFile():
		"""
		Returns the filename of the new .blend file to load.
		
		@rtype: string
		"""
	def setFile(filename):
		"""
		Sets the new .blend file to load.
		
		@param filename: The file name this actuator will load.
		@type filename: string
		"""

