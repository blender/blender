# Blender.Text3d.Font module and the Font PyType object

"""
The Blender.Text3d.Font subsubmodule.

Text3d.Font Objects
===================

This module provides access to B{Font} objects in Blender.

Example::
	import Blender
	from Blender import Text3d
	
	# Load a font
	myfont= Text3d.Font.Load('/usr/share/fonts/ttf/verdana.ttf')
	
	# 
	for font in Text3d.Font.Get():
		print font.name, font.filename, font.packed
"""

def Load (filename):
	"""
	Create a new Text3d.Font object.
	@type filename: string
	@param filename: file of the font
	@rtype: Blender Text3d.Font
	@return: The created Text3d.Font Data object.
	"""

def Get (name = None):
	"""
	Get the Text3d.Font object(s) from Blender.
	@type name: string
	@param name: The name of the Text3d object.
	@rtype: Blender Text3d or a list of Blender Text3ds
	@return: It depends on the 'name' parameter:
			- (name): The Text3d object with the given name;
			- ():     A list with all Font objects in the current .blend file.
	"""
class Font:
	"""
	The Text3d.Font object
	======================
		This object gives access  Blender's B{Font} objects
	@ivar filename: The filename (path) of the file loaded into this Font.
	@type filename: string
	@ivar packed: Boolean, True when the sample is packed (readonly).
	@type packed: string
	"""

	def pack():
		"""
		Packs the font into the current blend file.
		@note: 
		@returns: nothing
		@rtype: none
		"""

	def unpack(mode):
		"""
		Unpacks the font.
		@param mode: One of the values in Blender.UnpackModes dict.
		@note: An error will be raised if the font is not packed or the filename path does not exist.
		@returns: nothing
		@rtype: none
		@type mode: int
		"""
