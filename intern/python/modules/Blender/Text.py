"""The Blender Text module

  This module lets you manipulate the Text buffers inside Blender.
  Text objects are currently owned by the Text editor in Blender.

  Example::

    from Blender import Text
    text = Text.New('Text')  # create new text buffer
    text.write('hello')      # write string
    Text.unlink(text)        # delete
"""

import _Blender.Text as _Text

class Text:
	"""Wrapper for Text DataBlock"""

	def clear(self):
		"""Clears the Text objects text buffer"""
		pass

	def write(self, string):
		"""Appends 'string' to the text buffer"""
		pass

	def asLines(self):
		"""Returns the text buffer as a list of lines (strings)"""
		pass
	
	def set(self, attr, val):
		"""Set the Text attribute of name 'name' to value 'val'.

Currently supported::

  follow_cursor :  1: Text output follows the cursor"""  

# Module methods

def New(name = None):
	"""Creates new empty Text with (optionally given) name and returns it"""
	pass

def get(name = None):
	"""Returns a Text object with name 'name' if given, 'None' if not existing,
or a list of all Text objects in Blender otherwise."""
	pass

def unlink(text):
	"""Removes the Text 'text' from the Blender text window"""
	pass


# override:
New = _Text.New
get = _Text.get
unlink = _Text.unlink
