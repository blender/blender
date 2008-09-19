# Blender.Text module and the Text PyType object

"""
The Blender.Text submodule.

Text Objects
============

This module provides access to B{Text} objects in Blender.

Example::
	import Blender
	from Blender import Text
	#
	txt = Text.New("MyText")          # create a new Text object
	print Text.Get()                  # current list of Texts in Blender
	txt.write("Appending some ")      # appending text
	txt.write("text to my\\n")         # '\\n' inserts new-line markers
	txt.write("text buffer.")
	print txt.asLines()               # retrieving the buffer as a list of lines
	Text.unlink(txt)                  # removing a Text object
"""

def New (name = None, follow_cursor = 0):
	"""
	Create a new Text object.
	@type name: string
	@param name: The Text name.
	@type follow_cursor: int
	@param follow_cursor: The text follow flag: if 1, the text display always
			follows the cursor.
	@rtype: Blender Text
	@return: The created Text Data object.
	"""

def Get (name = None):
	"""
	Get the Text object(s) from Blender.
	@type name: string
	@param name: The name of the Text object.
	@rtype: Blender Text or a list of Blender Texts
	@return: It depends on the 'name' parameter:
			- (name): The Text object with the given name;
			- ():     A list with all Text objects in the current scene.
	"""

def Load (filename):
	"""
	Load a file into a Blender Text object.
	@type filename: string
	@param filename:  The name of the file to load.
	@rtype: Blender Text
	@return: A Text object with the contents of the loaded file.
	"""

def unlink(textobj):
	"""
	Unlink (remove) the given Text object from Blender.
	@type textobj: Blender Text
	@param textobj: The Text object to be deleted.
	"""

class Text:
	"""
	The Text object
	===============
		This object gives access to Texts in Blender.
	@ivar filename: The filename of the file loaded into this Text.
	@ivar mode: The follow_mode flag: if 1 it is 'on'; if 0, 'off'.
	@ivar nlines: The number of lines in this Text.
	"""

	def getName():
		"""
		Get the name of this Text object.
		@rtype: string
		"""

	def setName(name):
		"""
		Set the name of this Text object.
		@type name: string
		@param name: The new name.
		"""

	def getFilename():
		"""
		Get the filename of the file loaded into this Text object.
		@rtype: string
		"""

	def getNLines():
		"""
		Get the number of lines in this Text buffer.
		@rtype: int
		"""

	def clear():
		"""
		Clear this Text object: its buffer becomes empty.
		"""

	def set(attribute, value):
		"""
		Set this Text's attributes.
		@type attribute: string
		@param attribute: The attribute to change:
			currently, 'follow_cursor' is the only one available.  It can be
			turned 'on' with value = 1 and 'off' with value = 0.
		@type value: int
		@param value: The new attribute value. 
		"""

	def write(data):
		"""
		Append a string to this Text buffer.
		@type data: string
		@param data:  The string to append to the text buffer.
		"""

	def asLines():
		"""
		Retrieve the contents of this Text buffer as a list of strings.
		@rtype: list of strings
		@return:  A list of strings, one for each line in the buffer
		"""

import id_generics
Text.__doc__ += id_generics.attributes