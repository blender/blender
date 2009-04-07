class CListValue: # (PyObjectPlus)
	"""
	CListValue
	
	This is a list like object used in the game engine internally that behaves similar to a python list in most ways.
	
	As well as the normal index lookup.
	C{val= clist[i]}
	
	CListValue supports string lookups.
	C{val= scene.objects["OBCube"]}
	
	Other operations such as C{len(clist), list(clist), clist[0:10]} are also supported.
	"""
	def append(val):
		"""
		Add an item to the list (like pythons append)
		
		Warning: Appending values to the list can cause crashes when the list is used internally by the game engine.
		"""

	def count(val):
		"""
		Count the number of instances of a value in the list.
		
		@rtype: integer
		@return: number of instances
		"""
	def index(val):
		"""
		Return the index of a value in the list.
		
		@rtype: integer
		@return: The index of the value in the list.
		"""
	def reverse(val):
		"""
		Reverse the order of the list.
		"""