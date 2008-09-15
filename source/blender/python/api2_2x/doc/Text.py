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

	def reset():
		"""
		Reset the read IO pointer to the start of the buffer.
		"""

	def readline():
		"""
		Reads a line of text from the buffer from the current IO pointer
		position to the end of the line. If the text has changed since the last
		read, reset() *must* be called.
		@rtype: string
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

	def insert(data):
		"""
		Inserts a string into this Text buffer at the cursor.
		@type data: string
		@param data:  The string to insert into the text buffer.
		"""

	def asLines(start=0, end=-1):
		"""
		Retrieve the contents of this Text buffer as a list of strings between
		the start and end lines specified. If end < 0 all lines from start will
		be included.
		@type start int
		@param start:  Optional index of first line of the span to return
		@type end int
		@param end:  Optional index of the line to which the span is taken or
			-1 to include all lines from start
		@rtype: list of strings
		@return:  A list of strings, one for each line in the buffer between 
			start and end.
		"""

	def getCursorPos():
		"""
		Retrieve the position of the cursor in this Text buffer.
		@rtype: (int, int)
		@return:  A pair (row, col) indexing the line and character of the
			cursor.
		"""

	def setCursorPos(row, col):
		"""
		Set the position of the cursor in this Text buffer. Any selection will
		be cleared. Use setSelectPos to extend a selection from the point
		specified here.
		@type row: int
		@param row:  The index of the line in which to position the cursor.
		@type col: int
		@param col:  The index of the character within the line to position the
			cursor.
		"""

	def getSelectPos():
		"""
		Retrieve the position of the selection cursor in this Text buffer.
		@rtype: (int, int)
		@return:  A pair (row, col) indexing the line and character of the
			selection cursor.
		"""

	def setSelectPos(row, col):
		"""
		Set the position of the selection cursor in this Text buffer. This
		method should be called after setCursorPos to extend the selection to
		the specified point.
		@type row: int
		@param row:  The index of the line in which to position the cursor.
		@type col: int
		@param col:  The index of the character within the line to position the
			cursor.
		"""

	def suggest(list, prefix=''):
		"""
		Suggest a list of names. If list is a list of tuples (name, type) the
		list will be formatted to syntax-highlight each entry type. Types must
		be strings in the list ['m', 'f', 'v', 'k', '?']. It is recommended that
		the list be sorted, case-insensitively by name.
		
		@type list: list of tuples or strings
		@param list:  List of pair-tuples of the form (name, type) where name is
			the suggested name and type is one of 'm' (module or class), 'f'
			(function or method), 'v' (variable), 'k' (keyword), '?' (other).
			Lists of plain strings are also accepted where the type is always
			'?'.
		@type prefix: string
		@param prefix: The optional prefix used to limit what is suggested from
			the list. This is usually whatever precedes the cursor so that
			backspace will update it.
		"""

	def showDocs(docs):
		"""
		Displays a word-wrapped message box containing the specified
		documentation when this Text object is visible.
		@type docs: string
		@param docs: The documentation string to display.
		"""

import id_generics
Text.__doc__ += id_generics.attributes
