#
# Documentation for PyObjectPlus base class

class PyObjectPlus:
	"""
	PyObjectPlus base class of most other types in the Game Engine.
	
	@ivar invalid:	Test if the object has been freed by the game engine and is no longer valid.
					
					Normally this is not a problem but when storing game engine data in the GameLogic module,
					KX_Scenes or other KX_GameObjects its possible to hold a reference to invalid data.
					Calling an attribute or method on an invalid object will raise a SystemError.
					
					The invalid attribute allows testing for this case without exception handling.
	@type invalid:	bool
	"""
	
	def isA(game_type):
		"""
		Check if this is a type or a subtype game_type.

		@param game_type: the name of the type or the type its self from the L{GameTypes} module.
		@type game_type: string or type
		@return: True if this object is a type or a subtype of game_type.
		@rtype: bool
		"""
