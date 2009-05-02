# $Id$
# Documentation for KX_GameActuator
from SCA_IActuator import *

class KX_GameActuator(SCA_IActuator):
	"""
	The game actuator loads a new .blend file, restarts the current .blend file or quits the game.
	
	Properties:
	
	@ivar file: the new .blend file to load
	@type file: string.
	@ivar mode: The mode of this actuator
	@type mode: int from 0 to 5 L{GameLogic.Game Actuator}
	"""
	def getFile():
		"""
		DEPRECATED: use the file property
		Returns the filename of the new .blend file to load.
		
		@rtype: string
		"""
	def setFile(filename):
		"""
		DEPRECATED: use the file property
		Sets the new .blend file to load.
		
		@param filename: The file name this actuator will load.
		@type filename: string
		"""

