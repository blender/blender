from CPropValue import *

class CListValue(CPropValue):
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
	def reverse():
		"""
		Reverse the order of the list.
		"""
	def from_id(id):
		"""
		This is a funtion especially for the game engine to return a value with a spesific id.
		
		Since object names are not always unique, the id of an object can be used to get an object from the CValueList.
		
		Example.
			
		C{myObID = id(gameObject)}
		
		C{...}
		
		C{ob= scene.objects.from_id(myObID)}
		
		Where myObID is an int or long from the id function.
		
		This has the advantage that you can store the id in places you could not store a gameObject.
		
		Warning: the id is derived from a memory location and will be different each time the game engine starts.
		"""