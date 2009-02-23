# $Id$
# Documentation for the logic brick base class SCA_ILogicBrick
from KX_GameObject import *

class SCA_ILogicBrick:
	"""
	Base class for all logic bricks.
	
	@ivar executePriority: This determines the order controllers are evaluated, and actuators are activated (lower priority is executed first).
	@type executePriority: int
	"""
	
	def getOwner():
		"""
		Gets the game object associated with this logic brick.
		
		@rtype: L{KX_GameObject}
		"""

	#--The following methods are deprecated--
	def setExecutePriority(priority):
		"""
		Sets the priority of this logic brick.
		
		This determines the order controllers are evaluated, and actuators are activated.
		Bricks with lower priority will be executed first.
		
		Deprecated: Use the "executePriority" property instead.
		
		@type priority: integer
		@param priority: the priority of this logic brick.
		"""
	def getExecutePriority():
		"""
		Gets the execution priority of this logic brick.
		
		Deprecated: Use the "executePriority" property instead.
		
		@rtype: integer
		@return: this logic bricks current priority.
		"""
